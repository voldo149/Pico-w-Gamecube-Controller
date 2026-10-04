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

  // 2) Capa Z: sin retraso, Z no manda nada por sí sola
  const uint32_t bitZ = GC_BIT(GC_Z);
  const bool zAhora = b & bitZ;
  bool capa = false;
  if (MODO_Z == Z_MANTENER) {
    capa = zAhora;
  } else if (MODO_Z == Z_ALTERNAR) {
    if (zAhora && !(_botonesAntes & bitZ)) _capaFija = !_capaFija;
    capa = _capaFija;
  }

  // 3) Cada botón
  uint32_t sal = 0;
  uint8_t lt = 0, rt = 0;
  bool cComoBotonConZ = false;
  for (int i = 0; i < GC_NUM_BOTONES; i++) {
    if (i == GC_Z && MODO_Z != Z_BOTON) continue;
    const MapeoBoton &m = MAPEO[i];
    const uint32_t bit = GC_BIT(i);
    const bool abajo = b & bit;
    const bool antes = _botonesAntes & bit;
    EstadoBoton &e = _boton[i];

    if (abajo && !antes) {
      if (capa && m.conZ != NADA) {
        e.modo = CON_Z;
      } else if (m.mantener != NADA) {
        e.modo = ESPERANDO;
        e.desde = ahora;
      } else {
        e.modo = NORMAL;
      }
    }
    if (!abajo && antes) {
      if (e.modo == ESPERANDO) {  // se soltó antes de tiempo: fue un toque
        e.pulso = true;
        e.pulsoHasta = ahora + PULSO_MS;
      }
      e.modo = SUELTO;
    }
    if (abajo && e.modo == ESPERANDO && ahora - e.desde >= TIEMPO_MANTENER_MS) {
      e.modo = MANTENIDO;
    }

    uint32_t o = NADA;
    bool analogico = false;  // el gatillo sale de su propio gatillo del GC
    switch (e.modo) {
      case NORMAL:
        o = m.normal;
        analogico = true;
        break;
      case CON_Z:
        o = m.conZ;
        if (esCStick(i)) cComoBotonConZ = true;
        break;
      case MANTENIDO:
        o = m.mantener;
        break;
      default:
        break;
    }
    if (e.pulso) {
      if ((int32_t)(ahora - e.pulsoHasta) < 0) {
        o |= m.normal;
      } else {
        e.pulso = false;
      }
    }
    sal |= o;

    if (o & XB_LT) {
      uint8_t v = (analogico && i == GC_L) ? aGatillo(gc.lAnalog, gc.botones & GC_BIT(GC_L)) : 255;
      if (v > lt) lt = v;
    }
    if (o & XB_RT) {
      uint8_t v = (analogico && i == GC_R) ? aGatillo(gc.rAnalog, gc.botones & GC_BIT(GC_R)) : 255;
      if (v > rt) rt = v;
    }
  }

  // 4) Sticks. El C-stick es el stick derecho salvo que se use como botones
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
