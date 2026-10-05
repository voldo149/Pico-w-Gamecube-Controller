// Arranque del firmware de PC (un solo .uf2 con USB y Bluetooth).
//
//   1. Cable USB conectado a una PC -> control de Xbox 360 por USB. Siempre gana.
//   2. Sin cable -> Bluetooth. Arranca dormido (radio apagada, gasta poco):
//        cualquier boton    despertar y reconectar a las PCs guardadas
//        Start + Y (1 s)    despertar y emparejar una PC nueva
//      Mientras duerme, si se conecta el cable a una PC, pasa a USB.
//   Si la Pico se congela, el vigilante (watchdog) la reinicia y vuelve sola al
//   modo en el que estaba.

#include "ModoPc.h"

#include <string.h>

#include "AtajoBootsel.h"
#include "Despertar.h"
#include "LectorGc.h"
#include "XInputUsb.h"
#include "XboxBle.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

// Cuanto esperar al arrancar a que una PC reconozca el USB
#define USB_ESPERA_MS 2000
static void arrancarUsb(GamecubeController *controller) {
  watchdog_hw->scratch[SCRATCH_MODO] = 0;
  static XInputUsb usb(controller);
  usb.init();  // no regresa
}

// Dormido hasta un boton (ver Despertar.h). Devuelve true si es para emparejar.
static bool esperarDespertar(GamecubeController *controller) {
  LectorGc lector(controller);
  Despertar despertar;
  GcState gc;
  memset(&gc, 0, sizeof(gc));

  while (true) {
    watchdog_update();
    if (XInputUsb::hostPresente()) arrancarUsb(controller);

    const bool ok = lector.leer(&gc);
    const uint32_t ahora = to_ms_since_boot(get_absolute_time());
    const Despertar::Resultado r = despertar.actualizar(ok ? botonesAtajo(gc) : 0, ahora);
    if (r != Despertar::ESPERAR) return r == Despertar::EMPAREJAR;
    sleep_ms(ok ? 20 : 200);
  }
}

void modoPc(GamecubeController *controller) {
  const bool dormir = watchdog_hw->scratch[SCRATCH_DORMIR] == DORMIR_MAGIC;
  const bool estabaEnBt = watchdog_hw->scratch[SCRATCH_MODO] == MODO_BT_MAGIC;
  const bool trasCuelgue = watchdog_caused_reboot() && !dormir;
  watchdog_hw->scratch[SCRATCH_DORMIR] = 0;
  watchdog_hw->scratch[SCRATCH_MODO] = 0;

  watchdog_enable(3000, true);

  // 1) Cable USB a una PC: tiene prioridad
  if (XInputUsb::esperarHost(USB_ESPERA_MS)) arrancarUsb(controller);

  // 2) Se congelo estando en Bluetooth: volver directo, sin pedir botones
  if (trasCuelgue && estabaEnBt) XboxBle::init(controller, false);

  // 3) Bluetooth: dormido hasta un boton (reconectar) o Start+Y (emparejar)
  const bool emparejar = esperarDespertar(controller);
  XboxBle::init(controller, emparejar);
}
