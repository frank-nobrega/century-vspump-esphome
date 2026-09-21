# Century VSPump for ESPHome

An ESPHome external component for controlling Century/Regal variable-speed pool-pump motors over their RS485 custom-function protocol.

The component supports:

- Start and stop control
- Speed demand from 600 to 3450 RPM in 50 RPM increments
- Motor speed, current, input power, and shaft output power
- DC bus voltage, drive temperature, fault code, and operating status
- A physical maintenance-inhibit input that forces the pump to stop
- Four configurable, independently enabled daily RPM schedule blocks
- A configurable schedule end time, master enable, and manual override
- A 20x4 LCD status display, local web interface, and Wi-Fi fallback portal

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

See [`example.yaml`](example.yaml) for the complete controller configuration, including scheduling and the 20x4 PCF8574 LCD on GPIO5/GPIO6 at I2C address `0x27`. Adjust the pins or address if your hardware differs. Define `wifi_ssid`, `wifi_password`, and `fallback_password` in the Home Assistant ESPHome `secrets.yaml` file.

The schedule uses the Home Assistant time source with the `America/New_York`
timezone. Change the timezone in the YAML if the controller is installed
elsewhere. Schedule settings are stored on the ESP32 and continue to run after
Home Assistant disconnects, provided the controller has synchronized its clock
since boot.

## Gazoodle original-code trial

For the next supervised pump test, use
[`firmware/century-gazoodle-trial.yaml`](firmware/century-gazoodle-trial.yaml).
It loads Gazoodle's unchanged component at a pinned commit and uses this
board's ESP32-S3, GPIO2/GPIO1 RS-485 pins, 9600 baud, and address `0x15`.
It compiled successfully with **ESPHome 2026.7.4**; pump compatibility has not
yet been tested. Follow the [installation and test instructions](docs/gazoodle-trial.md).

This trial provides Run, Speed, and RPM readings. It has **no LCD support or
GPIO4 maintenance inhibit**. The existing `example.yaml` and adapted component
remain available separately. This trial needs its own ESPHome 2026.7.4 build
environment; the adapted component above targets a different ESPHome API.

## Temporary Hayward protocol capture

The photographed installation uses a Hayward wall display with a Century
motor, and its working bus may differ from the protocol assumptions in this
component. Before the next controller test, use the receive-only
[`century-bus-sniffer.yaml`](firmware/century-bus-sniffer.yaml) image and follow
the complete [passive sniffer field guide](docs/passive-rs485-sniffer.md). The
guide covers parallel four-wire wiring, pump-auxiliary power through the
existing buck converter, the hardware TX lockout, capture steps, and restoration
of the normal controller firmware.

The sniffer files are temporary diagnostics and do not replace `example.yaml`.

## Acknowledgements

The protocol work was informed by Gazoodle's [CenturyVSPump](https://github.com/gazoodle/CenturyVSPump) project and community research into the Century/Regal VGreen protocol. This implementation has been adapted for the current ESPHome Modbus client API and adds expanded telemetry and a maintenance inhibit.

## License

GPL-3.0-only. See [`LICENSE`](LICENSE).
