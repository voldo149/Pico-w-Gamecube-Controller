#include "Remapeo.h"

#include <string.h>

#include "MapeoXInput.h"

static inline bool esCStick(int b) { return b >= GC_C_ARRIBA && b <= GC_C_DER; }

static int16_t aEje(float v) {
  if (v > 1.0f) v = 1.0f;
  if (v < -1.0f) v = -1.0f;
  return (int16_t)(v * 32767.0f);
}

static uint8_t aGatillo(uint8_t analogico, bool clic) {
  if (clic || analogico >= GATILLO_MAX) return 255;
  if (analogico <= GATILLO_UMBRAL) return 0;
  return (uint8_t)((analogico - GATILLO_UMBRAL) * 255 / (GATILLO_MAX - GATILLO_UMBRAL));
}

void Remapeo::actualizar(const GcState &gc, uint32_t ahora, XInputReport *salida) {
  // 1) Botones lógicos: los gatillos cuentan desde el umbral, el C-stick como 4 direcciones
  uint32_t b = gc.botones;
  if (gc.lAnalog > GATILLO_UMBRAL) b |= GC_BIT(GC_L);
  if (gc.rAnalog > GATILLO_UMBRAL) b |= GC_BIT(GC_R);
  if (gc.cy > C_STICK_UMBRAL) b |= GC_BIT(GC_C_ARRIBA);
  if (gc.cy < -C_STICK_UMBRAL) b |= GC_BIT(GC_C_ABAJO);
  if (gc.cx < -C_STICK_UMBRAL) b |= GC_BIT(GC_C_IZQ);
  if (gc.cx > C_STICK_UMBRAL) b |= GC_BIT(GC_C_DER);

  const uint32_t bitZ = GC_BIT(GC_Z);
  const uint32_t nuevos = b & ~_botonesAntes & ~bitZ;

  // Solo los botones con función "con Z" convierten a Z en combo
  uint32_t combinables = 0;
  for (int i = 0; i < GC_NUM_BOTONES; i++) {
    if (i != GC_Z && MAPEO[i].conZ != NADA) combinables |= GC_BIT(i);
  }

  // 2) Capa Z
  if (USAR_CAPA_Z && (b & bitZ)) {
    if (_z == Z_SUELTA) {
      _z = Z_PENDIENTE;
      _zDesde = ahora;
    }
    if (_z == Z_PENDIENTE) {
      if (nuevos & combinables) {
        _z = Z_CAPA;
      } else if (ahora - _zDesde >= Z_TIEMPO_MS) {
        _z = Z_MANTENIDA;
      }
    }
  } else {
    if (_z == Z_PENDIENTE) {  // Z sola, soltada antes de tiempo: toque
      _pulsoActivo = true;
      _pulsoHasta = ahora + Z_PULSO_MS;
    }
    _z = Z_SUELTA;
  }
  const bool capa = (_z == Z_PENDIENTE || _z == Z_CAPA);

  // 3) Cada botón decide su capa al presionarse y la mantiene hasta soltarse
  uint32_t sal = 0;
  uint8_t lt = 0, rt = 0;
  bool cComoBotonConZ = false;
  for (int i = 0; i < GC_NUM_BOTONES; i++) {
    if (i == GC_Z) continue;
    const uint32_t bit = GC_BIT(i);
    if (!(b & bit)) {
      _conZ[i] = false;
      continue;
    }
    if (!(_botonesAntes & bit)) _conZ[i] = capa && MAPEO[i].conZ != NADA;

    const uint32_t o = _conZ[i] ? MAPEO[i].conZ : MAPEO[i].normal;
    sal |= o;
    if (esCStick(i) && _conZ[i]) cComoBotonConZ = true;

    // Gatillos analógicos solo cuando salen de su propio gatillo del GC
    if (o & XB_LT) {
      uint8_t v = (i == GC_L) ? aGatillo(gc.lAnalog, gc.botones & GC_BIT(GC_L)) : 255;
      if (v > lt) lt = v;
    }
    if (o & XB_RT) {
      uint8_t v = (i == GC_R) ? aGatillo(gc.rAnalog, gc.botones & GC_BIT(GC_R)) : 255;
      if (v > rt) rt = v;
    }
  }

  // 4) Lo que manda Z sola
  uint32_t z = 0;
  if (!USAR_CAPA_Z && (b & bitZ)) z = Z_SOLA;
  if (_z == Z_MANTENIDA) z = Z_SOLA;
  if (_pulsoActivo) {
    if ((int32_t)(ahora - _pulsoHasta) < 0) {
      z = Z_SOLA;
    } else {
      _pulsoActivo = false;
    }
  }
  sal |= z;
  if (z & XB_LT) lt = 255;
  if (z & XB_RT) rt = 255;

  // 5) Sticks. El C-stick es el stick derecho salvo que se use como botones
  bool cAnalogico = !cComoBotonConZ;
  for (int i = GC_C_ARRIBA; i <= GC_C_DER; i++) {
    if (MAPEO[i].normal != NADA) cAnalogico = false;
  }

  memset(salida, 0, sizeof(*salida));
  salida->reportId = 0;
  salida->reportSize = sizeof(XInputReport);
  salida->botones = (uint16_t)(sal & 0xFFFF);
  salida->lt = lt;
  salida->rt = rt;
  salida->lx = aEje(gc.lx);
  salida->ly = aEje(gc.ly);
  salida->rx = cAnalogico ? aEje(gc.cx) : 0;
  salida->ry = cAnalogico ? aEje(gc.cy) : 0;

  _botonesAntes = b;
}
