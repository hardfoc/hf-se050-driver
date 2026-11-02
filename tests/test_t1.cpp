/**
 * @file test_t1.cpp
 * @brief T=1 over I2C: EDC, block framing, sequence numbers, chaining both ways, WTX, error paths.
 */
#include "mock_se050.hpp"
#include "se050_crc.hpp"
#include "se050_t1_session.hpp"
#include "test_harness.hpp"

#include <cstdint>
#include <vector>

using se050::Error;
using se050_test::Bytes;
using se050_test::MockSe050;

TEST_CASE("EDC is CRC-16/X-25: check value of \"123456789\" is 0x906E") {
  const std::uint8_t msg[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK_EQ(se050::crc::Crc16Edc7816(msg, 0, sizeof(msg)), 0x906EU);
}

TEST_CASE("EDC goes on the wire LSB first and verifies") {
  std::uint8_t frame[5] = {0x5A, 0xC7, 0x00, 0, 0};
  se050::crc::AppendEdc(frame, 3);
  const std::uint16_t crc = se050::crc::Crc16Edc7816(frame, 0, 3);
  CHECK_EQ(frame[3], crc & 0xFFU);
  CHECK_EQ(frame[4], crc >> 8);
  CHECK(se050::crc::VerifyFrameCrc(frame, sizeof(frame)));
  frame[4] ^= 0x01U;
  CHECK(!se050::crc::VerifyFrameCrc(frame, sizeof(frame)));
}

TEST_CASE("GET ATR is an empty S-block 5A C7 00 and returns the profile bytes") {
  MockSe050 chip;
  se050::T1Session<MockSe050> t1(chip);
  std::uint8_t atr[64]{};
  std::size_t atr_len = 0;
  REQUIRE(t1.GetAnswerToReset(atr, sizeof(atr), &atr_len, 100) == Error::Ok);
  REQUIRE(chip.written.size() == 1U);
  CHECK(chip.written[0] == se050_test::MakeBlock(0x5A, 0xC7, {}));
  CHECK_EQ(atr_len, chip.atr.size());
  CHECK(Bytes(atr, atr + atr_len) == chip.atr);
}

TEST_CASE("Warm reset is S-block C6 and resets the sequence numbers") {
  MockSe050 chip;
  se050::T1Session<MockSe050> t1(chip);
  CHECK(t1.ChipWarmReset(100) == Error::Ok);
  CHECK(chip.written.back() == se050_test::MakeBlock(0x5A, 0xC6, {}));
}

TEST_CASE("I-blocks alternate N(S): PCB 0x00, then 0x40") {
  MockSe050 chip;
  se050::T1Session<MockSe050> t1(chip);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  REQUIRE(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) ==
          Error::Ok);
  REQUIRE(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) ==
          Error::Ok);
  REQUIRE(chip.written.size() == 2U);
  CHECK_EQ(chip.written[0][1], 0x00U);
  CHECK_EQ(chip.written[1][1], 0x40U);
  CHECK_EQ(rsp_len, 2U);
  CHECK_EQ(rsp[0], 0x90U);
}

TEST_CASE("A 300-byte command is chained: 254 bytes with M set, R-block, then 46 bytes") {
  MockSe050 chip;
  se050::T1Session<MockSe050> t1(chip);
  Bytes cmd(300);
  for (std::size_t i = 0; i < cmd.size(); ++i) {
    cmd[i] = static_cast<std::uint8_t>(i);
  }
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  REQUIRE(t1.ExchangeInformation(cmd.data(), cmd.size(), rsp, sizeof(rsp), &rsp_len, 100) ==
          Error::Ok);
  REQUIRE(chip.written.size() == 2U);
  CHECK_EQ(chip.written[0][1], 0x20U); // N(S) = 0, M = 1
  CHECK_EQ(chip.written[0][2], 254U);
  CHECK_EQ(chip.written[1][1], 0x40U); // N(S) = 1, M = 0
  CHECK_EQ(chip.written[1][2], 46U);
  REQUIRE(chip.capdus.size() == 1U);
  CHECK(chip.capdus[0] == cmd);
}

