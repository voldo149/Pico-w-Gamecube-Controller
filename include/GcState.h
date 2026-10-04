#ifndef GcState_h
#define GcState_h

#include <stdint.h>

// Botones del control de GameCube como "entradas" para el remapeo.
// Las 4 direcciones del C-stick también cuentan como botones (para la capa Z).
enum GcBoton : uint8_t {
  GC_A,
  GC_B,
  GC_X,
  GC_Y,
  GC_Z,
  GC_L,  // gatillo L: clic digital o presionado más allá del umbral analógico
  GC_R,
  GC_START,
  GC_DPAD_ARRIBA,
  GC_DPAD_ABAJO,
  GC_DPAD_IZQ,
  GC_DPAD_DER,
  GC_C_ARRIBA,
  GC_C_ABAJO,
  GC_C_IZQ,
  GC_C_DER,
  GC_NUM_BOTONES
};

#define GC_BIT(b) ((uint32_t)1 << (b))

// Estado ya leído y normalizado del control de GameCube
struct GcState {
  uint32_t botones;  // bits GC_BIT(GC_*)
  float lx, ly;      // stick principal, -1..1 (+y = arriba)
  float cx, cy;      // C-stick, -1..1 (+y = arriba)
  uint8_t lAnalog;   // gatillos analógicos crudos, 0..255
  uint8_t rAnalog;
};

#endif
