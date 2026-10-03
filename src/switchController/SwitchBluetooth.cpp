
#define __BTSTACK_FILE__ "SwitchBluetooth.cpp"

#include "SwitchBluetooth.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "BtStackUtils.h"
#include "btstack.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "pico/cyw43_arch.h"
#include "pico/rand.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "btstack_tlv.h"

Controller *globalController;
static uint8_t hid_service_buffer[700];
static uint8_t pnp_service_buffer[200];
static const char hid_device_name[] = "Wireless Gamepad";
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t hid_cid;
bool SwitchBluetooth::_imu_enabled = false;
bool SwitchBluetooth::_vibration_enabled = false;
uint8_t SwitchBluetooth::_vibration_report = 0x00;
uint8_t SwitchBluetooth::_player_number = 0x00;
uint32_t SwitchBluetooth::_timer = 0;
uint32_t SwitchBluetooth::_timestamp = 0;
bool SwitchBluetooth::_device_info_queried = false;
uint8_t SwitchBluetooth::_report[100] = {0x00};
uint8_t SwitchBluetooth::_switchRequestReport[100] = {0x00};
// Direccion Bluetooth FIJA: los primeros 3 bytes son Nintendo y los ultimos 3
// salen del ID unico de esta Pico, asi la Switch siempre ve el mismo control.
bd_addr_t SwitchBluetooth::_addr = {0x7c, 0xbb, 0x8a, 0x00, 0x00, 0x00};

// --- Reconexion al ultimo host ---
#define LAST_HOST_TAG \
  ((uint32_t)'G' << 24 | (uint32_t)'C' << 16 | (uint32_t)'H' << 8 | 'T')
// LOW_POWER_MODE=1 (bateria): arranca dormido, se apaga solo y reinicia.
// LOW_POWER_MODE=0 (cable): siempre visible, sin apagado automatico.
#ifndef LOW_POWER_MODE
#define LOW_POWER_MODE 0
#endif
#define RETRY_MS 2000             // espera entre intentos de conexion
#define SEARCH_WINDOW_MS 25000    // buscar/conectar como maximo 25 s y apagar
#define SEARCH_HOLD_MS 2000       // Y+Start 2 s: buscar dispositivos NUEVOS
#define FORGET_HOLD_MS 8000       // Y+Start 8 s: ademas olvidar todos
#define OFF_HOLD_MS 3000          // A+B+Z+Start 3 s: apagar todo (bateria)
#define BOOT_HOLD_MS 3000         // A+B+Z+Start 3 s: modo carga de .uf2
#define IDLE_OFF_MS (5UL * 60UL * 1000UL)  // 5 min sin actividad: apagar
#define WAKE_POLL_MS 1000         // dormido: revisa el control 1 vez por segundo
#define WAKE_POLLS 3              // A presionado en 3 revisiones seguidas (~3 s)
#define PRESS_TICKS 3             // 0.3 s con un boton para despertar
#define ACTIVITY_DEADZONE 500     // movimiento minimo de palanca (de 2048)
typedef enum {
  MODE_CONNECTING,  // conectando al ultimo host guardado
  MODE_SEARCHING,   // visible: solo acepta dispositivos nuevos
  MODE_CONNECTED,
  MODE_WAITING,     // visible y conectable; un boton intenta conectar
  MODE_SHUTDOWN     // desconectando y reiniciando = apagado total
} BtMode;
static BtMode mode = MODE_CONNECTING;
static uint32_t mode_deadline = 0;
static uint32_t shutdown_deadline = 0;
static uint32_t last_activity = 0;
static bool bt_ready = false;
static uint32_t retry_at = 0;
static int press_ticks = 0;
static btstack_timer_source_t connection_timer;
static bool connecting = false;
static bool has_host = false;
static bool host_checked = false;
static uint32_t led_ticks = 0;
static bd_addr_t known_hosts[16];  // hosts aprendidos al iniciar la busqueda
static int known_count = 0;
typedef struct {
  uint32_t since;
  bool fired;
} Hold;
static Hold hold_search = {0, false};
static Hold hold_forget = {0, false};
static Hold hold_off = {0, false};
static Hold hold_boot = {0, false};
static bd_addr_t host_addr;
#define SEARCH_MAGIC 0x53524348u  // 'SRCH' en scratch[0]: buscar al reiniciar
static bool search_on_boot = false;
#define WAKE_MAGIC 0x57414b45u    // 'WAKE' en scratch[1]: reiniciar ya despierta
#define LINK_STALE_MS 15000       // sin datos de la Switch en 15 s = enlace muerto
#define CTRL_FAIL_LIMIT 30        // lecturas fallidas seguidas antes de re-iniciar el control
static bool skip_sleep = false;
static uint32_t last_rx = 0;
static uint32_t rx_count = 0;  // paquetes recibidos del host en esta conexion
#define STALE_MIN_RX 500           // solo una Switch manda tantos (rumble constante)
static int ctrl_fails = 0;
static int bootsel_ticks = 0;

