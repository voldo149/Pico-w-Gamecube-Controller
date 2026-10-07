// Pruebas del remapeo (capa Z, toque/mantener, gatillos, sticks) que corren en la PC, sin la Pico:
//   g++ -std=c++17 -Iinclude src/xinputController/Remapeo.cpp tests/test_remapeo.cpp -o test_remapeo && ./test_remapeo
// Están escritas para el mapeo por defecto de include/MapeoXInput.h.
#include <cstdio>

#include "MapeoXInput.h"
#include "Remapeo.h"

static int fallos = 0;
#define CHECK(c, msg)                                        \
  do {                                                       \
    if (!(c)) {                                              \
      printf("FALLA: %s (linea %d)\n", msg, __LINE__);       \
      fallos++;                                              \
    } else {                                                 \
      printf("ok: %s\n", msg);                               \
    }                                                        \
  } while (0)

static GcState gc(uint32_t b, uint8_t la = 0, uint8_t ra = 0, float cx = 0, float cy = 0) {
  GcState s{};
  s.botones = b;
  s.lAnalog = la;
  s.rAnalog = ra;
  s.cx = cx;
  s.cy = cy;
  return s;
}
#define B(x) GC_BIT(x)

int main() {
  XInputReport r;
  {
    Remapeo m;
    m.actualizar(gc(B(GC_A)), 0, &r);
    CHECK(r.botones == XB_A, "A -> A al instante");
    m.actualizar(gc(B(GC_X)), 10, &r);
    CHECK(r.botones == XB_X, "X sin Start -> X");
    m.actualizar(gc(B(GC_Z)), 20, &r);
    CHECK(r.botones == XB_RB, "Z -> RB al instante");
    m.actualizar(gc(B(GC_DPAD_ARRIBA)), 30, &r);
    CHECK(r.botones == XB_DPAD_ARRIBA, "D-pad arriba sin Start -> D-pad arriba");
  }
  {
    Remapeo m;  // Start solo no manda nada, ni al presionar, ni mantenido, ni al soltar
    m.actualizar(gc(B(GC_START)), 0, &r);
    CHECK(r.botones == 0, "Start presionado: nada");
    m.actualizar(gc(B(GC_START)), 1000, &r);
    CHECK(r.botones == 0, "Start mantenido 1 s: nada");
    m.actualizar(gc(0), 1010, &r);
    m.actualizar(gc(0), 1020, &r);
    CHECK(r.botones == 0, "Start soltado: nada");
  }
  {
    Remapeo m;  // capa Start instantanea
    m.actualizar(gc(B(GC_START)), 0, &r);
    m.actualizar(gc(B(GC_START) | B(GC_Y)), 1, &r);
    CHECK(r.botones == XB_START, "Start + Y -> Start al instante");
    m.actualizar(gc(B(GC_START) | B(GC_Y) | B(GC_X)), 2, &r);
    CHECK(r.botones == (XB_START | XB_BACK), "Start + Y + X -> Start + Back");
    m.actualizar(gc(B(GC_Y) | B(GC_X)), 3, &r);
    CHECK(r.botones == (XB_START | XB_BACK), "suelto Start: Y y X siguen como Start/Back hasta soltarlos");
    m.actualizar(gc(0), 4, &r);
    m.actualizar(gc(B(GC_Y)), 5, &r);
    CHECK(r.botones == XB_Y, "Y sin Start otra vez -> Y");
  }
  {
    Remapeo m;  // todos los combos con Start
    m.actualizar(gc(B(GC_START)), 0, &r);
    m.actualizar(gc(B(GC_START) | B(GC_DPAD_ARRIBA)), 10, &r);
    CHECK(r.botones == XB_GUIA, "Start + D-pad arriba -> boton Xbox (Home)");
    m.actualizar(gc(B(GC_START)), 20, &r);
    m.actualizar(gc(B(GC_START) | B(GC_DPAD_IZQ)), 30, &r);
    CHECK(r.botones == XB_L3, "Start + D-pad izq -> L3");
    m.actualizar(gc(B(GC_START)), 40, &r);
    m.actualizar(gc(B(GC_START) | B(GC_DPAD_DER)), 50, &r);
    CHECK(r.botones == XB_R3, "Start + D-pad der -> R3");
    m.actualizar(gc(B(GC_START)), 60, &r);
    m.actualizar(gc(B(GC_START) | B(GC_Z)), 70, &r);
    CHECK(r.botones == XB_R3, "Start + Z -> R3");
    m.actualizar(gc(B(GC_START)), 80, &r);
    m.actualizar(gc(B(GC_START) | B(GC_L), 0xD0, 0), 90, &r);
    CHECK(r.botones == XB_LB && r.lt == 0, "Start + L -> LB (sin LT)");
    m.actualizar(gc(B(GC_START)), 100, &r);
    m.actualizar(gc(B(GC_START) | B(GC_R), 0, 0xD0), 110, &r);
    CHECK(r.botones == XB_RB && r.rt == 0, "Start + R -> RB (sin RT)");
    m.actualizar(gc(B(GC_START)), 120, &r);
    m.actualizar(gc(B(GC_START) | B(GC_X)), 130, &r);
    CHECK(r.botones == XB_BACK, "Start + X -> Back (-)");
    m.actualizar(gc(B(GC_START)), 140, &r);
    m.actualizar(gc(B(GC_START) | B(GC_A) | B(GC_DPAD_ABAJO)), 150, &r);
    CHECK(r.botones == (XB_A | XB_DPAD_ABAJO), "Start + A / D-pad abajo (libres) -> normales");
  }
  {
    Remapeo m;  // Start mantenido mucho tiempo antes del combo sigue funcionando
    m.actualizar(gc(B(GC_START)), 0, &r);
    m.actualizar(gc(B(GC_START) | B(GC_DPAD_ARRIBA)), 5000, &r);
    CHECK(r.botones == XB_GUIA, "Start mantenido 5 s + D-pad arriba -> Home");
  }
  {
    Remapeo m;  // boton presionado ANTES de Start se queda normal
    m.actualizar(gc(B(GC_Y)), 0, &r);
    m.actualizar(gc(B(GC_Y) | B(GC_START)), 10, &r);
    CHECK(r.botones == XB_Y, "Y antes de Start: sigue siendo Y");
  }
  {
    Remapeo m;  // gatillos
    m.actualizar(gc(0, 0x20, 0), 0, &r);
    CHECK(r.lt == 0, "L analogico bajo el umbral -> LT 0");
    m.actualizar(gc(0, 0x7C, 0), 10, &r);
    CHECK(r.lt > 100 && r.lt < 160, "L a medias -> LT a medias");
    m.actualizar(gc(B(GC_L), 0xD0, 0), 20, &r);
    CHECK(r.lt == 255, "L hasta el clic -> LT 255");
    m.actualizar(gc(0, 0, 0xFF), 30, &r);
    CHECK(r.rt == 255 && r.lt == 0, "R a fondo -> RT 255");
  }
  {
    Remapeo m;  // sticks
    GcState s = gc(B(GC_START), 0, 0, 0.5f, -1.0f);
    s.lx = 0.0f;
    s.ly = -1.0f;
    m.actualizar(s, 0, &r);
    CHECK(r.lx == 0 && r.ly == -32767 && r.rx == 16383 && r.ry == -32767,
          "sticks: +y arriba, C-stick tal cual, aun con Start");
    CHECK(r.reportId == 0 && r.reportSize == 20, "cabecera del reporte XInput correcta");
  }
  {
    Remapeo m;  // zona muerta del stick izquierdo (5%)
    GcState s = gc(0);
    s.lx = 0.04f;
    s.ly = 0.02f;
    m.actualizar(s, 0, &r);
    CHECK(r.lx == 0 && r.ly == 0, "stick izq dentro del 5% -> 0");
    s.lx = 0.06f;
    s.ly = 0.0f;
    m.actualizar(s, 10, &r);
    CHECK(r.lx > 0 && r.lx < 1000, "stick izq justo afuera del 5% -> empieza desde casi 0, sin salto");
    s.lx = 1.0f;
    m.actualizar(s, 20, &r);
    CHECK(r.lx == 32767, "stick izq al tope -> sigue llegando al 100%");
    s.lx = -0.7071f;
    s.ly = 0.7071f;
    m.actualizar(s, 30, &r);
    CHECK(r.lx < -23000 && r.ly > 23000, "diagonal al tope -> se mantiene la direccion");
    s = gc(0, 0, 0, 0.03f, 0.0f);
    m.actualizar(s, 40, &r);
    CHECK(r.rx > 0, "C-stick sin zona muerta (0%) -> pasa tal cual");
  }
  printf("\n%s (%d fallos)\n", fallos ? "HAY FALLOS" : "TODO BIEN", fallos);
  return fallos ? 1 : 0;
}
