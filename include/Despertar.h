#ifndef Despertar_h
#define Despertar_h

#include <stdint.h>

#include "GcState.h"

// Decide como despertar estando dormido. No depende del hardware: se prueba en la PC.
//   - Primero todo tiene que estar suelto 0.3 s (para no despertar con el mismo
//     atajo con el que se apago)
//   - Start+Y juntos 1 s                          -> emparejar
//   - Y, Start o Start+Y que no llegan al 1 s     -> reconectar (al soltar, o al
//     pasar 1 s sin completar el combo)
//   - cualquier otro boton 0.3 s                  -> reconectar
class Despertar {
 public:
  enum Resultado : int8_t { ESPERAR = -1, RECONECTAR = 0, EMPAREJAR = 1 };

  static const uint32_t SUELTO_MS = 300;
  static const uint32_t BOTON_MS = 300;
  static const uint32_t EMPAREJAR_MS = 1000;
  static const uint32_t COMBO = GC_BIT(GC_Y) | GC_BIT(GC_START);

  // 'botones' = botonesAtajo() del control (0 si no se pudo leer)
  Resultado actualizar(uint32_t botones, uint32_t ahora) {
    if (botones == 0) {
      // Un toque que se solto tambien despierta
      if (_presionadoDesde != 0 && ahora - _presionadoDesde >= 50) return RECONECTAR;
      _presionadoDesde = _comboDesde = 0;
      if (_sueltoDesde == 0) _sueltoDesde = ahora ? ahora : 1;
      return ESPERAR;
    }
    if (_sueltoDesde == 0 || ahora - _sueltoDesde < SUELTO_MS) return ESPERAR;

    if (_presionadoDesde == 0) _presionadoDesde = ahora ? ahora : 1;
    const bool parteDelCombo = (botones & ~COMBO) == 0;
    if (!parteDelCombo) {
      return ahora - _presionadoDesde >= BOTON_MS ? RECONECTAR : ESPERAR;
    }
    if (botones == COMBO) {
      if (_comboDesde == 0) _comboDesde = ahora ? ahora : 1;
      return ahora - _comboDesde >= EMPAREJAR_MS ? EMPAREJAR : ESPERAR;
    }
    _comboDesde = 0;
    return ahora - _presionadoDesde >= EMPAREJAR_MS ? RECONECTAR : ESPERAR;
  }

 private:
  uint32_t _sueltoDesde = 0;
  uint32_t _presionadoDesde = 0;
  uint32_t _comboDesde = 0;
};

#endif
