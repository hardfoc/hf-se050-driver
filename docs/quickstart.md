# Quickstart

Talk to an SE050 from your MCU: wire four signals, write a small port class, read 32 random bytes from the chip.

## 1. Wire it

| MCU | SE050 (HX2QFN20) | Notes |
| --- | --- | --- |
| I2C SDA | 9 (I2C_SDA) | External pull-up to VIN, sized for your bus speed |
| I2C SCL | 10 (I2C_SCL) | Pull-up to VIN |
| GPIO out (optional) | 11 (ENA) | Low = deep power-down; the port's `HardwareReset()` pulses it. Tie to VCC if unused |
| 3.3 V or 1.8 V | 12 (VIN), 18 (VCC) | 1.62 V to 3.6 V. For deep power-down, VCC is fed from VOUT (15) instead |
| GND | 19 (VSS) | Common ground |

The bus must be idle high when the SE050 powers up: it picks I2C only if SDA and SCL are both high at boot.
[Hardware](hardware.md) has the full pin list, bus speeds and power modes.

## 2. Add the driver

Add the repository as a submodule and link the `hf::se050` CMake target (or include `inc/` and build with
C++17). [Installation](installation.md) covers CMake and ESP-IDF.

```bash
git submodule add https://github.com/hardfoc/hf-se050-driver.git external/hf-se050-driver
```

## 3. Write the port

The port is the only platform code. It writes one block, reads a given number of bytes, waits and, if wired,
pulses ENA. Replace the four `platform_*` calls with your SDK's.

```cpp title="my_i2c.hpp"
--8<-- "tests/docs/my_i2c.hpp:port"
```

[Porting](porting.md) explains each method and the ESP-IDF reference port.

## 4. Read random bytes

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

What each step does:

1. `HardwareReset()` calls your port; with ENA wired it puts the chip through deep power-down and back.
   `ChipWarmReset()` sends the T=1 chip-reset S-block and resets the block sequence numbers on both sides.
2. `SelectDefaultIoTApplet()` sends `SELECT` with the IoT applet's AID. It returns the raw response; the status
   words at its end must be `90 00`.
3. `GetRandom()` asks the applet for 32 bytes from the on-chip random number generator.

This example is built by the host CI, so it compiles against the current headers.

## If it does not work

| Symptom | First thing to check |
| --- | --- |
| `ChipWarmReset()` returns `Transport` or `Timeout` | Address 0x48, pull-ups, VIN, ENA high, bus idle high at power-up |
| `ChipWarmReset()` returns `Crc` | Bus speed (start at 100 kHz), long wires, a port that splits one block into several writes |
| SELECT answers a status other than `90 00` | The applet in your configuration; see [Troubleshooting](troubleshooting.md) |
| Works once, then `Sequence` or `Protocol` | A previous exchange failed half-way: call `ResetSequenceState()` and `ChipWarmReset()` |

## Next

- [API guide](api-guide.md): objects, key generation, signatures, raw APDUs, errors.
- [Security model](security.md): what to provision, SCP03, and what the driver does not protect.
- [Examples](examples.md): ESP32 applications from bring-up to a full IoT lifecycle.
