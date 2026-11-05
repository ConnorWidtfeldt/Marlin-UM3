/*
  HardwareSerial.h - Hardware serial library for Wiring
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Modified 28 September 2010 by Mark Sproul
*/

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <WString.h>
#include "../native_api.h"
#define BYTE 0
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
namespace um3_stock {
class MarlinSerial {
public:
  void begin(long baud) { um3_native_serial_begin(baud); }
  void end() { um3_native_serial_end(); }
  int peek() { return um3_native_serial_peek(); }
  int read() { return um3_native_serial_read(); }
  int available() { return um3_native_serial_available(); }
  // Stock flush discards received bytes; modern flush drains the transmitter.
  void flush() { um3_native_serial_clear_rx(); }
  void write(uint8_t byte) { um3_native_serial_write(byte); }
  private:
    void printNumber(unsigned long, uint8_t);
    void printFloat(double, uint8_t);


  public:
    FORCE_INLINE void write(const char *str)
    {
      while (*str)
        write(*str++);
    }


    FORCE_INLINE void write(const uint8_t *buffer, size_t size)
    {
      while (size--)
        write(*buffer++);
    }

    FORCE_INLINE void print(const String &s)
    {
      for (int i = 0; i < (int)s.length(); i++) {
        write(s[i]);
      }
    }

    FORCE_INLINE void print(const char *str)
    {
      write(str);
    }
    void print(char, int = BYTE);
    void print(unsigned char, int = BYTE);
    void print(int, int = DEC);
    void print(unsigned int, int = DEC);
    void print(long, int = DEC);
    void print(unsigned long, int = DEC);
    void print(double, int = 2);

    void println(const String &s);
    void println(const char[]);
    void println(char, int = BYTE);
    void println(unsigned char, int = BYTE);
    void println(int, int = DEC);
    void println(unsigned int, int = DEC);
    void println(long, int = DEC);
    void println(unsigned long, int = DEC);
    void println(double, int = 2);
    void println(void);
};
extern MarlinSerial MSerial;
}
