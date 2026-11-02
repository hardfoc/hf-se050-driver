# Changelog

All notable changes to this driver are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Security

- SCP03: a response inside an open channel is never accepted without a verified R-MAC. A response shorter than
  R-MAC + status word (for example a bare `90 00` injected on the bus) used to pass through unchecked, faking
  success for status-only commands; it now returns `Error::Authentication` and closes the channel
  (GPC Amendment D 6.2.5).
- SCP03: R-MACs and the card cryptogram are compared in constant time.
- SCP03: session keys, MAC chaining value and counter are wiped with a non-elidable `SecureZero()` on `Close()`,
  on every failure and in the destructor; AES round keys, CMAC subkeys and handshake intermediates are wiped too.
- SCP03: after a lost channel the device refuses commands with `Error::Authentication` until `Scp03().Close()`
  or a new channel, instead of silently sending plaintext.

### Fixed

- `apdu::BuildCase4Extended()` wrote a three-byte Le after an extended Lc; case 4E has a two-byte Le
  (ISO/IEC 7816-4 5.1).
- SCP03 wrap read every command as short case 3: it dropped Le from case 2 and 4 commands and sent extended
  commands without their data. All cases (1, 2, 3, 4, short and extended) are now wrapped, extended commands in
  extended form, with Le kept.
- SCP03 erratum JOS.2 (errata sheet 3.13): commands without data now carry an encrypted padding block, so the
  SE050's counter stays in step; `GetVersion()` works inside the channel.
- T=1: a block with a bad EDC is requested again and an R-block NACK from the SE050 makes the host resend its
  I-block (ISO/IEC 7816-3 11.6.3), up to `SetMaxRetransmissions()` times.
- T=1: command blocks are cut at the SE050's IFSC from the ATR (`GetAnswerToReset()`), not always at 254 bytes.
- CMake: install rules and an exported `hf::se050` target, so `find_package(hf_se050)` works; the package
  config no longer includes the uninstalled build-settings script.
- The ESP-IDF component manifest said 0.2.0 while CMake said 0.3.0; both say 0.3.0 and configuring fails if they
  differ.

### Added

- `T1Session::RecoverAfterHostReset()`: the start-up read for erratum I2C.1 (errata sheet 3.1.3), then
  S(RESYNCH).
- `T1Session::SetIfsc()` / `Ifsc()`, `SetMaxRetransmissions()` / `MaxRetransmissions()`,
  `t1::RBlockNackPcb()`.
- `scp03::ParseCommand()`, `scp03::WipeChannel()`, `scp03::kWrapScratchBytesExtended`,
  `scp03::Session::Failed()` and `Abort()`, `scp03::crypto::SecureZero()`, `ScopedWipe`, `ConstantTimeEqual()`.
- Host tests for SCP03 against a card model (tampered R-MAC, status word only, replay, wiping, JOS.2,
  extended APDUs), T=1 retransmission, IFSC and I2C.1, and an install test (`hf_se050_install`).

- Host unit tests (`-DHF_SE050_BUILD_TESTS=ON`) against a model of the SE050's T=1 over I2C target, and a CI
  workflow that runs them with GCC and Clang.

### Changed

- Documentation moved to the HardFOC MkDocs platform: new page set (quickstart, hardware, architecture, T=1 over
  I2C, security model, API guide, configuration, porting, testing), code snippets compiled by the host build from
  `tests/docs/`, Doxygen under `/api/`, versioned with mike. The Jekyll site files and the doxygen-awesome
  submodule are gone.
- Shorter, more precise example descriptions in `examples/esp32/app_config.yml`.

### Breaking changes

- New enumerator `Error::Authentication`. A `switch` over `se050::Error` without `default` needs a case for it.
- An R-MAC failure returns `Error::Authentication` instead of `Error::Protocol`, and so does any response inside
  the channel without an R-MAC, including error status words the SE050 sends unprotected. After it,
  `TransmitApdu()` and every command fail with `Error::Authentication` until `Scp03().Close()` or a new channel
  (previously the device fell back to plaintext). A T=1 error inside the channel now also closes it.
- Wrapped commands are longer: every command carries at least one encrypted block (JOS.2), and commands with an
  Le now send it. Commands over 255 bytes of data need a wrap buffer of `kWrapScratchBytesExtended` (they used to
  be sent malformed).
- `scp03::Session` is no longer copyable (session keys are never duplicated), so neither is `Device`.
- A block with a bad EDC no longer ends the exchange with `Error::Crc` at once; it is retried first.

## 0.3.0 (version in the build settings; not tagged)

- CRTP I2C transport, T=1 over I2C session (CRC, chaining, WTX, resynchronisation), ATR parser, APDU and TLV
  helpers, IoT applet commands for management, binary objects, EC keys, ECDSA, AES and RFC 3394 unwrap.
- Platform SCP03: handshake, wrap and unwrap of short APDUs, Card Manager `SELECT` and `PUT KEY`, with no
  compiled-in keys.
- ESP-IDF port and seven ESP32 examples, from bring-up to an IoT lifecycle.
