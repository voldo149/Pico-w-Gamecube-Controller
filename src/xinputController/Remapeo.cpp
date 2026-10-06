#include "Remapeo.h"

#include <math.h>
#include <string.h>

#include "MapeoXInput.h"

static inline bool esCStick(int b) { return b >= GC_C_ARRIBA && b <= GC_C_DER; }

static int16_t aEje(float v) {
  if (v > 1.0f) v = 1.0f;
  if (v < -1.0f) v = -1.0f;
  return (int16_t)(v * 32767.0f);
}

// Zona muerta radial con reescalado: dentro de la zona = 0, fuera va de 0 a 1
static void zonaMuerta(float *x, float *y, float zona) {
  if (zona <= 0.0f) return;
  const float mag = sqrtf(*x * *x + *y * *y);
  if (mag <= zona) {
    *x = *y = 0.0f;
    return;
  }
  float nueva = (mag - zona) / (1.0f - zona);
  if (nueva > 1.0f) nueva = 1.0f;
  *x *= nueva / mag;
  *y *= nueva / mag;
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

  // 2) Capa (TECLA_CAPA). Sin toque/mantener propio es instantanea; con
  //    CAPA_TOQUE / CAPA_MANTENIDA espera a ver si hay combo (ver MapeoXInput.h)
  const uint32_t bitCapa = GC_BIT(TECLA_CAPA);
  const bool capaAhora = b & bitCapa;
  const bool capaConFuncion = (CAPA_TOQUE != NADA) || (CAPA_MANTENIDA != NADA);
  bool capa = false;
  uint32_t salCapa = 0;
  if (MODO_Z == Z_ALTERNAR) {
    if (capaAhora && !(_botonesAntes & bitCapa)) _capaFija = !_capaFija;
    capa = _capaFija;
  } else if (MODO_Z == Z_MANTENER && !capaConFuncion) {
    capa = capaAhora;
  } else if (MODO_Z == Z_MANTENER) {
    uint32_t combinables = 0;
    for (int i = 0; i < GC_NUM_BOTONES; i++) {
      if (i != TECLA_CAPA && MAPEO[i].conCapa != NADA) combinables |= GC_BIT(i);
    }
    const uint32_t nuevos = b & ~_botonesAntes & ~bitCapa;
    if (capaAhora) {
      if (_capa == CAPA_SUELTA) {
        _capa = CAPA_PENDIENTE;
        _capaDesde = ahora;
      }
      if (_capa == CAPA_PENDIENTE) {
        if (nuevos & combinables) {
          _capa = CAPA_COMBO;
        } else if (ahora - _capaDesde >= TIEMPO_MANTENER_MS) {
          _capa = CAPA_MANTENIDA_EST;
        }
      }
    } else {
      if (_capa == CAPA_PENDIENTE) {  // soltada antes de tiempo y sin combo: toque
        _capaPulso = true;
        _capaPulsoHasta = ahora + PULSO_MS;
      }
      _capa = CAPA_SUELTA;
    }
    capa = (_capa == CAPA_PENDIENTE || _capa == CAPA_COMBO);
    if (_capa == CAPA_MANTENIDA_EST) salCapa |= CAPA_MANTENIDA;
    if (_capaPulso) {
      if ((int32_t)(ahora - _capaPulsoHasta) < 0) {
        salCapa |= CAPA_TOQUE;
      } else {
        _capaPulso = false;
      }
    }
  }

  // 3) Cada botón
  uint32_t sal = 0;
  uint8_t lt = 0, rt = 0;
  bool cComoBotonConZ = false;
  for (int i = 0; i < GC_NUM_BOTONES; i++) {
    if (i == TECLA_CAPA && MODO_Z != Z_BOTON) continue;
    const MapeoBoton &m = MAPEO[i];
    const uint32_t bit = GC_BIT(i);
    const bool abajo = b & bit;
    const bool antes = _botonesAntes & bit;
    EstadoBoton &e = _boton[i];

    if (abajo && !antes) {
      if (capa && m.conCapa != NADA) {
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
        o = m.conCapa;
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

  // Lo que manda la tecla de capa por si sola (toque / mantenida)
  sal |= salCapa;
  if (salCapa & XB_LT) lt = 255;
  if (salCapa & XB_RT) rt = 255;

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
  float lx = gc.lx, ly = gc.ly, cx = gc.cx, cy = gc.cy;
  zonaMuerta(&lx, &ly, ZONA_MUERTA_IZQ);
  zonaMuerta(&cx, &cy, ZONA_MUERTA_DER);
  salida->lx = aEje(lx);
  salida->ly = aEje(ly);
  salida->rx = cAnalogico ? aEje(cx) : 0;
  salida->ry = cAnalogico ? aEje(cy) : 0;

  _botonesAntes = b;
}