// Lee el boton BOOTSEL con el programa en marcha (debe correr desde RAM)
static bool __no_inline_not_in_flash_func(get_bootsel_button)() {
  const uint CS_PIN_INDEX = 1;
  uint32_t flags = save_and_disable_interrupts();
  hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                  GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
  for (volatile int i = 0; i < 1000; ++i) {
  }
  bool pressed = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));
  hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                  GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
  restore_interrupts(flags);
  return pressed;
}

// Reinicio de emergencia: arranca ya despierta (sin esperar el boton A)
static void emergency_reboot() {
  watchdog_hw->scratch[1] = WAKE_MAGIC;
  watchdog_reboot(0, 0, 10);
  while (true) tight_loop_contents();
}
static bool radio_on = false;  // el chip de radio solo se enciende al despertar (bateria)
static void led(bool on) {
  if (radio_on) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}
static bool start_radio() {
  if (cyw43_arch_init()) {
    printf("failed to initialise cyw43_arch\n");
    return false;
  }
  radio_on = true;
  led(true);
  return true;
}
static bd_addr_t cur_remote;
static bool cur_remote_valid = false;
SwitchBtReport SwitchBluetooth::_switchBtReport = {
    .batteryConnection = 0x90, .buttons = {0x0}, .l = {0x0}, .r = {0x0}};

void SwitchBluetooth::init(Controller *controller) {
  search_on_boot = (watchdog_hw->scratch[0] == SEARCH_MAGIC);
  watchdog_hw->scratch[0] = 0;
  skip_sleep = (watchdog_hw->scratch[1] == WAKE_MAGIC);
  watchdog_hw->scratch[1] = 0;
  globalController = controller;

  pico_unique_board_id_t uid;
  pico_get_unique_board_id(&uid);
  _addr[3] = uid.id[5] ^ uid.id[0];
  _addr[4] = uid.id[6] ^ uid.id[1];
  _addr[5] = uid.id[7] ^ uid.id[2];

#if !LOW_POWER_MODE
  if (!start_radio()) return;  // LED encendido = la Pico esta viva
#endif

  // Preparar el control. Si no contesta: 3 parpadeos cortos, pausa, y se
  // reintenta (antes el programa se detenia en silencio).
  while (true) {
    try {
      controller->init();
      break;
    } catch (...) {
      for (int i = 0; i < 3; i++) {
        led(false);
        sleep_ms(150);
        led(true);
        sleep_ms(150);
      }
      led(false);
      sleep_ms(1000);
    }
  }
  // Arranque dormido: no se enciende solo. Espera un boton (0.3 s).
#if LOW_POWER_MODE
  // Dormido de verdad: la radio NI SIQUIERA esta iniciada. Solo se revisa el
  // control cada WAKE_POLL_MS; mantener A ~3 s lo despierta.
  if (!search_on_boot && !skip_sleep) wait_for_wake();
  if (!start_radio()) return;
  gap_discoverable_control(0);
  gap_connectable_control(0);
#else
  gap_discoverable_control(1);
  gap_connectable_control(1);
#endif
  gap_set_class_of_device(0x2508);
  gap_set_local_name("Pro Controller");
  gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_ROLE_SWITCH |
                                       LM_LINK_POLICY_ENABLE_SNIFF_MODE);
  gap_set_allow_role_switch(true);

  hci_set_chipset(btstack_chipset_cyw43_instance());
  hci_set_bd_addr(_addr);

  // L2CAP
  l2cap_init();
  sm_init();
  // SDP Server
  sdp_init();

  hid_sdp_record_t hid_sdp_record = {0x2508,
                                     33,
                                     1,
                                     1,
                                     1,
                                     0,
                                     0,
                                     0xFFFF,
                                     0xFFFF,
                                     3200,
                                     switch_bt_report_descriptor,
                                     sizeof(switch_bt_report_descriptor),
                                     hid_device_name};

  // Register SDP services
  memset(hid_service_buffer, 0, sizeof(hid_service_buffer));
  create_sdp_hid_record(hid_service_buffer, 0x10000, &hid_sdp_record);
  sdp_register_service(hid_service_buffer);

  memset(pnp_service_buffer, 0, sizeof(pnp_service_buffer));
  create_sdp_pnp_record(pnp_service_buffer, 0x10001,
                        DEVICE_ID_VENDOR_ID_SOURCE_USB, 0x057E, 0x2009, 0x0001);
  sdp_register_service(pnp_service_buffer);

  // HID Device
  hid_device_init(1, sizeof(switch_bt_report_descriptor),
                  switch_bt_report_descriptor);

  // Register callbacks
  hci_event_callback_registration.callback = &packet_handler;
  hci_add_event_handler(&hci_event_callback_registration);

  hid_device_register_packet_handler(&packet_handler);
  hid_device_register_report_data_callback(&hid_report_data_callback);

  // Temporizador de conexion (se inicia cuando el Bluetooth esta listo)
  btstack_run_loop_set_timer_handler(&connection_timer, &connection_tick);
  // El temporizador corre desde ya: si el Bluetooth no logra arrancar, el LED
  // da un latido lento (en vez de quedarse fijo).
  btstack_run_loop_set_timer(&connection_timer, 100);
  btstack_run_loop_add_timer(&connection_timer);

  // turn on!
  hci_power_control(HCI_POWER_ON);
  btstack_run_loop_execute();
}

