#ifndef XInputUsb_h
#define XInputUsb_h

#include "GamecubeController.h"
#include "LectorGc.h"
#include "Remapeo.h"
#include "XInput.h"

// Se presenta por USB como control de Xbox 360 (XInput) y aplica MapeoXInput.h
class XInputUsb {
 public:
  XInputUsb(GamecubeController *controller) : _lector(controller) {}
  void init();

  // Arranca el USB (una sola vez; se puede llamar varias)
  static void iniciarUsb();
  // Espera hasta 'ms' a que una PC reconozca el control por USB
  static bool esperarHost(uint32_t ms);
  // Atiende el USB y dice si hay una PC conectada por cable ahora mismo
  static bool hostPresente();

 private:
  LectorGc _lector;
  Remapeo _remapeo;
};

#endif
