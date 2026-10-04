// Control de Xbox por Bluetooth LE (HID over GATT) con BTstack en la Pico W.
//
// Windows reconoce los controles de Xbox Series por Bluetooth LE y les pone su
// driver de Xbox (XInput). Para eso imitamos lo que publica el control real:
// PnP ID 045E:0B13 (version 0x0509) y su descriptor HID. El descriptor viene
// del proyecto ESP32-BLE-CompositeHID (Mystfit, licencia MIT).
//
// El mapeo y la capa Z son los mismos del modo USB: include/MapeoXInput.h.

#include "XboxBle.h"

#include <string.h>

#include "AtajoBootsel.h"
#include "Remapeo.h"
#include "XboxBtReporte.h"
#include "btstack.h"
extern "C" {
#include "pico/btstack_chipset_cyw43.h"
}
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "xbox_ble.h"  // generado desde xbox_ble.gatt

// Cada cuanto se lee el control (ms)
#define PERIODO_MS 4
// Aunque nada cambie, se manda un reporte cada tanto (ms)
#define REENVIO_MS 100

#define XBOX_REPORTE_ENTRADA 0x01
#define XBOX_REPORTE_SALIDA 0x03

static const uint8_t xbox_hid_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x85, XBOX_REPORTE_ENTRADA,  //   Report ID (1)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x00,        //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65535)
    0x95, 0x02,        //     Report Count (2)
    0x75, 0x10,        //     Report Size (16)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0xC0,              //   End Collection
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x32,        //     Usage (Z)
    0x09, 0x35,        //     Usage (Rz)
    0x15, 0x00,        //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65535)
    0x95, 0x02,        //     Report Count (2)
    0x75, 0x10,        //     Report Size (16)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0xC0,              //   End Collection
    0x05, 0x02,        //   Usage Page (Simulation Controls)
    0x09, 0xC5,        //   Usage (Brake) = gatillo izquierdo
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x0A,        //   Report Size (10)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const) relleno
    0x05, 0x02,        //   Usage Page (Simulation Controls)
    0x09, 0xC4,        //   Usage (Accelerator) = gatillo derecho
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x0A,        //   Report Size (10)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const) relleno
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x09, 0x39,        //   Usage (Hat switch) = d-pad
    0x15, 0x01,        //   Logical Minimum (1)
    0x25, 0x08,        //   Logical Maximum (8)
    0x35, 0x00,        //   Physical Minimum (0)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x66, 0x14, 0x00,  //   Unit (Degrees)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x42,        //   Input (Data,Var,Abs,Null)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x35, 0x00,        //   Physical Minimum (0)
    0x45, 0x00,        //   Physical Maximum (0)
    0x65, 0x00,        //   Unit (None)
    0x81, 0x03,        //   Input (Const) relleno
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x0F,        //   Usage Maximum (15)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0F,        //   Report Count (15)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const) relleno
    0x05, 0x0C,        //   Usage Page (Consumer)
    0x0A, 0xB2, 0x00,  //   Usage (Record) = boton Compartir
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x01,        //   Report Size (1)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x07,        //   Report Size (7)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const) relleno
    0x05, 0x0F,        //   Usage Page (Physical Interface Device)
    0x09, 0x21,        //   Usage (Set Effect Report)
    0x85, XBOX_REPORTE_SALIDA,  //   Report ID (3): vibracion
    0xA1, 0x02,        //   Collection (Logical)
    0x09, 0x97,        //     Usage (DC Enable Actuators)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x00,        //     Logical Maximum (0)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x03,        //     Output (Const)
    0x09, 0x70,        //     Usage (Magnitude)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x64,        //     Logical Maximum (100)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x04,        //     Report Count (4)
    0x91, 0x02,        //     Output (Data,Var,Abs)
    0x09, 0x50,        //     Usage (Duration)
    0x66, 0x01, 0x10,  //     Unit (Seconds)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs)
    0x09, 0xA7,        //     Usage (Start Delay)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs)
    0x65, 0x00,        //     Unit (None)
    0x55, 0x00,        //     Unit Exponent (0)
    0x09, 0x7C,        //     Usage (Loop Count)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs)
    0xC0,              //   End Collection
    0xC0,              // End Collection
};

// ===================== Estado =====================

static GamecubeController *_controller;
static Remapeo _remapeo;
static GcState _gc;
static bool _gcListo = false;
static uint8_t _fallos = 0;

static hci_con_handle_t _con = HCI_CON_HANDLE_INVALID;
static bool _reportesActivos = false;
static bool _pendiente = false;
static XboxBtReport _reporte;
static XboxBtReport _enviado;
static uint32_t _ultimoEnvio = 0;

static btstack_timer_source_t _timer;
static btstack_packet_callback_registration_t _hciCallback;
static btstack_packet_callback_registration_t _smCallback;
static hids_device_report_t _reportesHid[2];

static const uint8_t adv_data[] = {
    // Flags: general discoverable, sin BR/EDR
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    // Apariencia: HID Gamepad (0x03C4)
    0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xC4, 0x03,
    // Servicio HID (0x1812)
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x12, 0x18,
};

static const uint8_t scan_resp_data[] = {
    0x19, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'X', 'b', 'o', 'x', ' ', 'W', 'i', 'r', 'e', 'l', 'e', 's', 's', ' ',
    'C', 'o', 'n', 't', 'r', 'o', 'l', 'l', 'e', 'r',
};

static void led(bool on) { cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on); }

// ===================== Bucle (temporizador de BTstack) =====================

