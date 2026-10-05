#ifndef ModoPc_h
#define ModoPc_h

#include "GamecubeController.h"

// Firmware de PC: un solo .uf2 con USB y Bluetooth.
//   - Cable USB conectado a una PC: control de Xbox 360 por USB (tiene prioridad)
//   - Sin cable: Bluetooth. Arranca dormido;
//       cualquier boton  = despertar y reconectar a las PCs guardadas
//       Start + Y (1 s)  = despertar y emparejar una PC nueva
void modoPc(GamecubeController *controller);

// Avisos que sobreviven a un reinicio (registros scratch del watchdog)
#define SCRATCH_DORMIR 2
#define SCRATCH_MODO 3
#define DORMIR_MAGIC 0x534C5050  // 'SLPP': arrancar dormido (se apago con L+R+Start)
#define MODO_BT_MAGIC 0x4D4F4442  // 'MODB': estaba en Bluetooth (para volver tras un cuelgue)

#endif
