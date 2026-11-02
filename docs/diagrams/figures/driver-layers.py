"""The three layers of the driver: hardware-agnostic core, communication interface, platform port.

Reads: From top to bottom. Your application talks to one se050::Device<Transport> object, whose public commands
are listed from inc/se050_device.hpp. Each command builds an APDU, which the T=1 session (inc/se050_t1_session.hpp)
frames into blocks with a CRC-16 EDC, chains and acknowledges; when an SCP03 session is open the APDU is wrapped
first. The communication interface (se050::I2cTransceiveInterface in inc/se050_i2c_transport_interface.hpp) is a
CRTP base. The platform port is the only code you write: the methods listed from that header's requirements.
Below it is the SE050 on I2C.
"""

import re
from pathlib import Path

from hardfoc_mkdocs_theme.figkit import Fig

ROOT = Path(__file__).resolve().parents[3]
DEVICE = "inc/se050_device.hpp"
T1 = "inc/se050_t1_session.hpp"
COMM = "inc/se050_i2c_transport_interface.hpp"

# Accessors and plumbing rather than chip commands.
_NOT_COMMANDS = {"EnsureInitialized", "T1", "Scp03", "SessionRef", "TransceiveRaw", "HardwareReset"}


def data():
    dev = (ROOT / DEVICE).read_text()
    names = re.findall(r"^\s+\[\[nodiscard\]\] Error (\w+)\(", dev, re.M)
    commands = [n for n in dict.fromkeys(names) if n not in _NOT_COMMANDS]
    t1 = (ROOT / T1).read_text()
    t1_calls = [n for n in ("ChipWarmReset", "GetAnswerToReset", "ExchangeInformation") if f"Error {n}(" in t1]
    comm = (ROOT / COMM).read_text()
    req = re.search(r"@par Derived requirements(.*?)\*/", comm, re.S).group(1)
    port = list(dict.fromkeys(re.findall(r"^\s*\* - `(?:\w+ )+(\w+)\(", req, re.M)))
    return {
        "commands": commands,
        "t1": t1_calls,
        "port_hooks": port,
        "sources": [f"{DEVICE}:Device commands", f"{T1}:T1Session", f"{COMM}:Derived requirements"],
    }


def build(d):
    f = Fig(
        "driver-layers",
        "Three layers, one of them yours",
        alt="Layer diagram: the application calls the hardware-agnostic SE050 Device and its commands, which go "
        "through the T=1 session and the optional SCP03 channel, then the CRTP I2C interface, which calls the "
        "platform port you write, which drives the I2C bus and the ENA pin of the SE050.",
        width=1200,
        kicker="Architecture",
        subtitle="The driver and the interface are the same on every MCU. The port is a small class with six methods.",
        profile="overview",
        legend=[("brand", "this library"), ("teal", "you write"), ("slate", "hardware")],
    )
    W = f.body_w
    app = f.card(0, 0, W, "Your application", ["provisioning, TLS identity, signing, secure storage ..."],
                 tone="slate")
    top = app.b + 40
    chips_h = f.chips(16, f.region_inner_top(top), W - 32, d["commands"], tone="brand")
    reg = f.region(0, top, W, f.region_inner_top(top) - top + chips_h + 18, "Hardware-agnostic core", tone="brand",
                   sub="se050::Device<Transport>, inc/se050_device.hpp")
    comm = f.card(0, reg.b + 40, W, "Communication interface",
                  ["T1Session: " + ", ".join(d["t1"]) + "; CRC-16 EDC, chaining, WTX, resync;",
                   "scp03::Session wraps APDUs when open; I2cTransceiveInterface<Self> is the CRTP base"],
                  tone="brand", accent=True)
    port = f.card(0, comm.b + 40, W, "Platform port (your class)", ["  ".join(h + "()" for h in d["port_hooks"])],
                  tone="teal", accent=True, mono=True)
    hw = f.card(0, port.b + 40, W, "SE050",
                ["I2C target, address 0x48, T=1 over I2C  /  ENA (deep power-down)"], tone="slate")
    for a, b in ((app, reg), (reg, comm), (comm, port), (port, hw)):
        f.connect(a, b, "bottom", "top", tone="rail")
    return f
