#ifndef AtajoBootsel_h
#define AtajoBootsel_h

#include "GcState.h"
#include "MapeoXInput.h"
#include "pico/bootrom.h"


// Los atajos comparan EXACTAMENTE estos botones (d-pad y palancas se ignoran).
// L y R cuentan como presionados desde el umbral analogico.
static inline uint32_t botonesAtajo(const GcState &gc) {
  uint32_t b = gc.botones & (GC_BIT(GC_A) | GC_BIT(GC_B) | GC_BIT(GC_X) | GC_BIT(GC_Y) |
                             GC_BIT(GC_Z) | GC_BIT(GC_L) | GC_BIT(GC_R) | GC_BIT(GC_START));
  if (gc.lAnalog > GATILLO_UMBRAL) b |= GC_BIT(GC_L);
  if (gc.rAnalog > GATILLO_UMBRAL) b |= GC_BIT(GC_R);
  return b;
}

#define ATAJO_BOOTSEL (GC_BIT(GC_A) | GC_BIT(GC_B) | GC_BIT(GC_Z) | GC_BIT(GC_START))

// A+B+Z+Start durante 3 s reinicia la Pico en modo carga de .uf2 (BOOTSEL).
#define BOOT_HOLD_MS 3000

static inline void revisarAtajoBootsel(const GcState &gc, uint32_t ahora) {
  static uint32_t desde = 0;
  if (botonesAtajo(gc) != ATAJO_BOOTSEL) {
    desde = 0;
    return;
  }
  if (desde == 0) desde = ahora ? ahora : 1;
  if (ahora - desde >= BOOT_HOLD_MS) reset_usb_boot(0, 0);
}

#endif
