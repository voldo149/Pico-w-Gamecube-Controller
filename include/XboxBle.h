#ifndef XboxBle_h
#define XboxBle_h

#include "GamecubeController.h"

// Se presenta por Bluetooth LE como control de Xbox (Series, modelo 1914) y
// aplica el mismo mapeo y capa Z que el modo USB (include/MapeoXInput.h).
class XboxBle {
 public:
  // emparejar = true: arranca visible para emparejar una PC nueva
  static void init(GamecubeController *controller, bool emparejar);
};

#endif
