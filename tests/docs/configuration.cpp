/**
 * @file configuration.cpp
 * @brief The Configuration page's snippets, compiled by the host build (never run).
 */
#include "my_i2c.hpp"
#include "se050_device.hpp"

void configure(se050::Device<MyI2c>& se) {
  // --8<-- [start:t1]
  auto& t1 = se.T1();
  t1.SetInterFrameDelayMs(3); // wait after each block write before the first read (default 2 ms)
  t1.SetReadRetries(10);      // re-read a NACKed or timed-out read this many times (default 6)
  t1.SetReadRetryDelayMs(2);  // between those reads (default 2 ms)
  t1.SetMaxWtxRequests(16);   // S(WTX) requests accepted in one exchange (default 8)
  // --8<-- [end:t1]
}

// --8<-- [start:address]
MyI2c i2c_default;                         // 0x48, ENA not wired
MyI2c i2c_on_board(0x48, /*ena_gpio=*/21); // ENA wired to GPIO 21
// --8<-- [end:address]
