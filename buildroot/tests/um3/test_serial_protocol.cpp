// SPDX-License-Identifier: GPL-3.0-or-later
#include "serial_protocol.h"
#include "fixtures.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using ultimaker3::SerialProtocol;
using Result = SerialProtocol::Result;

static void packet(SerialProtocol &protocol, const PacketFixture &fixture) {
  for (size_t i = 0; i < fixture.size; ++i)
    assert(protocol.feed(fixture.wire[i]) == (i + 1 == fixture.size ? Result::Packet : Result::Pending));
  assert(protocol.sequence() == fixture.sequence);
  assert(protocol.payload_size() == strlen(fixture.payload));
  assert(!strcmp(protocol.payload(), fixture.payload));
  assert(protocol.payload()[protocol.payload_size()] == '\0');
}

int main() {
  for (const PacketFixture &fixture : PACKETS) {
    SerialProtocol protocol;
    if (fixture.sequence) {
      assert(fixture.sequence == 1);
      packet(protocol, PACKETS[0]);
      assert(protocol.accept());
    }
    packet(protocol, fixture);
    assert(protocol.expected_sequence() == fixture.sequence);
    assert(protocol.accept());
    assert(protocol.expected_sequence() == fixture.sequence + 1);
    assert(!protocol.accept());
  }

  // Every possible read boundary leaves the partially received packet intact.
  for (size_t split = 0; split <= PACKETS[0].size; ++split) {
    SerialProtocol protocol;
    for (size_t i = 0; i < split; ++i)
      assert(protocol.feed(PACKETS[0].wire[i]) == (i + 1 == PACKETS[0].size ? Result::Packet : Result::Pending));
    for (size_t i = split; i < PACKETS[0].size; ++i)
      assert(protocol.feed(PACKETS[0].wire[i]) == (i + 1 == PACKETS[0].size ? Result::Packet : Result::Pending));
    assert(protocol.accept());
  }

  SerialProtocol backpressure;
  assert(!backpressure.accept());
  packet(backpressure, PACKETS[0]);
  backpressure.discard();
  assert(!backpressure.accept());
  assert(backpressure.expected_sequence() == 0);
  packet(backpressure, PACKETS[0]);
  assert(backpressure.accept());
  assert(backpressure.feed(0xFF) == Result::Pending);
  assert(backpressure.feed(0xFF) == Result::Pending);
  assert(backpressure.feed(0) == Result::Reject); // Duplicate after successful queue insertion.
  assert(backpressure.expected_sequence() == 1);

  for (const StreamFixture &fixture : INVALID) {
    SerialProtocol protocol;
    size_t rejects = 0;
    for (size_t i = 0; i < fixture.size; ++i) {
      const Result result = protocol.feed(fixture.wire[i]);
      assert(result != Result::Packet);
      rejects += result == Result::Reject;
    }
    assert(rejects == 1);
    assert(protocol.expected_sequence() == 0);
    assert(!protocol.accept());
    packet(protocol, PACKETS[0]);
    assert(protocol.accept());
  }

  // A new header replaces a truncated payload, as in the vendor decoder.
  SerialProtocol resync;
  const uint8_t truncated[] = {0x12, 0xFF, 0x13, 0xFF, 0xFF, 0, 5, 'G'};
  for (const uint8_t byte : truncated) assert(resync.feed(byte) == Result::Pending);
  packet(resync, PACKETS[0]);
  assert(resync.accept());

  SerialProtocol wrap;
  size_t packets = 0;
  for (const uint8_t byte : SEQUENCE_STREAM) {
    const Result result = wrap.feed(byte);
    assert(result != Result::Reject);
    if (result == Result::Packet) {
      assert(wrap.sequence() == packets % 255);
      assert(wrap.accept());
      ++packets;
    }
  }
  assert(packets == 256);
  assert(wrap.expected_sequence() == 1);

  // CRC errors must not execute or advance a packet, even if its payload is valid.
  for (size_t crc_byte = PACKETS[0].size - 2; crc_byte < PACKETS[0].size; ++crc_byte) {
    SerialProtocol protocol;
    for (size_t i = 0; i < PACKETS[0].size; ++i) {
      const uint8_t byte = PACKETS[0].wire[i] ^ (i == crc_byte ? 1 : 0);
      assert(protocol.feed(byte) == (i + 1 == PACKETS[0].size ? Result::Reject : Result::Pending));
    }
    assert(protocol.expected_sequence() == 0);
    assert(!protocol.accept());
    packet(protocol, PACKETS[0]);
  }

  for (const ResponseFixture &fixture : RESPONSES) {
    char output[SerialProtocol::RESPONSE_SIZE];
    assert(SerialProtocol::format_response(fixture.type, fixture.sequence, fixture.planner_free, output));
    assert(!strcmp(output, fixture.wire));
  }
  char output[SerialProtocol::RESPONSE_SIZE];
  memset(output, '#', sizeof(output));
  assert(!SerialProtocol::format_response('x', 0, 15, output));
  assert(!SerialProtocol::format_response('o', 255, 15, output));
  for (const char byte : output) assert(byte == '#');

  // Bounded deterministic malformed-input coverage under ASan and UBSan.
  SerialProtocol noise;
  uint32_t random = 1;
  for (size_t i = 0; i < 100000; ++i) {
    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
    if (noise.feed(uint8_t(random)) == Result::Packet) {
      assert(noise.payload_size() >= 1 && noise.payload_size() <= SerialProtocol::MAX_PAYLOAD);
      assert(noise.payload()[noise.payload_size()] == '\0');
      noise.accept();
    }
  }

  puts("UM3 protocol fixtures, boundaries, rejection, backpressure, wraparound and responses passed");
}
