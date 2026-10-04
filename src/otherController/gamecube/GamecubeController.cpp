
#include "GamecubeController.h"

#include "Controller.pio.h"
#include "pico/stdlib.h"

void GamecubeController::init() {
  if (!_pioReady) {
    _pio = pio0;
    _offset = pio_add_program(_pio, &controller_program);
    _sm = pio_claim_unused_sm(_pio, true);
    pio_sm_config tmpConfig = controller_program_get_default_config(_offset);
    _c = &tmpConfig;
    controller_program_init(_pio, _sm, _offset, _pin, _c);
    _pioReady = true;
  } else {
    // Reintento: limpiar restos de un intento fallido
    pio_sm_set_enabled(_pio, _sm, false);
    pio_sm_clear_fifos(_pio, _sm);
    pio_sm_restart(_pio, _sm);
    pio_sm_exec(_pio, _sm, pio_encode_jmp(_offset));
    pio_sm_set_enabled(_pio, _sm, true);
  }
  uint8_t data[1] = {0x00};
  uint8_t response[3];
  sendData(data, 1, response, 3);
  sleep_us(200);
  updatePioOutputSize(24);
}

void GamecubeController::updateState() {
  uint8_t data[3] = {0x40, 0x03, 0x0};
  sendData(data, 3, _controllerState, 8);
  busy_wait_us(500);
}

void GamecubeController::getSwitchUsbReport(SwitchUsbReport *switchUsbReport) {
  updateState();
  switchUsbReport->hat = SWITCH_USB_HAT_NOTHING;
  switchUsbReport->rx = SWITCH_USB_JOYSTICK_MID;
  switchUsbReport->ry = SWITCH_USB_JOYSTICK_MID;
  switchUsbReport->lx = SWITCH_USB_JOYSTICK_MID;
  switchUsbReport->ly = SWITCH_USB_JOYSTICK_MID;

  bool leftTrigger =
      (GC_MASK_L & _controllerState[1]) || _controllerState[6] > 0x30;
  bool dpadHeld = (GC_MASK_DPAD & _controllerState[1]) != 0;

  switchUsbReport->buttons =
      (GC_MASK_START & _controllerState[0] ? SWITCH_USB_MASK_PLUS : 0) |
      (GC_MASK_Y & _controllerState[0] ? SWITCH_USB_MASK_Y : 0) |
      (GC_MASK_X & _controllerState[0] ? SWITCH_USB_MASK_X : 0) |
      (GC_MASK_B & _controllerState[0] ? SWITCH_USB_MASK_B : 0) |
      (GC_MASK_A & _controllerState[0] ? SWITCH_USB_MASK_A : 0) |
      // Gatillo izquierdo: ZL; si hay direccion del d-pad presionada = L
      (leftTrigger && dpadHeld ? SWITCH_USB_MASK_L : 0) |
      (leftTrigger && !dpadHeld ? SWITCH_USB_MASK_ZL : 0) |
      // Gatillo derecho: toque analogico o presionado completo = ZR
      ((GC_MASK_R & _controllerState[1]) || _controllerState[7] > 0x30
           ? SWITCH_USB_MASK_ZR
           : 0) |
      // Boton Z = R
      (GC_MASK_Z & _controllerState[1] ? SWITCH_USB_MASK_R : 0) |
      ((GC_MASK_L & _controllerState[1]) && (GC_MASK_R & _controllerState[1]) &&
               (GC_MASK_START & _controllerState[0])
           ? SWITCH_USB_MASK_HOME
           : 0);

  switch (GC_MASK_DPAD & _controllerState[1]) {
    case GC_MASK_DPAD_UP:
      switchUsbReport->hat = SWITCH_USB_HAT_UP;
      break;
    case GC_MASK_DPAD_UPRIGHT:
      switchUsbReport->hat = SWITCH_USB_HAT_UPRIGHT;
      break;
    case GC_MASK_DPAD_RIGHT:
      switchUsbReport->hat = SWITCH_USB_HAT_RIGHT;
      break;
    case GC_MASK_DPAD_DOWNRIGHT:
      switchUsbReport->hat = SWITCH_USB_HAT_DOWNRIGHT;
      break;
    case GC_MASK_DPAD_DOWN:
      switchUsbReport->hat = SWITCH_USB_HAT_DOWN;
      break;
    case GC_MASK_DPAD_DOWNLEFT:
      switchUsbReport->hat = SWITCH_USB_HAT_DOWNLEFT;
      break;
    case GC_MASK_DPAD_LEFT:
      switchUsbReport->hat = SWITCH_USB_HAT_LEFT;
      break;
    case GC_MASK_DPAD_UPLEFT:
      switchUsbReport->hat = SWITCH_USB_HAT_UPLEFT;
      break;
  }

  // Scale for joystick insensitivity if needed
  // https://GCsquid.com/GC-joystick-360-degrees/ GC Y axis is inverted relative
  // to Switch
  switchUsbReport->lx = convertToSwitchUsbJoystick(_controllerState[2],
                                                   &_minAnalogX, &_maxAnalogX);
  switchUsbReport->ly = convertToSwitchUsbJoystick(
      GC_JOYSTICK_MAX - _controllerState[3], &_minAnalogY, &_maxAnalogY);
  switchUsbReport->rx =
      convertToSwitchUsbJoystick(_controllerState[4], &_minCX, &_maxCX);
  switchUsbReport->ry = convertToSwitchUsbJoystick(
      GC_JOYSTICK_MAX - _controllerState[5], &_minCY, &_maxCY);
  return;
}