TEST_CASE("A chained answer is acknowledged with R-blocks and reassembled") {
  MockSe050 chip;
  chip.card_ifs = 32;
  Bytes answer(100, 0xAB);
  answer.push_back(0x90);
  answer.push_back(0x00);
  chip.apdu_handler = [&](const Bytes&) { return answer; };
  se050::T1Session<MockSe050> t1(chip);
  const std::uint8_t capdu[] = {0x80, 0x02, 0x00, 0x00, 0x00};
  std::uint8_t rsp[256]{};
  std::size_t rsp_len = 0;
  REQUIRE(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) ==
          Error::Ok);
  CHECK_EQ(rsp_len, answer.size());
  CHECK(Bytes(rsp, rsp + rsp_len) == answer);
  // 102 bytes in 32-byte blocks: four blocks, three acknowledgements with N(R) = 1, 0, 1.
  REQUIRE(chip.written.size() == 4U);
  CHECK_EQ(chip.written[1][1], 0x90U);
  CHECK_EQ(chip.written[2][1], 0x80U);
  CHECK_EQ(chip.written[3][1], 0x90U);
}

TEST_CASE("A WTX request is answered with S(WTX response) echoing its INF") {
  MockSe050 chip;
  chip.wtx_before_reply = 2;
  se050::T1Session<MockSe050> t1(chip);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  REQUIRE(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) ==
          Error::Ok);
  REQUIRE(chip.written.size() == 3U);
  CHECK(chip.written[1] == se050_test::MakeBlock(0x5A, 0xE3, {0x01}));
  CHECK(chip.written[2] == se050_test::MakeBlock(0x5A, 0xE3, {0x01}));
}

TEST_CASE("More WTX requests than SetMaxWtxRequests() allows is a protocol error") {
  MockSe050 chip;
  chip.wtx_before_reply = 3;
  se050::T1Session<MockSe050> t1(chip);
  t1.SetMaxWtxRequests(2);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  CHECK(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) ==
        Error::Protocol);
}

TEST_CASE("A corrupt EDC on the answer is recovered: R-block with EDC error, card sends it again") {
  MockSe050 chip;
  chip.corrupt_next_edc = true;
  se050::T1Session<MockSe050> t1(chip);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  CHECK(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) == Error::Ok);
  CHECK_EQ(rsp_len, 2U);
  CHECK_EQ(chip.retransmissions_sent, 1);
  REQUIRE(chip.written.size() == 2U);
  CHECK(chip.written[1] == se050_test::MakeBlock(0x5A, 0x81, {})); // R(N(R)=0), EDC error
}

TEST_CASE("Persistent EDC errors end in Error::Crc after the retransmission limit") {
  MockSe050 chip;
  chip.corrupt_edc_blocks = 100;
  se050::T1Session<MockSe050> t1(chip);
  t1.SetMaxRetransmissions(3);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  CHECK(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) ==
        Error::Crc);
  CHECK_EQ(chip.retransmissions_sent, 3);
}

TEST_CASE("A corrupt block inside a chained answer is asked for again") {
  MockSe050 chip;
  chip.card_ifs = 16;
  Bytes answer(40, 0x33);
  answer.push_back(0x90);
  answer.push_back(0x00);
  chip.apdu_handler = [&](const Bytes&) { return answer; };
  chip.corrupt_next_edc = true;
  se050::T1Session<MockSe050> t1(chip);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[64]{};
  std::size_t rsp_len = 0;
  REQUIRE(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) == Error::Ok);
  CHECK(Bytes(rsp, rsp + rsp_len) == answer);
  CHECK_EQ(chip.retransmissions_sent, 1);
}

TEST_CASE("An R-block NACK from the card makes the host send the same I-block again") {
  MockSe050 chip;
  chip.nack_host_iblocks = 1;
  se050::T1Session<MockSe050> t1(chip);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  REQUIRE(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) == Error::Ok);
  REQUIRE(chip.written.size() == 2U);
  CHECK(chip.written[0] == chip.written[1]); // ISO/IEC 7816-3 11.6.3: same block, same N(S)
  CHECK_EQ(chip.capdus.size(), 1U);
  // N(S) still alternates normally afterwards.
  REQUIRE(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) == Error::Ok);
  CHECK_EQ(chip.written.back()[1], 0x40U);
}

TEST_CASE("A NACK of a chained command block is answered by sending that block again") {
  MockSe050 chip;
  chip.nack_host_iblocks = 1;
  se050::T1Session<MockSe050> t1(chip);
  const Bytes cmd(300, 0x5A);
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  REQUIRE(t1.ExchangeInformation(cmd.data(), cmd.size(), rsp, sizeof(rsp), &rsp_len, 100) ==
          Error::Ok);
  REQUIRE(chip.capdus.size() == 1U);
  CHECK(chip.capdus[0] == cmd);
  CHECK(chip.written[0] == chip.written[1]);
}

