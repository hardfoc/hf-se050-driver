/**
 * @file api_tasks.cpp
 * @brief The API guide's task snippets, compiled by the host build (never run).
 *
 * `Se` stands for `se050::Device<MyI2c>`. Object identifiers below are examples from the customer
 * range; pick your own and keep a table of them.
 */
#include "my_i2c.hpp"
#include "se050_driver.hpp"

#include <cstdio>

using Se = se050::Device<MyI2c>;

// Platform hooks the SCP03 snippet uses: where your product keeps its provisioned keys and a random
// source.
bool load_platform_scp03_keys(std::uint8_t enc[16], std::uint8_t mac[16]);
bool host_random(std::uint8_t* out, std::size_t len);
void platform_zeroize(void* p, std::size_t n); // e.g. mbedtls_platform_zeroize()

// --8<-- [start:sw]
// TransmitApdu() and the SELECT helpers return the raw R-APDU: check SW1 SW2 yourself.
bool sw_ok(const std::uint8_t* rapdu, std::size_t rapdu_len) {
  const std::uint8_t* data = nullptr;
  std::size_t data_len = 0;
  se050::apdu::StatusWords sw{};
  return se050::apdu::ParseResponse(rapdu, rapdu_len, &data, &data_len, &sw) == se050::Error::Ok &&
         se050::apdu::IsSuccess(sw);
}
// --8<-- [end:sw]

int bring_up(Se& se) {
  // --8<-- [start:bringup]
  (void)se.HardwareReset(); // ENA low, then high (no-op when ENA is not wired)
  se.T1().ResetSequenceState();
  if (se.T1().ChipWarmReset(100) != se050::Error::Ok) {
    return -1;
  }

  std::uint8_t atr[64]{};
  std::size_t atr_len = 0;
  se050::atr::Profile profile{};
  if (se.T1().GetAnswerToReset(atr, sizeof(atr), &atr_len, 100) != se050::Error::Ok ||
      se050::atr::Parse(atr, atr_len, &profile) != se050::Error::Ok) {
    return -2;
  }
  std::printf("IFSC %u bytes, max I2C %u kHz\n", static_cast<unsigned>(profile.ifsc),
              static_cast<unsigned>(profile.max_i2c_khz));

  std::uint8_t rsp[32]{};
  std::size_t rsp_len = 0;
  if (se.SelectDefaultIoTApplet(rsp, sizeof(rsp), &rsp_len, 300) != se050::Error::Ok ||
      !sw_ok(rsp, rsp_len)) {
    return -3;
  }
  // --8<-- [end:bringup]
  return 0;
}

int read_info(Se& se) {
  // --8<-- [start:info]
  se050::cmd::VersionInfo v{};
  if (se.GetVersion(&v, 300) != se050::Error::Ok) {
    return -1;
  }
  std::uint16_t free_bytes = 0;
  if (se.GetFreeMemory(se050::cmd::MemoryType::Persistent, &free_bytes, 300) != se050::Error::Ok) {
    return -2;
  }
  std::printf("applet %u.%u.%u, %u bytes free\n", v.applet_major, v.applet_minor, v.applet_patch,
              static_cast<unsigned>(free_bytes));
  // --8<-- [end:info]
  return 0;
}

int store_object(Se& se) {
  // --8<-- [start:objects]
  const se050::cmd::ObjectId id{0x20, 0x00, 0x10, 0x01}; // a binary file you own
  const std::uint8_t config[] = {0x01, 0x02, 0x03, 0x04};

  bool exists = false;
  if (se.CheckObjectExists(id, &exists, 300) != se050::Error::Ok) {
    return -1;
  }
  if (!exists && se.WriteBinary(id, config, sizeof(config), false, 0, true, sizeof(config), 500) !=
                     se050::Error::Ok) { // create with a fixed file length
    return -2;
  }

  std::uint8_t back[sizeof(config)]{};
  std::size_t back_len = 0;
  if (se.ReadObject(id, false, 0, false, 0, back, sizeof(back), &back_len, 300) !=
      se050::Error::Ok) {
    return -3;
  }
  // When the data is no longer needed:
  if (se.DeleteSecureObject(id, 300) != se050::Error::Ok) {
    return -4;
  }
  // --8<-- [end:objects]
  return 0;
}