void SwitchBluetooth::hid_report_data_callback(uint16_t cid,
                                               hid_report_type_t report_type,
                                               uint16_t report_id,
                                               int report_size,
                                               uint8_t *report) {
  memcpy(_switchRequestReport, report, report_size);
  last_rx = to_ms_since_boot(get_absolute_time());
  rx_count++;
}

void SwitchBluetooth::packet_handler(uint8_t packet_type, uint16_t channel,
                                     uint8_t *packet, uint16_t packet_size) {
  UNUSED(channel);
  UNUSED(packet_size);
  uint8_t status;
  if (packet_type != HCI_EVENT_PACKET) {
    return;
  }
  switch (packet[0]) {
    case BTSTACK_EVENT_STATE:
      break;
    case HCI_EVENT_HID_META:
      switch (hci_event_hid_meta_get_subevent_code(packet)) {
        case HID_SUBEVENT_CONNECTION_OPENED:
          status = hid_subevent_connection_opened_get_status(packet);
          if (status) {
            // la conexion fallo; el temporizador decide si reintenta
            hid_cid = 0;
            connecting = false;
            retry_at = to_ms_since_boot(get_absolute_time()) + RETRY_MS;
            return;
          }
          connecting = false;
          hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
          {
            bd_addr_t remote;
            hid_subevent_connection_opened_get_bd_addr(packet, remote);
            // Buscando: se acepta a cualquiera (antes se rechazaba a los ya
            // conocidos y la misma Switch esperaba toda la ventana de busqueda)
            if (false && mode == MODE_SEARCHING && is_known(remote)) {
              // Buscando dispositivos NUEVOS: rechazar los ya aprendidos
              hid_device_disconnect(hid_cid);
              return;
            }
            if (mode == MODE_SHUTDOWN) {
              hid_device_disconnect(hid_cid);
              return;
            }
            memcpy(cur_remote, remote, 6);
            cur_remote_valid = true;
            save_last_host(remote);
          }
          mode = MODE_CONNECTED;
          last_rx = to_ms_since_boot(get_absolute_time());
          rx_count = 0;
          last_activity = to_ms_since_boot(get_absolute_time());
#if LOW_POWER_MODE
          // Conectado: ya no hace falta estar visible ni conectable
          gap_discoverable_control(0);
          gap_connectable_control(0);
#endif
          hid_device_request_can_send_now_event(hid_cid);
          break;
        case HID_SUBEVENT_CONNECTION_CLOSED:
          hid_cid = 0;
          connecting = false;
          // Se perdio la conexion (la Switch se durmio, la desconectaron...):
          // apagado total hasta que presiones un boton
          if (mode == MODE_CONNECTED) give_up();
          break;
        case HID_SUBEVENT_GET_PROTOCOL_RESPONSE:
          break;
        case HID_SUBEVENT_CAN_SEND_NOW:
          generate_report();
          hid_device_request_can_send_now_event(hid_cid);
          break;
      }
      break;
  }
}

void SwitchBluetooth::generate_report() {
  set_empty_report();
  switch (_switchRequestReport[9]) {
    case REQUEST_DEVICE_INFO:
      _device_info_queried = true;
      set_subcommand_reply();
      set_device_info();
      break;
    case SET_SHIPMENT:
      set_subcommand_reply();
      set_shipment();
      break;
    case SPI_READ:
      set_subcommand_reply();
      spi_read();
      break;
    case SET_MODE:
      set_subcommand_reply();
      set_mode();
      break;
    case TRIGGER_BUTTONS:
      set_subcommand_reply();
      set_trigger_buttons();
      break;
    case TOGGLE_IMU:
      set_subcommand_reply();
      toggle_imu();
      break;
    case ENABLE_VIBRATION:
      set_subcommand_reply();
      enable_vibration();
      break;
    case SET_PLAYER:
      set_subcommand_reply();
      set_player_lights();
      break;
    case SET_NFC_IR_STATE:
      set_subcommand_reply();
      set_nfc_ir_state();
      break;
    case SET_NFC_IR_CONFIG:
      set_subcommand_reply();
      set_nfc_ir_config();
      break;
    default:
      set_full_input_report();
      break;
  }

  // Si el control no responde (p. ej. se esta recalibrando) NO se debe
  // congelar el firmware: se reutiliza el ultimo estado.
  try {
    globalController->getSwitchBtReport(&_switchBtReport);
    ctrl_fails = 0;
  } catch (...) {
    // El control no contesta (desconectado/reconectado): tras varios fallos
    // seguidos se repite el saludo inicial para que vuelva a funcionar.
    if (++ctrl_fails >= CTRL_FAIL_LIMIT) {
      ctrl_fails = 0;
      try {
        globalController->init();
      } catch (...) {
      }
    }
  }
  // Start solo, mantenido 2 s = Minus (reemplaza a Plus mientras dure)
  {
    static uint32_t start_since = 0;
    bool only_start = _switchBtReport.buttons[0] == 0 &&
                      _switchBtReport.buttons[1] == SWITCH_BT_MASK_PLUS &&
                      (_switchBtReport.buttons[2] & 0xCF) == 0;
    uint32_t t = to_ms_since_boot(get_absolute_time());
    if (!only_start) {
      start_since = 0;
    } else {
      if (start_since == 0) start_since = t ? t : 1;
      if (t - start_since >= 2000) {
        _switchBtReport.buttons[1] = SWITCH_BT_MASK_MINUS;
      }
    }
  }
  handle_shortcuts(_switchBtReport);
  if (has_activity(_switchBtReport)) {
    last_activity = to_ms_since_boot(get_absolute_time());
  }
  memcpy(_report + 3, (uint8_t *)&_switchBtReport, sizeof(SwitchBtReport));

  hid_device_send_interrupt_message(hid_cid, (uint8_t *)&_report, 50);
  memset(_switchRequestReport, 0x00, sizeof(_switchRequestReport));
}

