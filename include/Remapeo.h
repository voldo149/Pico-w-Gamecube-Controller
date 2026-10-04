#ifndef Remapeo_h
#define Remapeo_h

#include <stdint.h>

#include "GcState.h"
#include "XInput.h"

// Convierte el estado del GC en un reporte de Xbox 360 aplicando MapeoXInput.h
// (capa Z incluida). No depende del hardware, así que se puede probar en la PC.
class Remapeo {
 public:
  void actualizar(const GcState &gc, uint32_t ahoraMs, XInputReport *salida);

 private:
  enum EstadoZ : uint8_t { Z_SUELTA, Z_PENDIENTE, Z_CAPA, Z_MANTENIDA };
  EstadoZ _z = Z_SUELTA;
  uint32_t _zDesde = 0;
  uint32_t _pulsoHasta = 0;
  bool _pulsoActivo = false;
  uint32_t _botonesAntes = 0;
  // Cada botón recuerda con qué capa se presionó, hasta que se suelta
  bool _conZ[GC_NUM_BOTONES] = {};
};

#endif
