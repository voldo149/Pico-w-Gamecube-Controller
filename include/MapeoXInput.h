// =====================================================================
//  MAPEO GameCube -> control de Xbox 360  (modo XInput, por USB)
//  Edita este archivo, recompila y vuelve a cargar el .uf2.
// =====================================================================
//
// Salidas disponibles (control de Xbox 360):
//   XB_A  XB_B  XB_X  XB_Y            botones de la cara
//   XB_LB XB_RB                       bumpers (L / R de la Switch)
//   XB_LT XB_RT                       gatillos (analógicos si vienen de L/R del GC)
//   XB_START XB_BACK XB_GUIA          Start (+), Back (-) y botón Xbox (Home)
//   XB_L3 XB_R3                       clic de los sticks
//   XB_DPAD_ARRIBA / _ABAJO / _IZQ / _DER
//   NADA                              sin función
// Se pueden combinar con | (ej. XB_LB | XB_RB = los dos a la vez).
//
// Cada botón tiene 3 columnas:
//   normal    lo que manda al presionarlo
//   con Z     lo que manda si se presiona mientras Z está activa (NADA = lo normal)
//   mantener  si no es NADA: toque = "normal" (se manda al soltar),
//             mantener más de TIEMPO_MANTENER_MS = "mantener"
//             (OJO: el toque llega al soltar, así que ese botón tiene un poco de retraso)

#ifndef MapeoXInput_h
#define MapeoXInput_h

#include "GcState.h"
#include "XInput.h"

// Cómo funciona Z:
//   Z_MANTENER  la capa Z está activa mientras mantienes Z (recomendado)
//   Z_ALTERNAR  cada toque de Z prende / apaga la capa
//   Z_BOTON     Z es un botón normal (usa su fila "normal" de la tabla)
#define Z_MANTENER 0
#define Z_ALTERNAR 1
#define Z_BOTON 2
#ifndef MODO_Z
#define MODO_Z Z_MANTENER
#endif

// Tiempo (ms) para que un botón con columna "mantener" cuente como mantenido
#define TIEMPO_MANTENER_MS 300

// Duración (ms) del toque que se manda al soltar (3 frames a 60 fps = 50 ms)
#define PULSO_MS 50

// Gatillos: por debajo de este valor (0-255) no cuentan como presionados
#define GATILLO_UMBRAL 0x30
// Valor analógico al que el gatillo ya se manda al 100%
#define GATILLO_MAX 0xC8

// C-stick como botones en la capa Z: qué tanto hay que moverlo (0..1)
#define C_STICK_UMBRAL 0.5f

struct MapeoBoton {
  uint32_t normal;
  uint32_t conZ;
  uint32_t mantener;
};

// Una fila por botón del GC, en este orden exacto (ver GcState.h).
static const MapeoBoton MAPEO[GC_NUM_BOTONES] = {
    //  normal           con Z           mantener
    {XB_A,              NADA,           NADA},      // A
    {XB_B,              NADA,           NADA},      // B
    {XB_X,              XB_RB,          NADA},      // X          | Z+X = RB
    {XB_Y,              XB_LB,          NADA},      // Y          | Z+Y = LB
    {NADA,              NADA,           NADA},      // Z  (es la tecla de capa; ver MODO_Z)
    {XB_LT,             NADA,           NADA},      // L  (analógico)
    {XB_RT,             NADA,           NADA},      // R  (analógico)
    {XB_START,          XB_BACK,        XB_GUIA},   // Start      | toque = Start, mantener = Xbox, Z+Start = Back
    {XB_DPAD_ARRIBA,    XB_L3,          NADA},      // D-pad ↑    | Z+↑ = L3
    {XB_DPAD_ABAJO,     XB_R3,          NADA},      // D-pad ↓    | Z+↓ = R3
    {XB_DPAD_IZQ,       NADA,           NADA},      // D-pad ←    | (Z+← libre)
    {XB_DPAD_DER,       NADA,           NADA},      // D-pad →    | (Z+→ libre)
    {NADA,              NADA,           NADA},      // C-stick ↑  (sin Z es el stick derecho)
    {NADA,              NADA,           NADA},      // C-stick ↓
    {NADA,              NADA,           NADA},      // C-stick ←
    {NADA,              NADA,           NADA},      // C-stick →
};

#endif
