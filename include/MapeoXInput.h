// =====================================================================
//  MAPEO GameCube -> control de Xbox 360  (modo XInput, por USB)
//  Edita este archivo, recompila y vuelve a cargar el .uf2.
// =====================================================================
//
// Salidas disponibles (control de Xbox 360):
//   XB_A  XB_B  XB_X  XB_Y            botones de la cara
//   XB_LB XB_RB                       bumpers
//   XB_LT XB_RT                       gatillos (analógicos si vienen de L/R del GC)
//   XB_START XB_BACK XB_GUIA          Start, Back (View) y botón Xbox
//   XB_L3 XB_R3                       clic de los sticks
//   XB_DPAD_ARRIBA / _ABAJO / _IZQ / _DER
//   NADA                              sin función
// Se pueden combinar con | (ej. XB_LB | XB_RB = los dos a la vez).
//
// CAPA Z: mientras mantienes Z, los botones con algo en la columna "con Z"
// cambian de función. Los que tienen NADA siguen haciendo lo normal.
//   - Z sola (presionar y soltar sin combinar): manda Z_SOLA como un toque.
//   - Z sola mantenida más de Z_TIEMPO_MS: manda Z_SOLA mantenido (sirve
//     para menús radiales que se abren al mantener).
//   - Para combinar, presiona el otro botón antes de que pasen Z_TIEMPO_MS.

#ifndef MapeoXInput_h
#define MapeoXInput_h

#include "GcState.h"
#include "XInput.h"

// true = Z es la tecla de capa. false = Z es un botón normal (manda Z_SOLA al instante)
#define USAR_CAPA_Z true

// Lo que manda Z cuando se usa sola
#define Z_SOLA XB_RB

// Tiempo (ms) para decidir entre "Z sola mantenida" y "combo con Z"
#define Z_TIEMPO_MS 300

// Duración (ms) del toque de Z sola (3 frames a 60 fps = 50 ms)
#define Z_PULSO_MS 50

// Gatillos: por debajo de este valor (0-255) no cuentan como presionados
#define GATILLO_UMBRAL 0x30
// Valor analógico al que el gatillo ya se manda al 100%
#define GATILLO_MAX 0xC8

// C-stick como botones en la capa Z: qué tanto hay que moverlo (0..1)
#define C_STICK_UMBRAL 0.5f

struct MapeoBoton {
  uint32_t normal;
  uint32_t conZ;
};

// Una fila por botón del GC, en este orden exacto (ver GcState.h).
static const MapeoBoton MAPEO[GC_NUM_BOTONES] = {
    //  normal           con Z
    {XB_A,              NADA},          // A
    {XB_B,              NADA},          // B
    {XB_X,              NADA},          // X
    {XB_Y,              NADA},          // Y
    {NADA,              NADA},          // Z  (la maneja la capa; ver Z_SOLA)
    {XB_LT,             NADA},          // L  (analógico)
    {XB_RT,             NADA},          // R  (analógico)
    {XB_START,          XB_GUIA},       // Start      | Z+Start = botón Xbox
    {XB_DPAD_ARRIBA,    XB_L3},         // D-pad ↑    | Z+↑ = L3
    {XB_DPAD_ABAJO,     XB_R3},         // D-pad ↓    | Z+↓ = R3
    {XB_DPAD_IZQ,       XB_BACK},       // D-pad ←    | Z+← = Back (View)
    {XB_DPAD_DER,       XB_LB},         // D-pad →    | Z+→ = LB
    {NADA,              NADA},          // C-stick ↑  (sin Z es el stick derecho)
    {NADA,              NADA},          // C-stick ↓
    {NADA,              NADA},          // C-stick ←
    {NADA,              NADA},          // C-stick →
};

#endif
