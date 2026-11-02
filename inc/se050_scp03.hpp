/**
 * @file se050_scp03.hpp
 * @brief GlobalPlatform SCP03 session over an existing T=1 I²C link.
 *
 * @details Protects on-board APDUs against an I²C probe (the MitM that can
 *          forge "signature valid"). This is **not** the HMI channel — HMI
 *          trust remains SE-backed TLS 1.3. Host crypto is in
 *          @ref se050_scp03_crypto.hpp (AES-CMAC / CBC). Compiled-in NXP
 *          default ENC/MAC/DEK values are forbidden; @ref OpenSecureChannel
 *          returns @ref Error::NotSupported when static keys are absent.
 *
 *          Wrap/unwrap uses caller-owned buffers (CM4: D2 `.se_apdu_scratch`).
 *          Live T=1 exchanges stay plaintext until unique ENC/MAC keys exist.
 *
 * @copyright Copyright (c) 2026 HardFOC. All rights reserved.
 */
#pragma once

#include "se050_apdu.hpp"
#include "se050_scp03_crypto.hpp"
#include "se050_t1_session.hpp"
#include "se050_types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace se050::scp03 {

inline constexpr std::uint8_t kClaGp = 0x80U;
inline constexpr std::uint8_t kClaGpSecure = 0x84U;
inline constexpr std::uint8_t kInsInitializeUpdate = 0x50U;
inline constexpr std::uint8_t kInsExternalAuthenticate = 0x82U;
inline constexpr std::uint8_t kDefaultKeyVersion = 0x0BU;
inline constexpr std::uint8_t kSecLevelCmacCencRmacRenc = 0x33U;
inline constexpr std::size_t kInitUpdateRspMin = 10U + 3U + 8U + 8U + 2U;
inline constexpr std::uint8_t kClaSmBit = 0x04U;
inline constexpr std::size_t kMacFieldLen = 8U;
/** @brief Minimum wrap workspace, enough for short APDUs (CM4: D2 `.se_apdu_scratch`, not task stack). */
inline constexpr std::size_t kWrapScratchBytes = 320U;

/** @brief Runtime static keys (never compiled-in NXP defaults). */
struct StaticKeys {
    const std::uint8_t* host_static_key_enc{nullptr};
    std::size_t host_static_key_enc_len{0};
    const std::uint8_t* host_static_key_cmac{nullptr};
    std::size_t host_static_key_cmac_len{0};
    const std::uint8_t* host_challenge{nullptr};
    std::size_t host_challenge_len{0};
    std::uint8_t key_version{kDefaultKeyVersion};
};

/** @brief Live SCP03 session state after a successful handshake. */
struct ChannelState {
    bool open{false};
    std::uint8_t senc[crypto::kAes128KeyLen]{};
    std::uint8_t smac[crypto::kAes128KeyLen]{};
    std::uint8_t srmac[crypto::kAes128KeyLen]{};
    std::uint8_t mcv[16]{};
    std::uint8_t counter[16]{};
};

/** @brief Wrap workspace for any command up to @ref kMaxApduCommandBytes (extended header, padding, MAC, Le). */
inline constexpr std::size_t kWrapScratchBytesExtended = kMaxApduCommandBytes + 7U + 16U + kMacFieldLen + 2U;

/** @brief A C-APDU split into its ISO/IEC 7816-4 section 5.1 fields. */
struct CommandFields {
    const std::uint8_t* data{nullptr};
    std::size_t data_len{0};
    bool extended{false};  ///< Lc / Le were in extended form (cases 2E, 3E, 4E)
    bool has_le{false};    ///< an Le field was present (cases 2 and 4)
};

/**
 * @brief Split a serialized C-APDU into data and Le (ISO/IEC 7816-4 section 5.1, cases 1, 2S/E, 3S/E, 4S/E).
 * @return @ref Error::Protocol when the length bytes do not describe the buffer.
 */
