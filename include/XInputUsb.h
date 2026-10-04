#ifndef XInputUsb_h
#define XInputUsb_h

#include "GamecubeController.h"
#include "Remapeo.h"
#include "XInput.h"

// Se presenta por USB como control de Xbox 360 (XInput) y aplica MapeoXInput.h
class XInputUsb {
 public:
  XInputUsb(GamecubeController *controller) { _controller = controller; };
  void init();

 private:
  GamecubeController *_controller;
  Remapeo _remapeo;
};

#endif
