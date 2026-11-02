# Troubleshooting

Symptoms you may see while bringing up an SE050, their causes and the fix for each, cheapest check first.

Start with the `se050::Error` of the first call that fails. `Transport` and `Timeout` point at the bus, `Crc`,
`Sequence` and framing `Protocol` at the link, and a status word other than `90 00` at the applet or its
configuration.

## Nothing answers

| Symptom | Cause and fix |
| --- | --- |
| Every call returns `Transport` | Wrong address, no pull-ups, no supply, ENA low. Fix: address 0x48, external pull-ups to VIN, VIN and VCC 1.62 V to 3.6 V, ENA high (or tied to VCC) |
| `NotInitialized` | The port's `EnsureInitialized()` returns `false`. Fix: the I2C controller or GPIO set-up in your port |
| Worked, then the bus is stuck low on SCL | The SE050 is stretching the clock forever: a command was sent before the previous response was read (erratum I2C.1), or an address-only frame reached it (I2C.2). Fix: power-cycle the SE050 (ENA or supply). Remove bus scans; give the chip one owner; power-cycle it after an MCU reset |
| Answers only after a power cycle of the whole board | The bus was low while the SE050 booted, so it did not select I2C (erratum I2C.3). Fix: keep SDA and SCL high from power-up; power the MCU's I/O first |
| `HardwareReset()` has no effect | ENA not wired, or VCC tied to the supply instead of VOUT. Fix: deep power-down needs VIN supply with VCC fed from VOUT ([Hardware](hardware.md#reset-and-power-modes)) |

## The link fails

| Symptom | Cause and fix |
| --- | --- |
| `Crc` on most exchanges | Noise, a bus too fast for its pull-ups, a port that splits a block into several writes. Fix: drop to 100 kHz, stronger pull-ups, one I2C write per block |
| `Timeout` or `Transport` after long commands | The read gave up while the chip was still busy. Fix: raise `SetReadRetries()` or `SetReadRetryDelayMs()` ([Configuration](configuration.md#t1-timing)) |
| `Protocol` after key generation or RSA | More WTX requests than `SetMaxWtxRequests()` allows. Fix: raise it |
| `Sequence`, or `Protocol` on the next command after an error | Host and chip disagree on N(S). Fix: `ResetSequenceState()` then `ChipWarmReset()` ([API guide](api-guide.md#handle-errors)) |
| Clock stretching above 600 kHz breaks the MCU's controller | Configurations A, B, C, D, F stretch by default above 600 kHz. Fix: stay below 600 kHz, or use a configuration with stretching off (up to 1 MHz) |

## The applet says no

| Status word | Meaning and likely cause |
| --- | --- |
| `6A 82` on SELECT | File or application not found: Wrong AID for your configuration; check AN12436 |
| `69 85` | Conditions of use not satisfied: The object exists with another type (an AES key written as a binary file), or a policy forbids it |
| `69 82` | Security status not satisfied: The command needs an authenticated session or SCP03 on this configuration |
| `6A 80` | Wrong data: A malformed TLV; check the identifier and lengths |
| `6D 00` | Instruction not supported: The command is not in your applet version (`GetVersion()`) |

Typed commands turn any of these into `Error::Protocol`. To see the status word, send the same command with
`TransmitApdu()` and read the last two bytes.

## SCP03

| Symptom | Cause and fix |
| --- | --- |
| `OpenSecureChannel()` returns `NotSupported` | A key or the host challenge is missing or the wrong length. Fix: 16-byte ENC and MAC keys, 8-byte challenge |
| `OpenSecureChannel()` returns `Protocol` | The card cryptogram does not match: the keys are wrong, or the key version. Fix: keys and key version (`0x0B`) of your configuration or your provisioning |
| `TransmitApdu()` returns `BufferTooSmall` while the channel is open | No wrap buffer bound. Fix: `BindScp03WrapScratch()` with at least 320 bytes |
| The command after `GetVersion()` fails inside the channel | Erratum JOS.2: no-data commands do not advance the SE050's counter. Fix: call `GetVersion()` before opening the channel |
| The channel closes by itself | An R-MAC check failed. Fix: treat as tampering or a wiring fault; open a new channel |

## Build

| Symptom | Cause and fix |
| --- | --- |
| `APP_TYPE not defined` in an example build | CMake run directly. Fix: build through `examples/esp32/scripts/build_app.sh`, or pass `-DAPP_TYPE=...` |
| `scripts/` missing | Submodule not initialised. Fix: `git submodule update --init examples/esp32/scripts` |
| `se050_version.h` not found | Including the driver without its CMake target. Fix: link `hf::se050`, or use the ESP-IDF component |
