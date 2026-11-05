/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2026 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * UM3 wire contract: UltimakerMarlin 4545818112027387f09e87f930117ab8f3804327,
 * Marlin/{SerialProtocol,MarlinSerial,crc8,crc16}.{h,cpp}; corroborated by the
 * shipped UM3 5.3.0.20211003 host's networkPacket.py, protocolDataLine.py and
 * griffin/math/{crc8,crc16}.py. This is transport only, not hardware support.
 */
#pragma once

#include <stdint.h>

namespace ultimaker3 {

class SerialProtocol {
public:
  static constexpr uint8_t MAX_PAYLOAD = 96;
  static constexpr uint8_t RESPONSE_SIZE = 12; // CRLF + type/SS/PP/CC + CRLF + NUL

  enum class Result : uint8_t { Pending, Packet, Reject };

  SerialProtocol();

  // On Packet, copy the payload and call accept() or discard() before feeding more.
  Result feed(uint8_t byte);
  bool accept();
  void discard();

  const char *payload() const { return payload_; }
  uint8_t payload_size() const { return length_; }
  uint8_t sequence() const { return sequence_; }
  uint8_t expected_sequence() const { return expected_; }

  // Returns false for an invalid response type or reserved sequence, without output.
  static bool format_response(char type, uint8_t sequence, uint8_t planner_free,
                              char (&output)[RESPONSE_SIZE]);

private:
  enum class State : uint8_t { Header, SecondHeader, Sequence, Length, Payload, CrcHigh, CrcLow };

  State state_;
  uint8_t expected_, sequence_, length_, index_;
  uint16_t crc_, received_crc_;
  bool packet_ready_;
  char payload_[MAX_PAYLOAD + 1];
};

} // namespace ultimaker3
