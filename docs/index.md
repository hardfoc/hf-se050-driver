---
hide:
  - navigation
  - toc
---

<div class="hf-hero" markdown>
<span class="hf-kicker">HardFOC driver</span>

# SE050 Driver

<p>A C++17 driver for the NXP EdgeLock SE050 secure element. It brings up the T=1 over I2C link, selects the IoT
applet and sends its commands: random numbers, secure objects, on-chip EC key generation, ECDSA signing and
verification, AES, and an optional GlobalPlatform SCP03 channel, on any MCU with I2C.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/hardfoc/hf-se050-driver){ .md-button }
</div>

The driver has no platform code. You give it a small class that writes and reads bytes on I2C and, if your board
wires it, drives the SE050's ENA pin; it does the rest: T=1 framing and CRC, chaining, waiting-time extensions,
APDU and TLV encoding, and the SCP03 wrap. It allocates nothing and contains no keys.

<div class="hf-chip" markdown>
<div class="hf-chip__head">
<span class="hf-chip__kicker">Plug &amp; Trust secure element</span>
<span class="hf-chip__name">SE050</span>
<span class="hf-chip__vendor">NXP Semiconductors (EdgeLock)</span>
</div>

Function
:   Key storage and crypto: ECC (NIST P-192 to P-521, Brainpool, Curve25519), RSA, AES, SHA-1/2, HMAC, TRNG

Host interface
:   I2C target, address 0x48, T=1 over I2C (UM11225); up to 1 MHz, 3.4 MHz with clock stretching

Supply
:   VIN, VCC 1.62 V to 3.6 V (1.8 V typical); 6 V absolute maximum

Memory and security
:   Up to 50 kB user flash; CC EAL 6+; SCP03 bus encryption

Package and range
:   HX2QFN20, 3 mm × 3 mm; −25 °C to 85 °C or −40 °C to 105 °C by part number

Driver uses
:   The IoT applet over plaintext T=1, or over a platform SCP03 channel with keys you provision

[Datasheet (NXP)](https://www.nxp.com/docs/en/data-sheet/SE050-DATASHEET.pdf) ·
[APDU specification AN12413](https://www.nxp.com/docs/en/application-note/AN12413.pdf) ·
[Errata sheet](https://www.nxp.com/docs/en/errata/SE050_Erratasheet.pdf)
</div>

## What you get

- **One object per chip.** `se050::Device<YourI2c>` exposes the applet's commands: `GetRandom`, `WriteBinary`,
  `ReadObject`, `GenerateEcKeyPair`, `EcdsaSign`, `EcdsaVerify`, `CipherAesEcb` and more. See the
  [API guide](api-guide.md).
- **The link layer handled.** `T1Session` frames blocks, checks the CRC-16, chains in both directions and answers
  WTX and resynchronisation requests. See [T=1 over I2C](t1-over-i2c.md).
- **Keys stay in the chip.** Key pairs are generated inside the SE050; the host only ever sees public keys and
  signatures. See [Security model](security.md).
- **SCP03 without default keys.** The secure channel opens only with keys you supply at run time; the driver
  ships no NXP default keys. See [Security model](security.md#protect-the-bus-with-scp03).
- **Zero-overhead portability.** The port is a CRTP class: no virtual calls, no heap, no RTOS dependency.
  See [Porting](porting.md).
- **Tested without hardware.** Host unit tests run the driver against a model of the SE050's T=1 target. See
  [Testing and CI](testing.md).

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire four signals, write the port, read random bytes from the chip.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Pins, supplies, I2C speed and address, power-down and reset.

-   :material-shield-key-outline: **[Security model](security.md)**

    ---

    What the driver protects, key provisioning, SCP03, deployment flows.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Bring-up, objects, keys and signatures, raw APDUs and errors, task by task.

-   :material-swap-horizontal: **[Porting](porting.md)**

    ---

    Implement the I2C interface for your MCU.

-   :material-book-open-variant: **[API reference](api-reference.md)**

    ---

    Every class and function, generated from the headers.

</div>
