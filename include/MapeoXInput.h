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
// Hay dos mapeos; se elige al compilar:
//   - capa Start (por defecto): Z = RB; mientras mantienes Start cambian los
//     botones (Start+Y = Start, Start+X = Back, Start+↑ = botón Xbox...)
//   - clásico (-DMAPEO_CLASICO=ON): Z = RB; Start: toque = Start, mantener = Back,
//     Start + D-pad ↑ = botón Xbox
//
// Cada botón tiene 3 columnas:
//   normal    lo que manda al presionarlo
//   con capa  lo que manda si se presiona mientras la tecla de capa está activa
//             (NADA = lo normal)
//   mantener  si no es NADA: toque = "normal" (se manda al soltar),
//             mantener más de TIEMPO_MANTENER_MS = "mantener"
//             (OJO: el toque llega al soltar, así que ese botón tiene un poco de retraso)

#ifndef MapeoXInput_h
#define MapeoXInput_h

#include "GcState.h"
#include "XInput.h"

// Cómo funciona la tecla de capa (TECLA_CAPA, en cada mapeo de abajo):
//   Z_MANTENER  la capa está activa mientras la mantienes (recomendado)
//   Z_ALTERNAR  cada toque prende / apaga la capa
//   Z_BOTON     no hay capa: la tecla es un botón normal (usa su fila "normal")
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

// Zona muerta de los sticks (0.05 = 5%). Radial: por debajo vale 0 y el resto se
// reescala para que el stick siga llegando al 100% sin saltos.
#define ZONA_MUERTA_IZQ 0.05f
#define ZONA_MUERTA_DER 0.0f

// C-stick como botones en la capa: qué tanto hay que moverlo (0..1)
#define C_STICK_UMBRAL 0.5f

struct MapeoBoton {
  uint32_t normal;
  uint32_t conCapa;
  uint32_t mantener;
};

#ifndef MAPEO_CLASICO
// ===================== Mapeo "capa Start" (principal) =====================
// Start no manda nada: mientras lo mantienes, cambian los botones con "con Start".
// (El Start y el Back del juego salen de Start+Y y Start+X.)
#define TECLA_CAPA GC_START
#define CAPA_TOQUE NADA      // lo que manda Start solo al tocarlo
#define CAPA_MANTENIDA NADA  // lo que manda Start solo al mantenerlo

// Una fila por botón del GC, en este orden exacto (ver GcState.h).
static const MapeoBoton MAPEO[GC_NUM_BOTONES] = {
    //  normal           con Start       mantener
    {XB_A,              NADA,           NADA},      // A
    {XB_B,              NADA,           NADA},      // B
    {XB_X,              XB_BACK,        NADA},      // X          | Start+X = Back (-)
    {XB_Y,              XB_START,       NADA},      // Y          | Start+Y = Start (+)
    {XB_RB,             XB_R3,          NADA},      // Z = RB     | Start+Z = R3
    {XB_LT,             XB_LB,          NADA},      // L  (analógico) | Start+L = LB
    {XB_RT,             XB_RB,          NADA},      // R  (analógico) | Start+R = RB
    {NADA,              NADA,           NADA},      // Start (es la tecla de capa)
    {XB_DPAD_ARRIBA,    XB_GUIA,        NADA},      // D-pad ↑    | Start+↑ = botón Xbox (Home)
    {XB_DPAD_ABAJO,     NADA,           NADA},      // D-pad ↓    | (Start+↓ libre)
    {XB_DPAD_IZQ,       XB_L3,          NADA},      // D-pad ←    | Start+← = L3
    {XB_DPAD_DER,       XB_R3,          NADA},      // D-pad →    | Start+→ = R3
    {NADA,              NADA,           NADA},      // C-stick ↑  (es el stick derecho)
    {NADA,              NADA,           NADA},      // C-stick ↓
    {NADA,              NADA,           NADA},      // C-stick ←
    {NADA,              NADA,           NADA},      // C-stick →
};

#else
// ===================== Mapeo "clásico" =====================
// Z = RB. Start es la tecla de capa, con toque y mantener propios:
//   toque = Start (+), mantener 0.3 s = Back (-), Start + D-pad ↑ = botón Xbox.
// Para el combo, presiona D-pad ↑ antes de que pasen 0.3 s con Start.
#define TECLA_CAPA GC_START
#define CAPA_TOQUE XB_START     // Start tocado
#define CAPA_MANTENIDA XB_BACK  // Start mantenido

// Una fila por botón del GC, en este orden exacto (ver GcState.h).
static const MapeoBoton MAPEO[GC_NUM_BOTONES] = {
    //  normal           con Start       mantener
    {XB_A,              NADA,           NADA},      // A
    {XB_B,              NADA,           NADA},      // B
    {XB_X,              NADA,           NADA},      // X
    {XB_Y,              NADA,           NADA},      // Y
    {XB_RB,             NADA,           NADA},      // Z = RB
    {XB_LT,             NADA,           NADA},      // L  (analógico)
    {XB_RT,             NADA,           NADA},      // R  (analógico)
    {NADA,              NADA,           NADA},      // Start (es la tecla de capa; ver CAPA_TOQUE/CAPA_MANTENIDA)
    {XB_DPAD_ARRIBA,    XB_GUIA,        NADA},      // D-pad ↑    | Start+↑ = botón Xbox
    {XB_DPAD_ABAJO,     NADA,           NADA},      // D-pad ↓
    {XB_DPAD_IZQ,       NADA,           NADA},      // D-pad ←
    {XB_DPAD_DER,       NADA,           NADA},      // D-pad →
    {NADA,              NADA,           NADA},      // C-stick ↑  (es el stick derecho)
    {NADA,              NADA,           NADA},      // C-stick ↓
    {NADA,              NADA,           NADA},      // C-stick ←
    {NADA,              NADA,           NADA},      // C-stick →
};
#endif

#endif
