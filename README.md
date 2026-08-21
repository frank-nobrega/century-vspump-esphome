# Century VSPump for ESPHome

An ESPHome external component for controlling Century/Regal variable-speed pool-pump motors over their RS485 custom-function protocol.

The component supports:

- Start and stop control
- Speed demand from 600 to 3450 RPM in 50 RPM increments
- Motor speed, current, input power, and shaft output power
- DC bus voltage, drive temperature, fault code, and operating status
- A physical maintenance-inhibit input that forces the pump to stop

## Compatibility

This version targets ESPHome 2026.8 or newer and uses the current `ModbusClientDevice` PDU callback API.

The pump must be configured for its documented Modbus-compatible automation mode. Verify the motor model, DIP-switch configuration, wiring, and protocol before issuing commands. Pool-pump equipment operates around mains voltage and water; use suitable isolation and follow applicable electrical codes.

## Installation

Reference the repository directly from the ESPHome device configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/frank-nobrega/century-vspump-esphome
      ref: main
    components:
      - century_vspump
```

For a stable installation, replace `main` with a release tag after selecting a tested release.

See [`example.yaml`](example.yaml) for the complete controller configuration, including the 20x4 PCF8574 LCD on GPIO5/GPIO6 at I2C address `0x27`. Adjust the pins or address if your hardware differs. Keep Wi-Fi credentials, API keys, and OTA passwords in the Home Assistant ESPHome `secrets.yaml` file.

## Acknowledgements

The protocol work was informed by Gazoodle's [CenturyVSPump](https://github.com/gazoodle/CenturyVSPump) project and community research into the Century/Regal VGreen protocol. This implementation has been adapted for the current ESPHome Modbus client API and adds expanded telemetry and a maintenance inhibit.

## License

GPL-3.0-only. See [`LICENSE`](LICENSE).
