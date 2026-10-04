// Pruebas del remapeo (capa Z, gatillos, sticks) que corren en la PC, sin la Pico:
//   g++ -std=c++17 -Iinclude src/xinputController/Remapeo.cpp tests/test_remapeo.cpp -o test_remapeo && ./test_remapeo
#include <cstdio>
#include <cstdlib>
#include "Remapeo.h"
#include "MapeoXInput.h"
static int fallos = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FALLA: %s (linea %d)\n", msg, __LINE__); fallos++; } else printf("ok: %s\n", msg); } while (0)
static GcState gc(uint32_t b, uint8_t la = 0, uint8_t ra = 0, float cx = 0, float cy = 0) {
  GcState s{}; s.botones = b; s.lAnalog = la; s.rAnalog = ra; s.cx = cx; s.cy = cy; return s; }
#define B(x) GC_BIT(x)
int main() {
  XInputReport r;
  { Remapeo m; m.actualizar(gc(B(GC_A)), 0, &r); CHECK(r.botones == XB_A, "A -> A al instante");
    m.actualizar(gc(B(GC_DPAD_DER)), 10, &r); CHECK(r.botones == XB_DPAD_DER, "D-pad derecha sin Z -> D-pad derecha"); }
  { Remapeo m; // Z toque: nada al presionar, RB al soltar durante 50 ms
    m.actualizar(gc(B(GC_Z)), 0, &r); CHECK(r.botones == 0, "Z presionada: aun no manda nada");
    m.actualizar(gc(B(GC_Z)), 100, &r); CHECK(r.botones == 0, "Z a 100 ms: sigue esperando");
    m.actualizar(gc(0), 120, &r); CHECK(r.botones == XB_RB, "Z soltada a 120 ms: toque -> RB");
    m.actualizar(gc(0), 160, &r); CHECK(r.botones == XB_RB, "pulso RB sigue a 160 ms");
    m.actualizar(gc(0), 171, &r); CHECK(r.botones == 0, "pulso RB termina despues de 50 ms"); }
  { Remapeo m; // Z mantenida -> RB mantenido
    m.actualizar(gc(B(GC_Z)), 0, &r); m.actualizar(gc(B(GC_Z)), 299, &r); CHECK(r.botones == 0, "Z a 299 ms: nada");
    m.actualizar(gc(B(GC_Z)), 300, &r); CHECK(r.botones == XB_RB, "Z a 300 ms: RB mantenido");
    m.actualizar(gc(B(GC_Z)), 2000, &r); CHECK(r.botones == XB_RB, "Z a 2 s: RB sigue");
    m.actualizar(gc(0), 2001, &r); CHECK(r.botones == 0, "soltar Z: RB se suelta, sin pulso extra"); }
  { Remapeo m; // combos
    m.actualizar(gc(B(GC_Z)), 0, &r);
    m.actualizar(gc(B(GC_Z) | B(GC_DPAD_DER)), 50, &r); CHECK(r.botones == XB_LB, "Z + D-pad derecha -> LB");
    m.actualizar(gc(B(GC_Z) | B(GC_DPAD_DER)), 900, &r); CHECK(r.botones == XB_LB, "combo mantenido mucho tiempo: sigue LB, sin RB");
    m.actualizar(gc(B(GC_DPAD_DER)), 950, &r); CHECK(r.botones == XB_LB, "suelto Z primero: D-pad sigue siendo LB hasta soltarlo");
    m.actualizar(gc(0), 1000, &r); CHECK(r.botones == 0, "soltar todo: nada, y Z no manda RB"); }
  { Remapeo m;
    m.actualizar(gc(B(GC_Z) | B(GC_START)), 0, &r); CHECK(r.botones == XB_GUIA, "Z + Start juntos -> boton Xbox");
    m.actualizar(gc(0), 10, &r);
    m.actualizar(gc(B(GC_Z)), 20, &r); m.actualizar(gc(B(GC_Z) | B(GC_DPAD_IZQ)), 30, &r); CHECK(r.botones == XB_BACK, "Z + D-pad izq -> Back");
    m.actualizar(gc(B(GC_Z) | B(GC_DPAD_IZQ) | B(GC_DPAD_ARRIBA)), 40, &r); CHECK(r.botones == (XB_BACK | XB_L3), "Z + izq + arriba -> Back + L3");
    m.actualizar(gc(B(GC_Z) | B(GC_DPAD_ABAJO)), 50, &r); CHECK(r.botones == XB_R3, "Z + abajo -> R3"); }
  { Remapeo m; // boton sin funcion con Z no rompe el toque de Z
    m.actualizar(gc(B(GC_Z)), 0, &r); m.actualizar(gc(B(GC_Z) | B(GC_A)), 30, &r); CHECK(r.botones == XB_A, "Z + A (sin funcion con Z) -> A normal");
    m.actualizar(gc(B(GC_A)), 60, &r); CHECK(r.botones == (XB_A | XB_RB), "y Z cuenta como toque -> RB"); }
  { Remapeo m; // D-pad presionado ANTES de Z sigue siendo D-pad
    m.actualizar(gc(B(GC_DPAD_DER)), 0, &r); m.actualizar(gc(B(GC_DPAD_DER) | B(GC_Z)), 20, &r);
    CHECK(r.botones == XB_DPAD_DER, "D-pad antes de Z: sigue siendo D-pad"); }
  { Remapeo m; // gatillos
    m.actualizar(gc(0, 0x20, 0), 0, &r); CHECK(r.lt == 0, "L analogico bajo el umbral -> LT 0");
    m.actualizar(gc(0, 0x7C, 0), 10, &r); CHECK(r.lt > 100 && r.lt < 160, "L a medias -> LT a medias");
    m.actualizar(gc(B(GC_L), 0xD0, 0), 20, &r); CHECK(r.lt == 255, "L hasta el clic -> LT 255");
    m.actualizar(gc(0, 0, 0xFF), 30, &r); CHECK(r.rt == 255 && r.lt == 0, "R a fondo -> RT 255"); }
  { Remapeo m; // sticks
    GcState s = gc(0, 0, 0, 0.5f, -1.0f); s.lx = 1.0f; s.ly = -0.5f;
    m.actualizar(s, 0, &r); CHECK(r.lx == 32767 && r.ly == -16383 && r.rx == 16383 && r.ry == -32767, "sticks pasan tal cual (+y arriba)");
    CHECK(r.reportId == 0 && r.reportSize == 20, "cabecera del reporte XInput correcta"); }
  printf("\n%s (%d fallos)\n", fallos ? "HAY FALLOS" : "TODO BIEN", fallos);
  return fallos ? 1 : 0;
}
