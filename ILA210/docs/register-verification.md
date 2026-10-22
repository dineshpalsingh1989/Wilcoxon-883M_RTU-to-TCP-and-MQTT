# ILA210 register verification

Checked against the supplied **ILA210 manual 97049 Rev.A1, 09/26**, pages
13-19, 23 and 30. Addresses below are hexadecimal, zero-based protocol
addresses. This is a document comparison; physical hardware remains untested.

## Static measurements: FC04 input registers

All fourteen addresses and numeric scale factors used by the gateway match
the manual. All fourteen are **unsigned 16-bit**, including the two temperature
entries as printed in this revision. Negative temperature encoding requires
manufacturer clarification; do not infer two's-complement encoding.

| Gateway index | Measurement | Sensor address | Multiply raw word by | Unit |
| --- | --- | --- | --- | --- |
| 0 | Acceleration RMS, 2 Hz–1 kHz | `0x0000` | 0.01 | g |
| 1 | Acceleration peak, 2 Hz–1 kHz | `0x0004` | 0.01 | g |
| 2 | Acceleration RMS, 10 Hz–10 kHz | `0x0010` | 0.01 | g |
| 3 | Acceleration peak, 10 Hz–10 kHz | `0x0014` | 0.01 | g |
| 4 | Velocity RMS, 10 Hz–1 kHz | `0x0020` | 0.1 | mm/s |
| 5 | Velocity peak, 10 Hz–1 kHz | `0x0024` | 0.1 | mm/s |
| 6 | Displacement RMS, 2 Hz–1 kHz | `0x0030` | 1 | µm |
| 7 | Displacement peak, 2 Hz–1 kHz | `0x0034` | 1 | µm |
| 8 | True peak | `0x0040` | 0.01 | g |
| 9 | Crest factor | `0x0044` | 0.01 | — |
| 10 | Standard deviation | `0x0048` | 0.01 | not specified as g |
| 11 | Temperature | `0x0050` | 0.1 | °C |
| 12 | Drive voltage | `0x0060` | 0.01 | V |
| 13 | Board temperature | `0x007C` | 0.1 | °C |

The previous signed conversion made raw `0x8000` velocity appear as
−3276.8 mm/s instead of 3276.8 mm/s. Both ESP32-S3 and IRIV now preserve
unsigned static measurements. The unsupported g unit on standard deviation
was removed from register metadata.

## Capture sequence and data

- FC06 writes `0xFFFF` to holding register `0x0200` to request capture.
- FC03 polls holding register `0x0200`; `0x0000` means ready. The manual also
  lists `0xEEEE` for initial empty data; it must not be accepted as ready.
- FC04 spectrum: `0x0600`–`0x1F00`, 6,401 **unsigned** words, 0.001 g/word,
  frequency spacing 1.5625 Hz, covering 0–10,000 Hz.
- FC04 waveform: `0x1F01`–`0x8300`, 25,600 **signed** words, 0.001 g/word,
  one second at 25,600 samples/s.

These addresses, lengths and function codes already matched. Spectrum buffers,
analysis and HTTP output now retain unsigned words; waveform buffers stay signed.
For example, spectrum `0xFFFF` represents 65.535 g, not 0.001 g.
Requests remain capped at 64 registers, within the sensor's 125-register maximum.

## Integration limits

The gateway's TCP holding addresses **0–13 are its own cache**, not the sensor's
RTU input addresses. Existing signed, scaled TCP encoding is preserved, including
clamping above 32767 raw counts. MQTT/dashboard readings use corrected full-range
values; TCP clients must retain the documented legacy interpretation. A future
full-range TCP map requires an explicit versioned contract change.

Manual factory serial settings are **9600 baud, 8N1, slave ID 1** (page 14).
The gateway's existing default is 115200 baud. Select 9600 in RS485 settings for
an unmodified factory sensor; saved settings and defaults were not changed.
The firmware exposes a subset of the manual's measurements and baud rates.
Device identification FC43/14 is not implemented by this change.

Validate live static readings, full capture endpoints, large unsigned values,
temperature behavior and alarm timing on each board before deployment.

## Software checks

After these corrections, both PlatformIO targets (`esp32s3` and
`cytron_iriv_io_controller`) build successfully. All three native Modbus suites
pass, including unsigned `0xFFFF` conversions for every static metric and capture
endpoint checks. Simulated desktop/mobile browser checks pass for both boards,
and the four repository Python tests pass. These are software checks only.
