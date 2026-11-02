/**
 * @file test_apdu.cpp
 * @brief APDU and TLV encoding, typed commands, Device end to end over the T=1 model, SCP03 key
 * gate, RFC 3394.
 */
#include "mock_se050.hpp"
#include "se050_device.hpp"
#include "test_harness.hpp"

#include <array>
#include <cstdint>

using se050::Error;
using se050_test::Bytes;
using se050_test::MockSe050;

namespace {

/// Wraps @p value as the applet's response TLV 0x41 followed by SW 90 00.
Bytes Tag1Response(const Bytes& value) {
  Bytes r{0x41, static_cast<std::uint8_t>(value.size())};
  r.insert(r.end(), value.begin(), value.end());
  r.push_back(0x90);
  r.push_back(0x00);
  return r;
}

} // namespace

TEST_CASE("SELECT of the IoT applet is 00 A4 04 00 10 <AID> 00") {
  std::uint8_t out[32]{};
  std::size_t len = 0;
  REQUIRE(se050::applet::BuildSelectDefaultIot(out, sizeof(out), &len) == Error::Ok);
  CHECK_EQ(len, 5U + 16U + 1U);
  CHECK(Bytes(out, out + 5) == (Bytes{0x00, 0xA4, 0x04, 0x00, 0x10}));
  CHECK_EQ(out[5], 0xA0U);
  CHECK_EQ(out[len - 1], 0x00U);
}

TEST_CASE("Short CASE 3 omits Le; a too-small buffer is BufferTooSmall") {
  const std::uint8_t data[] = {1, 2, 3};
  std::uint8_t out[8]{};
  std::size_t len = 0;
  REQUIRE(se050::apdu::BuildCaseShort(0x80, 0x01, 0x02, 0x03, data, 3, false, 0, out, sizeof(out),
                                      &len) == Error::Ok);
  CHECK(Bytes(out, out + len) == (Bytes{0x80, 0x01, 0x02, 0x03, 0x03, 1, 2, 3}));
  CHECK(se050::apdu::BuildCaseShort(0x80, 0x01, 0x02, 0x03, data, 3, true, 0, out, 8, &len) ==
        Error::BufferTooSmall);
}

TEST_CASE("BuildCase4Extended uses short encoding up to 255 data bytes and extended Lc above") {
  std::uint8_t out[400]{};
  std::size_t len = 0;
  REQUIRE(se050::apdu::BuildCase4Extended(0x80, 0x04, 0x00, 0x20, nullptr, 0, out, sizeof(out),
                                          &len) == Error::Ok);
  CHECK(Bytes(out, out + len) == (Bytes{0x80, 0x04, 0x00, 0x20, 0x00})); // CASE 2, Le = 00

  const Bytes data(300, 0x5A);
  REQUIRE(se050::apdu::BuildCase4Extended(0x80, 0x01, 0x06, 0x00, data.data(), data.size(), out,
                                          sizeof(out), &len) == Error::Ok);
  CHECK(Bytes(out, out + 7) == (Bytes{0x80, 0x01, 0x06, 0x00, 0x00, 0x01, 0x2C})); // 00 Lc_hi Lc_lo
  CHECK_EQ(out[7], 0x5AU);
}

TEST_CASE("R-APDU parsing splits data and status words") {
  const std::uint8_t rsp[] = {0xDE, 0xAD, 0x6A, 0x82};
  const std::uint8_t* data = nullptr;
  std::size_t data_len = 0;
  se050::apdu::StatusWords sw{};
  REQUIRE(se050::apdu::ParseResponse(rsp, sizeof(rsp), &data, &data_len, &sw) == Error::Ok);
  CHECK_EQ(data_len, 2U);
  CHECK_EQ(sw.sw1, 0x6AU);
  CHECK_EQ(sw.sw2, 0x82U);
  CHECK(!se050::apdu::IsSuccess(sw));
  CHECK(se050::apdu::ParseResponse(rsp, 1, &data, &data_len, &sw) == Error::Protocol);
}

TEST_CASE("TLV lengths use the short, 0x81 and 0x82 forms and decode back") {
  std::uint8_t out[12]{};
  std::size_t off = 0;
  REQUIRE(se050::tlv::AppendTagAndLength(0x41, 0x7F, out, sizeof(out), &off) == Error::Ok);
  REQUIRE(se050::tlv::AppendTagAndLength(0x42, 0x80, out, sizeof(out), &off) == Error::Ok);
  REQUIRE(se050::tlv::AppendTagAndLength(0x43, 0x0123, out, sizeof(out), &off) == Error::Ok);
  CHECK(Bytes(out, out + off) == (Bytes{0x41, 0x7F, 0x42, 0x81, 0x80, 0x43, 0x82, 0x01, 0x23}));
  std::size_t pos = 6;
  std::size_t value_len = 0;
  REQUIRE(se050::tlv::DecodeLength(out, off, &pos, &value_len));
  CHECK_EQ(value_len, 0x0123U);
}

