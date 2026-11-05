/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2026 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 * SPDX-License-Identifier: GPL-3.0-or-later
 * See serial_protocol.h for the vendor and shipped-host source provenance.
 */
#if defined(UM3_COMPATIBILITY) || defined(UM3_PROTOCOL_TEST)

#include "serial_protocol.h"

namespace ultimaker3 {

static uint16_t crc16_update(uint16_t crc, const uint8_t byte) {
  crc ^= uint16_t(byte) << 8;
  for (uint8_t bit = 0; bit < 8; ++bit)
    crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
  return crc;
}

static uint8_t crc8_update(uint8_t crc, const uint8_t byte) {
  crc ^= byte;
  for (uint8_t bit = 0; bit < 8; ++bit)
    crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x07) : uint8_t(crc << 1);
  return crc;
}

SerialProtocol::SerialProtocol()
  : state_(State::Header), expected_(0), sequence_(0), length_(0), index_(0),
    crc_(0xFFFF), received_crc_(0), packet_ready_(false), payload_{} {}

SerialProtocol::Result SerialProtocol::feed(const uint8_t byte) {
  // Unaccepted packets never advance the sequence, even if the caller continues.
  packet_ready_ = false;

  switch (state_) {
    case State::Header:
      if (byte == 0xFF) state_ = State::SecondHeader;
      break;

    case State::SecondHeader:
      state_ = byte == 0xFF ? State::Sequence : State::Header;
      break;

    case State::Sequence:
    case State::Length:
    case State::Payload:
      // Header bytes resynchronize the vendor parser, except inside the CRC.
      if (byte == 0xFF) {
        state_ = State::SecondHeader;
        break;
      }
      if (state_ == State::Sequence) {
        if (byte != expected_) {
          state_ = State::Header;
          return Result::Reject;
        }
        sequence_ = byte;
        crc_ = crc16_update(0xFFFF, byte);
        state_ = State::Length;
      }
      else if (state_ == State::Length) {
        if (!byte || byte > MAX_PAYLOAD) {
          state_ = State::Header;
          return Result::Reject;
        }
        length_ = byte;
        index_ = 0;
        crc_ = crc16_update(crc_, byte);
        state_ = State::Payload;
      }
      else {
        if (byte < 0x20 || byte > 0x7E) {
          state_ = State::Header;
          return Result::Reject;
        }
        payload_[index_++] = char(byte);
        crc_ = crc16_update(crc_, byte);
        if (index_ == length_) state_ = State::CrcHigh;
      }
      break;

    case State::CrcHigh:
      received_crc_ = uint16_t(byte) << 8;
      state_ = State::CrcLow;
      break;

    case State::CrcLow:
      state_ = State::Header;
      received_crc_ |= byte;
      if (received_crc_ != crc_) return Result::Reject;
      payload_[length_] = '\0';
      packet_ready_ = true;
      return Result::Packet;
  }
  return Result::Pending;
}

bool SerialProtocol::accept() {
  if (!packet_ready_) return false;
  expected_ = expected_ == 254 ? 0 : expected_ + 1;
  packet_ready_ = false;
  return true;
}

void SerialProtocol::discard() {
  state_ = State::Header;
  packet_ready_ = false;
}

bool SerialProtocol::format_response(const char type, const uint8_t sequence,
                                     const uint8_t planner_free, char (&output)[RESPONSE_SIZE]) {
  if ((type != 'o' && type != 'r' && type != 'q') || sequence == 255) return false;

  const char hex[] = "0123456789ABCDEF";
  output[0] = '\r'; output[1] = '\n'; output[2] = type;
  output[3] = hex[sequence >> 4]; output[4] = hex[sequence & 0x0F];
  output[5] = hex[planner_free >> 4]; output[6] = hex[planner_free & 0x0F];
  uint8_t crc = 0;
  for (uint8_t i = 2; i < 7; ++i) crc = crc8_update(crc, uint8_t(output[i]));
  output[7] = hex[crc >> 4]; output[8] = hex[crc & 0x0F];
  output[9] = '\r'; output[10] = '\n'; output[11] = '\0';
  return true;
}

} // namespace ultimaker3

#endif // UM3_COMPATIBILITY || UM3_PROTOCOL_TEST
