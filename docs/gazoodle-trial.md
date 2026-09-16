# Gazoodle upstream trial

[`century-gazoodle-trial.yaml`](../firmware/century-gazoodle-trial.yaml) uses the unchanged
[Gazoodle CenturyVSPump component](https://github.com/gazoodle/CenturyVSPump)
at commit `0dafb0657ec63941440e8fbc63be7a07aca3265e`.
It is separate from the local `century_vspump` adaptation and Pentair firmware.

Use ESPHome **2026.7.4** for this trial. The upstream component uses the older
Modbus callbacks; do not assume compatibility with newer ESPHome releases.
The configuration selects ESP32-S3, GPIO2 TX, GPIO1 RX, 9600 baud, 8N1, and
address `0x15`. Wi-Fi values come from the existing `secrets.yaml`.

The unchanged upstream source compiled successfully for ESP32-S3 with ESPHome
2026.7.4 on 2026-09-16. This confirms build compatibility, not pump operation.

To use this from a newer Home Assistant ESPHome installation, build in a
separate environment with the version below. Do not downgrade the shared
ESPHome installation used by other controllers. With Python 3.11 or newer:

```sh
python -m venv .venv-gazoodle
# Linux/macOS:
source .venv-gazoodle/bin/activate
# Windows PowerShell instead:
# .\.venv-gazoodle\Scripts\Activate.ps1
python -m pip install esphome==2026.7.4
```

Copy `firmware/gazoodle-secrets.example.yaml` to `firmware/secrets.yaml` and
enter the destination Wi-Fi credentials; preserve an existing secrets file.
Run from the repository root:

```sh
esphome compile firmware/century-gazoodle-trial.yaml
esphome upload firmware/century-gazoodle-trial.yaml --device DEVICE
esphome logs firmware/century-gazoodle-trial.yaml --device DEVICE
```

Replace `DEVICE` with the intended ESP32's USB serial port or IP address.
Use a USB flash if the existing firmware's OTA credentials are unavailable.

The trial exposes Run, Speed (600–3450 RPM), Motor RPM, and Requested RPM to
Home Assistant. UART transmit and receive bytes are logged for troubleshooting.
There is no LCD configuration and **no GPIO4 maintenance inhibit** in this
upstream trial. Run is configured `ALWAYS_OFF`; that setting is not evidence
that the physical motor has stopped. This firmware can send control commands.

Target hardware reported by the user: Century drive `10017000-001-001`, the
same pump used for the earlier Hayward controller capture. Switches 1 and 2
are ON; the supplied photo appears to show 3–5 OFF. Exact protocol support
remains unverified. Leave these settings unchanged for the initial trial.

Flash with the pump bus disconnected. For a supervised pump test, isolate
pump mains before changing wiring, disconnect the original controller from
the RS-485 bus, and connect the ESP32 controller using the verified terminals.
First look for valid RPM/demand responses. Set 600 RPM before issuing Run,
and verify actual RPM and physical operation rather than relying on the Run
switch alone (the upstream switch publishes commands optimistically).
Verify Stop before ending the test. This is a compatibility trial, not a
validated unattended controller.
