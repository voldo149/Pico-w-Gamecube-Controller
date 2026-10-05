#ifndef LectorGc_h
#define LectorGc_h

#include <string.h>

#include "GamecubeController.h"
#include "GcState.h"

// Lee el control de GC sin trabarse nunca:
//  - si una lectura falla, el control se reinicia en la siguiente
//  - tras 3 fallos seguidos (control desconectado) todo queda suelto
class LectorGc {
 public:
  explicit LectorGc(GamecubeController *controller) : _controller(controller) {}

  bool leer(GcState *estado) {
    try {
      if (!_listo) {
        _controller->init();
        _listo = true;
      }
      _controller->getGcState(estado);
      _fallos = 0;
      return true;
    } catch (...) {
      _listo = false;
      if (++_fallos >= 3) {
        memset(estado, 0, sizeof(*estado));
        _fallos = 3;
      }
      return false;
    }
  }

 private:
  GamecubeController *_controller;
  bool _listo = false;
  uint8_t _fallos = 0;
};

#endif