[[nodiscard]] inline Error ParseCommand(const std::uint8_t* capdu, std::size_t len, CommandFields* f) noexcept {
    if (capdu == nullptr || f == nullptr || len < 4U) {
        return Error::InvalidArgument;
    }
    *f = CommandFields{};
    if (len == 4U) {
        return Error::Ok; /* case 1 */
    }
    if (len == 5U) {
        f->has_le = true; /* case 2S */
        return Error::Ok;
    }
    if (capdu[4] != 0x00U) {
        const std::size_t lc = capdu[4];
        if (len == 5U + lc || len == 6U + lc) { /* case 3S / 4S */
            f->data = capdu + 5U;
            f->data_len = lc;
            f->has_le = (len == 6U + lc);
            return Error::Ok;
        }
        return Error::Protocol;
    }
    f->extended = true;
    if (len == 7U) {
        f->has_le = true; /* case 2E: 00 Le_hi Le_lo */
        return Error::Ok;
    }
    const std::size_t lc = (static_cast<std::size_t>(capdu[5]) << 8) | capdu[6];
    if (lc == 0U) {
        return Error::Protocol;
    }
    if (len == 7U + lc || len == 9U + lc) { /* case 3E / 4E */
        f->data = capdu + 7U;
        f->data_len = lc;
        f->has_le = (len == 9U + lc);
        return Error::Ok;
    }
    return Error::Protocol;
}

/**
 * @brief C-MAC + C-ENC wrap of a C-APDU in any ISO/IEC 7816-4 case, short or extended.
 * @details GPC Amendment D 6.2.4 / 6.2.6: the command data is padded (`80 00..`) and encrypted with
 *          the command ICV, then the MAC is computed over the MAC chaining value, the header (CLA with
 *          the secure-messaging bit) and the new Lc and data. The wrapped command uses extended
 *          length when the input did or when the new Lc exceeds 255. When the input had an Le, the
 *          wrapped command keeps one, set to the maximum (`00`, or `00 00` extended): the protected
 *          answer is longer than the plain one by its padding and R-MAC, so the plain Ne cannot be
 *          reused. Le is not covered by the C-MAC.
 *          Caller owns @p out (CM4: D2 `.se_apdu_scratch`); @ref kWrapScratchBytes covers short
 *          commands, @ref kWrapScratchBytesExtended any command. Updates the MCV. The command counter
 *          moves on in @ref UnwrapResponse once the card has answered.
 */
