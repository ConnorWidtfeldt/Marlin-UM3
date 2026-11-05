// Exercise production serial code with mocked UART register boundaries.
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <cassert>
#include <iostream>
#define MARLIN_H
#define FORCE_INLINE inline __attribute__((always_inline))
#define F_CPU 16000000UL
#define SERIAL_PORT 2
#define SIGNAL(vector) void vector(void)
#define cbi(reg,bit) ((reg) &= ~(1 << (bit)))
#define sbi(reg,bit) ((reg) |= (1 << (bit)))
#define RXEN2 4
#define TXEN2 3
#define RXCIE2 7
#define RXC2 7
#define UDRE2 5
#define U2X2 1
static uint8_t ubrr2h,ubrr2l,ucsr2a=1<<UDRE2,ucsr2b;
#define UBRR2H ubrr2h
#define UBRR2L ubrr2l
#define UCSR2A ucsr2a
#define UCSR2B ucsr2b
static std::vector<uint8_t> transmitted;
struct TxRegister {
 uint8_t value;
 operator uint8_t() const { return value; }
 TxRegister& operator=(uint8_t c) { value=c; transmitted.push_back(c); return *this; }
} udr2;
#define UDR2 udr2
class String {
 std::string value;
public:
 size_t length() const { return value.length(); }
 char operator[](int i) const { return value[i]; }
};
#include "../Marlin/MarlinSerial.cpp"
int main() {
 static_assert(sizeof(ring_buffer)==258,"stock storage contract");
 for (unsigned cycle=0;cycle<3;++cycle) {
  assert(MSerial.available()==0);
  for (unsigned i=0;i<255;++i) store_char(uint8_t(i));
  assert(MSerial.available()==255);
  store_char(255);
  assert(MSerial.available()==255);
  for (unsigned i=0;i<255;++i) {
   assert(MSerial.peek()==int(i));
   assert(MSerial.read()==int(i));
  }
  assert(MSerial.read()==-1);
  assert(MSerial.peek()==-1);
 }
 udr2.value=42;
 ucsr2a |= 1<<RXC2;
 MSerial.checkRx();
 assert(MSerial.read()==42);
 ucsr2a &= ~(1<<RXC2);
 store_char(12); store_char(34); MSerial.flush();
 assert(MSerial.available()==0 && MSerial.read()==-1);
 transmitted.clear(); MSerial.println();
 assert(transmitted==std::vector<uint8_t>{10});
 std::cout << "Stock serial ring capacity, overflow, wraparound, flush and LF framing passed\n";
}