void SwitchBluetooth::set_empty_report() {
  memset(_report, 0x00, sizeof(_report));
  _report[0] = 0xa1;
}

void SwitchBluetooth::set_subcommand_reply() {
  // Input Report ID
  _report[1] = 0x21;

  // TODO: Find out what the vibrator byte is doing.
  // This is a hack in an attempt to semi-emulate
  // actions of the vibrator byte as it seems to change
  // when a subcommand reply is sent.
  _vibration_report = VIB_OPTS[rand() % 4];

  set_standard_input_report();
}

void SwitchBluetooth::set_unknown_subcommand(uint8_t subcommand_id) {
  // Set NACK
  _report[14];

  // Set unknown subcommand ID
  _report[15] = subcommand_id;
}

void SwitchBluetooth::set_timer() {
  // If the timer hasn't been set before
  if (_timestamp == 0) {
    _timestamp = to_ms_since_boot(get_absolute_time());
    _report[2] = 0x00;
    return;
  }

  // Get the time that has passed since the last timestamp
  // in milliseconds
  uint32_t now = to_ms_since_boot(get_absolute_time());
  uint32_t delta_t = (now - _timestamp);

  // Get how many ticks have passed in hex with overflow at 255
  // Joy-Con uses 4.96ms as the timer tick rate
  uint32_t elapsed_ticks = int(delta_t * 4);
  _timer = (_timer + elapsed_ticks) & 0xFF;

  _report[2] = _timer;
  _timestamp = now;
}

void SwitchBluetooth::set_full_input_report() {
  // Setting Report ID to full standard input report ID
  _report[1] = 0x30;
  set_standard_input_report();
  set_imu_data();
}

void SwitchBluetooth::set_standard_input_report() {
  set_timer();

  if (_device_info_queried) {
    _report[3] = 0x90;
    _report[4] = 0x00;
    _report[5] = 0x00;
    _report[6] = 0x00;

    _report[7] = 0x00;
    _report[8] = 0x08;
    _report[9] = 0x80;

    _report[10] = 0x16;
    _report[11] = 0xd8;
    _report[12] = 0x7d;

    _report[13] = _vibration_report;
    _device_info_queried = false;
  }
}

void SwitchBluetooth::set_device_info() {
  // ACK Reply
  _report[14] = 0x82;

  // Subcommand Reply
  _report[15] = 0x02;

  // Firmware version
  _report[16] = 0x03;
  _report[17] = 0x8B;

  // Controller ID
  _report[18] = 0x03;

  // Unknown Byte, always 2
  _report[19] = 0x02;

  // Controller Bluetooth Address
  memcpy(_report + 20, _addr, 6);

  // Unknown byte, always 1
  _report[26] = 0x01;

  // Controller colours location (read from SPI)
  _report[27] = 0x01;
}

void SwitchBluetooth::set_shipment() {
  // ACK Reply
  _report[14] = 0x80;

  // Subcommand reply
  _report[15] = 0x08;
}

void SwitchBluetooth::toggle_imu() {
  _imu_enabled = _switchRequestReport[10] == 0x01;

  // ACK Reply
  _report[14] = 0x80;

  // Subcommand reply
  _report[15] = 0x40;
}