[[nodiscard]] inline Error WrapCommand(ChannelState& st, const std::uint8_t* capdu,
                                       std::size_t capdu_len, std::uint8_t* out,
                                       std::size_t out_cap, std::size_t* out_len) noexcept {
    if (out_len == nullptr) {
        return Error::InvalidArgument;
    }
    *out_len = 0;
    if (!st.open) {
        return Error::NotSupported;
    }
    if (capdu == nullptr || out == nullptr || capdu_len < 4U) {
        return Error::InvalidArgument;
    }
    CommandFields f{};
    const Error pe = ParseCommand(capdu, capdu_len, &f);
    if (pe != Error::Ok) {
        return pe;
    }

    const std::size_t enc_len = (f.data_len > 0U) ? ((f.data_len / 16U) + 1U) * 16U : 0U;
    const std::size_t lc = enc_len + kMacFieldLen;
    const bool extended = f.extended || lc > 255U;
    if (lc > 0xFFFFU) {
        return Error::InvalidArgument;
    }
    const std::size_t hdr = extended ? 7U : 5U;
    const std::size_t le_len = f.has_le ? (extended ? 2U : 1U) : 0U;
    if ((hdr + lc + le_len) > out_cap) {
        return Error::BufferTooSmall;
    }

    out[0] = static_cast<std::uint8_t>(capdu[0] | kClaSmBit);
    out[1] = capdu[1];
    out[2] = capdu[2];
    out[3] = capdu[3];
    if (extended) {
        out[4] = 0x00U;
        out[5] = static_cast<std::uint8_t>((lc >> 8) & 0xFFU);
        out[6] = static_cast<std::uint8_t>(lc & 0xFFU);
    } else {
        out[4] = static_cast<std::uint8_t>(lc);
    }

    if (enc_len > 0U) {
        std::memmove(out + hdr, f.data, f.data_len);
        std::size_t padded = f.data_len;
        if (!crypto::IsoPad80(out + hdr, &padded, enc_len) || padded != enc_len) {
            crypto::SecureZero(out, out_cap);
            return Error::BufferTooSmall;
        }
        std::uint8_t icv[16]{};
        crypto::ScopedWipe wipe_icv(icv, sizeof(icv));
        crypto::CommandIcv(st.senc, st.counter, icv);
        if (!crypto::Aes128CbcEncrypt(st.senc, icv, out + hdr, out + hdr, enc_len)) {
            crypto::SecureZero(out, out_cap);
            return Error::InvalidArgument;
        }
    }

    crypto::Cmac cm{};
    crypto::CmacStart(cm, st.smac);
    crypto::CmacUpdate(cm, st.mcv, 16U);
    crypto::CmacUpdate(cm, out, hdr + enc_len);
    std::uint8_t mac_full[16]{};
    crypto::ScopedWipe wipe_mac(mac_full, sizeof(mac_full));
    crypto::CmacFinish(cm, mac_full);
    std::memcpy(out + hdr + enc_len, mac_full, kMacFieldLen);
    std::memcpy(st.mcv, mac_full, 16);
    std::size_t n = hdr + lc;
    for (std::size_t i = 0; i < le_len; ++i) {
        out[n++] = 0x00U;
    }
    *out_len = n;
    return Error::Ok;
}

/**
 * @brief End a session: wipe the session keys, MAC chaining value and counter, mark it closed.
 * @details Uses @ref crypto::SecureZero so the wipe is not optimised away.
 */
inline void WipeChannel(ChannelState& st) noexcept { crypto::SecureZero(&st, sizeof(st)); }

/**
 * @brief R-MAC check and R-ENC decryption of one response. Increments the counter on success.
 * @details GPC Amendment D 6.2.5 / 6.2.7 (security level 0x33): every response in the session carries
 *          an R-MAC over the MAC chaining value, the encrypted data and SW1 SW2. A response that is
 *          shorter than R-MAC + SW (for example a bare `90 00` an attacker injects on the bus), whose
 *          R-MAC does not match, or whose padding is invalid returns @ref Error::Authentication, and the
 *          channel is closed with its keys wiped. Nothing from such a response is written to @p out.
 *          The MAC is compared in constant time and checked before anything is decrypted.
 */
