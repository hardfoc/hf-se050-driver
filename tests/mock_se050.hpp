/**
 * @file mock_se050.hpp
 * @brief A model of the SE050's T=1 over I2C target behind se050::I2cTransceiveInterface.
 *
 * The model answers the blocks the driver writes the way UM11225 describes: S-blocks for GET ATR,
 * warm reset, WTX and resynchronisation; I-blocks with alternating N(S) and the M (more) bit for
 * chaining in both directions; R-blocks that acknowledge a chained block. A test sets
 * `apdu_handler` to the applet: it receives the reassembled C-APDU and returns the R-APDU (data +
 * SW1 SW2). Faults can be injected: a corrupt EDC, a number of WTX requests before the answer,
 * failing reads. Time is virtual (`delay_total_ms`).
 */
#pragma once

#include "se050_crc.hpp"
#include "se050_i2c_transport_interface.hpp"
#include "se050_types.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace se050_test {

using Bytes = std::vector<std::uint8_t>;

/** @brief One T=1 block with its EDC appended (LSB first, as on the wire). */
inline Bytes MakeBlock(std::uint8_t nad, std::uint8_t pcb, const Bytes& inf) {
  Bytes f{nad, pcb, static_cast<std::uint8_t>(inf.size())};
  f.insert(f.end(), inf.begin(), inf.end());
  f.resize(f.size() + 2U);
  se050::crc::AppendEdc(f.data(), 3U + inf.size());
  return f;
}

class MockSe050 : public se050::I2cTransceiveInterface<MockSe050> {
public:
  // ---- configuration a test sets
  // -------------------------------------------------------------------------
  bool init_ok{true};
  Bytes atr{0x00, 0xA0, 0x00, 0x00, 0x03, 0x96, 0x04, 0x03,
            0xE8, 0x00, 0xFE, 0x02, 0x0B, 0x03, 0xE8, 0x08};
  std::function<Bytes(const Bytes&)> apdu_handler{[](const Bytes&) { return Bytes{0x90, 0x00}; }};
  std::size_t card_ifs{254};    ///< INF bytes per response block
  int wtx_before_reply{0};      ///< S(WTX request) blocks sent before the answer
  bool corrupt_next_edc{false}; ///< flip one EDC bit in the next block the card sends
  int failing_reads{0};         ///< this many reads return Error::Timeout first

  // ---- what a test inspects
  // ------------------------------------------------------------------------------
  std::vector<Bytes> written; ///< every block the host wrote, in order
  std::vector<Bytes> capdus;  ///< every reassembled C-APDU
  int resets{0};
  std::uint32_t delay_total_ms{0};

  // ---- the interface the driver calls
  // --------------------------------------------------------------------
  bool EnsureInitialized() noexcept {
    return init_ok;
  }

  se050::Error I2cWrite(const std::uint8_t* tx, std::size_t tx_len,
                        std::uint32_t /*timeout_ms*/) noexcept {
    if (tx == nullptr || tx_len == 0U) {
      return se050::Error::InvalidArgument; // address-only writes are forbidden on the SE050
    }
    Bytes block(tx, tx + tx_len);
    written.push_back(block);
    OnHostBlock(block);
    return se050::Error::Ok;
  }

  se050::Error I2cRead(std::uint8_t* rx, std::size_t rx_len,
                       std::uint32_t /*timeout_ms*/) noexcept {
    if (failing_reads > 0) {
      --failing_reads;
      return se050::Error::Timeout;
    }
    if (rx_out_.size() < rx_len) {
      return se050::Error::Transport; // the target NACKs: nothing (or not enough) to read
    }
    for (std::size_t i = 0; i < rx_len; ++i) {
      rx[i] = rx_out_.front();
      rx_out_.pop_front();
    }
    return se050::Error::Ok;
  }