void SwitchBluetooth::set_imu_data() {
  if (!_imu_enabled) {
    return;
  }

  uint8_t imu_data[49] = {0x75, 0xFD, 0xFD, 0xFF, 0x09, 0x10, 0x21, 0x00, 0xD5,
                          0xFF, 0xE0, 0xFF, 0x72, 0xFD, 0xF9, 0xFF, 0x0A, 0x10,
                          0x22, 0x00, 0xD5, 0xFF, 0xE0, 0xFF, 0x76, 0xFD, 0xFC,
                          0xFF, 0x09, 0x10, 0x23, 0x00, 0xD5, 0xFF, 0xE0, 0xFF};
  memcpy(_report + 14, imu_data, sizeof(imu_data));
}
void SwitchBluetooth::spi_read() {
  uint8_t addr_top = _switchRequestReport[11];
  uint8_t addr_bottom = _switchRequestReport[10];
  uint8_t read_length = _switchRequestReport[14];

  // ACK byte
  _report[14] = 0x90;

  // Subcommand reply
  _report[15] = 0x10;

  // Read address
  _report[16] = addr_bottom;
  _report[17] = addr_top;

  // Read length
  _report[20] = read_length;

  // Stick Parameters
  // Params are generally the same for all sticks
  // Notable difference is the deadzone (10%
  // Joy-Con vs 15% Pro Con)
  uint8_t params[18] = {0x0F, 0x30, 0x61,  // Unused
                        0x96, 0x30,
                        0xF3,               // Dead Zone/Range Ratio
                        0xD4, 0x14, 0x54,   // X/Y ?
                        0x41, 0x15, 0x54,   // X/Y ?
                        0xC7, 0x79, 0x9C,   // X/Y ?
                        0x33, 0x36, 0x63};  // X/Y ?

  // Serial Number read
  if (addr_top == 0x60 && addr_bottom == 0x00) {
    // Switch will take this as no serial number
    memset(_report + 21, 0xff, 16);
  } else if (addr_top == 0x60 && addr_bottom == 0x50) {
    // Body colour
    memset(_report + 21, 0x82, 3);
    // Buttons colour
    memset(_report + 24, 0x0f, 3);
    // Left/right grip colours (Pro controller)
    memset(_report + 27, 0xff, 7);
  } else if (addr_top == 0x60 && addr_bottom == 0x80) {
    // Six-Axis factory parameters
    _report[21] = 0x50;
    _report[22] = 0xFD;
    _report[23] = 0x00;
    _report[24] = 0x00;
    _report[25] = 0xC6;
    _report[26] = 0x0F;

    memcpy(_report + 27, params, sizeof(params));

    // Stick device parameters 2
  } else if (addr_top == 0x60 && addr_bottom == 0x98) {
    // Setting same params since controllers always
    // have duplicates of stick params 1 for stick params 2
    memcpy(_report + 21, params, sizeof(params));

    // User analog stick calibration
  } else if (addr_top == 0x80 && addr_bottom == 0x10) {
    // Fill report with null user calibration info
    memset(_report + 21, 0xff, 3);

    // Factory analog stick calibration
  } else if (addr_top == 0x60 && addr_bottom == 0x3D) {
    // Left/right stick calibration
    uint8_t l_calibration[9] = {0xBA, 0xF5, 0x62, 0x6F, 0xC8,
                                0x77, 0xED, 0x95, 0x5B};
    uint8_t r_calibration[9] = {0x16, 0xD8, 0x7D, 0xF2, 0xB5,
                                0x5F, 0x86, 0x65, 0x5E};

    // Left stick calibration

    memcpy(_report + 21, l_calibration, sizeof(l_calibration));

    // Right stick calibration
    memcpy(_report + 30, r_calibration, sizeof(r_calibration));

    // Spacer byte
    _report[39] = 0xFF;

    // Body colour
    memset(_report + 40, 0x82, 3);
    // Buttons colour
    memset(_report + 43, 0x0f, 3);

    // Six-Axis motion sensor factor
    // calibration
  } else if (addr_top == 0x60 && addr_bottom == 0x20) {
    // 1: Acceleration origin position
    // 2: Acceleration sensitivity coefficient
    // 3: Gyro origin when still
    // 4: Gyro sensitivity coefficient
    uint8_t sa_calibration[24] = {0xD3, 0xFF, 0xD5, 0xFF, 0x55, 0x01,   // 1
                                  0x00, 0x40, 0x00, 0x40, 0x00, 0x40,   // 2
                                  0x19, 0x00, 0xDD, 0xFF, 0xDC, 0xFF,   // 3
                                  0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34};  // 4

    memcpy(_report + 21, sa_calibration, sizeof(sa_calibration));
  }
}

void SwitchBluetooth::set_mode() {
  // ACK byte
  _report[14] = 0x80;

  // Subcommand reply
  _report[15] = 0x03;
}

void SwitchBluetooth::set_trigger_buttons() {
  // ACK byte
  _report[14] = 0x83;

  // Subcommand reply
  _report[15] = 0x04;
}

void SwitchBluetooth::enable_vibration() {
  // ACK Reply
  _report[14] = 0x82;

  // Subcommand reply
  _report[15] = 0x48;

  // Set class property
  _vibration_enabled = true;
}

void SwitchBluetooth::set_player_lights() {
  // ACK byte
  _report[14] = 0x80;

  // Subcommand reply
  _report[15] = 0x30;

  uint8_t bitfield = _switchRequestReport[10];

  if (bitfield == 0x01 || bitfield == 0x10) {
    _player_number = 1;
  } else if (bitfield == 0x03 || bitfield == 0x30) {
    _player_number = 2;
  } else if (bitfield == 0x07 || bitfield == 0x70) {
    _player_number = 3;
  } else if (bitfield == 0x0F || bitfield == 0xF0) {
    _player_number = 4;
  }
}

void SwitchBluetooth::set_nfc_ir_state() {
  // ACK byte
  _report[14] = 0x80;

  // Subcommand reply
  _report[15] = 0x22;
}