TEST_CASE("GET ATR takes the IFSC from the chip's profile and commands are split at it") {
  MockSe050 chip;
  chip.atr[9] = 0x00; // DLLP IFSC = 0x0020: 32 bytes per block
  chip.atr[10] = 0x20;
  se050::T1Session<MockSe050> t1(chip);
  CHECK_EQ(t1.Ifsc(), 254U);
  std::uint8_t atr[64]{};
  std::size_t atr_len = 0;
  REQUIRE(t1.GetAnswerToReset(atr, sizeof(atr), &atr_len, 100) == Error::Ok);
  CHECK_EQ(t1.Ifsc(), 32U);
  const Bytes cmd(100, 0x11);
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  REQUIRE(t1.ExchangeInformation(cmd.data(), cmd.size(), rsp, sizeof(rsp), &rsp_len, 100) ==
          Error::Ok);
  CHECK(chip.host_inf_sizes == (std::vector<std::size_t>{32, 32, 32, 4}));
  CHECK(chip.capdus.back() == cmd);
}

TEST_CASE("SetIfsc keeps the block size between 1 and 254") {
  MockSe050 chip;
  se050::T1Session<MockSe050> t1(chip);
  t1.SetIfsc(0);
  CHECK_EQ(t1.Ifsc(), 1U);
  t1.SetIfsc(1000);
  CHECK_EQ(t1.Ifsc(), 254U);
  t1.SetIfsc(64);
  CHECK_EQ(t1.Ifsc(), 64U);
}

TEST_CASE("Erratum I2C.1: an unread answer from before a host reset is drained first") {
  MockSe050 chip;
  chip.LeaveStaleAnswer({0x6A, 0x80}); // the host reset while the chip held this answer
  se050::T1Session<MockSe050> t1(chip);
  REQUIRE(t1.RecoverAfterHostReset(100) == Error::Ok);
  CHECK(!chip.locked);
  CHECK(chip.written.back() == se050_test::MakeBlock(0x5A, 0xC0, {})); // S(RESYNCH request)
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  CHECK(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) == Error::Ok);
  CHECK_EQ(rsp[0], 0x90U);
}

TEST_CASE("Erratum I2C.1: without the start-up read, the stale answer locks the bus (model check)") {
  MockSe050 chip;
  chip.LeaveStaleAnswer({0x6A, 0x80});
  se050::T1Session<MockSe050> t1(chip);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  CHECK(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) != Error::Ok);
  CHECK(chip.locked);
}

TEST_CASE("RecoverAfterHostReset with nothing pending just resynchronises") {
  MockSe050 chip;
  se050::T1Session<MockSe050> t1(chip);
  t1.SetReadRetries(1);
  CHECK(t1.RecoverAfterHostReset(100) == Error::Ok);
  CHECK(chip.written.back() == se050_test::MakeBlock(0x5A, 0xC0, {}));
  CHECK(!chip.locked);
}

TEST_CASE("Retryable read errors are retried with the configured delay") {
  MockSe050 chip;
  chip.failing_reads = 3;
  se050::T1Session<MockSe050> t1(chip);
  t1.SetInterFrameDelayMs(0);
  t1.SetReadRetries(3);
  t1.SetReadRetryDelayMs(5);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  CHECK(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) == Error::Ok);
  CHECK_EQ(chip.delay_total_ms, 15U);

  chip.failing_reads = 4;
  t1.ResetSequenceState();
  CHECK(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) ==
        Error::Timeout);
}

TEST_CASE("An answer larger than the buffer is Error::BufferTooSmall") {
  MockSe050 chip;
  chip.apdu_handler = [](const Bytes&) { return Bytes(40, 0x11); };
  se050::T1Session<MockSe050> t1(chip);
  const std::uint8_t capdu[] = {0x80, 0x02, 0x00, 0x00, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  CHECK(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) ==
        Error::BufferTooSmall);
}

TEST_CASE("A transport that is not initialised is Error::NotInitialized") {
  MockSe050 chip;
  chip.init_ok = false;
  se050::T1Session<MockSe050> t1(chip);
  const std::uint8_t capdu[] = {0x80, 0x04, 0x00, 0x20, 0x00};
  std::uint8_t rsp[16]{};
  std::size_t rsp_len = 0;
  CHECK(t1.ExchangeInformation(capdu, sizeof(capdu), rsp, sizeof(rsp), &rsp_len, 100) ==
        Error::NotInitialized);
  CHECK(chip.written.empty());
}
