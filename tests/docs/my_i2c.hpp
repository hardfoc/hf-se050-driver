/**
 * @file my_i2c.hpp
 * @brief The I2C port shown on the documentation's Porting page, compiled by the host build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a real port replaces
 * them with its SDK (ESP-IDF, STM32 HAL, Zephyr, Linux i2c-dev). The class is what the driver
 * needs, nothing more.
 */
#pragma once

#include "se050_i2c_transport_interface.hpp"
#include "se050_types.hpp"

#include <cstddef>
#include <cstdint>

// Platform hooks: your SDK's calls. Declared only, so the docs build checks the port compiles.
bool platform_i2c_write(std::uint8_t addr7, const std::uint8_t* data, std::size_t len,
                        std::uint32_t timeout_ms);
bool platform_i2c_read(std::uint8_t addr7, std::uint8_t* data, std::size_t len,
                       std::uint32_t timeout_ms);
void platform_gpio_write(int gpio, bool level);
void platform_delay_ms(std::uint32_t ms);

// --8<-- [start:port]
class MyI2c : public se050::I2cTransceiveInterface<MyI2c> {
public:
  explicit MyI2c(std::uint8_t addr7 = se050::kDefaultI2cAddress7, int ena_gpio = -1) noexcept
      : addr7_(addr7), ena_gpio_(ena_gpio) {}

  // Bus and pins are set up by your board code; report whether they are ready.
  bool EnsureInitialized() noexcept {
    return true;
  }

  // One complete T=1 block in one I2C write (START, address + W, bytes, STOP). Never zero-length.
  se050::Error I2cWrite(const std::uint8_t* tx, std::size_t tx_len,
                        std::uint32_t timeout_ms) noexcept {
    if (tx == nullptr || tx_len == 0U) {
      return se050::Error::InvalidArgument;
    }
    return platform_i2c_write(addr7_, tx, tx_len, timeout_ms) ? se050::Error::Ok
                                                              : se050::Error::Transport;
  }

  // Exactly rx_len bytes in one I2C read. A NACK (the SE050 is still busy) is Transport: the driver
  // retries.
  se050::Error I2cRead(std::uint8_t* rx, std::size_t rx_len, std::uint32_t timeout_ms) noexcept {
    if (rx == nullptr || rx_len == 0U) {
      return se050::Error::InvalidArgument;
    }
    return platform_i2c_read(addr7_, rx, rx_len, timeout_ms) ? se050::Error::Ok
                                                             : se050::Error::Transport;
  }

  // Bring-up helper only (Device::TransceiveRaw); T=1 uses I2cWrite / I2cRead.
  se050::Error Transceive(const std::uint8_t* tx, std::size_t tx_len, std::uint8_t* rx,
                          std::size_t rx_cap, std::size_t* rx_len_out,
                          std::uint32_t timeout_ms) noexcept {
    *rx_len_out = 0;
    se050::Error e = I2cWrite(tx, tx_len, timeout_ms);
    if (e != se050::Error::Ok || rx == nullptr || rx_cap == 0U) {
      return e;
    }
    delay_ms_impl(2);
    e = I2cRead(rx, rx_cap, timeout_ms);
    if (e == se050::Error::Ok) {
      *rx_len_out = rx_cap;
    }
    return e;
  }

  // Power-cycle through ENA (deep power-down while low). Return Ok when ENA is not wired.
  se050::Error HardwareReset() noexcept {
    if (ena_gpio_ < 0) {
      return se050::Error::Ok;
    }
    platform_gpio_write(ena_gpio_, false);
    delay_ms_impl(2);
    platform_gpio_write(ena_gpio_, true);
    delay_ms_impl(10);
    return se050::Error::Ok;
  }

  void delay_ms_impl(std::uint32_t ms) noexcept {
    platform_delay_ms(ms);
  }

private:
  std::uint8_t addr7_;
  int ena_gpio_;
};
// --8<-- [end:port]