[[nodiscard]] inline Error UnwrapResponse(ChannelState& st, const std::uint8_t* rapdu,
                                          std::size_t rapdu_len, std::uint8_t* out,
                                          std::size_t out_cap, std::size_t* out_len) noexcept {
    if (out_len == nullptr) {
        return Error::InvalidArgument;
    }
    *out_len = 0;
    if (!st.open) {
        return Error::NotSupported;
    }
    if (rapdu == nullptr || out == nullptr) {
        return Error::InvalidArgument;
    }
    if (rapdu_len < (kMacFieldLen + 2U)) {
        WipeChannel(st);
        return Error::Authentication;
    }

    const std::uint8_t sw1 = rapdu[rapdu_len - 2U];
    const std::uint8_t sw2 = rapdu[rapdu_len - 1U];
    const std::size_t enc_len = rapdu_len - kMacFieldLen - 2U;

    crypto::Cmac cm{};
    crypto::CmacStart(cm, st.srmac);
    crypto::CmacUpdate(cm, st.mcv, 16U);
    crypto::CmacUpdate(cm, rapdu, enc_len);
    crypto::CmacUpdate(cm, rapdu + rapdu_len - 2U, 2U);
    std::uint8_t mac_full[16]{};
    crypto::ScopedWipe wipe_mac(mac_full, sizeof(mac_full));
    crypto::CmacFinish(cm, mac_full);
    if (!crypto::ConstantTimeEqual(mac_full, rapdu + enc_len, kMacFieldLen)) {
        WipeChannel(st);
        return Error::Authentication;
    }

    if (enc_len == 0U) {
        crypto::IncCommandCounter(st.counter);
        if (out_cap < 2U) {
            return Error::BufferTooSmall;
        }
        out[0] = sw1;
        out[1] = sw2;
        *out_len = 2U;
        return Error::Ok;
    }
    if ((enc_len % 16U) != 0U) {
        WipeChannel(st);
        return Error::Authentication;
    }
    if (enc_len > out_cap) {
        crypto::IncCommandCounter(st.counter); /* answer consumed: stay in step with the card */
        return Error::BufferTooSmall;
    }

    std::uint8_t icv[16]{};
    crypto::ScopedWipe wipe_icv(icv, sizeof(icv));
    crypto::ResponseIcv(st.senc, st.counter, icv);
    std::size_t plain_len = 0;
    if (!crypto::Aes128CbcDecrypt(st.senc, icv, rapdu, out, enc_len) ||
        !crypto::IsoUnpad80(out, enc_len, &plain_len)) {
        crypto::SecureZero(out, enc_len);
        WipeChannel(st);
        return Error::Authentication;
    }
    crypto::IncCommandCounter(st.counter);
    if ((plain_len + 2U) > out_cap) {
        crypto::SecureZero(out, enc_len);
        return Error::BufferTooSmall;
    }
    out[plain_len] = sw1;
    out[plain_len + 1U] = sw2;
    *out_len = plain_len + 2U;
    return Error::Ok;
}

inline constexpr std::uint8_t kInsPutKey = 0xD8U;
inline constexpr std::uint8_t kPutKeyTypeAes = 0x88U;
inline constexpr std::uint8_t kPutKeyP2Multiple = 0x81U;
inline constexpr std::uint8_t kPutKeyKcvLen = 0x03U;
inline constexpr std::size_t kPutKeyBlockBytes = 23U;  ///< type + lens + enc key + KCV
inline constexpr std::size_t kPutKeyDataBytes = 1U + 3U * kPutKeyBlockBytes;  ///< version + ENC/MAC/DEK
inline constexpr std::size_t kPutKeyExpectRspBytes = 1U + 3U * kPutKeyKcvLen;  ///< version + 3 KCVs

/**
 * @brief One AES key field inside GP `PUT KEY` (encrypted with the **current** DEK).
 * @details Layout matches NXP `ex_se05x_rotate_scp03_keys`: type `0x88`,
 *          KCV = first 3 bytes of AES-ECB(plain, 0x01×16).
 *          This is GPCS B.2.1 AES-ECB of a 16-byte key with the current DEK
 *          (NXP `ex_se05x_rotate_scp03_keys`). It is **not** RFC 3394 AES Key
 *          Wrap (that wrap is ADR-0054 sec_rpc / image KEK). Do not substitute.
 */
inline bool FillPutKeyBlock(std::uint8_t* block, const std::uint8_t current_dek[crypto::kAes128KeyLen],
                            const std::uint8_t plain[crypto::kAes128KeyLen],
                            std::uint8_t kcv3[kPutKeyKcvLen]) noexcept {
    if (block == nullptr || current_dek == nullptr || plain == nullptr || kcv3 == nullptr) {
        return false;
    }
    block[0] = kPutKeyTypeAes;
    block[1] = static_cast<std::uint8_t>(crypto::kAes128KeyLen + 1U);
    block[2] = static_cast<std::uint8_t>(crypto::kAes128KeyLen);
    crypto::Aes128EncryptBlock(current_dek, plain, block + 3U);
    std::uint8_t ones[crypto::kAes128KeyLen];
    std::memset(ones, 0x01, sizeof(ones));
    std::uint8_t kcv_full[crypto::kAes128KeyLen]{};
    crypto::Aes128EncryptBlock(plain, ones, kcv_full);
    block[3U + crypto::kAes128KeyLen] = kPutKeyKcvLen;
    std::memcpy(block + 3U + crypto::kAes128KeyLen + 1U, kcv_full, kPutKeyKcvLen);
    std::memcpy(kcv3, kcv_full, kPutKeyKcvLen);
    return true;
}

