# API guide

How to do the common jobs with the driver: bring the chip up, read its state, store data, use keys, send your own
APDUs, open a secure channel and handle errors.

Every snippet on this page is compiled by the host CI from
[`tests/docs/api_tasks.cpp`](../tests/docs/api_tasks.cpp), with `Se` standing for `se050::Device<MyI2c>`. The
[API reference](api-reference.md) lists every member.

## The shape of the API

One `Device<Transport>` per chip. It owns a `T1Session` (the link) and an `scp03::Session` (the optional secure
channel), and offers the IoT applet's commands as member functions. Under it, free functions build and parse the
bytes, so you can send any command the device object does not cover.

| Part | Where | Use it for |
| --- | --- | --- |
| `Device` commands | `se050_device.hpp` | The applet commands below, with typed arguments and results |
| `T1()` | `T1Session`, `se050_t1_session.hpp` | Link bring-up (`ChipWarmReset()`, `GetAnswerToReset()`), timing settings, recovery |
| `Scp03()` | `scp03::Session`, `se050_scp03.hpp` | Opening and closing the platform SCP03 channel |
| `TransmitApdu()` | `Device` | Any serialised C-APDU; wrapped when the channel is open |
| `se050::apdu`, `se050::tlv`, `se050::cmd` | `se050_apdu.hpp`, `se050_tlv.hpp`, `se050_commands.hpp` | Building and parsing APDUs, TLVs and applet commands yourself |
| `se050::atr`, `se050::applet` | `se050_atr.hpp`, `se050_applet.hpp` | ATR profile parsing; `SELECT` builders and AIDs |

| `Device` command | Applet command (AN12413) | Returns |
| --- | --- | --- |
| `SelectDefaultIoTApplet()`, `SelectCardManager()` | `SELECT` | The raw R-APDU: check SW yourself |
| `GetVersion()` | `GetVersion` | `cmd::VersionInfo`: applet version, configuration, secure box |
| `GetRandom()` | `GetRandom` | Up to the bytes you ask for |
| `GetFreeMemory()` | `GetFreeMemory` | Free bytes of one memory type (saturates at 32767) |
| `WriteBinary()`, `ReadObject()`, `DeleteSecureObject()`, `CheckObjectExists()` | `WriteBinary`, `ReadObject`, `DeleteSecureObject`, `CheckObjectExists` | Status or data |
| `GenerateEcKeyPair()`, `ReadPublicEcKey()` | `WriteECKey` (generate), `ReadObject` | Status; the public key |
| `EcdsaSign()`, `EcdsaVerify()` | `ECDSASign`, `ECDSAVerify` | DER signature; `bool` result |
| `WriteAesKey()`, `CipherAesEcb()`, `UnwrapAes256Rfc3394()` | `WriteSymmKey`, `CipherOneShot` | Status; ciphertext or plaintext; an unwrapped 32-byte key |

Every call returns `se050::Error`. Every argument that receives data comes with its capacity and a length out.
Timeouts are in milliseconds and apply to each I2C transfer of the exchange.

## Bring the chip up

```cpp
--8<-- "tests/docs/api_tasks.cpp:bringup"
```

The status-word helper used here and below:

```cpp
--8<-- "tests/docs/api_tasks.cpp:sw"
```

`SELECT` must succeed once after every reset before applet commands work. [T=1 over I2C](t1-over-i2c.md#the-atr)
explains the profile fields.

## Read the chip's state

```cpp
--8<-- "tests/docs/api_tasks.cpp:info"
```

`GetFreeMemory()` takes `MemoryType::Persistent`, `TransientReset` or `TransientDeselect`; the applet reports at
most 32767 bytes.

## Store and read data

```cpp
--8<-- "tests/docs/api_tasks.cpp:objects"
```

`WriteBinary()` creates the object when you pass a file length (`has_file_length`, `file_length`), and writes at an
offset into an existing one (`has_offset`, `offset`). `ReadObject()` reads all of it, or a range. The identifiers
here are examples; [Security model](security.md#device-identity) says how to choose yours.

## Generate a key and sign

```cpp
--8<-- "tests/docs/api_tasks.cpp:keys"
```

The private key is created inside the SE050 and never leaves it. `EcdsaSign()` signs a digest you computed on the
host: pass 32 bytes for `EcdsaAlgo::Sha256`. The signature is ASN.1 DER (at most 72 bytes for P-256).
`GenerateEcKeyPair()` takes `EcCurve::NistP192` to `NistP521`; key generation takes longer than other commands,
so give it a longer timeout.

## Send your own APDU

```cpp
--8<-- "tests/docs/api_tasks.cpp:raw"
```

`TransmitApdu()` takes a serialised C-APDU and returns the R-APDU with `SW1 SW2` at the end; it wraps and unwraps
when an SCP03 channel is open. `apdu::BuildCaseShort()` serialises a command with up to 255 data bytes;
`apdu::BuildCase4Extended()` uses the short form when the data fits and extended length above that. Use
`BuildCaseShort()` only for commands that carry data: with zero data bytes it still writes an `Lc` byte.

## Open an SCP03 channel

```cpp
--8<-- "tests/docs/api_tasks.cpp:scp03"
```

`OpenSecureChannel()` returns `Error::NotSupported` when a key or the 8-byte host challenge is missing, and
`Error::Protocol` when the card cryptogram does not match (wrong keys). The [Security model](security.md#protect-the-bus-with-scp03)
page covers provisioning, the limits of the channel and an erratum that affects commands without data.

## Handle errors

| `se050::Error` | Meaning | What to do |
| --- | --- | --- |
| `Ok` | Success | |
| `NotInitialized` | The port's `EnsureInitialized()` returned `false` | Check bus set-up |
| `Transport`, `Timeout` | An I2C write failed, or a read failed after the retries | Wiring, address, power, ENA; [Troubleshooting](troubleshooting.md) |
| `Crc` | A block arrived with a wrong CRC | Noise, speed, a port that splits writes; recover |
| `Sequence` | The SE050's N(S) is not the expected one | Recover |
| `Protocol` | Bad framing, too many WTX requests, an R-MAC failure, or a typed command answered SW other than `90 00` | Recover; for a status word, send the command with `TransmitApdu()` to see it |
| `BufferTooSmall` | The answer does not fit your buffer, or the SCP03 scratch is missing | Pass a larger buffer |
| `InvalidArgument` | A null pointer or a length out of range | Fix the call |
| `NotSupported` | SCP03 without keys, or wrap without an open channel | Provision keys |

After any error below the applet level (`Transport`, `Timeout`, `Crc`, `Sequence`, `Protocol` from framing), the
T=1 state of host and chip may differ. Start over:

```cpp
--8<-- "tests/docs/api_tasks.cpp:errors"
```

If that fails too, power-cycle the SE050 with `HardwareReset()` (ENA) and bring it up again. A chip that stretches
the clock forever needs a power cycle: see erratum I2C.1 on the [Hardware](hardware.md#errata-that-affect-the-host)
page.
