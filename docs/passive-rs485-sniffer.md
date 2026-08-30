# Temporary passive RS-485 sniffer

This procedure captures the known-working traffic between the Hayward wall
display and the Century motor. It is intended to identify the actual protocol,
address, functions, and packet format before changing the pump-control
component. The sniffer firmware contains no Modbus client and no pump-control
component.

The Hayward controller-to-pump bus uses 19200 baud, 8 data bits, no parity,
and one stop bit. The initial 9600-baud capture produced only `00`, `80`,
`C0`, `E0`, `F0`, and `F8` because it sampled Hayward traffic at half its
actual rate. Leave the known-working Hayward controller connected and capture
again with the updated 19200-baud sniffer before deriving packet fields.

The photographed Hayward display board is marked `G1-066182C-1 REV B` and
`090072-202-01`. Its four-wire connection carries auxiliary power, common,
RS-485 A, and RS-485 B. Follow the markings at the pump connector rather than
assuming that wire colors are consistent.

## What to take to the pump

- ESP32-S3-Zero test rig with the isolated automatic-direction RS-485 module
- Existing buck converter, already adjusted and verified at 5.00 V output
- 0.5-1 A inline fuse for the pump auxiliary-power branch
- One 10 kOhm, 1/8 W or 1/4 W resistor (4.7-22 kOhm is acceptable)
- Short branch wires and connectors that safely accept the existing wire plus
  the new branch; do not force two wires under a terminal not rated for them
- Phone or laptop able to open ESPHome Wi-Fi/API logs
- Multimeter

## Flash the sniffer before going live

1. With pump/field power disconnected, install
   [`firmware/century-bus-sniffer.yaml`](../firmware/century-bus-sniffer.yaml)
   through a normal USB connection.
2. Confirm that the YAML uses the Wi-Fi credentials stored in the Home
   Assistant ESPHome `secrets.yaml` file.
3. Confirm that the node comes online as `century-bus-sniffer`.
4. Unplug the normal USB cable before connecting pump auxiliary power.

Do not connect a normally powered USB cable while the buck converter is
feeding the ESP32 power input. Use Wi-Fi/API logging during the live capture.
A USB data cable with VBUS physically removed is an alternative only if it has
been verified with a meter.

## Make the RS-485 interface receive-only

With all power removed:

1. Disconnect the wire between ESP32 `GPIO2` and the RS-485 module's TTL-side
   `TXD` input.
2. Install the 10 kOhm resistor from the module's TTL-side `TXD` input to the
   ESP32 `3V3` rail. This holds the automatic-direction module in its idle,
   receive state. Do not connect this resistor to RS-485 A or B.
3. Keep the module's TTL-side `RXD` output connected to ESP32 `GPIO1`.
4. Keep module TTL `VIN` connected to ESP32 `3V3` and TTL `GND` connected to
   ESP32 `GND`.
5. Leave the isolated field-side `GND` pad unconnected.
6. If the module has a selectable 120-ohm termination resistor, disable it for
   this parallel sniffer tap. Do not add a third termination to the bus.

The resistor is a precaution against a floating TX input. A value from
4.7-22 kOhm is suitable; 10 kOhm is recommended.

## Parallel field wiring

Leave the original four-wire cable connected between the pump and the Hayward
display. Add the test rig as a parallel branch, not in series:

```text
Pump connector       Hayward display       Sniffer/test rig
--------------       ---------------       ----------------
A    -------------------- A --------------- RS-485 module A
B    -------------------- B --------------- RS-485 module B
AUX+ ----------------- +10/+12 V ----------- fuse -> buck IN+
COM  ------------------- COM --------------- buck IN-
```

The buck converter continues to power the test rig exactly as it did during
the controller test:

```text
buck OUT+ (5.00 V) -> ESP32 5V and LCD VCC
buck OUT-          -> ESP32 GND and LCD GND
ESP32 3V3          -> RS-485 module TTL VIN
ESP32 GND          -> RS-485 module TTL GND
```

Use the printed `A`, `B`, auxiliary-power, and common labels as the authority.
If A/B naming differs between the pump and module, initially connect
label-to-label and record the result before reversing only the sniffer A/B
pair.

## Capture procedure at the pump

1. Shut off and lock out pump mains power before opening a wiring compartment
   or changing any connection. Wait the manufacturer-specified discharge time.
2. Leave the known-working DIP-switch pattern unchanged: switches 1 and 2 ON,
   switches 3-5 OFF.
3. Connect the Hayward display normally and add the four test-rig branches
   shown above.
4. Verify buck polarity and confirm its output is 4.90-5.10 V before connecting
   the ESP32 load.
5. Restore power and open the `century-bus-sniffer` logs in ESPHome.
6. Capture at least 30 seconds with the pump idle.
7. From the Hayward display, start the pump and let it stabilize.
8. Select at least two additional speeds, waiting about 15 seconds at each.
   Record the selected speeds and approximate times so packets can be matched
   to actions.
9. Stop the pump and capture at least 15 more seconds.
10. Copy or download the complete log from initial power-up through stop. Keep
    every line beginning with `[PUMP BUS]`.

Expected log format:

```text
[D][uart_debug]: [PUMP BUS] <<< 15 41 10 ...
```

If no bus bytes appear, first verify that the panel still operates the pump,
the module is powered, module `RXD` reaches GPIO1, and the pull-up holds module
`TXD` near 3.3 V. With power removed, reverse only the sniffer module's A and B
connections and retry. Do not change the known-working panel wiring.

## Restore controller operation

With pump power removed, remove the temporary TXD pull-up resistor, reconnect
ESP32 GPIO2 to module TTL TXD, and reinstall the normal
[`example.yaml`](../example.yaml) controller configuration. Do not operate the
normal controller firmware while the Hayward display is connected unless the
captured protocol proves that multi-client operation is safe.

This sniffer is a diagnostic aid, not an electrical safety device. Follow the
pump manufacturer's lockout, discharge-time, and wiring requirements.
