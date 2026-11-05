// GPL-3.0-or-later. Test-only hardware boundary, never in the firmware build.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string>
#include <algorithm>
template<class A,class B> auto min(A a,B b)->decltype(a<b?a:b) { return a<b?a:b; }
using String=std::string;
#define _FASTIO_ARDUINO_H
#define PROGMEM
#define PSTR(s) (s)
#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t*>(p))
#define pgm_read_word(p) (*reinterpret_cast<const uint16_t*>(p))
#define pgm_read_float(p) (*reinterpret_cast<const float*>(p))
#define pgm_read_float_near(p) pgm_read_float(p)
#define pgm_read_byte_near(p) pgm_read_byte(p)
#define pgm_read_word_near(p) pgm_read_word(p)
#define _BV(n) (1U<<(n))
#define ATOMIC_BLOCK(x) for(bool atomic_once=true;atomic_once;atomic_once=false)
#define ATOMIC_RESTORESTATE 0
#define ISR(vector) void vector()
#define cli() ((void)0)
#define sei() ((void)0)
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
extern uint8_t mock_pins[256],mock_pwm[256];
extern uint8_t SREG,ADCSRA,ADCSRB,ADMUX,DIDR0,DIDR2,OCR0B,TIMSK0,TCCR5B;
extern uint16_t ADC;
extern int mock_board_adc;
enum {F5=59,F6=60,F7=61,J5=75,J6=76};
enum {ADEN=7,ADSC=6,ADIF=4,MUX5=3,REFS0=6,OCIE0B=2,CS50=0,CS51=1,CS52=2};
#define READ(p) (mock_pins[(p)]!=0)
#define WRITE(p,v) (mock_pins[(p)]=(v))
#define SET_OUTPUT(p) ((void)(p))
#define SET_INPUT(p) ((void)(p))
#define SET_INPUT_PULLUP(p) (mock_pins[(p)]=1)
unsigned long millis();
void mock_delay(unsigned long ms);
inline void _delay_ms(double ms) { mock_delay(ms); }
inline void _delay_us(double us) { if(us>=1000) mock_delay(us/1000); }
inline void delay(unsigned long ms) { mock_delay(ms); }
inline void pinMode(uint8_t,uint8_t) {}
inline void analogWrite(uint8_t p,int v) { mock_pins[p]=v;mock_pwm[p]=(v>0&&v<255)?v:0; }
inline int analogRead(uint8_t) { return mock_board_adc; }
inline int digitalRead(uint8_t p) { return READ(p); }
inline void digitalWrite(uint8_t p,uint8_t v) { WRITE(p,v); }
#define constrain(x,a,b) ((x)<(a)?(a):(x)>(b)?(b):(x))