  se050::Error Transceive(const std::uint8_t* tx, std::size_t tx_len, std::uint8_t* rx,
                          std::size_t rx_cap, std::size_t* rx_len_out,
                          std::uint32_t timeout_ms) noexcept {
    const se050::Error w = I2cWrite(tx, tx_len, timeout_ms);
    if (w != se050::Error::Ok) {
      return w;
    }
    const std::size_t n = rx_out_.size() < rx_cap ? rx_out_.size() : rx_cap;
    *rx_len_out = n;
    return n > 0U ? I2cRead(rx, n, timeout_ms) : se050::Error::Ok;
  }

  se050::Error HardwareReset() noexcept {
    ++resets;
    card_ns_ = 0;
    rx_out_.clear();
    pending_.clear();
    return se050::Error::Ok;
  }

  void delay_ms_impl(std::uint32_t ms) noexcept {
    delay_total_ms += ms;
  }

private:
  static constexpr std::uint8_t kNadIn = 0x5A;
  static constexpr std::uint8_t kNadOut = 0xA5;

  void Send(const Bytes& block) {
    Bytes b = block;
    if (corrupt_next_edc) {
      b.back() ^= 0x01U;
      corrupt_next_edc = false;
    }
    rx_out_.insert(rx_out_.end(), b.begin(), b.end());
  }

  void OnHostBlock(const Bytes& b) {
    if (b.size() < 5U || b[0] != kNadIn || !se050::crc::VerifyFrameCrc(b.data(), b.size())) {
      Send(MakeBlock(kNadOut, 0x81, {})); // R-block, EDC error
      return;
    }
    const std::uint8_t pcb = b[1];
    const Bytes inf(b.begin() + 3, b.end() - 2);
    if ((pcb & 0x80U) == 0U) { // I-block
      cmd_.insert(cmd_.end(), inf.begin(), inf.end());
      const std::uint8_t ns = static_cast<std::uint8_t>((pcb >> 6) & 1U);
      if ((pcb & 0x20U) != 0U) { // more to come: acknowledge with N(R) = next expected N(S)
        Send(MakeBlock(kNadOut, static_cast<std::uint8_t>(0x80U | ((ns ^ 1U) << 4)), {}));
        return;
      }
      capdus.push_back(cmd_);
      QueueAnswer(apdu_handler(cmd_));
      cmd_.clear();
      return;
    }
    if ((pcb & 0xC0U) == 0x80U) { // R-block from the host: it acknowledged a chained answer block
      ReleaseNext();
      return;
    }
    switch (pcb) { // S-blocks
    case 0xC7:
      Send(MakeBlock(kNadOut, 0xE7, atr));
      break;
    case 0xC6:
      card_ns_ = 0;
      Send(MakeBlock(kNadOut, 0xE6, {}));
      break;
    case 0xE3: // WTX response from the host
      if (wtx_left_ > 0) {
        --wtx_left_;
        Send(MakeBlock(kNadOut, 0xC3, {0x01}));
      } else {
        ReleaseNext();
      }
      break;
    default:
      break;
    }
  }

  void QueueAnswer(const Bytes& rapdu) {
    pending_.clear();
    std::size_t off = 0;
    do {
      const std::size_t n = (rapdu.size() - off) < card_ifs ? (rapdu.size() - off) : card_ifs;
      const bool more = off + n < rapdu.size();
      const auto pcb = static_cast<std::uint8_t>((card_ns_ << 6) | (more ? 0x20U : 0U));
      pending_.push_back(
          MakeBlock(kNadOut, pcb, Bytes(rapdu.begin() + off, rapdu.begin() + off + n)));
      card_ns_ ^= 1U;
      off += n;
    } while (off < rapdu.size());
    wtx_left_ = wtx_before_reply;
    if (wtx_left_ > 0) {
      --wtx_left_;
      Send(MakeBlock(kNadOut, 0xC3, {0x01}));
    } else {
      ReleaseNext();
    }
  }

  void ReleaseNext() {
    if (!pending_.empty()) {
      Send(pending_.front());
      pending_.pop_front();
    }
  }

  std::deque<std::uint8_t> rx_out_;
  std::deque<Bytes> pending_;
  Bytes cmd_;
  std::uint8_t card_ns_{0};
  int wtx_left_{0};
};

} // namespace se050_test
