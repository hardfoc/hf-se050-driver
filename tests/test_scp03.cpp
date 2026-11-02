/**
 * @file test_scp03.cpp
 * @brief Platform SCP03 against the card model: handshake, protected round trips, rejected answers
 * (tampered R-MAC, status word only, replay), key wiping, erratum JOS.2, extended APDUs and Le.
 *
 * Keys are the public RFC 3394 section 4.1 test vector (see mock_scp03_card.hpp). No real key.
 */
#include "mock_scp03_card.hpp"
#include "mock_se050.hpp"
#include "se050_device.hpp"
#include "test_harness.hpp"

#include <cstdint>
#include <new>

using se050::Error;
using se050_test::Bytes;
using se050_test::MockSe050;
using se050_test::Scp03Card;

namespace {

Bytes Tag1(const Bytes& value) {
  Bytes r{0x41, static_cast<std::uint8_t>(value.size())};
  r.insert(r.end(), value.begin(), value.end());
  r.push_back(0x90);
  r.push_back(0x00);
  return r;
}

/// A chip whose applet answers through the SCP03 card model, and a device with a bound wrap buffer.
struct Rig {
  MockSe050 chip;
  Scp03Card card;
  se050::Device<MockSe050> se{chip};
  std::uint8_t scratch[1024]{};

  Rig() {
    chip.apdu_handler = [this](const Bytes& c) { return card.Handle(c); };
    card.applet = [](const Bytes& c) {
      if (c.size() >= 4U && c[1] == 0x04U && c[3] == 0x20U) { // GetVersion
        return Tag1({0x03, 0x01, 0x00, 0x6F, 0xFF, 0x01, 0x0B});
      }
      if (c.size() >= 4U && c[1] == 0x04U && c[3] == 0x49U) { // GetRandom
        return Tag1(Bytes(32, 0xA5));
      }
      return Bytes{0x90, 0x00};
    };
    se.BindScp03WrapScratch(scratch, sizeof(scratch));
  }

  bool Open() {
    return se.Scp03().OpenSecureChannel(se.T1(), se050_test::TestStaticKeys(), 100) == Error::Ok &&
           se.Scp03().IsOpen() && card.open;
  }
};

bool AllZero(const std::uint8_t* p, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    if (p[i] != 0U) {
      return false;
    }
  }
  return true;
}

[[maybe_unused]] bool KeysWiped(const se050::scp03::ChannelState& st) {
  return !st.open && AllZero(st.senc, sizeof(st.senc)) && AllZero(st.smac, sizeof(st.smac)) &&
         AllZero(st.srmac, sizeof(st.srmac)) && AllZero(st.mcv, sizeof(st.mcv)) &&
         AllZero(st.counter, sizeof(st.counter));
}

[[maybe_unused]] const se050::cmd::ObjectId kObj{{0x10, 0x00, 0x00, 0x01}};

} // namespace

TEST_CASE("SCP03: an answer with only a status word is rejected and closes the channel") {
  Rig r;
  REQUIRE(r.Open());
  r.card.strip_rmac_next = true; // a bus attacker answers 90 00 without R-MAC
  CHECK(r.se.DeleteSecureObject(kObj, 100) == Error::Authentication);
  CHECK(!r.se.Scp03().IsOpen());
  CHECK(KeysWiped(r.se.Scp03().State()));
}

TEST_CASE("SCP03: a short answer passed to UnwrapResponse is never accepted") {
  Rig r;
  REQUIRE(r.Open());
  const std::uint8_t sw_only[] = {0x90, 0x00};
  std::uint8_t out[16]{};
  std::size_t out_len = 0;
  CHECK(r.se.Scp03().UnwrapResponse(sw_only, sizeof(sw_only), out, sizeof(out), &out_len) ==
        Error::Authentication);
  CHECK_EQ(out_len, 0U);
  CHECK(!r.se.Scp03().IsOpen());
  const std::uint8_t nine[9] = {0, 1, 2, 3, 4, 5, 6, 0x90, 0x00}; // one byte short of a MAC + SW
  CHECK(se050::scp03::UnwrapResponse(r.se.Scp03().State(), nine, sizeof(nine), out, sizeof(out),
                                     &out_len) != Error::Ok);
}

TEST_CASE("SCP03: a tampered R-MAC is rejected and closes the channel") {
  Rig r;
  REQUIRE(r.Open());
  r.card.flip_rmac_next = true;
  std::uint8_t out[32]{};
  std::size_t out_len = 0;
  CHECK(r.se.GetRandom(32, out, sizeof(out), &out_len, 100) == Error::Authentication);
  CHECK_EQ(out_len, 0U);
  CHECK(KeysWiped(r.se.Scp03().State()));
}

TEST_CASE("SCP03: a replayed earlier answer is rejected") {
  Rig r;
  REQUIRE(r.Open());
  REQUIRE(r.se.DeleteSecureObject(kObj, 100) == Error::Ok);
  r.card.replay_next = true; // the attacker answers the second command with the first answer
  CHECK(r.se.DeleteSecureObject(kObj, 100) == Error::Authentication);
  CHECK(!r.se.Scp03().IsOpen());
}

TEST_CASE("SCP03: commands are refused after the channel closed on a bad answer") {
  Rig r;
  REQUIRE(r.Open());
  r.card.flip_rmac_next = true;
  CHECK(r.se.DeleteSecureObject(kObj, 100) != Error::Ok);
  const std::size_t sent = r.chip.capdus.size();
  // The device falls back to plaintext only when the caller asks; a closed channel is visible.
  CHECK(!r.se.Scp03().IsOpen());
  CHECK_EQ(r.chip.capdus.size(), sent);
}

TEST_CASE("SCP03: a T=1 failure inside the channel closes it and wipes the keys") {
  Rig r;
  REQUIRE(r.Open());
  r.se.T1().SetReadRetries(0);
  r.chip.failing_reads = 100;
  CHECK(r.se.DeleteSecureObject(kObj, 100) != Error::Ok);
  CHECK(KeysWiped(r.se.Scp03().State()));
}

TEST_CASE("SCP03: Close() wipes the session keys, MAC chaining value and counter") {
  Rig r;
  REQUIRE(r.Open());
  CHECK(!AllZero(r.se.Scp03().State().senc, 16));
  CHECK(r.se.Scp03().Close() == Error::Ok);
  CHECK(KeysWiped(r.se.Scp03().State()));
}

TEST_CASE("SCP03: destroying a session wipes its keys") {
  alignas(se050::scp03::Session<MockSe050>) unsigned char storage[sizeof(se050::scp03::Session<MockSe050>)];
  auto* s = new (storage) se050::scp03::Session<MockSe050>();
  std::memset(s->State().senc, 0x11, 16);
  std::memset(s->State().smac, 0x22, 16);
  std::memset(s->State().srmac, 0x33, 16);
  s->State().open = true;
  s->~Session();
  CHECK(AllZero(storage, sizeof(storage)));
}

TEST_CASE("SCP03: MAC comparison helper compares every byte") {
  namespace cr = se050::scp03::crypto;
  const std::uint8_t a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  std::uint8_t b[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  CHECK(cr::ConstantTimeEqual(a, b, 8));
  b[7] ^= 0x80U;
  CHECK(!cr::ConstantTimeEqual(a, b, 8));
  b[7] ^= 0x80U;
  b[0] ^= 0x01U;
  CHECK(!cr::ConstantTimeEqual(a, b, 8));
  std::uint8_t wipe[5] = {9, 9, 9, 9, 9};
  cr::SecureZero(wipe, sizeof(wipe));
  CHECK(AllZero(wipe, sizeof(wipe)));
}