/**
 * @brief Build GP `PUT KEY` (CLA `0x80`, INS `0xD8`, P1 = key version, P2 `0x81`).
 * @details Replaces ISD key set @p key_version (product: `0x0B`) with @p new_enc,
 *          @p new_mac, @p new_dek. The **current** DEK encrypts the new key
 *          values. Caller must have an open SCP03 session with the current set.
 *          Never embeds NXP default key bytes.
 * @param expect_rsp Version byte + three 3-byte KCVs the card must echo.
 */
[[nodiscard]] inline Error BuildPutKey(std::uint8_t key_version,
                                       const std::uint8_t current_dek[crypto::kAes128KeyLen],
                                       const std::uint8_t new_enc[crypto::kAes128KeyLen],
                                       const std::uint8_t new_mac[crypto::kAes128KeyLen],
                                       const std::uint8_t new_dek[crypto::kAes128KeyLen],
                                       std::uint8_t* capdu, std::size_t capdu_cap, std::size_t* capdu_len,
                                       std::uint8_t* expect_rsp, std::size_t expect_cap,
                                       std::size_t* expect_len) noexcept {
    if (capdu_len == nullptr || expect_len == nullptr) {
        return Error::InvalidArgument;
    }
    *capdu_len = 0;
    *expect_len = 0;
    if (current_dek == nullptr || new_enc == nullptr || new_mac == nullptr || new_dek == nullptr ||
        capdu == nullptr || expect_rsp == nullptr) {
        return Error::InvalidArgument;
    }
    if (expect_cap < kPutKeyExpectRspBytes) {
        return Error::BufferTooSmall;
    }
    std::uint8_t data[kPutKeyDataBytes]{};
    std::size_t off = 0;
    data[off++] = key_version;
    expect_rsp[0] = key_version;
    std::size_t kcv_off = 1;
    if (!FillPutKeyBlock(data + off, current_dek, new_enc, expect_rsp + kcv_off)) {
        return Error::InvalidArgument;
    }
    off += kPutKeyBlockBytes;
    kcv_off += kPutKeyKcvLen;
    if (!FillPutKeyBlock(data + off, current_dek, new_mac, expect_rsp + kcv_off)) {
        return Error::InvalidArgument;
    }
    off += kPutKeyBlockBytes;
    kcv_off += kPutKeyKcvLen;
    if (!FillPutKeyBlock(data + off, current_dek, new_dek, expect_rsp + kcv_off)) {
        return Error::InvalidArgument;
    }
    *expect_len = kPutKeyExpectRspBytes;
    return apdu::BuildCaseShort(kClaGp, kInsPutKey, key_version, kPutKeyP2Multiple, data,
                                static_cast<std::uint8_t>(kPutKeyDataBytes), false, 0x00U, capdu, capdu_cap,
                                capdu_len);
}

/**
 * @brief SCP03 session bound to a T=1 transport.
 * @tparam TransportT CRTP I²C transport type used by @ref T1Session.
 */
template <typename TransportT>
class Session {
public:
    Session() noexcept = default;
    /** @brief Wipes the session keys (see @ref Close). */
    ~Session() { WipeChannel(st_); }
    Session(const Session&) = delete;             ///< session keys are never copied
    Session& operator=(const Session&) = delete;  ///< session keys are never copied

