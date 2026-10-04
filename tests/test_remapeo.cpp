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
    CHECK(r.botones == XB_X, "X sin Z -> X");
  }
  {
    Remapeo m;  // Z sola no manda nada, ni al presionar ni al soltar ni mantenida
    m.actualizar(gc(B(GC_Z)), 0, &r);
    CHECK(r.botones == 0, "Z presionada: nada");
    m.actualizar(gc(B(GC_Z)), 1000, &r);
    CHECK(r.botones == 0, "Z mantenida 1 s: nada");
    m.actualizar(gc(0), 1010, &r);
    m.actualizar(gc(0), 1020, &r);
    CHECK(r.botones == 0, "Z soltada: nada");
  }
  {
    Remapeo m;  // la capa es instantanea
    m.actualizar(gc(B(GC_Z)), 0, &r);
    m.actualizar(gc(B(GC_Z) | B(GC_Y)), 1, &r);
    CHECK(r.botones == XB_LB, "Z + Y -> LB al instante");
    m.actualizar(gc(B(GC_Z) | B(GC_Y) | B(GC_X)), 2, &r);
    CHECK(r.botones == (XB_LB | XB_RB), "Z + Y + X -> LB + RB");
    m.actualizar(gc(B(GC_Y) | B(GC_X)), 3, &r);
    CHECK(r.botones == (XB_LB | XB_RB), "suelto Z: Y y X siguen como LB/RB hasta soltarlos");
    m.actualizar(gc(0), 4, &r);
    m.actualizar(gc(B(GC_Y)), 5, &r);
    CHECK(r.botones == XB_Y, "Y sin Z otra vez -> Y");
  }
  {
    Remapeo m;  // Z mantenida mucho tiempo antes del combo sigue funcionando
    m.actualizar(gc(B(GC_Z)), 0, &r);
    m.actualizar(gc(B(GC_Z) | B(GC_X)), 5000, &r);
    CHECK(r.botones == XB_RB, "Z mantenida 5 s + X -> RB");
  }
  {
    Remapeo m;  // botones sin funcion con Z
    m.actualizar(gc(B(GC_Z) | B(GC_A)), 0, &r);
    CHECK(r.botones == XB_A, "Z + A (sin funcion con Z) -> A normal");
    m.actualizar(gc(B(GC_Z) | B(GC_A) | B(GC_DPAD_ARRIBA)), 10, &r);
    CHECK(r.botones == (XB_A | XB_L3), "Z + D-pad arriba -> L3");
    m.actualizar(gc(B(GC_Z) | B(GC_DPAD_ABAJO)), 20, &r);
    CHECK(r.botones == XB_R3, "Z + D-pad abajo -> R3");
    m.actualizar(gc(B(GC_Z) | B(GC_DPAD_IZQ)), 30, &r);
    CHECK(r.botones == XB_DPAD_IZQ, "Z + D-pad izq (libre) -> D-pad izq");
  }
  {
    Remapeo m;  // boton presionado ANTES de Z se queda normal
    m.actualizar(gc(B(GC_Y)), 0, &r);
    m.actualizar(gc(B(GC_Y) | B(GC_Z)), 10, &r);
    CHECK(r.botones == XB_Y, "Y antes de Z: sigue siendo Y");
  }
  {
    Remapeo m;  // Start: toque / mantener
    m.actualizar(gc(B(GC_START)), 0, &r);
    CHECK(r.botones == 0, "Start presionado: espera");
    m.actualizar(gc(B(GC_START)), 100, &r);
    CHECK(r.botones == 0, "Start a 100 ms: sigue esperando");
    m.actualizar(gc(0), 120, &r);
    CHECK(r.botones == XB_START, "Start soltado a 120 ms: toque -> Start");
    m.actualizar(gc(0), 169, &r);
    CHECK(r.botones == XB_START, "pulso Start dura 50 ms");
    m.actualizar(gc(0), 171, &r);
    CHECK(r.botones == 0, "pulso Start termina");
  }
  {
    Remapeo m;
    m.actualizar(gc(B(GC_START)), 0, &r);
    m.actualizar(gc(B(GC_START)), 299, &r);
    CHECK(r.botones == 0, "Start a 299 ms: nada");
    m.actualizar(gc(B(GC_START)), 300, &r);
    CHECK(r.botones == XB_GUIA, "Start mantenido 300 ms -> boton Xbox");
    m.actualizar(gc(B(GC_START)), 1500, &r);
    CHECK(r.botones == XB_GUIA, "sigue el boton Xbox mientras se mantiene");
    m.actualizar(gc(0), 1510, &r);
    CHECK(r.botones == 0, "soltar: nada, sin toque de Start extra");
  }
  {
    Remapeo m;  // Z + Start = Back, al instante
    m.actualizar(gc(B(GC_Z)), 0, &r);
    m.actualizar(gc(B(GC_Z) | B(GC_START)), 10, &r);
    CHECK(r.botones == XB_BACK, "Z + Start -> Back al instante");
    m.actualizar(gc(B(GC_Z) | B(GC_START)), 900, &r);
    CHECK(r.botones == XB_BACK, "Z + Start mantenido: sigue Back (no boton Xbox)");
    m.actualizar(gc(0), 910, &r);
    CHECK(r.botones == 0, "soltar: nada");
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
    GcState s = gc(B(GC_Z), 0, 0, 0.5f, -1.0f);
    s.lx = 0.0f;
    s.ly = -1.0f;
    m.actualizar(s, 0, &r);
    CHECK(r.lx == 0 && r.ly == -32767 && r.rx == 16383 && r.ry == -32767,
          "sticks: +y arriba, C-stick tal cual, aun con Z");
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
