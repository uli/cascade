/*
 * serial.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef __SERIAL_H
#define __SERIAL_H

#include <stdint.h>
#include "interface.h"
#include "ui.h"
#include "ring.h"

#define SERCON_MODE_MASK (3)
#define SERCON_MODE_SYNC 0
#define SERCON_MODE_ASYNC_8 1

#define SERCON_PEN (1 << 2)
#define SERCON_REN (1 << 3)
#define SERCON_TB8 (1 << 4)

#define SERSTAT_OE (1 << 2)
#define SERSTAT_TXE (1 << 3)
#define SERSTAT_FE (1 << 4)
#define SERSTAT_TI (1 << 5)
#define SERSTAT_RI (1 << 6)
#define SERSTAT_RPERB8 (1 << 7)

typedef enum {
  SI_NORMAL,
  SI_ISO_SLOW_INIT,
  SI_SPAM,
} slow_init_state_t;

#define NUM_FORCED_BAUD_RATES 3
static const int forced_baud_rates[] = {
  0x8081 /* 9600 */,
  0x8077 /* 10400 */,
  0x8102 /* 4800 */,
};

typedef enum {
  BR_AUTO,
  BR_AUTOPLUS,
  BR_FORCE,
  NUM_BAUDRATE_METHODS
} baudrate_method_t;

class Cpu;
class Interface;
class UI;
class Hints;

class Serial {
public:
  void setBaudrate(uint8_t divisor);
  uint16_t getBaudDivisor(void);
  void setControl(uint8_t val);
  void setStat(uint8_t val);

  void setEcho(bool echo);
  bool getEcho();
  
  uint8_t readControl(void);
  uint8_t readStat(void);
  uint8_t readData(void);
  void writeData(uint8_t data);

  void slowInit(uint8_t bit);
  void setL(uint8_t bit);

  void addRxData(const int *data);
  void addRxData(uint8_t byte);
  void prependRxData(uint8_t byte);
  bool rxBufEmpty();
  
  int getRxState(void);
  
  int snoopByte(void);
  void skipByte(void);
  
  Serial(Cpu *cpu, Interface *iface, UI *ui, Hints *hints);
  ~Serial();

  void flushRxBuf();
  
  void setBaudrateMethod(baudrate_method_t n);
  void setFixedBaudrate(int baudrate);
  
  void setCommLine(int line);

  void loadSaveState(statefile_t fp, bool write);
  
  void reset();
  
protected:
  int retrieveRxData();
  
private:
  void checkInput();
  
  uint16_t baudrate;			/* actual baudrate used */
  uint16_t specified_baudrate;		/* baudrate requested */
  baudrate_method_t baudrate_method;
  int fixed_baudrate;			/* used with BR_FORCE */
  int comm_line;
  uint8_t control;
  uint8_t stat;
  int baudrate_pos;
  uint8_t slow_init_target;
  int slow_init_count;
  Interface *iface;
  slow_init_state_t si_state;
  
  uint64_t ti_set_time;		/* time at which the TI flag should be set */
  uint64_t ri_set_time;
  bool read_after_write;	/* has a byte been read since the last write? */
  bool enable_echo;		/* is artificial echo enabled? */
  uint64_t echo_back_on_time;	/* disable echo setting in setControl() until this time */
  uint8_t ti_set_byte;		/* last byte written */
  
  /* speed detection workaround, see slowInit() */
  bool force_baud_rate;
  int bitbang_reads_after_slow_init;
  int forced_baud_rate_no;
  uint64_t last_slow_init;
  
  Cpu *cpu;
  UI *ui;

  // serial input via bitbanging (used to detect baudrate, we have to fake it)
  bool serial_bitbang_enabled;          // bitbanging serial input enabled
  int serial_bitbang_bit_pos;           // bit position
  uint64_t serial_bitbang_last;         // last cycle time a bit was sent
  uint8_t serial_bitbang_last_bit_sent; // last bit transmitted; we have to
                                        // remember it so we can send it again
                                        // if insufficient time has passed
                                        // since the last read
  Ring<uint8_t> *rx_buf;
  Hints *hints;
};

#endif

