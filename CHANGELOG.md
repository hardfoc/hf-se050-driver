# Changelog

All notable changes to this driver are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Host unit tests (`-DHF_SE050_BUILD_TESTS=ON`) against a model of the SE050's T=1 over I2C target, and a CI
  workflow that runs them with GCC and Clang.

### Changed

- Documentation moved to the HardFOC MkDocs platform: new page set (quickstart, hardware, architecture, T=1 over
  I2C, security model, API guide, configuration, porting, testing), code snippets compiled by the host build from
  `tests/docs/`, Doxygen under `/api/`, versioned with mike. The Jekyll site files and the doxygen-awesome
  submodule are gone.
- Shorter, more precise example descriptions in `examples/esp32/app_config.yml`.

## 0.3.0 (version in the build settings; not tagged)

- CRTP I2C transport, T=1 over I2C session (CRC, chaining, WTX, resynchronisation), ATR parser, APDU and TLV
  helpers, IoT applet commands for management, binary objects, EC keys, ECDSA, AES and RFC 3394 unwrap.
- Platform SCP03: handshake, wrap and unwrap of short APDUs, Card Manager `SELECT` and `PUT KEY`, with no
  compiled-in keys.
- ESP-IDF port and seven ESP32 examples, from bring-up to an IoT lifecycle.
