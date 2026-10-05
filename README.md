# Pico W Gamecube Controller

Adaptador de control de **GameCube (o N64) → Nintendo Switch / PC** con una Raspberry Pi Pico W.
Basado en [retro-pico-switch](https://github.com/DavidPagels/retro-pico-switch) de David Pagels (licencia MIT),
con cambios propios: mapeo de gatillos (L analógico = ZL, L + D-pad = L; Z = R), atajos, memoria del
último host Bluetooth y versión de batería.

## Firmware compilado (`firmware/`)

| Archivo | Conexión | Se presenta como |
|---|---|---|
| `gc_picow_bluetooth.uf2` | Bluetooth (Pico W) | Pro Controller |
| `gc_picow_bluetooth_bateria.uf2` | Bluetooth, versión batería (arranca dormido y se apaga solo) | Pro Controller |
| `gc_picow_usb.uf2` | USB | HORIPAD (HORI, compatible con Switch) |
| `gc_picow_xinput.uf2` | USB, para PC | **Control de Xbox 360** (con capa Z) |
| `gc_picow_pc.uf2` | **PC, todo en uno:** cable USB → Xbox 360 por USB; sin cable → Xbox por Bluetooth | **Control de Xbox** (con capa Z) |
| `retro_pico_switch.uf2` | USB (compilación anterior, nombre por defecto) | HORIPAD |

Para instalar: mantener BOOTSEL al conectar la Pico por USB y arrastrar el `.uf2` a la unidad que aparece.

## Modo PC: control de Xbox (`gc_picow_pc.uf2`)

Un solo firmware para PC:

- **Cable USB conectado a una PC → control de Xbox 360 por USB.** Siempre tiene prioridad,
  aunque esté en Bluetooth: al conectar el cable se reinicia como USB.
- **Sin cable → Bluetooth** ("Xbox Wireless Controller", modelo Series, Bluetooth LE).
  Arranca dormido (radio apagada):
  - **cualquier botón** → despierta y se reconecta solo a la PC guardada
  - **Start+Y 1 s** → despierta y se hace visible para emparejar una PC nueva

  | Atajo (Bluetooth, ya prendido) | Qué hace |
  |---|---|
  | Y+Start 1 s | Emparejar una PC nueva (visible 60 s) |
  | Y+Start 8 s | Olvidar todas las PCs y quedar visible |
  | L+R+Start 3 s | Apagar (se duerme) |
  | 5 min sin usarlo | Se apaga solo |
  | A+B+Z+Start 3 s | Modo carga de `.uf2` (BOOTSEL) |

  Se empareja desde Configuración → Bluetooth → Agregar dispositivo, sin PIN.
  LED: fijo = conectado · parpadeo lento = esperando a la PC · parpadeo rápido =
  emparejando · apagado = dormido. Mientras se mantiene un atajo no se manda nada al juego.
- **Si algo se congela**, un vigilante (watchdog) reinicia la Pico sola en 2-3 s y vuelve
  al modo en el que estaba.

`gc_picow_xinput.uf2` es la versión solo USB. El mapeo de los dos se edita en
[`include/MapeoXInput.h`](include/MapeoXInput.h) (incluye zona muerta del stick izquierdo, 5%).

| GameCube | Xbox 360 | Con Z mantenida |
|---|---|---|
| A / B | A / B | — |
| X | X | **RB** |
| Y | Y | **LB** |
| L / R | LT / RT (analógicos) | — |
| Start | toque = **Start** · mantener 0.3 s = **botón Xbox** | **Back** |
| D-pad ↑ / ↓ | D-pad ↑ / ↓ | **L3** / **R3** |
| D-pad ← / → | D-pad ← / → | (libres) |
| Stick / C-stick | Stick izquierdo / derecho | — |
| **Z** | no manda nada: es solo la tecla de capa (instantánea) | |

**A+B+Z+Start durante 3 s** reinicia la Pico en modo carga de `.uf2` (BOOTSEL), igual que el firmware Bluetooth.

Capa Z: mientras mantienes Z, los botones con función "con Z" cambian al instante
(`MODO_Z` también permite que Z prenda/apague la capa con cada toque). Los botones sin
función "con Z" siguen funcionando normal. Cada botón puede tener toque/mantener
(columna "mantener"); el toque se manda al soltar. Las pruebas de esta lógica
están en `tests/` (`test_remapeo.cpp`, `test_xbox_bt.cpp`, `test_despertar.cpp`) y corren en la PC.

## Compilar

`CMakeLists.txt` acepta `-DPICO_BOARD=pico_w|pico`, `-DXINPUT=ON|OFF` (Xbox 360 por USB),
`-DGC_PC=ON|OFF` (firmware de PC: USB + Bluetooth en uno, solo Pico W), `-DSWITCH_BLUETOOTH=ON|OFF`,
`-DLOW_POWER_MODE=ON|OFF` y `-DCONTROLLER_TYPE=Gamecube|N64`. También hay un `Dockerfile`.

---

## README original

### N64/Gamecube -> Raspberry Pi Pico -> Nintendo Switch

## Table of Contents

- [Description](#description)
- [Inspiration](#inspiration)
- [Update](#update)
- [Update 2](#update-2)
- [Setup](#setup)
- [3D Printed Enclosure/Plug](#3d-printed-enclosureplug)
- [Credits](#credits)

---

## Description

This code allows a Raspberry Pi Pico to control a Nintendo Switch using button and joystick inputs from an original N64 or Gamecube controller. This is a spiritual successor to my [N64-Arduino-Switch](https://github.com/DavidPagels/n64-arduino-switch) project.

## Inspiration

It's been a year since I published the [N64-Arduino-Switch](https://github.com/DavidPagels/n64-arduino-switch) project. Since then, the wireless N64 Nintendo Switch controllers have still been largely out of stock and scalped. One of the pieces of feedback from the last project is that it looked intimidating to setup. I've since discovered the Raspberry Pi Pico and decided to redo this project for the Pico. This has 2 advantages over the Arduino project:
  
- Programming involves plugging in a Micro USB cable while holding a button, dragging and dropping a file, and you're done! No more installing a Device Firmware Update (DFU) programmer, installing extra libraries, or shorting pins to get the device into a DFU mode.
- The Pico only costs $4 instead of $10 for an off-brand, slower Arduino!
- For 2 more dollars than a base Pico, the Pico W microcontroller supports Bluetooth!

Also, when playing through The Legend of Zelda: Ocarina of Time last year on the Arduino version, it was annoying not having a way to go to the Switch's Home screen using the N64 controller. Since I didn't rely on a library to handle N64 controller communication this time, I found out that there is a special 'Reset' bit that's set when L + R + Start are held at the same time. This project maps that input to the Home button on the Switch!

## Update

This project no longer only supports the N64 controller, but now supports Gamecube controllers too!

Both N64 and Gamecube versions have L + R + Start mapped to the Switch home button. They also both have dynamic scaling on each axis of each analog joystick to account for reduced joystick range on old controllers.

## Update 2

Shortly after the last update, the Pico SDK was updated to include beta Bluetooth support. This project now supports connecting to a Nintendo Switch via USB or Bluetooth! This is my first project with Bluetooth, so this first iteration requires you to connect to a Switch in the 'Change Grip/Order' menu. If the Pico becomes disconnected, it may also have to be power cycled to get it to pair with the Switch again. I hope to improve the project so you only have to do this the first time you connect to a Switch, and so it automatically reconnects without power cycling. Also, while the Pico wirelessly connects to the Switch, it will still need to be powered via USB (any USB port - a battery bank should work).

## Setup

To program the Raspberry Pi Pico

1. Hold down the boot sel button on the Pico, and while holding, plug it into your PC via Micro USB cable.
2. Open the Pico in your File Explorer.
3. Download the .uf2 file from [this repository's most recent release](https://github.com/davidpagels/retro-pico-switch/releases).
4. Drag and drop the .uf2 file to the Pico folder from step 2.
5. Done!

Once these steps are done, [wire up your N64 controller](https://github.com/pothos/arduino-n64-controller-library/blob/master/README.md#wireing) or [your Gamecube controller](https://simplecontrollers.com/blogs/resources/gamecube-protocol). The pre-built .uf2 file assumes the data pin is on the Pico's [GP18 pin](https://datasheets.raspberrypi.com/pico/Pico-R3-A4-Pinout.pdf). A 1k pullup resistor should also be wired between 3.3v and your data pin to work properly.

## 3D Printed Enclosure/Plug

I've also designed a small enclosure to house the Pico and act as a plug for a switch controller! More details and files are available [here](https://www.thingiverse.com/thing:5823446)

<a href="https://www.thingiverse.com/thing:5823446">
  <img width="400" src="resources/N64%20Male%20Connector.jpg"/>
  <img width="400" src="resources/Pico%20Enclosure.jpg"/>
</a>

## Credits

This project would have taken a lot longer without the work done by everyone credited in the original [N64-Arduino-Switch](https://github.com/DavidPagels/n64-arduino-switch) project along with the creators and contributors of [TinyUsb](https://github.com/hathach/tinyusb), the [GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE) project, and the [MPG](https://github.com/OpenStickCommunity/MPG) project. Figuring out how to interface via Bluetooth also took me 1.5 months of work in my off time and would have taken even longer without the work done by Brikwerk on the [nxbt](https://github.com/Brikwerk/nxbt) project. If you're into developing this sort of stuff, definitely check their stuff out!