static void tick(btstack_timer_source_t *ts) {
  const uint32_t ahora = to_ms_since_boot(get_absolute_time());

  try {
    if (!_gcListo) {
      _controller->init();  // reintenta hasta que el control de GC responda
      _gcListo = true;
    }
    _controller->getGcState(&_gc);
    _fallos = 0;
  } catch (...) {
    // Un fallo suelto se ignora; si el control se desconecta, todo se suelta
    if (++_fallos >= 3) {
      memset(&_gc, 0, sizeof(_gc));
      _gcListo = false;
      _fallos = 3;
    }
  }

  revisarAtajoBootsel(_gc, ahora);
  XInputReport x;
  _remapeo.actualizar(_gc, ahora, &x);
  aXboxBt(x, &_reporte);

  if (_con != HCI_CON_HANDLE_INVALID && _reportesActivos && !_pendiente &&
      (memcmp(&_reporte, &_enviado, sizeof(_reporte)) != 0 ||
       ahora - _ultimoEnvio >= REENVIO_MS)) {
    _pendiente = true;
    hids_device_request_can_send_now_event(_con);
  }

  // LED: fijo = conectado, parpadeo = esperando conexion
  led(_reportesActivos ? true : ((ahora / 500) % 2 == 0));

  btstack_run_loop_set_timer(ts, PERIODO_MS);
  btstack_run_loop_add_timer(ts);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet,
                           uint16_t size) {
  (void)channel;
  (void)size;
  if (packet_type != HCI_EVENT_PACKET) return;

  switch (hci_event_packet_get_type(packet)) {
    case HCI_EVENT_DISCONNECTION_COMPLETE:
      _con = HCI_CON_HANDLE_INVALID;
      _reportesActivos = false;
      _pendiente = false;
      break;
    case SM_EVENT_JUST_WORKS_REQUEST:
      sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
      break;
    case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
      sm_numeric_comparison_confirm(sm_event_numeric_comparison_request_get_handle(packet));
      break;
    case HCI_EVENT_HIDS_META:
      switch (hci_event_hids_meta_get_subevent_code(packet)) {
        case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
          _con = hids_subevent_input_report_enable_get_con_handle(packet);
          _reportesActivos = hids_subevent_input_report_enable_get_enable(packet);
          _pendiente = false;
          if (_reportesActivos) {
            // Pedir intervalo corto (7.5 a 11.25 ms) para menos retraso
            gap_request_connection_parameter_update(_con, 6, 9, 0, 200);
          }
          break;
        case HIDS_SUBEVENT_CAN_SEND_NOW:
          if (_con != HCI_CON_HANDLE_INVALID) {
            hids_device_send_input_report_for_id(_con, XBOX_REPORTE_ENTRADA,
                                                 (const uint8_t *)&_reporte,
                                                 sizeof(_reporte));
            _enviado = _reporte;
            _ultimoEnvio = to_ms_since_boot(get_absolute_time());
          }
          _pendiente = false;
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

// ===================== Arranque =====================

void XboxBle::init(GamecubeController *controller) {
  _controller = controller;
  memset(&_gc, 0, sizeof(_gc));
  memset(&_enviado, 0xFF, sizeof(_enviado));  // fuerza el primer envio

  if (cyw43_arch_init()) return;
  led(true);

  // Direccion propia (distinta a la del modo Switch, para que la PC no los confunda)
  pico_unique_board_id_t uid;
  pico_get_unique_board_id(&uid);
  bd_addr_t addr = {0x7C, 0xBB, 0x8B, (uint8_t)(uid.id[5] ^ uid.id[0]),
                    (uint8_t)(uid.id[6] ^ uid.id[1]), (uint8_t)(uid.id[7] ^ uid.id[2])};
  hci_set_chipset(btstack_chipset_cyw43_instance());
  hci_set_bd_addr(addr);

  l2cap_init();

  // Emparejamiento sin PIN ("Just Works") y recordando a la PC
  sm_init();
  sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
  sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

  att_server_init(profile_data, NULL, NULL);

  battery_service_server_init(100);

  device_information_service_server_init();
  device_information_service_server_set_manufacturer_name("Microsoft");
  device_information_service_server_set_model_number("1914");
  device_information_service_server_set_firmware_revision("5.9.2709.0");
  device_information_service_server_set_pnp_id(DEVICE_ID_VENDOR_ID_SOURCE_USB, 0x045E,
                                               0x0B13, 0x0509);

  hids_device_init_with_storage(0, xbox_hid_descriptor, sizeof(xbox_hid_descriptor),
                                sizeof(_reportesHid) / sizeof(_reportesHid[0]), _reportesHid);

  bd_addr_t sin_direccion;
  memset(sin_direccion, 0, sizeof(sin_direccion));
  gap_advertisements_set_params(0x0030, 0x0030, 0, 0, sin_direccion, 0x07, 0x00);
  gap_advertisements_set_data(sizeof(adv_data), (uint8_t *)adv_data);
  gap_scan_response_set_data(sizeof(scan_resp_data), (uint8_t *)scan_resp_data);
  gap_advertisements_enable(1);

  _hciCallback.callback = &packet_handler;
  hci_add_event_handler(&_hciCallback);
  _smCallback.callback = &packet_handler;
  sm_add_event_handler(&_smCallback);
  hids_device_register_packet_handler(packet_handler);

  btstack_run_loop_set_timer_handler(&_timer, &tick);
  btstack_run_loop_set_timer(&_timer, PERIODO_MS);
  btstack_run_loop_add_timer(&_timer);

  hci_power_control(HCI_POWER_ON);
  btstack_run_loop_execute();
}
