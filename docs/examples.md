# Examples

Complete ESP32 applications that exercise the driver on a real SE050, from first bring-up to a full IoT device
lifecycle.

They run on an ESP32-S3 with the SE050 on I2C port 0 (SDA GPIO 47, SCL GPIO 48, 100 kHz); the port is
[`hf_se050_esp_i2c.hpp`](../examples/esp32/main/include/hf_se050_esp_i2c.hpp). CI builds every app below on each
pull request. Several examples write objects and generate keys: run them on a development part, not on a
provisioned product.

## The apps

<!-- generated: examples -->
| App | What it shows | Source | CI |
| --- | --- | --- | --- |
| `se050_minimal_example` :material-star:{ title="featured" } | Bring-up: power cycle, T=1 reset, ATR, SELECT of the IoT applet | [`se050_minimal_example.cpp`](../examples/esp32/main/se050_minimal_example.cpp) | yes |
| `se050_smoke_example` :material-star:{ title="featured" } | Health check: ATR fields, applet version, random bytes, free memory | [`se050_smoke_example.cpp`](../examples/esp32/main/se050_smoke_example.cpp) | yes |
| `se050_object_lifecycle_example` :material-star:{ title="featured" } | Write, read back and delete a binary object | [`se050_object_lifecycle_example.cpp`](../examples/esp32/main/se050_object_lifecycle_example.cpp) | yes |
| `se050_cloud_onboarding_example` :material-star:{ title="featured" } | Generate a P-256 key on chip, sign a challenge and verify the signature | [`se050_cloud_onboarding_example.cpp`](../examples/esp32/main/se050_cloud_onboarding_example.cpp) | yes |
| `se050_secure_board_comms_example` :material-star:{ title="featured" } | Signed messages between two boards, with a counter and nonce against replay | [`se050_secure_board_comms_example.cpp`](../examples/esp32/main/se050_secure_board_comms_example.cpp) | yes |
| `se050_cloud_registration_packet_example` :material-star:{ title="featured" } | Registration payload: reuse or create a key, export the public key, sign a challenge | [`se050_cloud_registration_packet_example.cpp`](../examples/esp32/main/se050_cloud_registration_packet_example.cpp) | yes |
| `se050_aws_iot_lifecycle` :material-star:{ title="featured" } | IoT lifecycle: provisioning, Wi-Fi, TLS identity, signed MQTT telemetry, OTA manifest check | [`se050_aws_iot_lifecycle/se050_aws_iot_lifecycle.cpp`](../examples/esp32/main/se050_aws_iot_lifecycle/se050_aws_iot_lifecycle.cpp) | yes |

Build and flash one (from `examples/esp32`, ESP-IDF environment loaded):

```bash
./scripts/build_app.sh se050_minimal_example Release
./scripts/flash_app.sh flash_monitor se050_minimal_example Release
```

Source of this table: [`examples/esp32/app_config.yml`](../examples/esp32/app_config.yml).
<!-- /generated -->

## Set up once

```bash
git submodule update --init examples/esp32/scripts   # the HardFOC ESP-IDF build scripts
cd examples/esp32
. $IDF_PATH/export.sh                                 # ESP-IDF v5.5
./scripts/build_app.sh list                           # every app and build type
```

The scripts read [`app_config.yml`](../examples/esp32/app_config.yml) (the same file this page's table comes
from) and build each app as its own firmware image. Set `ESPPORT` to flash, for example
`export ESPPORT=/dev/ttyACM0`.

## What to read in an example

| You want to | Read |
| --- | --- |
| Check a new board: I2C, T=1, ATR, applet | `se050_minimal_example.cpp` |
| Baseline a chip: version, random, free memory, ATR fields | `se050_smoke_example.cpp` |
| Write, read back and delete a binary object | `se050_object_lifecycle_example.cpp` |
| Generate an identity key and sign a challenge | `se050_cloud_onboarding_example.cpp` |
| Build a registration payload: idempotent key, public key, signed challenge | `se050_cloud_registration_packet_example.cpp` |
| Sign and verify messages between two boards with a counter and nonce | `se050_secure_board_comms_example.cpp` |
| Provisioning, Wi-Fi, TLS identity in mbedTLS, signed MQTT telemetry, OTA manifest check | `se050_aws_iot_lifecycle/` and its [README](../examples/esp32/main/se050_aws_iot_lifecycle/README.md) |

The examples' object identifiers (`0xF0...`) are placeholders for a development part; choose your own for a
product ([Security model](security.md#device-identity)). The
[example folder's README](../examples/esp32/README.md) has the wiring and the build commands.