void SwitchBluetooth::set_nfc_ir_config() {
  // ACK byte
  _report[14] = 0xA0;

  // Subcommand reply
  _report[15] = 0x21;

  // NFC/IR state data
  uint8_t params[8] = {0x01, 0x00, 0xFF, 0x00, 0x08, 0x00, 0x1B, 0x01};
  memcpy(_report + 16, params, sizeof(params));
  _report[49] = 0xC8;
}

// ---------------------------------------------------------------------------
// Memoria del ultimo host
// ---------------------------------------------------------------------------

void SwitchBluetooth::save_last_host(bd_addr_t host) {
  const btstack_tlv_t *tlv_impl;
  void *tlv_ctx;
  btstack_tlv_get_instance(&tlv_impl, &tlv_ctx);
  has_host = true;
  host_checked = true;
  if (!tlv_impl) return;
  bd_addr_t old;
  if (load_last_host(old) && memcmp(old, host, 6) == 0) return;  // sin cambios
  tlv_impl->store_tag(tlv_ctx, LAST_HOST_TAG, host, 6);
}

bool SwitchBluetooth::load_last_host(bd_addr_t host) {
  const btstack_tlv_t *tlv_impl;
  void *tlv_ctx;
  btstack_tlv_get_instance(&tlv_impl, &tlv_ctx);
  if (tlv_impl && tlv_impl->get_tag(tlv_ctx, LAST_HOST_TAG, host, 6) == 6) {
    return true;
  }
  // Respaldo: primer host con clave de emparejamiento guardada
  btstack_link_key_iterator_t it;
  if (!gap_link_key_iterator_init(&it)) return false;
  link_key_t key;
  link_key_type_t type;
  bool found = gap_link_key_iterator_get_next(&it, host, key, &type);
  gap_link_key_iterator_done(&it);
  return found;
}

void SwitchBluetooth::forget_pairing() {
  const btstack_tlv_t *tlv_impl;
  void *tlv_ctx;
  btstack_tlv_get_instance(&tlv_impl, &tlv_ctx);
  if (tlv_impl) tlv_impl->delete_tag(tlv_ctx, LAST_HOST_TAG);
  gap_delete_all_link_keys();
  has_host = false;
  host_checked = true;
  known_count = 0;  // ya no hay "aprendidos": todo dispositivo sera nuevo
}

// Guarda quien esta emparejado AHORA, para rechazarlos mientras se busca
void SwitchBluetooth::snapshot_known() {
  known_count = 0;
  btstack_link_key_iterator_t it;
  if (!gap_link_key_iterator_init(&it)) return;
  bd_addr_t addr;
  link_key_t key;
  link_key_type_t type;
  while (known_count < 16 &&
         gap_link_key_iterator_get_next(&it, addr, key, &type)) {
    memcpy(known_hosts[known_count++], addr, 6);
  }
  gap_link_key_iterator_done(&it);
}