uint16_t GamecubeController::convertToSwitchUsbJoystick(uint8_t axisPos,
                                                        double *minAxis,
                                                        double *maxAxis) {
  double unscaledAxisPos =
      (axisPos - GC_JOYSTICK_MID) / (double)GC_JOYSTICK_MID;
  double scaledAxisPos = getScaledAnalogAxis(unscaledAxisPos, minAxis, maxAxis);
  return scaledAxisPos * SWITCH_USB_JOYSTICK_MID + SWITCH_USB_JOYSTICK_MID - 1;
}

void GamecubeController::getSwitchBtReport(SwitchBtReport *switchBtReport) {
  updateState();

  switchBtReport->buttons[0] =
      // Gatillo derecho: toque analogico o presionado completo = ZR
      ((GC_MASK_R & _controllerState[1]) || _controllerState[7] > 0x30
           ? SWITCH_BT_MASK_ZR
           : 0) |
      // Boton Z = R
      (GC_MASK_Z & _controllerState[1] ? SWITCH_BT_MASK_R : 0) |
      (GC_MASK_A & _controllerState[0] ? SWITCH_BT_MASK_A : 0) |
      (GC_MASK_B & _controllerState[0] ? SWITCH_BT_MASK_B : 0) |
      (GC_MASK_X & _controllerState[0] ? SWITCH_BT_MASK_X : 0) |
      (GC_MASK_Y & _controllerState[0] ? SWITCH_BT_MASK_Y : 0);

  switchBtReport->buttons[1] =
      ((GC_MASK_L & _controllerState[1]) && (GC_MASK_R & _controllerState[1]) &&
               (GC_MASK_START & _controllerState[0])
           ? SWITCH_BT_MASK_HOME
           : 0) |
      (GC_MASK_START & _controllerState[0] ? SWITCH_BT_MASK_PLUS : 0);

  switchBtReport->buttons[2] = 0x00;
  switch (GC_MASK_DPAD & _controllerState[1]) {
    case GC_MASK_DPAD_UP:
      switchBtReport->buttons[2] |= SWITCH_BT_HAT_UP;
      break;
    case GC_MASK_DPAD_UPRIGHT:
      switchBtReport->buttons[2] |= SWITCH_BT_HAT_UPRIGHT;
      break;
    case GC_MASK_DPAD_RIGHT:
      switchBtReport->buttons[2] |= SWITCH_BT_HAT_RIGHT;
      break;
    case GC_MASK_DPAD_DOWNRIGHT:
      switchBtReport->buttons[2] |= SWITCH_BT_HAT_DOWNRIGHT;
      break;
    case GC_MASK_DPAD_DOWN:
      switchBtReport->buttons[2] |= SWITCH_BT_HAT_DOWN;
      break;
    case GC_MASK_DPAD_DOWNLEFT:
      switchBtReport->buttons[2] |= SWITCH_BT_HAT_DOWNLEFT;
      break;
    case GC_MASK_DPAD_LEFT:
      switchBtReport->buttons[2] |= SWITCH_BT_HAT_LEFT;
      break;
    case GC_MASK_DPAD_UPLEFT:
      switchBtReport->buttons[2] |= SWITCH_BT_HAT_UPLEFT;
      break;
  }

  // Gatillo izquierdo: ZL; si hay direccion del d-pad presionada = L
  bool leftTrigger =
      (GC_MASK_L & _controllerState[1]) || _controllerState[6] > 0x30;
  bool dpadHeld = (GC_MASK_DPAD & _controllerState[1]) != 0;
  switchBtReport->buttons[2] |=
      (leftTrigger && dpadHeld ? SWITCH_BT_MASK_L : 0) |
      (leftTrigger && !dpadHeld ? SWITCH_BT_MASK_ZL : 0);

  // Scale for joystick insensitivity if needed
  uint16_t lx = convertToSwitchBtJoystick(_controllerState[2], &_minAnalogX,
                                          &_maxAnalogX);
  uint16_t ly = convertToSwitchBtJoystick(_controllerState[3], &_minAnalogY,
                                          &_maxAnalogY);
  switchBtReport->l[0] = lx & 0xff;
  switchBtReport->l[1] = ((ly & 0xf) << 4) | (lx >> 8);
  switchBtReport->l[2] = ly >> 4;

  uint16_t rx =
      convertToSwitchBtJoystick(_controllerState[4], &_minCX, &_maxCX);
  uint16_t ry =
      convertToSwitchBtJoystick(_controllerState[5], &_minCY, &_maxCY);
  switchBtReport->r[0] = rx & 0xff;
  switchBtReport->r[1] = ((ry & 0xf) << 4) | (rx >> 8);
  switchBtReport->r[2] = ry >> 4;

  return;
}

