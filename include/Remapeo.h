#ifndef Remapeo_h
#define Remapeo_h

#include <stdint.h>

#include "GcState.h"
#include "XInput.h"

// Convierte el estado del GC en un reporte de Xbox 360 aplicando MapeoXInput.h
// (capa Z y toque/mantener). No depende del hardware: se puede probar en la PC.
class Remapeo {
 public:
  void actualizar(const GcState &gc, uint32_t ahoraMs, XInputReport *salida);

 private:
  // Cada botón decide qué es al presionarse y lo mantiene hasta soltarse
  enum Modo : uint8_t { SUELTO, NORMAL, CON_Z, ESPERANDO, MANTENIDO };
  struct EstadoBoton {
    Modo modo = SUELTO;
    uint32_t desde = 0;
    bool pulso = false;  // toque pendiente de mandar tras soltar
    uint32_t pulsoHasta = 0;
  };
  EstadoBoton _boton[GC_NUM_BOTONES];
  uint32_t _botonesAntes = 0;
  bool _capaFija = false;  // para MODO_Z == Z_ALTERNAR
  // Tecla de capa con toque/mantener propio (CAPA_TOQUE / CAPA_MANTENIDA)
  enum EstadoCapa : uint8_t { CAPA_SUELTA, CAPA_PENDIENTE, CAPA_COMBO, CAPA_MANTENIDA_EST };
  EstadoCapa _capa = CAPA_SUELTA;
  uint32_t _capaDesde = 0;
  bool _capaPulso = false;
  uint32_t _capaPulsoHasta = 0;
};

#endif
