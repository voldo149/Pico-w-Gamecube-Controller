#ifndef XInput_h
#define XInput_h

#include <stdint.h>

// Botones del control de Xbox 360 (bits del reporte XInput)
#define XB_DPAD_ARRIBA 0x0001
#define XB_DPAD_ABAJO 0x0002
#define XB_DPAD_IZQ 0x0004
#define XB_DPAD_DER 0x0008
#define XB_START 0x0010
#define XB_BACK 0x0020
#define XB_L3 0x0040
#define XB_R3 0x0080
#define XB_LB 0x0100
#define XB_RB 0x0200
#define XB_GUIA 0x0400  // botón Xbox (centro)
#define XB_A 0x1000
#define XB_B 0x2000
#define XB_X 0x4000
#define XB_Y 0x8000

// Gatillos como "botones" de salida. Si los activa L/R del GC van analógicos;
// desde cualquier otro botón van al 100%.
#define XB_LT 0x10000
#define XB_RT 0x20000

#define NADA 0

// Reporte de entrada XInput (20 bytes)
struct __attribute__((packed)) XInputReport {
  uint8_t reportId;    // siempre 0
  uint8_t reportSize;  // siempre 20
  uint16_t botones;
  uint8_t lt;
  uint8_t rt;
  int16_t lx;
  int16_t ly;
  int16_t rx;
  int16_t ry;
  uint8_t reservado[6];
};

static_assert(sizeof(XInputReport) == 20, "El reporte XInput debe medir 20 bytes");

#endif
