// Pruebas de la conversion al reporte de Xbox por Bluetooth (corren en la PC):
//   g++ -std=c++17 -Iinclude tests/test_xbox_bt.cpp -o test_xbox_bt && ./test_xbox_bt
#include <cstdio>
#include <cstring>

#include "XboxBtReporte.h"

static int fallos = 0;
#define CHECK(c, msg)                                  \
  do {                                                 \
    if (!(c)) {                                        \
      printf("FALLA: %s (linea %d)\n", msg, __LINE__); \
      fallos++;                                        \
    } else {                                           \
      printf("ok: %s\n", msg);                         \
    }                                                  \
  } while (0)

static XboxBtReport conv(uint16_t botones, int16_t lx = 0, int16_t ly = 0, uint8_t lt = 0, uint8_t rt = 0) {
  XInputReport x;
  memset(&x, 0, sizeof(x));
  x.botones = botones;
  x.lx = lx;
  x.ly = ly;
  x.lt = lt;
  x.rt = rt;
  XboxBtReport o;
  aXboxBt(x, &o);
  return o;
}

int main() {
  XboxBtReport o = conv(0);
  CHECK(o.lx == 32768 && o.ly == 32768 && o.rx == 32768 && o.ry == 32768, "sticks en reposo -> centro 32768");
  CHECK(o.hat == 0 && o.botones == 0 && o.lt == 0 && o.rt == 0, "sin botones -> todo en 0");
  o = conv(0, 32767, 32767);
  CHECK(o.lx == 65535, "stick a la derecha -> 65535");
  CHECK(o.ly == 1, "stick arriba -> Y casi 0 (HID: arriba = minimo)");
  o = conv(0, -32767, -32767);
  CHECK(o.lx == 1 && o.ly == 65535, "stick abajo-izquierda -> X minimo, Y maximo");
  o = conv(0, 0, 0, 255, 128);
  CHECK(o.lt == 1023 && o.rt == 513, "gatillos 0-255 -> 0-1023");
  CHECK(conv(XB_DPAD_ARRIBA).hat == 1, "d-pad arriba -> 1");
  CHECK(conv(XB_DPAD_ARRIBA | XB_DPAD_DER).hat == 2, "d-pad arriba-derecha -> 2");
  CHECK(conv(XB_DPAD_DER).hat == 3, "d-pad derecha -> 3");
  CHECK(conv(XB_DPAD_ABAJO).hat == 5, "d-pad abajo -> 5");
  CHECK(conv(XB_DPAD_IZQ).hat == 7, "d-pad izquierda -> 7");
  CHECK(conv(XB_DPAD_ARRIBA | XB_DPAD_IZQ).hat == 8, "d-pad arriba-izquierda -> 8");
  CHECK(conv(XB_A).botones == 0x0001 && conv(XB_B).botones == 0x0002, "A y B -> bits 1 y 2");
  CHECK(conv(XB_X).botones == 0x0008 && conv(XB_Y).botones == 0x0010, "X y Y -> bits 4 y 5");
  CHECK(conv(XB_LB).botones == 0x0040 && conv(XB_RB).botones == 0x0080, "LB y RB -> bits 7 y 8");
  CHECK(conv(XB_BACK).botones == 0x0400 && conv(XB_START).botones == 0x0800, "Back y Start -> bits 11 y 12");
  CHECK(conv(XB_GUIA).botones == 0x1000, "boton Xbox -> bit 13");
  CHECK(conv(XB_L3).botones == 0x2000 && conv(XB_R3).botones == 0x4000, "L3 y R3 -> bits 14 y 15");
  CHECK(conv(XB_DPAD_ARRIBA).botones == 0, "el d-pad va en el hat, no en los botones");
  printf("\n%s (%d fallos)\n", fallos ? "HAY FALLOS" : "TODO BIEN", fallos);
  return fallos ? 1 : 0;
}
