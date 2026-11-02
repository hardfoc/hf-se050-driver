/**
 * @file mock_scp03_card.hpp
 * @brief Card side of GlobalPlatform SCP03 (Amendment D) for the host tests, in front of an applet model.
 *
 * Plug `Handle()` into `MockSe050::apdu_handler`. The model answers INITIALIZE UPDATE and EXTERNAL
 * AUTHENTICATE, then checks the C-MAC of every command, decrypts its data, passes the plain C-APDU to
 * `applet` and protects the answer with R-ENC and R-MAC, the way an SE050 platform SCP03 session does.
 *
 * Faults a test can inject on the next protected answer: drop the R-MAC (status word only), flip a bit of
 * the R-MAC, or replay an earlier answer. `jos2_bug` makes the model behave like an SE050 with erratum
 * JOS.2: a command without C-DATA does not advance the encryption counter.
 *
 * Keys: the static keys are the RFC 3394 section 4.1 KEK (00 01 .. 0F), a public test vector. No real key.
 */
#pragma once

#include "mock_se050.hpp"
#include "se050_scp03.hpp"
#include "se050_scp03_crypto.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

namespace se050_test {

/** @brief Public RFC 3394 section 4.1 KEK, used as both static SCP03 keys in the tests. */
inline constexpr std::uint8_t kTestStaticKey[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
/** @brief First half of the RFC 3394 section 4.1 key data, used as the host challenge. */
inline constexpr std::uint8_t kTestHostChallenge[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
/** @brief Second half of the same key data, used as the card challenge. */
inline constexpr std::uint8_t kTestCardChallenge[8] = {0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

class Scp03Card {
public:
  /** Plain C-APDU in, plain R-APDU (data + SW1 SW2) out. */
  std::function<Bytes(const Bytes&)> applet{[](const Bytes&) { return Bytes{0x90, 0x00}; }};
  bool jos2_bug{false};          ///< behave like an SE050 with erratum JOS.2
  bool strip_rmac_next{false};   ///< next protected answer is the bare status word
  bool flip_rmac_next{false};    ///< flip one bit of the next R-MAC
  bool replay_next{false};       ///< answer the next command with the previous protected answer

  std::vector<Bytes> wrapped;    ///< protected C-APDUs as received
  std::vector<Bytes> plain;      ///< the same commands after C-MAC check and decryption
  std::vector<bool> had_cdata;   ///< whether each protected command carried C-DATA
  bool open{false};

  Bytes Handle(const Bytes& c) {
    if (c.size() >= 4U && c[0] == 0x80U && c[1] == 0x50U) {
      return InitializeUpdate(c);
    }
    if (c.size() >= 4U && c[0] == 0x84U && c[1] == 0x82U) {
      return ExternalAuthenticate(c);
    }
    if (!open || (c[0] & 0x04U) == 0U) {
      return applet(c);
    }
    return Protected(c);
  }

private:
  Bytes InitializeUpdate(const Bytes& c) {
    open = false;
    std::memcpy(host_challenge_, c.data() + 5, 8);
    namespace cr = se050::scp03::crypto;
    cr::DeriveSessionKey(kTestStaticKey, cr::kDeriveSenc, host_challenge_, kTestCardChallenge, senc_);
    cr::DeriveSessionKey(kTestStaticKey, cr::kDeriveSmac, host_challenge_, kTestCardChallenge, smac_);
    cr::DeriveSessionKey(kTestStaticKey, cr::kDeriveSrmac, host_challenge_, kTestCardChallenge, srmac_);
    std::uint8_t cryptogram[8]{};
    cr::ComputeCryptogram(smac_, cr::kDeriveCardCryptogram, host_challenge_, kTestCardChallenge,
                          cryptogram);
    Bytes r(10, 0x00);                         // key diversification data
    r.insert(r.end(), {c[2], 0x03, 0x00});     // key version, SCP03, i parameter
    r.insert(r.end(), kTestCardChallenge, kTestCardChallenge + 8);
    r.insert(r.end(), cryptogram, cryptogram + 8);
    r.insert(r.end(), {0x90, 0x00});
    return r;
  }

  Bytes ExternalAuthenticate(const Bytes& c) {
    namespace cr = se050::scp03::crypto;
    std::uint8_t expect[8]{};
    cr::ComputeCryptogram(smac_, cr::kDeriveHostCryptogram, host_challenge_, kTestCardChallenge, expect);
    if (c.size() != 21U || std::memcmp(expect, c.data() + 5, 8) != 0) {
      return {0x63, 0x00};
    }
    std::uint8_t zero_mcv[16]{};
    std::uint8_t mac[16]{};
    Mac(zero_mcv, c.data(), 13U, mac);
    if (std::memcmp(mac, c.data() + 13, 8) != 0) {
      return {0x69, 0x82};
    }
    std::memcpy(mcv_, mac, 16);
    std::memset(counter_, 0, sizeof(counter_));
    counter_[15] = 0x01U;
    open = true;
    return {0x90, 0x00};
  }

  void Mac(const std::uint8_t mcv[16], const std::uint8_t* msg, std::size_t len, std::uint8_t out[16]) {
    namespace cr = se050::scp03::crypto;
    cr::Cmac cm{};
    cr::CmacStart(cm, smac_);
    cr::CmacUpdate(cm, mcv, 16U);
    cr::CmacUpdate(cm, msg, len);
    cr::CmacFinish(cm, out);
  }

  Bytes Protected(const Bytes& c) {
    namespace cr = se050::scp03::crypto;
    wrapped.push_back(c);
    // ISO/IEC 7816-4 5.1: short (Lc one byte) or extended (00 Lc_hi Lc_lo) command.
    const bool ext = c.size() >= 7U && c[4] == 0x00U;
    const std::size_t hdr = ext ? 7U : 5U;
    const std::size_t lc = ext ? (static_cast<std::size_t>(c[5]) << 8 | c[6]) : c[4];
    if (c.size() < hdr + lc || lc < 8U) {
      open = false;
      return {0x67, 0x00};
    }
    const std::size_t le_len = c.size() - hdr - lc;
    std::uint8_t mac[16]{};
    Mac(mcv_, c.data(), hdr + lc - 8U, mac);
    if (std::memcmp(mac, c.data() + hdr + lc - 8U, 8) != 0) {
      open = false;
      return {0x69, 0x82};
    }
    std::memcpy(mcv_, mac, 16);
    const std::size_t enc_len = lc - 8U;
    had_cdata.push_back(enc_len > 0U);
    Bytes data;
    if (enc_len > 0U) {
      std::uint8_t icv[16]{};
      cr::CommandIcv(senc_, counter_, icv);
      Bytes dec(enc_len);
      cr::Aes128CbcDecrypt(senc_, icv, c.data() + hdr, dec.data(), enc_len);
      std::size_t n = 0;
      if (!cr::IsoUnpad80(dec.data(), enc_len, &n)) {
        open = false;
        return {0x69, 0x88};
      }
      data.assign(dec.begin(), dec.begin() + static_cast<std::ptrdiff_t>(n));
    }
    Bytes p{static_cast<std::uint8_t>(c[0] & ~0x04U), c[1], c[2], c[3]};
    if (!data.empty()) {
      if (ext) {
        p.insert(p.end(), {0x00, static_cast<std::uint8_t>(data.size() >> 8),
                           static_cast<std::uint8_t>(data.size())});
      } else {
        p.push_back(static_cast<std::uint8_t>(data.size()));
      }
      p.insert(p.end(), data.begin(), data.end());
    }
    p.insert(p.end(), c.end() - static_cast<std::ptrdiff_t>(le_len), c.end());
    plain.push_back(p);

    const Bytes r = applet(p);
    const Bytes rdata(r.begin(), r.end() - 2);
    Bytes out;
    if (!rdata.empty()) {
      Bytes padded = rdata;
      padded.push_back(0x80);
      while (padded.size() % 16U != 0U) {
        padded.push_back(0x00);
      }
      std::uint8_t icv[16]{};
      cr::ResponseIcv(senc_, counter_, icv);
      out.resize(padded.size());
      cr::Aes128CbcEncrypt(senc_, icv, padded.data(), out.data(), padded.size());
    }
    cr::Cmac cm{};
    cr::CmacStart(cm, srmac_);
    cr::CmacUpdate(cm, mcv_, 16U);
    cr::CmacUpdate(cm, out.data(), out.size());
    cr::CmacUpdate(cm, r.data() + r.size() - 2, 2U);
    std::uint8_t rmac[16]{};
    cr::CmacFinish(cm, rmac);
    out.insert(out.end(), rmac, rmac + 8);
    out.insert(out.end(), r.end() - 2, r.end());

    if (!jos2_bug || enc_len > 0U) {
      cr::IncCommandCounter(counter_);
    }

    const Bytes previous = last_answer_;
    last_answer_ = out;
    if (replay_next) {
      replay_next = false;
      return previous;
    }
    if (strip_rmac_next) {
      strip_rmac_next = false;
      return Bytes(out.end() - 2, out.end());
    }
    if (flip_rmac_next) {
      flip_rmac_next = false;
      out[out.size() - 3U] ^= 0x01U;
    }
    return out;
  }

  std::uint8_t host_challenge_[8]{};
  std::uint8_t senc_[16]{};
  std::uint8_t smac_[16]{};
  std::uint8_t srmac_[16]{};
  std::uint8_t mcv_[16]{};
  std::uint8_t counter_[16]{};
  Bytes last_answer_;
};

/** @brief Static keys and challenge for `OpenSecureChannel()` from the public test vector above. */
inline se050::scp03::StaticKeys TestStaticKeys() {
  se050::scp03::StaticKeys k{};
  k.host_static_key_enc = kTestStaticKey;
  k.host_static_key_enc_len = sizeof(kTestStaticKey);
  k.host_static_key_cmac = kTestStaticKey;
  k.host_static_key_cmac_len = sizeof(kTestStaticKey);
  k.host_challenge = kTestHostChallenge;
  k.host_challenge_len = sizeof(kTestHostChallenge);
  return k;
}

} // namespace se050_test
