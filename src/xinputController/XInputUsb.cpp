// Control de Xbox 360 (XInput) por USB con TinyUSB.
// XInput no es HID: usa una interfaz "vendor" de Microsoft (clase 0xFF,
// subclase 0x5D, protocolo 0x01) con dos endpoints interrupt. Windows le pone
// su driver de Xbox 360 por el VID/PID 045E:028E.

#include "XInputUsb.h"

#include <string.h>

#include "AtajoBootsel.h"
#include "ModoPc.h"
#include "hardware/watchdog.h"
#include "device/usbd_pvt.h"
#include "pico/stdlib.h"
#include "tusb.h"

// ===================== Descriptores =====================

static const tusb_desc_device_t xinput_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xFF,
    .bDeviceSubClass = 0xFF,
    .bDeviceProtocol = 0xFF,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x045E,   // Microsoft
    .idProduct = 0x028E,  // Xbox 360 Controller
    .bcdDevice = 0x0114,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01};

#define XINPUT_EP_IN 0x81
#define XINPUT_EP_OUT 0x01

static const uint8_t xinput_configuration_descriptor[] = {
    // Configuracion: 1 interfaz, alimentada por el bus, 500 mA
    0x09, TUSB_DESC_CONFIGURATION, 0x31, 0x00, 0x01, 0x01, 0x00, 0x80, 0xFA,
    // Interfaz 0: XInput (vendor 0xFF / 0x5D / 0x01), 2 endpoints
    0x09, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, 0xFF, 0x5D, 0x01, 0x00,
    // Descriptor propio de XInput (tipo 0x21) como el de un control original
    0x11, 0x21, 0x00, 0x01, 0x01, 0x25, XINPUT_EP_IN, 0x14, 0x00, 0x00, 0x00,
    0x00, 0x13, XINPUT_EP_OUT, 0x08, 0x00, 0x00,
    // Endpoint IN (reportes del control), 32 bytes, cada 1 ms
    0x07, TUSB_DESC_ENDPOINT, XINPUT_EP_IN, TUSB_XFER_INTERRUPT, 0x20, 0x00,
    0x01,
    // Endpoint OUT (vibracion y LEDs que manda la PC), 32 bytes, cada 8 ms
    0x07, TUSB_DESC_ENDPOINT, XINPUT_EP_OUT, TUSB_XFER_INTERRUPT, 0x20, 0x00,
    0x08,
};
static_assert(sizeof(xinput_configuration_descriptor) == 0x31,
              "wTotalLength del descriptor de configuracion no coincide");

static const char xinput_idioma[] = {0x09, 0x04};  // ingles (0x0409)
static const char *xinput_strings[] = {
    xinput_idioma,               // 0: idioma
    "Pico GC",                   // 1: fabricante
    "GC a Xbox 360",             // 2: producto
    "GC360-0001",                // 3: numero de serie
};

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&xinput_device_descriptor;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return xinput_configuration_descriptor;
}

static uint16_t _desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  uint8_t chr_count;
  if (index == 0) {
    memcpy(&_desc_str[1], xinput_strings[0], 2);
    chr_count = 1;
  } else {
    // 0xEE (descriptor de Microsoft OS) y demas: no los tenemos
    if (index >= sizeof(xinput_strings) / sizeof(xinput_strings[0])) return NULL;
    const char *str = xinput_strings[index];
    chr_count = (uint8_t)strlen(str);
    if (chr_count > 31) chr_count = 31;
    for (uint8_t i = 0; i < chr_count; i++) _desc_str[1 + i] = str[i];
  }
  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}

// ===================== Driver de clase XInput para TinyUSB =====================

static uint8_t _epIn = 0;
static uint8_t _epOut = 0;
CFG_TUSB_MEM_ALIGN static uint8_t _bufIn[32];
CFG_TUSB_MEM_ALIGN static uint8_t _bufOut[32];

static void xinput_init(void) { _epIn = _epOut = 0; }

static bool xinput_deinit(void) { return true; }

static void xinput_reset(uint8_t rhport) {
  (void)rhport;
  _epIn = _epOut = 0;
}