    /**
     * @brief `INITIALIZE UPDATE` / `EXTERNAL AUTHENTICATE`.
     * @details Returns @ref Error::NotSupported when static keys or the
     *          8-byte host challenge are missing. Never returns Ok for a
     *          plaintext stand-in.
     */
    [[nodiscard]] Error OpenSecureChannel(T1Session<TransportT>& t1, const StaticKeys& keys,
                                          std::uint32_t timeout_ms) noexcept {
        using crypto::kAes128KeyLen;
        using crypto::kChallengeLen;
        using crypto::kCryptogramLen;
        WipeChannel(st_);
        failed_ = false;
        if (!crypto::StaticKeysUsable(keys.host_static_key_enc, keys.host_static_key_enc_len,
                                      keys.host_static_key_cmac, keys.host_static_key_cmac_len) ||
            keys.host_challenge == nullptr || keys.host_challenge_len != kChallengeLen) {
            return Error::NotSupported;
        }

        std::uint8_t capdu[16]{};
        std::size_t capdu_len = 0;
        Error e = apdu::BuildCaseShort(kClaGp, kInsInitializeUpdate, keys.key_version, 0x00U,
                                       keys.host_challenge, static_cast<std::uint8_t>(kChallengeLen),
                                       false, 0x00U, capdu, sizeof(capdu), &capdu_len);
        if (e != Error::Ok) {
            return e;
        }

        std::uint8_t rapdu[48]{};
        std::size_t rapdu_len = 0;
        e = t1.ExchangeInformation(capdu, capdu_len, rapdu, sizeof(rapdu), &rapdu_len, timeout_ms);
        if (e != Error::Ok) {
            return e;
        }
        if (rapdu_len < kInitUpdateRspMin) {
            return Error::Protocol;
        }
        const std::uint8_t sw1 = rapdu[rapdu_len - 2U];
        const std::uint8_t sw2 = rapdu[rapdu_len - 1U];
        if (sw1 != 0x90U || sw2 != 0x00U) {
            return Error::Protocol;
        }

        const std::uint8_t* card_challenge = rapdu + 13U;
        const std::uint8_t* card_cryptogram = rapdu + 21U;

        std::uint8_t senc[kAes128KeyLen]{};
        std::uint8_t smac[kAes128KeyLen]{};
        std::uint8_t srmac[kAes128KeyLen]{};
        crypto::ScopedWipe wipe_senc(senc, sizeof(senc));
        crypto::ScopedWipe wipe_smac(smac, sizeof(smac));
        crypto::ScopedWipe wipe_srmac(srmac, sizeof(srmac));
        if (!crypto::DeriveSessionKey(keys.host_static_key_enc, crypto::kDeriveSenc,
                                      keys.host_challenge, card_challenge, senc) ||
            !crypto::DeriveSessionKey(keys.host_static_key_cmac, crypto::kDeriveSmac,
                                      keys.host_challenge, card_challenge, smac) ||
            !crypto::DeriveSessionKey(keys.host_static_key_cmac, crypto::kDeriveSrmac,
                                      keys.host_challenge, card_challenge, srmac)) {
            return Error::InvalidArgument;
        }

        std::uint8_t expect_card[kCryptogramLen]{};
        if (!crypto::ComputeCryptogram(smac, crypto::kDeriveCardCryptogram, keys.host_challenge,
                                       card_challenge, expect_card) ||
            !crypto::ConstantTimeEqual(expect_card, card_cryptogram, kCryptogramLen)) {
            return Error::Protocol;
        }

        std::uint8_t host_cryptogram[kCryptogramLen]{};
        if (!crypto::ComputeCryptogram(smac, crypto::kDeriveHostCryptogram, keys.host_challenge,
                                       card_challenge, host_cryptogram)) {
            return Error::InvalidArgument;
        }

        std::uint8_t ext[32]{};
        ext[0] = kClaGpSecure;
        ext[1] = kInsExternalAuthenticate;
        ext[2] = kSecLevelCmacCencRmacRenc;
        ext[3] = 0x00U;
        ext[4] = 0x10U;
        std::memcpy(ext + 5, host_cryptogram, kCryptogramLen);

        std::uint8_t mcv[16]{};
        std::uint8_t mac_in[16 + 13]{};
        std::uint8_t mac_full[16]{};
        crypto::ScopedWipe wipe_mac(mac_full, sizeof(mac_full));
        std::memcpy(mac_in, mcv, 16);
        std::memcpy(mac_in + 16, ext, 13);
        crypto::Aes128Cmac(smac, mac_in, 16U + 13U, mac_full);
        std::memcpy(ext + 13, mac_full, kCryptogramLen);
        const std::size_t ext_len = 5U + 2U * kCryptogramLen;

        std::uint8_t ext_rapdu[16]{};
        std::size_t ext_rapdu_len = 0;
        e = t1.ExchangeInformation(ext, ext_len, ext_rapdu, sizeof(ext_rapdu), &ext_rapdu_len,
                                   timeout_ms);
        if (e != Error::Ok) {
            return e;
        }
        if (ext_rapdu_len < 2U || ext_rapdu[ext_rapdu_len - 2U] != 0x90U ||
            ext_rapdu[ext_rapdu_len - 1U] != 0x00U) {
            return Error::Protocol;
        }

        std::memcpy(st_.senc, senc, kAes128KeyLen);
        std::memcpy(st_.smac, smac, kAes128KeyLen);
        std::memcpy(st_.srmac, srmac, kAes128KeyLen);
        std::memcpy(st_.mcv, mac_full, 16);
        std::memset(st_.counter, 0, sizeof(st_.counter));
        st_.counter[15] = 0x01U;
        st_.open = true;
        return Error::Ok;
    }