bool SwitchBluetooth::is_known(bd_addr_t addr) {
  for (int i = 0; i < known_count; i++) {
    if (memcmp(known_hosts[i], addr, 6) == 0) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Lectura del control, atajos y apagado
// ---------------------------------------------------------------------------

bool SwitchBluetooth::read_buttons(SwitchBtReport *r) {
  memset(r, 0, sizeof(*r));
  try {
    globalController->getSwitchBtReport(r);
  } catch (...) {
    return false;
  }
  return true;
}

bool SwitchBluetooth::any_button(const SwitchBtReport &r) {
  return (r.buttons[0] | r.buttons[1] | r.buttons[2]) != 0;
}

// Actividad = algun boton o una palanca fuera del centro
bool SwitchBluetooth::has_activity(const SwitchBtReport &r) {
  if (any_button(r)) return true;
  int lx = r.l[0] | ((r.l[1] & 0x0F) << 8);
  int ly = (r.l[1] >> 4) | (r.l[2] << 4);
  int rx = r.r[0] | ((r.r[1] & 0x0F) << 8);
  int ry = (r.r[1] >> 4) | (r.r[2] << 4);
  int mid = SWITCH_BT_JOYSTICK_MID;
  return abs(lx - mid) > ACTIVITY_DEADZONE || abs(ly - mid) > ACTIVITY_DEADZONE ||
         abs(rx - mid) > ACTIVITY_DEADZONE || abs(ry - mid) > ACTIVITY_DEADZONE;
}

// true una sola vez cuando 'active' lleva 'ms' milisegundos seguidos
static bool hold_reached(Hold &h, bool active, uint32_t ms) {
  if (!active) {
    h.since = 0;
    h.fired = false;
    return false;
  }
  uint32_t now = to_ms_since_boot(get_absolute_time());
  if (h.since == 0) h.since = now ? now : 1;
  if (!h.fired && now - h.since >= ms) {
    h.fired = true;
    return true;
  }
  return false;
}

// Atajos (coinciden exactamente con estos botones; d-pad y palancas se ignoran)
//   Y+Start      2 s: buscar dispositivos nuevos (sin soltar)  /  8 s: olvidar todo
//   L+R+Start      5 s: apagar todo
//   A+B+Z+Start    3 s: modo carga de .uf2
// Nota: el boton Z del GC se envia como R, y el gatillo R del GC como ZR.
void SwitchBluetooth::handle_shortcuts(const SwitchBtReport &r) {
  bool y = r.buttons[0] & SWITCH_BT_MASK_Y;
  bool x = r.buttons[0] & SWITCH_BT_MASK_X;
  bool b = r.buttons[0] & SWITCH_BT_MASK_B;
  bool a = r.buttons[0] & SWITCH_BT_MASK_A;
  bool z = r.buttons[0] & SWITCH_BT_MASK_R;
  bool zr = r.buttons[0] & SWITCH_BT_MASK_ZR;
  bool plus = r.buttons[1] & SWITCH_BT_MASK_PLUS;
  bool l = r.buttons[2] & SWITCH_BT_MASK_L;
  bool zl = r.buttons[2] & SWITCH_BT_MASK_ZL;

  bool cSearch = y && plus && !x && !a && !b && !z && !l && !zl && !zr;
  bool cOff = a && b && z && plus && !x && !y && !l && !zl && !zr;
  bool cBoot = a && b && z && plus && !x && !y && !l && !zl && !zr;

  if (hold_reached(hold_forget, cSearch, FORGET_HOLD_MS)) forget_pairing();
  if (hold_reached(hold_search, cSearch, SEARCH_HOLD_MS)) start_search();
#if LOW_POWER_MODE
  // Bateria: A+B+Z+Start apaga todo
  if (hold_reached(hold_off, cOff, OFF_HOLD_MS)) begin_shutdown();
  (void)cBoot;
#else
  // Cable: A+B+Z+Start entra al modo de carga de .uf2
  (void)cOff;
  if (hold_reached(hold_boot, cBoot, BOOT_HOLD_MS)) reset_usb_boot(0, 0);
#endif
}

// Busca dispositivos NUEVOS: visible y conectable, pero rechaza a los que ya
// estaban aprendidos al empezar. No borra nada.
// Reinicio completo (radio incluido): la conexion con la Switch se corta del
// todo. Al arrancar de nuevo entra directo en modo busqueda. No borra nada.
void SwitchBluetooth::start_search() {
  if (hid_cid || mode == MODE_CONNECTED) {
    watchdog_hw->scratch[0] = SEARCH_MAGIC;
    watchdog_reboot(0, 0, 50);
    while (true) tight_loop_contents();
  }
  start_search_now();
}

void SwitchBluetooth::start_search_now() {
  snapshot_known();
  // Aseguramos que el host actual y el ultimo guardado cuenten como conocidos
  bd_addr_t last;
  if (cur_remote_valid && hid_cid && known_count < 16 && !is_known(cur_remote))
    memcpy(known_hosts[known_count++], cur_remote, 6);
  if (load_last_host(last) && known_count < 16 && !is_known(last))
    memcpy(known_hosts[known_count++], last, 6);
  mode = MODE_SEARCHING;
  mode_deadline = to_ms_since_boot(get_absolute_time()) + SEARCH_WINDOW_MS;
  connecting = false;
  gap_discoverable_control(1);
  gap_connectable_control(1);
  if (hid_cid) hid_device_disconnect(hid_cid);  // soltar la Switch actual
  // Cortar tambien el enlace Bluetooth (si no, la Switch reabre el HID sola)
  if (cur_remote_valid) {
    hci_connection_t *c =
        hci_connection_for_bd_addr_and_type(cur_remote, BD_ADDR_TYPE_ACL);
    if (c) gap_disconnect(c->con_handle);
  }
}

// Apagado total: se desconecta y reinicia la Pico. Al reiniciar queda dormida
// (sin Bluetooth) hasta que presiones un boton.
void SwitchBluetooth::begin_shutdown() {
  if (mode == MODE_SHUTDOWN) return;
  mode = MODE_SHUTDOWN;
  shutdown_deadline = to_ms_since_boot(get_absolute_time()) + 1500;
  connecting = false;
  gap_discoverable_control(0);
  gap_connectable_control(0);
  if (hid_cid) hid_device_disconnect(hid_cid);
}

// Dormido (radio sin iniciar): primero espera a que no haya botones
// presionados (para no despertar de nuevo al apagar con un atajo) y despues
// espera a que un boton lleve 0.3 s presionado.
void SwitchBluetooth::wait_for_wake() {
  int released = 0;
  while (released < PRESS_TICKS) {  // fase 1: todo suelto
    SwitchBtReport r;
    if (read_buttons(&r) && !any_button(r)) {
      released++;
    } else {
      released = 0;
    }
    sleep_ms(100);
  }
  int ticks = 0;
  while (true) {  // fase 2: revisar poco; A sostenido = despertar
    SwitchBtReport r;
    if (read_buttons(&r) && (r.buttons[0] & SWITCH_BT_MASK_A)) {
      if (++ticks >= WAKE_POLLS) return;
    } else {
      ticks = 0;
    }
    sleep_ms(WAKE_POLL_MS);
  }
}

void SwitchBluetooth::start_connect() {
  if (hid_cid != 0) return;
  if (!load_last_host(host_addr)) {
    // No hay host guardado: esperar a que la Switch nos empareje
    connecting = false;
    start_search();
    return;
  }
  connecting = true;
  uint16_t cid = 0;
  uint8_t status = hid_device_connect(host_addr, &cid);
  if (status != ERROR_CODE_SUCCESS) {
    connecting = false;
    retry_at = to_ms_since_boot(get_absolute_time()) + RETRY_MS;
  }
}

// Cable: rendirse = quedar visible y conectable. Bateria: apagar.
void SwitchBluetooth::give_up() {
#if LOW_POWER_MODE
  begin_shutdown();
#else
  enter_waiting();
#endif
}

void SwitchBluetooth::enter_waiting() {
  mode = MODE_WAITING;
  connecting = false;
  press_ticks = 0;
  gap_discoverable_control(1);
  gap_connectable_control(1);
}

void SwitchBluetooth::update_led() {
  led_ticks++;
  bool on = false;
  switch (mode) {
    case MODE_CONNECTED:
      on = true;                    // fijo
      break;
    case MODE_CONNECTING:
      on = (led_ticks / 5) % 2;     // parpadeo lento: conectando a la ultima Switch
      break;
    case MODE_SEARCHING:
    case MODE_WAITING:
      on = (led_ticks / 2) % 2;     // parpadeo rapido: buscando emparejarse
      break;
    case MODE_SHUTDOWN:
      on = led_ticks % 2;           // muy rapido: apagando
      break;
  }
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

// Cada 100 ms.
void SwitchBluetooth::connection_tick(btstack_timer_source_t *ts) {
  uint32_t now = to_ms_since_boot(get_absolute_time());

  // Esperar a que el Bluetooth termine de arrancar. Latido lento del LED
  // (un destello cada 2 s) mientras tanto; si se queda asi, el BT no arranco.
  if (!bt_ready) {
    if (hci_get_state() == HCI_STATE_WORKING) {
      bt_ready = true;
      last_rx = now;
      watchdog_enable(8000, true);  // si el firmware se congela, reinicia solo
      // Visibilidad/conexion mas rapida (ventana de escaneo ~50 %)
      gap_set_page_scan_activity(0x0200, 0x0100);
      hci_send_cmd(&hci_write_inquiry_scan_activity, 0x0200, 0x0100);
      bd_addr_t tmp;
      has_host = load_last_host(tmp);
      host_checked = true;
      last_activity = now;
      if (search_on_boot) {
        search_on_boot = false;
        start_search();  // reinicio completo pedido con Y+Start
      } else if (has_host) {
        // Al encender/despertar: conectar al ultimo host guardado
        mode = MODE_CONNECTING;
        mode_deadline = now + SEARCH_WINDOW_MS;
        retry_at = 0;
#if LOW_POWER_MODE
        gap_discoverable_control(0);
        gap_connectable_control(0);
#endif
      } else if (LOW_POWER_MODE) {
        start_search();  // sin host guardado: esperar a que nos emparejen
      } else {
        enter_waiting();
      }
    } else {
      led_ticks++;
      cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, (led_ticks % 20) == 0);
      btstack_run_loop_set_timer(ts, 100);
      btstack_run_loop_add_timer(ts);
      return;
    }
  }

  watchdog_update();
  // Boton BOOTSEL (de la placa) presionado un momento = reinicio de emergencia
  if (get_bootsel_button()) {
    if (++bootsel_ticks >= 2) emergency_reboot();
  } else {
    bootsel_ticks = 0;
  }
  switch (mode) {
    case MODE_SHUTDOWN:
      if (hid_cid == 0 || (int32_t)(now - shutdown_deadline) >= 0) {
        watchdog_reboot(0, 0, 0);  // reinicio = apagado total
        while (true) tight_loop_contents();
      }
      break;
    case MODE_CONNECTED:
#if LOW_POWER_MODE
      // 5 min sin actividad: apagar
      if ((uint32_t)(now - last_activity) >= IDLE_OFF_MS) begin_shutdown();
#endif
      break;
    case MODE_WAITING:
      if (hid_cid == 0) {
        SwitchBtReport r;
        if (read_buttons(&r)) {
          handle_shortcuts(r);
          if (mode == MODE_WAITING && any_button(r)) {
            if (++press_ticks >= PRESS_TICKS) {
              // Un boton: intentar conectar al ultimo host guardado
              press_ticks = 0;
              if (load_last_host(host_addr)) {
                mode = MODE_CONNECTING;
                mode_deadline = now + SEARCH_WINDOW_MS;
                retry_at = 0;
                connecting = false;
              }
            }
          } else {
            press_ticks = 0;
          }
        }
      }
      break;
    case MODE_CONNECTING:
    case MODE_SEARCHING:
      if ((int32_t)(now - mode_deadline) >= 0) {
        give_up();  // no logro conectar a tiempo
        break;
      }
      if (hid_cid == 0) {
        SwitchBtReport r;
        if (read_buttons(&r)) handle_shortcuts(r);
      }
      if (mode == MODE_CONNECTING && !connecting &&
          (int32_t)(now - retry_at) >= 0) {
        start_connect();
      }
      break;
  }
  update_led();
  btstack_run_loop_set_timer(ts, 100);
  btstack_run_loop_add_timer(ts);
}
