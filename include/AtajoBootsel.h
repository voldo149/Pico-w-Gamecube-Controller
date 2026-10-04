#ifndef AtajoBootsel_h
#define AtajoBootsel_h

#include "GcState.h"
#include "MapeoXInput.h"
#include "pico/bootrom.h"

// A+B+Z+Start (exactamente esos; d-pad y palancas se ignoran) durante 3 s
// reinicia la Pico en modo carga de .uf2 (BOOTSEL).
#define BOOT_HOLD_MS 3000

static inline void revisarAtajoBootsel(const GcState &gc, uint32_t ahora) {
  static uint32_t desde = 0;
  const uint32_t necesarios =
      GC_BIT(GC_A) | GC_BIT(GC_B) | GC_BIT(GC_Z) | GC_BIT(GC_START);
  const uint32_t prohibidos = GC_BIT(GC_X) | GC_BIT(GC_Y) | GC_BIT(GC_L) | GC_BIT(GC_R);
  const bool gatillos = gc.lAnalog > GATILLO_UMBRAL || gc.rAnalog > GATILLO_UMBRAL;
  const bool activo = (gc.botones & necesarios) == necesarios &&
                      !(gc.botones & prohibidos) && !gatillos;
  if (!activo) {
    desde = 0;
    return;
  }
  if (desde == 0) desde = ahora ? ahora : 1;
  if (ahora - desde >= BOOT_HOLD_MS) reset_usb_boot(0, 0);
}

#endif