TEST_CASE("GetRandom sends 80 04 00 49 04 41 02 00 20 00 over T=1 and returns tag 0x41") {
  MockSe050 chip;
  Bytes random(32);
  for (std::size_t i = 0; i < random.size(); ++i) {
    random[i] = static_cast<std::uint8_t>(0xA0U + i);
  }
  chip.apdu_handler = [&](const Bytes&) { return Tag1Response(random); };
  se050::Device<MockSe050> se(chip);
  std::uint8_t out[32]{};
  std::size_t out_len = 0;
  REQUIRE(se.GetRandom(32, out, sizeof(out), &out_len, 100) == Error::Ok);
  REQUIRE(chip.capdus.size() == 1U);
  CHECK(chip.capdus[0] == (Bytes{0x80, 0x04, 0x00, 0x49, 0x04, 0x41, 0x02, 0x00, 0x20, 0x00}));
  CHECK_EQ(out_len, 32U);
  CHECK(Bytes(out, out + out_len) == random);
}

TEST_CASE("GetVersion decodes applet version, config and secure box") {
  MockSe050 chip;
  chip.apdu_handler = [](const Bytes&) {
    return Tag1Response({0x03, 0x01, 0x00, 0x6F, 0xFF, 0x01, 0x0B});
  };
  se050::Device<MockSe050> se(chip);
  se050::cmd::VersionInfo v{};
  REQUIRE(se.GetVersion(&v, 100) == Error::Ok);
  CHECK_EQ(v.applet_major, 3U);
  CHECK_EQ(v.applet_minor, 1U);
  CHECK_EQ(v.applet_config, 0x6FFFU);
  CHECK_EQ(v.secure_box, 0x010BU);
}

TEST_CASE("A status word other than 90 00 is Error::Protocol") {
  MockSe050 chip;
  chip.apdu_handler = [](const Bytes&) { return Bytes{0x6A, 0x82}; };
  se050::Device<MockSe050> se(chip);
  const se050::cmd::ObjectId id{0x20, 0x00, 0x00, 0x01};
  CHECK(se.DeleteSecureObject(id, 100) == Error::Protocol);
  bool exists = true;
  CHECK(se.CheckObjectExists(id, &exists, 100) == Error::Protocol);
}

TEST_CASE("EcdsaVerify reports the applet's result byte") {
  MockSe050 chip;
  chip.apdu_handler = [](const Bytes&) { return Tag1Response({0x01}); };
  se050::Device<MockSe050> se(chip);
  const se050::cmd::ObjectId key{0x20, 0x00, 0x00, 0x10};
  const std::array<std::uint8_t, 32> digest{};
  const std::array<std::uint8_t, 72> sig{};
  bool ok = false;
  REQUIRE(se.EcdsaVerify(key, se050::cmd::EcdsaAlgo::Sha256, digest.data(), digest.size(),
                         sig.data(), sig.size(), &ok, 100) == Error::Ok);
  CHECK(ok);
}

TEST_CASE("SCP03 refuses to open without static keys and a host challenge") {
  MockSe050 chip;
  se050::Device<MockSe050> se(chip);
  se050::scp03::StaticKeys none{};
  CHECK(se.Scp03().OpenSecureChannel(se.T1(), none, 100) == Error::NotSupported);
  CHECK(!se.Scp03().IsOpen());
  CHECK(chip.written.empty());
}

TEST_CASE("RFC 3394 wraps and unwraps the RFC's 128-bit test vector (section 4.1)") {
  // Public test vector from RFC 3394, not a real key.
  static const std::uint8_t kek[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                       0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
  const std::uint8_t plain[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                  0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  const Bytes expected{0x1F, 0xA6, 0x8B, 0x0A, 0x81, 0x12, 0xB4, 0x47, 0xAE, 0xF3, 0x4B, 0xD8,
                       0xFB, 0x5A, 0x7B, 0x82, 0x9D, 0x3E, 0x86, 0x23, 0x71, 0xD2, 0xCF, 0xE5};
  auto enc = [](const std::uint8_t in[16], std::uint8_t out[16], void*) -> Error {
    se050::scp03::crypto::Aes128EncryptBlock(kek, in, out);
    return Error::Ok;
  };
  auto dec = [](const std::uint8_t in[16], std::uint8_t out[16], void*) -> Error {
    se050::scp03::crypto::Aes128DecryptBlock(kek, in, out);
    return Error::Ok;
  };
  std::uint8_t wrapped[24]{};
  std::size_t wrapped_len = 0;
  REQUIRE(se050::rfc3394::Wrap(plain, sizeof(plain), wrapped, sizeof(wrapped), &wrapped_len, enc,
                               nullptr) == Error::Ok);
  CHECK(Bytes(wrapped, wrapped + wrapped_len) == expected);
  std::uint8_t back[16]{};
  std::size_t back_len = 0;
  REQUIRE(se050::rfc3394::Unwrap(wrapped, wrapped_len, back, sizeof(back), &back_len, dec,
                                 nullptr) == Error::Ok);
  CHECK(Bytes(back, back + back_len) == Bytes(plain, plain + sizeof(plain)));
  wrapped[0] ^= 0x01U;
  CHECK(se050::rfc3394::Unwrap(wrapped, wrapped_len, back, sizeof(back), &back_len, dec, nullptr) !=
        Error::Ok);
}
