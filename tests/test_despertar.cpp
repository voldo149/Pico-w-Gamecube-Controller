// Pruebas de como despierta el firmware de PC (corren en la PC, sin la Pico):
//   g++ -std=c++17 -Iinclude tests/test_despertar.cpp -o test_despertar && ./test_despertar
#include <cstdio>

#include "Despertar.h"

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

#define B(x) GC_BIT(x)
static const uint32_t Y = B(GC_Y), START = B(GC_START), A = B(GC_A);

// Simula 'botones' presionados desde 'desde' hasta 'hasta' (cada 20 ms)
static Despertar::Resultado simular(Despertar &d, uint32_t botones, uint32_t desde, uint32_t hasta,
                                    uint32_t *cuando = nullptr) {
  for (uint32_t t = desde; t <= hasta; t += 20) {
    Despertar::Resultado r = d.actualizar(botones, t);
    if (r != Despertar::ESPERAR) {
      if (cuando) *cuando = t;
      return r;
    }
  }
  return Despertar::ESPERAR;
}

int main() {
  uint32_t t;
  {
    Despertar d;
    CHECK(simular(d, 0, 0, 5000) == Despertar::ESPERAR, "sin tocar nada: sigue dormido");
    CHECK(simular(d, A, 5020, 5300) == Despertar::ESPERAR, "A 0.28 s: todavia no");
    CHECK(simular(d, A, 5320, 6000, &t) == Despertar::RECONECTAR && t >= 5320,
          "A 0.3 s: despierta y reconecta");
  }
  {
    Despertar d;
    simular(d, 0, 0, 400);
    CHECK(simular(d, A, 420, 460) == Despertar::ESPERAR, "toque corto de A...");
    CHECK(d.actualizar(0, 480) == Despertar::RECONECTAR, "...al soltarlo despierta (cualquier boton)");
  }
  {
    Despertar d;
    simular(d, 0, 0, 400);
    CHECK(simular(d, Y | START, 420, 1400) == Despertar::ESPERAR, "Start+Y 0.98 s: todavia no");
    CHECK(simular(d, Y | START, 1420, 2000) == Despertar::EMPAREJAR, "Start+Y 1 s: emparejar");
  }
  {
    Despertar d;  // Start primero y luego Y: no despierta en "reconectar" antes de tiempo
    simular(d, 0, 0, 400);
    CHECK(simular(d, START, 420, 700) == Despertar::ESPERAR, "solo Start 0.3 s: espera (puede ser el combo)");
    CHECK(simular(d, Y | START, 720, 1800) == Despertar::EMPAREJAR, "luego Start+Y 1 s: emparejar");
  }
  {
    Despertar d;
    simular(d, 0, 0, 400);
    CHECK(simular(d, Y, 420, 1500) == Despertar::RECONECTAR, "solo Y 1 s sin Start: reconectar");
  }
  {
    Despertar d;
    simular(d, 0, 0, 400);
    CHECK(simular(d, Y | START, 420, 800) == Despertar::ESPERAR, "Start+Y 0.4 s...");
    CHECK(d.actualizar(0, 820) == Despertar::RECONECTAR, "...soltado antes de 1 s: reconectar");
  }
  {
    Despertar d;  // se apago con L+R+Start y los botones siguen presionados al dormir
    const uint32_t LRS = B(GC_L) | B(GC_R) | START;
    CHECK(simular(d, LRS, 0, 3000) == Despertar::ESPERAR, "botones del apagado aun presionados: no despierta");
    CHECK(simular(d, 0, 3020, 3200) == Despertar::ESPERAR, "soltados: sigue dormido");
    CHECK(simular(d, A, 3220, 3260) == Despertar::ESPERAR, "boton antes de 0.3 s suelto: no cuenta");
    CHECK(simular(d, 0, 3280, 3700) == Despertar::ESPERAR, "suelto otra vez: dormido");
    CHECK(simular(d, A, 3720, 4200) == Despertar::RECONECTAR, "ya con todo suelto 0.3 s: A despierta");
  }
  printf("\n%s (%d fallos)\n", fallos ? "HAY FALLOS" : "TODO BIEN", fallos);
  return fallos ? 1 : 0;
}
