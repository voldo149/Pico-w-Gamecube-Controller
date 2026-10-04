#include <stdio.h>
#include <string.h>

#include "GamecubeController.h"
#include "N64Controller.h"
#include "pico/stdlib.h"

#if defined(XINPUT)
#include "XInputUsb.h"
#elif defined(SWITCH_BLUETOOTH)
#include "SwitchBluetooth.h"
#else
#include "SwitchUsb.h"
#endif

#ifdef CONTROLLER_TYPE
#define controllerType CONTROLLER_TYPE
#else
#define controllerType "N64"
#endif

int main() {
  stdio_init_all();

#ifdef XINPUT
  // Modo PC: control de Xbox 360 por USB (solo control de Gamecube)
  static GamecubeController gamecube(3);
  XInputUsb xinputUsb(&gamecube);
  xinputUsb.init();  // el control de GC se inicializa dentro, con reintentos
#else

  Controller *controller;
  if (strcmp(controllerType, "N64") == 0) {
    controller = new N64Controller(3);
  } else if (strcmp(controllerType, "Gamecube") == 0) {
    controller = new GamecubeController(3);
  }
#ifdef SWITCH_BLUETOOTH
  // SwitchBluetooth::init prepara el control con reintentos y avisa con el LED
  SwitchBluetooth::init(controller);
#else
  controller->init();
  SwitchUsb switchUsb(controller);
  switchUsb.init();
#endif
#endif  // XINPUT
}
