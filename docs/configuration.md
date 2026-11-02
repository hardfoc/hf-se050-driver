# Configuration

Every setting the driver has: T=1 timing, the I2C address and ENA in your port, the ESP32 reference port's
options, and the memory the driver uses.

## T=1 timing

```cpp
--8<-- "tests/docs/configuration.cpp:t1"
```

| Setter | Default | Raise it when |
| --- | --- | --- |
| `SetInterFrameDelayMs(ms)` | 2 ms | The first read after a write keeps failing; some boards need 3 ms |
| `SetReadRetries(n)` | 6 | The chip NACKs for longer than retries × delay (slow configurations, low bus speed) |
| `SetReadRetryDelayMs(ms)` | 2 ms | Same; together with retries it bounds how long a read waits |
| `SetMaxWtxRequests(n)` | 8 | Long operations (RSA, key generation) end in `Error::Protocol` after many WTX requests |

Each setter has a getter (`InterFrameDelayMs()`, `ReadRetries()`, `ReadRetryDelayMs()`, `MaxWtxRequests()`). The
settings live in the `T1Session` of each device object and apply to every later exchange.

## Address and ENA

The I2C address and the ENA pin belong to your port, not the driver. The port on the [Porting](porting.md) page
takes both in its constructor:

```cpp
--8<-- "tests/docs/configuration.cpp:address"
```

`se050::kDefaultI2cAddress7` is `0x48`, the SE050's default target address.

## ESP32 reference port

The examples' port, `hf_se050_examples::HfSe050EspIdfI2c`, takes a `Se050EspI2cConfig`:

| Field | Default | Meaning |
| --- | --- | --- |
| `port` | `I2C_NUM_0` | ESP-IDF I2C controller |
| `sda`, `scl` | `GPIO_NUM_47`, `GPIO_NUM_48` | Pins (ESP32-S3 example wiring) |
| `freq_hz` | `100000` | SCL frequency |
| `device_address7` | `0x48` | SE050 address |
| `reset_gpio` | `GPIO_NUM_NC` | GPIO wired to ENA, or not connected |

It uses the ESP-IDF v5 `i2c_master` driver with the internal pull-ups enabled and a glitch filter of 7 clock
cycles; see [`hf_se050_esp_i2c.hpp`](../examples/esp32/main/include/hf_se050_esp_i2c.hpp).

## Limits

| Constant | Value | Meaning |
| --- | --- | --- |
| `se050::kMaxApduCommandBytes` | 892 | Largest C-APDU the builders produce |
| `se050::kMaxApduResponseBytes` | 892 | Largest R-APDU buffer the commands use |
| `se050::t1::kMaxInformationField` | 254 | INF bytes per T=1 block |
| `se050::scp03::kWrapScratchBytes` | 320 | Minimum size of the buffer for `BindScp03WrapScratch()` |

## Buffers and stack

The driver allocates nothing. Commands put their working buffers on the stack of the calling task:

| Call | Largest stack buffers |
| --- | --- |
| `WriteBinary()` | 892-byte C-APDU, 64-byte response |
| `GetRandom()`, `ReadObject()`, `EcdsaSign()` | 892-byte response |
| Any exchange (`T1Session`) | Two 259-byte block buffers (send and receive) |

The deepest call needs about 1.5 kB for these buffers, plus call frames. Data you read goes into buffers you pass,
and the SCP03 wrap uses the buffer you bind with `BindScp03WrapScratch()`, which can live in static memory.

## Compile-time

There are no configuration macros. `se050_version.h`, generated at configure time, gives the version
([Installation](installation.md#how-the-build-is-organised)).