uint16_t GamecubeController::convertToSwitchBtJoystick(uint8_t axisPos,
                                                       double *minAxis,
                                                       double *maxAxis) {
  double unscaledAxisPos =
      (axisPos - GC_JOYSTICK_MID) / (double)GC_JOYSTICK_MID;
  double scaledAxisPos = getScaledAnalogAxis(unscaledAxisPos, minAxis, maxAxis);
  return scaledAxisPos * SWITCH_BT_JOYSTICK_MID + SWITCH_BT_JOYSTICK_MID - 1;
}
float GamecubeController::normalizeAxis(uint8_t axisPos, double *minAxis,
                                        double *maxAxis) {
  double unscaledAxisPos =
      (axisPos - GC_JOYSTICK_MID) / (double)GC_JOYSTICK_MID;
  return (float)getScaledAnalogAxis(unscaledAxisPos, minAxis, maxAxis);
}

void GamecubeController::getGcState(GcState *state) {
  updateState();
  const uint8_t b0 = _controllerState[0];
  const uint8_t b1 = _controllerState[1];
  uint32_t botones = 0;
  if (b0 & GC_MASK_A) botones |= GC_BIT(GC_A);
  if (b0 & GC_MASK_B) botones |= GC_BIT(GC_B);
  if (b0 & GC_MASK_X) botones |= GC_BIT(GC_X);
  if (b0 & GC_MASK_Y) botones |= GC_BIT(GC_Y);
  if (b0 & GC_MASK_START) botones |= GC_BIT(GC_START);
  if (b1 & GC_MASK_Z) botones |= GC_BIT(GC_Z);
  // L/R aqui solo es el clic digital; el remapeo agrega el umbral analogico
  if (b1 & GC_MASK_L) botones |= GC_BIT(GC_L);
  if (b1 & GC_MASK_R) botones |= GC_BIT(GC_R);
  // El d-pad del GC son 4 bits independientes (arriba 0x8, abajo 0x4,
  // derecha 0x2, izquierda 0x1), asi que las diagonales salen solas
  if (b1 & GC_MASK_DPAD_UP) botones |= GC_BIT(GC_DPAD_ARRIBA);
  if (b1 & GC_MASK_DPAD_DOWN) botones |= GC_BIT(GC_DPAD_ABAJO);
  if (b1 & GC_MASK_DPAD_LEFT) botones |= GC_BIT(GC_DPAD_IZQ);
  if (b1 & GC_MASK_DPAD_RIGHT) botones |= GC_BIT(GC_DPAD_DER);
  state->botones = botones;

  state->lx = normalizeAxis(_controllerState[2], &_minAnalogX, &_maxAnalogX);
  state->ly = normalizeAxis(_controllerState[3], &_minAnalogY, &_maxAnalogY);
  state->cx = normalizeAxis(_controllerState[4], &_minCX, &_maxCX);
  state->cy = normalizeAxis(_controllerState[5], &_minCY, &_maxCY);
  state->lAnalog = _controllerState[6];
  state->rAnalog = _controllerState[7];
}
