# Installation

Add the driver to a CMake or ESP-IDF project. It is a header-only C++17 library with no dependencies.

## Requirements

- A C++17 compiler. CI builds with the GCC and Clang of Ubuntu 24.04; the examples with ESP-IDF v5.5.
- No RTOS, heap or exceptions: every call is `noexcept` and returns `se050::Error` (or `bool`).
- About 1.5 kB of stack for the deepest call: an 892-byte APDU buffer and two 259-byte T=1 block buffers live on
  the stack while a command runs ([Configuration](configuration.md#buffers-and-stack)).

## Get the source

A git submodule keeps the driver pinned to a version you chose:

```bash
git submodule add https://github.com/hardfoc/hf-se050-driver.git external/hf-se050-driver
git submodule update --init external/hf-se050-driver
```

Only `inc/`, `cmake/` and `CMakeLists.txt` are needed to build. The examples have their own submodule
(`examples/esp32/scripts`); you do not need it to use the driver.

## CMake

```cmake
add_subdirectory(external/hf-se050-driver)
target_link_libraries(my_app PRIVATE hf::se050)
```

`hf::se050` (alias of `hf_se050`) is an `INTERFACE` target: it carries the include directories, including the
generated version header, and requires C++17 (`cxx_std_17`).

| Option | Default | Effect |
| --- | --- | --- |
| `HF_SE050_ENABLE_WARNINGS` | `OFF` | `-Wall -Wextra -Wpedantic` on targets that link `hf::se050` |
| `HF_SE050_BUILD_TESTS` | `OFF` | Builds the host unit tests in `tests/` ([Testing and CI](testing.md)) |

To use an installed copy instead, install the driver and find it as a package:

```bash
cmake -S hf-se050-driver -B build && cmake --install build --prefix /opt/hf
```

```cmake
find_package(hf_se050 0.3 REQUIRED CONFIG)   # CMAKE_PREFIX_PATH=/opt/hf
target_link_libraries(app PRIVATE hf::se050)
```

The headers and `se050_version.h` install under `include/hf_se050`, the package files under
`lib/cmake/hf_se050`. The CMake version, `HF_SE050_VERSION` and the ESP-IDF component manifest carry the same
version; configuring fails if they differ.

## ESP-IDF

The ESP-IDF component wrapper is [`examples/esp32/components/hf_se050/`](../examples/esp32/components/hf_se050/CMakeLists.txt).
Copy that folder into your project's `components/` (or point `EXTRA_COMPONENT_DIRS` at it) and set the
`HF_SE050_ROOT` cache variable to the driver checkout; the wrapper stops with a clear message when the path is
wrong. Your component then declares `REQUIRES hf_se050`. The wrapper itself requires `driver` and `freertos`, for
the ESP-IDF port in the examples.

## How the build is organised

| Layer | File | Role |
| --- | --- | --- |
| Settings | [`cmake/hf_se050_build_settings.cmake`](../cmake/hf_se050_build_settings.cmake) | Include directories, version, version header: the single source of truth |
| Library | [`CMakeLists.txt`](../CMakeLists.txt) | `INTERFACE` target for host builds and CI |
| ESP-IDF | [`examples/esp32/components/hf_se050/CMakeLists.txt`](../examples/esp32/components/hf_se050/CMakeLists.txt) | IDF component wrapper over the same settings |

The version is set once in the settings file and written into `se050_version.h` in the build tree at configure
time:

| Macro | Value |
| --- | --- |
| `HF_SE050_VERSION_MAJOR`, `_MINOR`, `_PATCH` | `0`, `3`, `0` |
| `HF_SE050_VERSION_STRING` | `"0.3.0"` |

## Headers

Include `se050_device.hpp` for the device object, or `se050_driver.hpp` for everything (T=1, APDU, TLV, ATR,
commands, SCP03, RFC 3394). The [API reference](api-reference.md) lists every header.

## Next

[Quickstart](quickstart.md) wires the chip and reads random bytes; [Porting](porting.md) writes the I2C interface
for your MCU.