static uint16_t xinput_open(uint8_t rhport, tusb_desc_interface_t const *itf,
                            uint16_t max_len) {
  if (itf->bInterfaceClass != 0xFF || itf->bInterfaceSubClass != 0x5D ||
      itf->bInterfaceProtocol != 0x01) {
    return 0;
  }
  uint8_t const *p = (uint8_t const *)itf;
  uint16_t len = tu_desc_len(p);
  p = tu_desc_next(p);
  uint8_t abiertos = 0;
  while (len < max_len && abiertos < itf->bNumEndpoints) {
    if (tu_desc_type(p) == TUSB_DESC_ENDPOINT) {
      tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)p;
      TU_ASSERT(usbd_edpt_open(rhport, ep), 0);
      if (tu_edpt_dir(ep->bEndpointAddress) == TUSB_DIR_IN) {
        _epIn = ep->bEndpointAddress;
      } else {
        _epOut = ep->bEndpointAddress;
      }
      abiertos++;
    }
    len += tu_desc_len(p);
    p = tu_desc_next(p);
  }
  // Escuchar lo que mande la PC (vibracion/LEDs); por ahora se ignora
  if (_epOut) usbd_edpt_xfer(rhport, _epOut, _bufOut, sizeof(_bufOut));
  return len;
}

static bool xinput_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                   tusb_control_request_t const *request) {
  (void)rhport;
  (void)stage;
  (void)request;
  return false;  // sin peticiones de control propias
}

static bool xinput_xfer_cb(uint8_t rhport, uint8_t ep_addr,
                           xfer_result_t result, uint32_t xferred_bytes) {
  (void)result;
  (void)xferred_bytes;
  if (ep_addr == _epOut) {
    usbd_edpt_xfer(rhport, _epOut, _bufOut, sizeof(_bufOut));
  }
  return true;
}

static usbd_class_driver_t _xinputDriver;

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
  _xinputDriver.name = "XInput";
  _xinputDriver.init = xinput_init;
  _xinputDriver.deinit = xinput_deinit;
  _xinputDriver.reset = xinput_reset;
  _xinputDriver.open = xinput_open;
  _xinputDriver.control_xfer_cb = xinput_control_xfer_cb;
  _xinputDriver.xfer_cb = xinput_xfer_cb;
  _xinputDriver.sof = NULL;
  *driver_count = 1;
  return &_xinputDriver;
}

static bool enviarReporte(const XInputReport &reporte) {
  if (!tud_ready() || _epIn == 0 || usbd_edpt_busy(0, _epIn)) return false;
  memcpy(_bufIn, &reporte, sizeof(reporte));
  return usbd_edpt_xfer(0, _epIn, _bufIn, sizeof(reporte));
}

// ===================== Bucle principal =====================

static bool _usbIniciado = false;

void XInputUsb::iniciarUsb() {
  if (_usbIniciado) return;
  tusb_init();
  _usbIniciado = true;
}

bool XInputUsb::hostPresente() {
  if (!_usbIniciado) return false;
  tud_task();
  return tud_mounted();
}

bool XInputUsb::esperarHost(uint32_t ms) {
  iniciarUsb();
  const uint32_t hasta = to_ms_since_boot(get_absolute_time()) + ms;
  while ((int32_t)(to_ms_since_boot(get_absolute_time()) - hasta) < 0) {
    watchdog_update();
    if (hostPresente()) return true;
    sleep_us(500);
  }
  return false;
}

#ifdef GC_PC
// Firmware combinado: si la PC deja de atender el USB (cable desconectado con
// la pila puesta) por este tiempo, se reinicia para pasar a Bluetooth.
#define USB_SIN_PC_MS 5000
#endif

void XInputUsb::init() {
  iniciarUsb();
  // Vigilante: si algo se congela mas de 2 s, la Pico se reinicia sola
  watchdog_enable(2000, true);

  GcState gc;
  memset(&gc, 0, sizeof(gc));
  XInputReport reporte;
  uint32_t suspendidoDesde = 0;

  while (true) {
    watchdog_update();
    tud_task();
    _lector.leer(&gc);

    const uint32_t ahora = to_ms_since_boot(get_absolute_time());
    if (tud_suspended()) {
      // Despertar a la PC solo si se presiona algo (antes se pedia sin parar)
      if (botonesAtajo(gc) != 0) tud_remote_wakeup();
      if (suspendidoDesde == 0) suspendidoDesde = ahora ? ahora : 1;
#ifdef GC_PC
      if (ahora - suspendidoDesde >= USB_SIN_PC_MS) {
        watchdog_hw->scratch[SCRATCH_MODO] = 0;
        watchdog_reboot(0, 0, 10);
        while (true) tight_loop_contents();
      }
#endif
    } else {
      suspendidoDesde = 0;
    }

    revisarAtajoBootsel(gc, ahora);
    _remapeo.actualizar(gc, ahora, &reporte);
    enviarReporte(reporte);
  }
}
