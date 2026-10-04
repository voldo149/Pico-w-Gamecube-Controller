#ifndef XboxBtReporte_h
#define XboxBtReporte_h

#include <stdint.h>

#include "XInput.h"

// Conversion del reporte XInput (el que arma el remapeo) al reporte del control
// de Xbox por Bluetooth. No depende del hardware: se prueba en la PC.

// Si el stick sale al reves por Bluetooth (arriba = abajo), cambia esto a 0
#ifndef XBOX_BT_INVERTIR_Y
#define XBOX_BT_INVERTIR_Y 1
#endif

// Reporte de entrada (id 1), 16 bytes, en el orden del descriptor
struct __attribute__((packed)) XboxBtReport {
  uint16_t lx, ly;  // 0..65535, centro 32768
  uint16_t rx, ry;
  uint16_t lt, rt;  // 0..1023
  uint8_t hat;      // 0 = suelto, 1 = arriba ... 8 = arriba-izquierda (sentido horario)
  uint16_t botones;
  uint8_t compartir;
};
static_assert(sizeof(XboxBtReport) == 16, "El reporte de Xbox BT debe medir 16 bytes");

// Botones del reporte Bluetooth (no es el mismo orden que XInput por USB)
#define XBT_A 0x0001
#define XBT_B 0x0002
#define XBT_X 0x0008
#define XBT_Y 0x0010
#define XBT_LB 0x0040
#define XBT_RB 0x0080
#define XBT_BACK 0x0400
#define XBT_START 0x0800
#define XBT_GUIA 0x1000
#define XBT_L3 0x2000
#define XBT_R3 0x4000

static inline uint16_t aEjeBt(int16_t v, bool invertir) {
  int32_t r = invertir ? 32768 - (int32_t)v : (int32_t)v + 32768;
  if (r < 0) r = 0;
  if (r > 65535) r = 65535;
  return (uint16_t)r;
}

static inline uint8_t aHat(uint16_t b) {
  const bool arriba = b & XB_DPAD_ARRIBA, abajo = b & XB_DPAD_ABAJO;
  const bool izq = b & XB_DPAD_IZQ, der = b & XB_DPAD_DER;
  if (arriba && der) return 2;
  if (abajo && der) return 4;
  if (abajo && izq) return 6;
  if (arriba && izq) return 8;
  if (arriba) return 1;
  if (der) return 3;
  if (abajo) return 5;
  if (izq) return 7;
  return 0;
}

static inline void aXboxBt(const XInputReport &x, XboxBtReport *o) {
  o->lx = aEjeBt(x.lx, false);
  o->ly = aEjeBt(x.ly, XBOX_BT_INVERTIR_Y);
  o->rx = aEjeBt(x.rx, false);
  o->ry = aEjeBt(x.ry, XBOX_BT_INVERTIR_Y);
  o->lt = (uint16_t)(x.lt * 1023 / 255);
  o->rt = (uint16_t)(x.rt * 1023 / 255);
  o->hat = aHat(x.botones);
  const uint16_t b = x.botones;
  o->botones = (b & XB_A ? XBT_A : 0) | (b & XB_B ? XBT_B : 0) |
               (b & XB_X ? XBT_X : 0) | (b & XB_Y ? XBT_Y : 0) |
               (b & XB_LB ? XBT_LB : 0) | (b & XB_RB ? XBT_RB : 0) |
               (b & XB_BACK ? XBT_BACK : 0) | (b & XB_START ? XBT_START : 0) |
               (b & XB_GUIA ? XBT_GUIA : 0) | (b & XB_L3 ? XBT_L3 : 0) |
               (b & XB_R3 ? XBT_R3 : 0);
  o->compartir = 0;
}

#endif
