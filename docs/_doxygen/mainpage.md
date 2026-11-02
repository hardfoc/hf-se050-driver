# SE050 Driver API reference {#mainpage}

The classes, functions and constants of the HardFOC SE050 driver, generated from the headers.

Start with se050::Device, the object your application uses, and se050::I2cTransceiveInterface, the interface your
platform port implements. The T=1 over I2C link is se050::T1Session; APDU and command builders are in the
se050::apdu and se050::cmd namespaces; secure messaging is se050::scp03::Session.

The guides, with examples that compile, are on the documentation site: <a href="../">SE050 Driver
documentation</a>.