    /**
     * @brief Close the channel: wipe session keys, MAC chaining value and counter.
     * @details Also clears @ref Failed, so the device sends plaintext again until the next
     *          @ref OpenSecureChannel.
     */
    [[nodiscard]] Error Close() noexcept {
        WipeChannel(st_);
        failed_ = false;
        return Error::Ok;
    }

    [[nodiscard]] bool IsOpen() const noexcept { return st_.open; }

    /**
     * @brief `true` after the channel was lost (a response failed its R-MAC, or the exchange broke
     *        inside the channel). The device refuses commands with @ref Error::Authentication until
     *        @ref Close or a new @ref OpenSecureChannel, so it never falls back to plaintext silently.
     */
    [[nodiscard]] bool Failed() const noexcept { return failed_; }

    /** @brief Mark the channel lost and wipe its keys (used by the device on a broken exchange). */
    void Abort() noexcept {
        WipeChannel(st_);
        failed_ = true;
    }

    [[nodiscard]] Error WrapCommand(const std::uint8_t* capdu, std::size_t capdu_len,
                                    std::uint8_t* out, std::size_t out_cap,
                                    std::size_t* out_len) noexcept {
        return se050::scp03::WrapCommand(st_, capdu, capdu_len, out, out_cap, out_len);
    }

    [[nodiscard]] Error UnwrapResponse(const std::uint8_t* rapdu, std::size_t rapdu_len,
                                       std::uint8_t* out, std::size_t out_cap,
                                       std::size_t* out_len) noexcept {
        const Error e = se050::scp03::UnwrapResponse(st_, rapdu, rapdu_len, out, out_cap, out_len);
        if (e == Error::Authentication) {
            failed_ = true;
        }
        return e;
    }

    [[nodiscard]] ChannelState& State() noexcept { return st_; }
    [[nodiscard]] const ChannelState& State() const noexcept { return st_; }

private:
    ChannelState st_{};
    bool failed_{false};
};

}  // namespace se050::scp03