int sign_challenge(Se& se, const std::uint8_t (&sha256_digest)[32]) {
  // --8<-- [start:keys]
  const se050::cmd::ObjectId key{0x20, 0x00, 0x20, 0x01}; // device identity key

  bool exists = false;
  if (se.CheckObjectExists(key, &exists, 300) != se050::Error::Ok) {
    return -1;
  }
  if (!exists &&
      se.GenerateEcKeyPair(key, se050::cmd::EcCurve::NistP256, 2000) != se050::Error::Ok) {
    return -2; // the private key is created inside the SE050
  }

  std::uint8_t pub[96]{};
  std::size_t pub_len = 0; // the public key, for your backend
  if (se.ReadPublicEcKey(key, pub, sizeof(pub), &pub_len, 300) != se050::Error::Ok) {
    return -3;
  }

  std::uint8_t sig[80]{};
  std::size_t sig_len = 0; // ASN.1 DER ECDSA signature
  if (se.EcdsaSign(key, se050::cmd::EcdsaAlgo::Sha256, sha256_digest, sizeof(sha256_digest), sig,
                   sizeof(sig), &sig_len, 1000) != se050::Error::Ok) {
    return -4;
  }

  bool valid = false;
  if (se.EcdsaVerify(key, se050::cmd::EcdsaAlgo::Sha256, sha256_digest, sizeof(sha256_digest), sig,
                     sig_len, &valid, 1000) != se050::Error::Ok ||
      !valid) {
    return -5;
  }
  // --8<-- [end:keys]
  return 0;
}

int raw_apdu(Se& se) {
  // --8<-- [start:raw]
  // GET RANDOM by hand: CLA 80, INS 04 (management), P1 00, P2 49, data TLV 41 02 00 10 (16 bytes),
  // Le 00.
  std::uint8_t data[4]{};
  std::size_t data_len = 0;
  std::uint8_t capdu[16]{};
  std::size_t capdu_len = 0;
  if (se050::tlv::AppendU16Be(0x41, 16, data, sizeof(data), &data_len) != se050::Error::Ok ||
      se050::apdu::BuildCaseShort(0x80, 0x04, 0x00, 0x49, data, static_cast<std::uint8_t>(data_len),
                                  true, 0x00, capdu, sizeof(capdu),
                                  &capdu_len) != se050::Error::Ok) {
    return -1;
  }
  std::uint8_t rapdu[64]{};
  std::size_t rapdu_len = 0;
  if (se.TransmitApdu(capdu, capdu_len, rapdu, sizeof(rapdu), &rapdu_len, 300) !=
          se050::Error::Ok ||
      !sw_ok(rapdu, rapdu_len)) {
    return -2;
  }
  // --8<-- [end:raw]
  return 0;
}

const char* describe(se050::Error e) {
  switch (e) {
  case se050::Error::Ok:
    return "ok";
  case se050::Error::NotInitialized:
    return "transport not ready (EnsureInitialized() is false)";
  case se050::Error::Transport:
    return "I2C write or read failed after the retries";
  case se050::Error::Timeout:
    return "I2C timeout after the retries";
  case se050::Error::Crc:
    return "T=1 EDC mismatch: noise, wrong speed, or a block cut short";
  case se050::Error::Sequence:
    return "T=1 N(S) out of step: resynchronise";
  case se050::Error::Protocol:
    return "bad framing, or the applet answered SW != 90 00";
  case se050::Error::BufferTooSmall:
    return "the answer does not fit the buffer you passed";
  case se050::Error::InvalidArgument:
    return "null pointer or length out of range";
  case se050::Error::NotSupported:
    return "feature unavailable, e.g. SCP03 without keys";
  }
  return "unknown";
}

// --8<-- [start:errors]
// After any T=1-level error the sequence numbers may disagree with the chip: start over.
se050::Error recover(Se& se) {
  se.T1().ResetSequenceState();
  return se.T1().ChipWarmReset(100);
}
// --8<-- [end:errors]

int open_scp03(Se& se) {
  // --8<-- [start:scp03]
  // Keys come from your provisioning store at run time. Never compile keys into the firmware.
  std::uint8_t enc[16]{};
  std::uint8_t mac[16]{};
  std::uint8_t challenge[8]{};
  if (!load_platform_scp03_keys(enc, mac) || !host_random(challenge, sizeof(challenge))) {
    return -1;
  }
  se050::scp03::StaticKeys keys{};
  keys.host_static_key_enc = enc;
  keys.host_static_key_enc_len = sizeof(enc);
  keys.host_static_key_cmac = mac;
  keys.host_static_key_cmac_len = sizeof(mac);
  keys.host_challenge = challenge;
  keys.host_challenge_len = sizeof(challenge);

  static std::uint8_t wrap_scratch[se050::scp03::kWrapScratchBytes]; // not on a small task stack
  se.BindScp03WrapScratch(wrap_scratch, sizeof(wrap_scratch));

  const se050::Error e = se.Scp03().OpenSecureChannel(se.T1(), keys, 500);
  // The session keys are derived now: wipe the static key copies.
  platform_zeroize(enc, sizeof(enc));
  platform_zeroize(mac, sizeof(mac));
  if (e != se050::Error::Ok) {
    return -2; // NotSupported, or Protocol: wrong keys
  }
  // From here TransmitApdu() and every Device command are wrapped (C-MAC, C-ENC, R-MAC, R-ENC).
  // --8<-- [end:scp03]
  return 0;
}
