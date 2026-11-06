// GPL-3.0-or-later. Production protocol/frontend/thermal/probe with hardware mocks.
#include "Marlin.h"
#include "Board.h"
#include "SerialProtocol.h"
#include "crc16.h"
#include "temperature.h"
#include "planner.h"
#include "i2c_driver.h"
#include "i2c_capacitance_FDC1004.h"
#include "i2c_onewire_ds2482.h"
#include "usart_spi_driver.h"
#include "stepper_TMC2130.h"
#include "../motion_contract.h"
#include <assert.h>
#include <deque>
#include <vector>
#include <string.h>
#include <stdio.h>
uint8_t mock_pins[256]={},mock_pwm[256]={};
uint8_t SREG=0,ADCSRA=0,ADCSRB=0,ADMUX=0,DIDR0=0,DIDR2=0,OCR0B=0,TIMSK0=0,TCCR5B=0;
uint16_t ADC=253;
int mock_board_adc=447;
static uint32_t clock_ms=0,watchdog_pets=0;
static bool core_present[2]={true,true},adc_failure=false,adc_stall=false;
static bool endstop_hit=false;
static int16_t cap_reading=1000;
static std::deque<uint8_t> rx;
static std::string tx;
static float steps_per_mm[4]={80,80,200,369};
static int32_t executed[4]={},offsets[3][2]={};
static std::deque<std::vector<int32_t>> moves;
static std::deque<uint8_t> move_fans;
namespace um3_stock { void TIMER0_COMPB_vect(); }
unsigned long millis() { return clock_ms; }
void mock_delay(unsigned long ms) {
  while(ms--) {
    ++clock_ms;
    if(TIMSK0&_BV(OCIE0B)) um3_stock::TIMER0_COMPB_vect();
  }
}
extern "C" {
int um3_native_serial_read() { if(rx.empty())return -1; int v=rx.front();rx.pop_front();return v; }
int um3_native_serial_peek() { return rx.empty()?-1:rx.front(); }
int um3_native_serial_available() { return rx.size(); }
void um3_native_serial_write(uint8_t b) { tx+=char(b); }
void um3_native_serial_begin(uint32_t) {}
void um3_native_serial_end() {}
void um3_native_serial_clear_rx() { rx.clear(); }
void um3_native_stepper_init() { for(int p:{27,31,34,37,48})mock_pins[p]=1; }
void um3_native_synchronize() { while(!moves.empty()) { for(uint8_t a=0;a<4;a++)executed[a]=moves.front()[a];moves.pop_front();if(!move_fans.empty())move_fans.pop_front(); } }
void um3_native_set_steps(int32_t x,int32_t y,int32_t z,int32_t e) { executed[0]=x;executed[1]=y;executed[2]=z;executed[3]=e; }
int32_t um3_native_get_steps(uint8_t a) { return executed[a]; }
void um3_native_set_e_steps(int32_t e) { executed[3]=e; }
void um3_native_wake() {}
void um3_native_endstops(bool) {}
void um3_native_clear_endstop_hits() {}
bool um3_native_endstop_hit() { return endstop_hit; }
void um3_native_disable_steppers() { for(int p:{27,31,34,37,48})mock_pins[p]=1; }
void um3_native_enable_axis(uint8_t a,bool on) { const int pins[]={27,31,34,37,48};mock_pins[pins[a]]=!on; }
void um3_native_quickstop() { moves.clear();move_fans.clear(); }
void um3_native_abort_motion() { moves.clear();move_fans.clear(); }
uint8_t um3_native_planner_free() { return 15-moves.size(); }
bool um3_native_blocks_queued() { return !moves.empty(); }
void um3_native_activity() { um3_stock_apply_cooling_fan(move_fans.empty()?um3_stock_target_fan_speed():move_fans.front()); }
void um3_native_buffer(float x,float y,float z,float e,float,uint8_t tool) {
  float logical[]={x,y,z,e};int32_t result[4],off[3]={offsets[0][tool],offsets[1][tool],offsets[2][tool]};
  assert(ultimaker3::physical_target_steps(logical,off,steps_per_mm,result));
  moves.push_back(std::vector<int32_t>(result,result+4));
  move_fans.push_back(um3_stock_target_fan_speed());
}
void um3_native_position(float x,float y,float z,float e) {
  const uint8_t t=um3_stock::active_extruder;
  const float logical[]={x,y,z,e}; const int32_t off[3]={offsets[0][t],offsets[1][t],offsets[2][t]};
  assert(ultimaker3::physical_target_steps(logical,off,steps_per_mm,executed));
}
void um3_native_e_position(float e) { executed[3]=e*steps_per_mm[3]; }
bool um3_native_calibrate(const float *steps,const float *,const uint32_t *,float,float,float,float,float,float,float,uint32_t,int,const bool *) {
  for(uint8_t a=0;a<4;a++) steps_per_mm[a]=steps[a]; return true;
}
void um3_native_select_tool(uint8_t) {}
void um3_native_tool_offsets(const int32_t *p) { memcpy(offsets,p,sizeof(offsets)); }
void um3_native_min_extrude(float) {}
void um3_native_arc(const float *,const float *,const float *,float,bool,uint8_t) { assert(false && "Arc geometry is verified by the modern AVR engine build"); }
void um3_native_watchdog_init() {}
void um3_native_watchdog_reset() { ++watchdog_pets; }
void um3_native_temperature_tick() {}
void um3_native_thermal_sample(const float *,const float *,const bool *,float,float) {}
}
namespace um3_stock {
void i2cDriverInit() {}
void i2cDriverPlan(i2cCommand *c) {
  if(adc_stall && c->slave_address_rw==(0x48<<1|1)) { c->finished=false; return; }
  if(c->slave_address_rw==(0x48<<1|1)) {
    // ADS1015 signed differential code -1500, representing a cold present core.
    const uint16_t raw=uint16_t(uint16_t(-1500)<<4);
    c->buffer[0]=adc_failure?255:raw>>8;c->buffer[1]=adc_failure?255:raw&255;
  }
  c->finished=true;
}
void i2cDriverExecuteAndWait(i2cCommand *c) { i2cDriverPlan(c); }
void i2cCapacitanceInit() {}
void i2cCapacitanceReset() {}
void i2cCapacitanceSetCAPDAC(uint8_t) {}
bool i2cCapacitanceDone(int16_t &v) { v=cap_reading; return true; }
void i2cOneWireInit() {}
bool i2cOneWireReset(uint8_t e) { return core_present[e]; }
void i2cOneWireWrite(uint8_t,uint8_t) {}
uint8_t i2cOneWireRead(uint8_t) { return 0x12; }
void UsartSpiDriver::init(uint8_t) { assert(false); }
uint8_t UsartSpiDriver::transceive(uint8_t) { assert(false);return 0; }
void UsartSpiDriver::transceive(uint8_t*,uint8_t) { assert(false); }
void StepperTMC2130::init() { assert(false); }
uint32_t StepperTMC2130::readRegister(uint8_t,uint8_t) { assert(false);return 0; }
void StepperTMC2130::writeRegister(uint8_t,uint8_t,uint32_t) { assert(false); }
}
static uint8_t sequence=0;
static std::vector<uint8_t> packet_for(const std::string &line,uint8_t number) {
  assert(line.size()<=96);
  std::vector<uint8_t> packet={0xff,0xff,number,uint8_t(line.size())};
  packet.insert(packet.end(),line.begin(),line.end());
  const uint16_t crc=um3_stock::Crc16(packet.data()+2,packet.size()-2).getCrc();
  packet.push_back(crc>>8);packet.push_back(crc&255);
  return packet;
}
static void command(const std::string &line) {
  auto packet=packet_for(line,sequence);
  if(sequence==0 && line=="M115") {
    const uint8_t official[]={0xff,0xff,0,4,0x4d,0x31,0x31,0x35,0x72,0xcb};
    assert(packet==std::vector<uint8_t>(official,official+sizeof(official)));
  }
  rx.insert(rx.end(),packet.begin(),packet.end());
  um3_stock::loop();
  sequence=(sequence+1)%255;
}
int main(int argc,char **argv) {
  assert(argc==3);
  mock_board_adc=atoi(argv[1]);
  mock_pins[22]=mock_pins[26]=mock_pins[29]=1;
  um3_stock::setup();
  if(mock_board_adc==869) {
    assert(um3_stock::Board::getId()==um3_stock::Board::BOARD_UNKNOWN);
    assert(um3_stock::isStopped() && mock_pins[24]==0);
    um3_stock::Board::powerUp();assert(mock_pins[24]==0);
    puts("Unsupported S-line board refused without energizing relay");
    return 0;
  }
  assert(tx.find("Marlin started\n")!=std::string::npos);
  assert(um3_stock::Board::getId()==(mock_board_adc==447?um3_stock::Board::BOARD_V2_X:um3_stock::Board::BOARD_REV_I));
  command("M115");
  assert(tx.find(mock_board_adc==447?"MACHINE_TYPE:UM3 PCB_ID:2":"MACHINE_TYPE:UM3 PCB_ID:3")!=std::string::npos);
  assert(tx.find("\no000FF8\n")!=std::string::npos);
  assert(tx.find('\r')==std::string::npos); // Exact shipped LF framing.
  // Corrupt CRC and wrong sequence retain the expected sequence and request resend.
  auto broken=packet_for("M115",sequence);broken.back()^=1;
  size_t output_at=tx.size();rx.insert(rx.end(),broken.begin(),broken.end());um3_stock_read_commands();
  assert(tx.substr(output_at).find("\nr")!=std::string::npos);
  auto wrong=packet_for("M115",uint8_t((sequence+1)%255));
  output_at=tx.size();rx.insert(rx.end(),wrong.begin(),wrong.end());um3_stock_read_commands();
  assert(tx.substr(output_at).find("\nr")!=std::string::npos);
  auto fragmented=packet_for("M115",sequence);
  output_at=tx.size();
  for(uint8_t b:fragmented) { rx.push_back(b);um3_stock_read_commands(); }
  assert(tx.size()==output_at); // Acknowledge after execution, not receipt.
  um3_stock::loop();sequence=(sequence+1)%255;
  assert(tx.size()>output_at);
  // Eight packets may queue while a blocking move is executing; the ninth resends.
  for(uint8_t i=0;i<9;i++) {
    auto p=packet_for("M115",(sequence+i)%255);
    rx.insert(rx.end(),p.begin(),p.end());
  }
  output_at=tx.size();
  for(int i=0;i<9;i++)um3_stock_read_commands();
  assert(tx.substr(output_at).find("\nr")!=std::string::npos && rx.empty());
  for(int i=0;i<8;i++)um3_stock::loop();
  sequence=(sequence+8)%255;command("M115");
  for(int i=0;i<15;i++)moves.push_back(std::vector<int32_t>(4,0));
  command("M115");moves.clear();output_at=tx.size();um3_stock::loop();
  assert(tx.substr(output_at).find("\nq000F")!=std::string::npos);
  command("M12000 X215 Y215 Z310");
  command("M12001 X0 Y0 Z0");
  command("M92 X80 Y80 Z400 E369");
  command("M12004 X1 Y0 Z1 T0 E1");
  command("M12004 T1 E0");
  assert(um3_stock::max_pos[2]==310 && steps_per_mm[2]==400);
  command("M907 X1300 Z1300 E1250");
  assert(mock_pins[44]==165 && mock_pins[45]==165 && mock_pins[46]==159);
  command("M218 T1 X18 Z1.5");
  assert(offsets[0][1]==-1440 && offsets[2][1]==-600);
  command("T1");
  command("G92 X100 Y100 Z10 E0");
  assert(executed[0]==9440 && executed[2]==4600);
  command("M302 S0");
  command("G1 X110 Y100 Z12 E1 F1200");
  assert(moves.back()[0]==10240 && moves.back()[2]==5400);
  executed[0]=10000;executed[1]=8000;executed[2]=5000;executed[3]=369;
  command("M401");
  assert(moves.empty());
  assert(um3_stock::current_position[0]==107 && um3_stock::current_position[2]==11);
  output_at=tx.size();command("M114");
  assert(tx.substr(output_at).find("X:107.00")!=std::string::npos);
  command("M119");assert(tx.find("OPEN")!=std::string::npos);
  command("M142 w120");assert(mock_pins[8]==120);
  command("M42 P8 S64");assert(mock_pins[8]==64);
  command("M106 S128");assert(um3_stock::target_fan_speed==128 && mock_pins[7]==255);
  mock_delay(120);um3_stock::manage_inactivity();assert(mock_pins[7]==128);
  command("G1 X108 F1200");command("M106 S200");assert(mock_pins[7]==128);
  command("M400");assert(mock_pins[7]==200);
  command("M107");assert(um3_stock::target_fan_speed==0 && mock_pins[7]==0);
  command("M17");assert(mock_pins[27]==0 && mock_pins[48]==0);
  command("M18");assert(mock_pins[27]==1 && mock_pins[48]==1);
  command("M17");command("M84 X");assert(mock_pins[27]==1 && mock_pins[31]==0);
  command("M149 T0");assert(tx.find("SERIAL:121212121212\n")!=std::string::npos);
  command("M150 T1 P0");assert(tx.find("DATA: 0x1212")!=std::string::npos);
  command("M310 S2");assert(tx.find("1000 ")!=std::string::npos);
  command("M311 S5");assert(tx.find("Stddev^2: 0")!=std::string::npos || tx.find("Stddev^2:0")!=std::string::npos);
  command("M405 S0");
  command("M290 P221 I46");
  command("M291 R3.678255 A0.012285 V24");
  command("M292 T0 R18 V24 P75");
  command("M292 T1 R18 V24 P75");
  command("M301 T0 P20 I0 D0 F0 i60 R25 C0");
  command("M301 T1 P20 I0 D0 F0 i60 R25 C0");
  command("M304 P125 I0 D0 F2.5 i60 R25 C0");
  for(int i=0;i<500;i++) { mock_delay(1);um3_stock::manage_heater(); }
  assert(!um3_stock::isStopped() && watchdog_pets>0);
  const float nominal=um3_stock::current_temperature[1];
  command("M12005 T1 F0.985 O0.75");
  command("M12006 F1 O0");
  for(int i=0;i<80;i++) { mock_delay(1);um3_stock::manage_heater(); }
  assert(fabsf(um3_stock::current_temperature[1]-(nominal*0.985f+0.75f))<0.01f);
  command("M104 T0 S200"); // Removal permission initially forbids heating.
  assert(um3_stock::hotend_pid[0].getTargetTemperature()==0);
  command("M145 T0");command("M145 T1");
  command("M292 T0 P20");command("M292 T1 P20");
  command("M104 T0 S200");command("M104 T1 S200");command("M140 S100");command("M80");
  for(int i=0;i<100;i++) { mock_delay(1);um3_stock::manage_heater(); }
  assert(mock_pins[2]>0 && mock_pins[3]>0 && mock_pins[4]>0 && mock_pins[24]==1);
  output_at=tx.size();command("M105");
  const std::string temperatures=tx.substr(output_at);
  assert(temperatures.find(" T0:")!=std::string::npos && temperatures.find(" T1:")!=std::string::npos);
  assert(temperatures.find("/200.0@")!=std::string::npos && temperatures.find("p1e1f65535/0")!=std::string::npos);
  assert(temperatures.find(" B")!=std::string::npos && temperatures.find("/100.0@")!=std::string::npos);
  command("M301 T0 Pnan");
  for(int i=0;i<80;i++) { mock_delay(1);um3_stock::manage_heater(); }
  assert(mock_pins[2]==0);
  command("M301 T0 P20");
  for(int i=0;i<80;i++) { mock_delay(1);um3_stock::manage_heater(); }
  const uint8_t output_before=mock_pins[2];
  command("M42 P2 S17");assert(mock_pins[2]==output_before);
  command("M42 P4 S255");assert(mock_pins[4]!=255); // Cannot bypass the power budget.
  command("M144 T0");assert(!um3_stock::allow_temperature_sensor_errors[0]); // Hot target denies removal.
  assert(mock_pwm[2]>0 && mock_pwm[3]>0 && mock_pwm[4]>0);
  command("G1 X111 F1200");assert(!moves.empty());
  command("M998");assert(moves.empty() && um3_stock::isStopped() && mock_pins[2]==0 && mock_pins[3]==0 && mock_pins[4]==0 && mock_pins[24]==0);
  assert(mock_pwm[2]==0 && mock_pwm[3]==0 && mock_pwm[4]==0);
  command("M104 T0 S200");command("M80");
  for(int i=0;i<40;i++) { mock_delay(1);um3_stock::manage_heater(); }
  assert(mock_pins[2]==0 && mock_pins[24]==0 && um3_stock::hotend_pid[0].getTargetTemperature()==0);
  command("M999");assert(!um3_stock::isStopped());command("M104 T0 S0");command("M104 T1 S0");
  command("M144 T0");assert(um3_stock::allow_temperature_sensor_errors[0]);
  command("M145 T0");
  adc_failure=true;
  for(int i=0;i<1000;i++) { mock_delay(1);um3_stock::manage_heater(); }
  assert(um3_stock::getStoppedReason()==STOP_REASON_I2C_HEAD_COMM_ERROR);
  assert(mock_pins[2]==0 && mock_pins[3]==0 && mock_pins[4]==0);
  adc_failure=false;command("M999");
  command("M80");command("G28 X");assert(um3_stock::getStoppedReason()==STOP_REASON_XY_ENDSTOP_BROKEN_ERROR);
  command("M999");endstop_hit=true;mock_pins[22]=0;
  command("M80");command("G28 X");assert(um3_stock::getStoppedReason()==STOP_REASON_XY_ENDSTOP_STUCK_ERROR);
  command("M999");endstop_hit=false;
  command("M80");cap_reading=0;command("G30 F48 V0");
  assert(tx.find("CAPACITIVE_SENSOR_PROBLEM")!=std::string::npos && moves.empty());
  cap_reading=1000;
  // A full 96-byte command must include terminator storage and survive wraparound.
  command("M115"+std::string(92,' '));
  for(int i=0;i<260;i++) command("M115");
  adc_stall=true;
  for(int i=0;i<300;i++) { mock_delay(1);um3_stock::manage_heater(); }
  const uint32_t pets=watchdog_pets;
  for(int i=0;i<300;i++) { mock_delay(1);um3_stock::manage_heater(); }
  assert(watchdog_pets==pets); // A wedged bus cannot keep the watchdog alive.
  clock_ms=UINT32_MAX-50;
  um3_stock_apply_cooling_fan(0);um3_stock_apply_cooling_fan(128);assert(mock_pins[7]==255);
  clock_ms+=120;um3_stock_apply_cooling_fan(128);assert(mock_pins[7]==128);
  FILE *trace=fopen(argv[2],"wb");assert(trace);
  assert(fwrite(tx.data(),1,tx.size(),trace)==tx.size());fclose(trace);
  puts("UM3 production protocol, calibration, motion adapters, core commands, heating/interlocks, recovery and communication failure passed");
}
