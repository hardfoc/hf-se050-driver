# Testing and CI

How the driver is checked: host unit tests against a model of the SE050's T=1 target, compiled documentation,
ESP32 builds and static analysis, on every pull request.

## Host unit tests

The tests build the real driver against a model of the SE050 behind the I2C interface. The model checks the NAD
and CRC of every block the host writes, answers S-blocks (GET ATR, reset, WTX, resynchronisation), reassembles
chained commands and acknowledges them, splits long answers into chained blocks, and passes each complete C-APDU
to an applet stub the test provides. It can inject a corrupt CRC, WTX requests and failing reads, and records every
block, so tests check exactly what went over the bus.

```bash
cmake -S . -B build-host -DHF_SE050_BUILD_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_t1.cpp`](../tests/test_t1.cpp) | CRC-16 check value and byte order, S-block framing, N(S) alternation, chaining both ways, WTX limits, retransmission after EDC errors and NACKs, IFSC from the ATR, the erratum I2C.1 start-up read, retry errors |
| [`tests/test_apdu.cpp`](../tests/test_apdu.cpp) | `SELECT`, short and extended APDUs, R-APDU and TLV parsing, `GetRandom` / `GetVersion` / `EcdsaVerify` end to end, status-word errors, the SCP03 key gate, the RFC 3394 test vector |
| [`tests/test_scp03.cpp`](../tests/test_scp03.cpp) | SCP03 against the card model: round trips, rejected answers (tampered R-MAC, status word only, replay), wiping on `Close()` and destruction, erratum JOS.2, extended APDUs and Le |
| [`tests/mock_se050.hpp`](../tests/mock_se050.hpp) | The T=1 over I2C model, with EDC, NACK and I2C.1 fault injection |
| [`tests/mock_scp03_card.hpp`](../tests/mock_scp03_card.hpp) | The card side of SCP03 in front of an applet model |
| [`tests/install_check.cmake`](../tests/install_check.cmake) | CTest `hf_se050_install`: install, then build a `find_package()` consumer |
| [`tests/test_harness.hpp`](../tests/test_harness.hpp), [`tests/test_main.cpp`](../tests/test_main.cpp) | A dependency-free test registry and runner |

The tests use no real keys: the only key material is the public RFC 3394 test vector, which the SCP03 tests also
use as static keys and challenges.

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_i2c.hpp), which the host build
compiles (target `hf_se050_doc_snippets`, `-Wall -Wextra`) with `Device`, `Session`, `T1Session` and
`scp03::Session` explicitly instantiated over the documented port. A change to the API that breaks a page breaks
the build first. The README's example is a copy of the Quickstart snippet that `hardfoc-docs check` keeps in step.

## CI workflows

| Workflow | What it checks |
| --- | --- |
| Host unit tests | GCC and Clang build and run the tests and the doc snippets |
| ESP32 examples build | Every app in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and build type |
| C++ lint, C++ analysis | clang-format, clang-tidy and cppcheck |
| Docs | Strict site build, figure and generated-table drift, Doxygen, link check, Lighthouse budget |
| Markdown lint, YAML lint, link check | Repository text files |

The workflows call the shared, versioned workflows of
[hf-general-ci-tools](https://github.com/hardfoc/hf-general-ci-tools) and
[hf-espidf-ci-tools](https://github.com/hardfoc/hf-espidf-ci-tools).

## On hardware

The [ESP32 examples](examples.md) are the hardware tests: start with `se050_minimal_example` and
`se050_smoke_example` on a new board, and run them after any transport change. The lifecycle example's Python
tools have their own tests in `examples/esp32/main/se050_aws_iot_lifecycle/tools/tests/`.
