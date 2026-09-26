# AquaMQTT V5 ESPHome external component

This is an ESPHome-native port of the AquaMQTT V5 protocol implementation.

## Current target

The component targets **ESPHome 2026.9.x** and ESP32/ESP-IDF. ESPHome's current UART component is used for the physical UARTs; the AquaMQTT component owns only V5 framing/protocol handling and the ESP32-specific one-wire MITM GPIO-matrix switching required by the original V5 hardware.

ESPHome's native entity platforms are used for sensors, binary sensors, text sensors and the operation-mode select. The original custom MQTT client, Home Assistant MQTT discovery and custom Wi-Fi handling are not included.

## Repository layout

```text
components/aquamqtt/
```

Put this directory in a Git repository and reference it with `external_components`.

## Important hardware detail

The V5 MITM board uses two UARTs and two TX-enable pins. The default pin mapping used by the example is:

- MAIN RX: GPIO5
- MAIN TX: GPIO6
- HMI RX: GPIO7
- HMI TX: GPIO8
- MAIN TX enable: GPIO9
- HMI TX enable: GPIO10

The upstream V5 relay implementation uses these signals to connect the ESP32 UART TX signal to both sides of the one-wire level shifter while transmitting, then disconnect it again while receiving.

Because ESPHome's public UART API does not expose the physical RX/TX GPIO numbers from a `UARTComponent`, the pins are intentionally specified twice: once in the native `uart:` blocks and once under `aquamqtt:`. They must match.

## Wi-Fi provisioning

The example deliberately does **not** contain a normal Wi-Fi SSID/password. It starts an ESPHome fallback access point named `AquaMQTT V5 Setup`. With `captive_portal:` enabled, connect to that hotspot and use the portal to enter the real Wi-Fi credentials.

Change the hotspot and OTA passwords before installing the device.

## GitHub testing

If the repository contains `components/aquamqtt`, add this to the test YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/YOUR_GITHUB_USER/YOUR_REPOSITORY
      ref: main
    components: [aquamqtt]
```

Then use the supplied `example-aquamqtt-v5.yaml` as the starting configuration.

## Listener mode

The same protocol component can run without MITM forwarding:

```yaml
aquamqtt:
  id: aquamqtt
  mode: listener
  uart_id: main_uart
```

In listener mode only one UART is consumed and no frames are transmitted or modified.

## Testing notes

This package was checked for Python syntax in this environment, but a complete ESPHome/PlatformIO firmware build could not be run here because ESPHome itself is not installed in the build environment. The first GitHub/ESPHome compile should therefore be treated as the API compatibility test.
