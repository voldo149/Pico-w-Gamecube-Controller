// Pruebas del mapeo "clasico" (Z = RB, Start toque/mantener, Start+D-pad arriba = Xbox).
// Corren en la PC, sin la Pico:
//   g++ -std=c++17 -DMAPEO_CLASICO -Iinclude src/xinputController/Remapeo.cpp tests/test_remapeo_clasico.cpp -o test_clasico && ./test_clasico
#include <cstdio>

#include "MapeoXInput.h"
#include "Remapeo.h"

#ifndef MAPEO_CLASICO
#error "Compila con -DMAPEO_CLASICO"
#endif

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

static GcState gc(uint32_t b) {
  GcState s{};
  s.botones = b;
  return s;
}
#define B(x) GC_BIT(x)

int main() {
  XInputReport r;
  {
    Remapeo m;
    m.actualizar(gc(B(GC_Z)), 0, &r);
    CHECK(r.botones == XB_RB, "Z -> RB al instante");
    m.actualizar(gc(B(GC_Z)), 2000, &r);
    CHECK(r.botones == XB_RB, "Z mantenida -> RB sigue");
    m.actualizar(gc(B(GC_Z) | B(GC_Y)), 2010, &r);
    CHECK(r.botones == (XB_RB | XB_Y), "Z + Y -> RB + Y (Z ya no es capa)");
    m.actualizar(gc(B(GC_X)), 2020, &r);
    CHECK(r.botones == XB_X, "X -> X");
  }
  {
    Remapeo m;  // Start toque
    m.actualizar(gc(B(GC_START)), 0, &r);
    CHECK(r.botones == 0, "Start presionado: espera");
    m.actualizar(gc(0), 120, &r);
    CHECK(r.botones == XB_START, "Start tocado -> Start (+)");
    m.actualizar(gc(0), 171, &r);
    CHECK(r.botones == 0, "pulso de Start termina");
  }
  {
    Remapeo m;  // Start mantener
    m.actualizar(gc(B(GC_START)), 0, &r);
    m.actualizar(gc(B(GC_START)), 299, &r);
    CHECK(r.botones == 0, "Start a 299 ms: nada");
    m.actualizar(gc(B(GC_START)), 300, &r);
    CHECK(r.botones == XB_BACK, "Start mantenido 300 ms -> Back (-)");
    m.actualizar(gc(B(GC_START)), 2000, &r);
    CHECK(r.botones == XB_BACK, "sigue Back mientras se mantiene");
    m.actualizar(gc(0), 2010, &r);
    CHECK(r.botones == 0, "soltar: nada, sin Start extra");
  }
  {
    Remapeo m;  // Start + D-pad arriba = Xbox
    m.actualizar(gc(B(GC_START)), 0, &r);
    m.actualizar(gc(B(GC_START) | B(GC_DPAD_ARRIBA)), 100, &r);
    CHECK(r.botones == XB_GUIA, "Start + D-pad arriba -> boton Xbox");
    m.actualizar(gc(B(GC_START) | B(GC_DPAD_ARRIBA)), 1500, &r);
    CHECK(r.botones == XB_GUIA, "mantenido: sigue Xbox, sin Back");
    m.actualizar(gc(0), 1510, &r);
    m.actualizar(gc(0), 1520, &r);
    CHECK(r.botones == 0, "soltar: nada, sin Start extra");
  }
  {
    Remapeo m;  // D-pad arriba solo y con Start mantenido ya como Back
    m.actualizar(gc(B(GC_DPAD_ARRIBA)), 0, &r);
    CHECK(r.botones == XB_DPAD_ARRIBA, "D-pad arriba solo -> D-pad arriba");
    m.actualizar(gc(0), 10, &r);
    m.actualizar(gc(B(GC_START)), 20, &r);
    m.actualizar(gc(B(GC_START)), 400, &r);
    m.actualizar(gc(B(GC_START) | B(GC_DPAD_ARRIBA)), 410, &r);
    CHECK(r.botones == (XB_BACK | XB_DPAD_ARRIBA), "D-pad arriba despues de 0.3 s de Start -> Back + D-pad");
  }
  {
    Remapeo m;  // Start + otro boton (sin funcion con Start): el boton normal y Start sigue su curso
    m.actualizar(gc(B(GC_START)), 0, &r);
    m.actualizar(gc(B(GC_START) | B(GC_A)), 50, &r);
    CHECK(r.botones == XB_A, "Start + A -> A normal");
    m.actualizar(gc(B(GC_A)), 100, &r);
    CHECK(r.botones == (XB_A | XB_START), "y Start soltado rapido cuenta como toque -> Start");
  }
  printf("\n%s (%d fallos)\n", fallos ? "HAY FALLOS" : "TODO BIEN", fallos);
  return fallos ? 1 : 0;
}
