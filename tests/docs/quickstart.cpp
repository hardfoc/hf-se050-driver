/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_i2c.hpp"

// --8<-- [start:quickstart]
#include "se050_device.hpp"

int read_32_random_bytes(MyI2c& i2c, std::uint8_t (&random)[32]) {
  se050::Device<MyI2c> se(i2c);

  // 1. Power-cycle through ENA (a no-op when ENA is not wired), then bring the T=1 link up.
  (void)se.HardwareReset();
  if (se.T1().ChipWarmReset(100) != se050::Error::Ok) {
    return -1; // wiring, pull-ups, supply or address
  }

  // 2. Select the IoT applet: the call succeeds and the applet answers SW 90 00.
  std::uint8_t rsp[32]{};
  std::size_t rsp_len = 0;
  if (se.SelectDefaultIoTApplet(rsp, sizeof(rsp), &rsp_len, 300) != se050::Error::Ok ||
      rsp_len < 2 || rsp[rsp_len - 2] != 0x90 || rsp[rsp_len - 1] != 0x00) {
    return -2;
  }

  // 3. Use it: 32 bytes from the SE050's random number generator.
  std::size_t n = 0;
  if (se.GetRandom(32, random, sizeof(random), &n, 300) != se050::Error::Ok || n != 32) {
    return -3;
  }
  return 0;
}
// --8<-- [end:quickstart]
