# Suggested first build

1. Create a GitHub repository with this directory structure:

```text
components/aquamqtt/...
example-aquamqtt-v5.yaml
```

2. Replace the `external_components` source in the YAML with your GitHub repository.

3. Change the OTA password and hotspot password.

4. Compile before connecting the heat pump:

```text
esphome compile example-aquamqtt-v5.yaml
```

5. Flash by USB the first time.

6. Connect to `AquaMQTT V5 Setup` and configure Wi-Fi using the captive portal.

7. After the node joins Wi-Fi, use the ESPHome dashboard/CLI for subsequent OTA updates.

8. Only after the firmware is compiling and booting normally should the two UARTs be connected to the V5 MITM hardware.
