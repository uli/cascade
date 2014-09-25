/*
 * cpu.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _CPU_H
#define _CPU_H

#include <stdint.h>
#include "debug.h"
#include "state.h"
#include "ring.h"

#ifdef LATENCY
#include <sys/time.h>
#endif

#define CPU_CMD_NONE	0
#define CPU_CMD_EXIT 1
#define CPU_CMD_TOGGLE_ECHO 2
#define CPU_CMD_RATE 3
#define CPU_CMD_SAVE 4
#define CPU_CMD_LOAD 5
#define CPU_CMD_RESET 6
#define CPU_CMD_FRESET 7
#define CPU_CMD_RECORD 8
#define CPU_CMD_PLAY 9
#define CPU_CMD_STOP_RECPLAY 10
#define CPU_CMD_LOAD_ROM 11

/* documented in 272238 C-52 */
#define PSW_ST (1<<0)
#define PSW_INTE (1<<1)
#define PSW_PTSE (1<<2)
#define PSW_C (1<<3)
#define PSW_VT (1<<4)
#define PSW_V (1<<5)
#define PSW_N (1<<6)
#define PSW_Z (1<<7)

#define EVENT_INVALID 0
#define EVENT_KEYDOWN 1
#define EVENT_KEYUP 2
#define EVENT_SERIALRX 3
#define EVENT_SERIALRXBIT 4
#define EVENT_SERIALSTAT 5
#define EVENT_EEPROMREAD 6

// recorded event structure
struct Event {
  uint64_t cycles;
  int type;
  int value;
};

extern uint32_t debug_level;
extern uint32_t debug_level_unabridged;

class Serial;
class Keypad;
class Lcd;
class Eeprom;
class Interface;
class UI;
class Hsio;
class Hints;

class Cpu {
friend class Keypad;
public:
  Cpu(UI *ui);
  ~Cpu();
  
  void reset();
  
  void setRomSize(size_t size);
  void setMappedRamSize(size_t size);
  bool loadRom(const char* name);
  bool loadExtendedRom(const char *name);
  
  void enableRecording(const char *rname);
  void disableRecording();
  void enableReplaying(const char *rname);
  
  void setSerial(Interface *iface, bool expect_echo);
  
  void setWatchpoint(uint16_t lo, uint16_t hi);
  void setMaximumCycles(uint64_t max);
  void setDebugTrigger(uint32_t trigger, uint32_t level);
  
  int emulate(void);
  void dumpMem();

  void setSlowDown(float factor);

  void recordEvent(int type, int value);
  struct Event retrieveEvent(int type);
  int retrieveEventValue(int type);

  inline uint64_t getCycles() {
    return cycles;
  }
  inline bool isReplaying() {
    return replaying;
  }
  inline uint32_t getClock() {
    return oclock;
  }

  bool is_hiscan;

  int serDivToBaud(uint16_t divisor) {
    return getClock() / 2 / (divisor & 0x7fff) / 8;
  }
  uint16_t serBaudToDiv(int baud) {
    return (getClock() / 2 / baud / 8) | 0x8000;
  }

  void sync(bool exact);

  void sendCommand(int cmd);

  Lcd *getLcd() {
    return lcd;
  }

  void stop() {
    emulation_stopped = true;
  }
  void resume() {
    emulation_stopped = false;
  }

protected:
  inline uint32_t virtToPhys(uint16_t addr, int fetch) {
    if (addr < 0xc000)
      return addr;
    else
      return virtToPhysSlow(addr, fetch);
  }

  virtual uint8_t ioRead8(uint16_t addr);
  virtual void ioWrite8(uint16_t addr, uint8_t value);

  uint8_t mappedRead8(uint16_t addr, int fetch);
  uint8_t memRead8Bus(uint16_t addr, int fetch);

  typedef uint8_t (Cpu::*memReader)(uint16_t addr);
  inline uint8_t memRead8(uint16_t addr) {
    switch (addr) {
      case 0 ... 1: return 0;
      case 0x18 ... 0xff:
      case 0x2000 ... 0xbfff:
        return memRead8Ram(addr);
      case 0xc000 ... 0xffff:
        return memRead8Mapped(addr);
      default:
        return memRead8Slow(addr);
    }
  }
  
  uint8_t memRead8Ram(uint16_t addr) {
#ifndef NDEBUG
  if (watchpoint_lo && watchpoint_lo <= addr && watchpoint_hi >= addr)
    DEBUG(WARN, "%04X/%08X: WATCH READ %04X: %02X\n", opc, virtToPhys(opc, 1), addr, ram[addr]);
#endif
    return ram[addr];
  }
  uint8_t memRead8Null(uint16_t addr) {
    return 0;
  }
  uint8_t memRead8Slow(uint16_t addr) {
    return memRead8Bus(addr, 0);
  }
  uint8_t memRead8Mapped(uint16_t addr) {
    return data_ptr[addr - 0xc000];
  }
  
  inline uint16_t memRead16(uint16_t addr) {
    return memRead8(addr) | (memRead8(addr + 1) << 8);
  }
  inline uint32_t memRead32(uint16_t addr) {
    return memRead16(addr) | (memRead16(addr + 2) << 16);
  } 
  
  inline uint8_t fetch(void) {
    if (pc >= 0xc000)
      return code_ptr[pc++ - 0xc000];
    else
      return memRead8Bus(pc++, 1);
  }
  
  inline uint8_t peek(int offset) {
    return memRead8Bus(pc + offset, 1);
  }
  inline uint16_t fetch16(void) {
    return (uint16_t)fetch() | ((uint16_t)fetch() << 8);
  }
  inline uint16_t peek16(int offset) {
    return (uint16_t)peek(offset) | ((uint16_t)peek(offset + 1) << 8);
  }
  
  typedef void (Cpu::*memWriter)(uint16_t addr, uint8_t value);
  inline void memWrite8(uint16_t addr, uint8_t value) {
    switch (addr) {
      case 0 ... 0x17:
      case 0x200 ... 0x2ff:
        ioWrite8(addr, value);
        break;
      case 0x18 ... 0xff:
      case 0x2000 ... 0xbfff:
        memWrite8Ram(addr, value);
        break;
      case 0xc000 ... 0xffff:
        memWrite8Mapped(addr, value);
        break;
      default:
        memWrite8Slow(addr, value);
        break;
    }
  }
  void memWrite8Slow(uint16_t addr, uint8_t value);
  void memWrite8Mapped(uint16_t addr, uint8_t value) {
    DEBUG(MEM, "WRITE %02X -> %04X\n", value, addr);
    data_ptr[addr - 0xc000] = value;
  }
  void memWrite8Ram(uint16_t addr, uint8_t value) {
    DEBUG(MEM, "WRITE %02X -> %04X\n", value, addr);
#ifndef NDEBUG
    if (watchpoint_lo && addr >= watchpoint_lo && addr <= watchpoint_hi)
      DEBUG(WARN, "%04X/%08X: WATCH %04X: %02X -> %02X\n", opc, virtToPhys(opc, 1), addr, memRead8(addr), value);
#endif
    ram[addr] = value;
  }
  
  inline void memWrite16(uint16_t addr, uint16_t value) {
    memWrite8(addr, value & 0xff);
    memWrite8(addr + 1, value >> 8);
  }

  inline void memWrite32(uint16_t addr, uint32_t value) {
    memWrite16(addr, value & 0xffff);
    memWrite16(addr + 2, value >> 16);
  }

  inline void push16(uint16_t word) {
    uint16_t new_sp = memRead16(0x18) - 2;
    memWrite16(0x18, new_sp);
    memWrite16(new_sp, word);
  }

  inline uint16_t pop16(void) {
    uint16_t sp = memRead16(0x18);
    uint16_t val = memRead16(sp);
    memWrite16(0x18, sp + 2);
    return val;
  }

  inline uint16_t indexedAddr(void) {
    uint8_t imm8 = fetch();
    uint16_t target;
    if (imm8 & 1) {
      target = fetch16() + memRead16(imm8 & 0xfe);
      cycle(1);
    }
    else {
      target = fetch() + memRead16(imm8);
    }
    return target;
  }

  uint32_t virtToPhysSlow(uint16_t addr, int fetch);
  
  inline void cycle(int c) {
    cycles += c;
  }
  inline void cycleShift(int c, int shift) {
    cycles += c + (shift? shift : 1);
  }
  inline void cycleRM3(int c, uint16_t addr) {
    cycles += c;
    if (addr >= 0x200)
      cycles += 3;
  }
  inline void cycleRM2(int c, uint16_t addr) {
    cycles += c;
    if (addr >= 0x200)
      cycles += 2;
  }
  
  void resetTiming();

  const char *disassemble();
  
private:
  inline uint16_t getTimer1() {
    return (uint16_t)(getCycles() / 8) + timer1_offset;
  }
  
  bool loadSaveState(const char *name, bool write);
  bool loadSaveState(statefile_t fp, bool write);
  
  uint32_t clock, oclock;
  
#ifndef NDEBUG
  uint16_t watchpoint_lo, watchpoint_hi;
  uint32_t trigger;
  uint32_t trigger_level;
  uint8_t *mem_profile;
#endif

  const uint8_t *rom;
  uint8_t *ram;
  const uint8_t *exrom;
#ifdef LATENCY
  struct latency_t {
    uint16_t min;
    uint16_t max;
    uint32_t total;
    uint32_t count;
    uint8_t cycles;
    uint8_t opcode;
    uint8_t _reserved[2];
  };
  struct latency_t *mem_latency;
  struct timeval tv, tv2;
  bool do_latency;
#endif
  uint8_t *mapped_ram;
  const uint8_t *code_ptr;
  uint8_t *data_ptr;
  uint16_t pc;
  uint16_t opc; /* PC at start of insn */
  uint8_t psw;
  uint8_t code_hi, code_lo;
  uint8_t data_hi, data_lo;
  uint8_t wsr;
  uint8_t wsr1;
  uint8_t int_mask, int_mask1;
  uint16_t ptssel, ptssrv;
  uint8_t ad_command;

  uint64_t cycles;
  uint64_t end_cycles;
  
  uint8_t ioc0, ioc1, ios0, ios1;
  uint16_t last_ios1_read;
  Lcd *lcd;
  Serial *serial;
  uint8_t ioport1, ioport2;
  uint16_t ad_result;
  uint16_t timer1_offset;
  uint32_t timer2;
  int32_t timer2_inc_factor;

  Eeprom* eeprom;
  
  uint32_t starttime;
  uint32_t oldtime;
  uint32_t nowtime;
  uint64_t oldcycles;
  uint64_t next_sampling;
  uint64_t next_lcd_update;
  uint64_t next_event_pumping;
  
  float slowdown;
  
  Hsio *hsi;
  
  Keypad *keypad;
  
  UI *ui;
  Hints *hints;
  
  // event recording/replaying
  bool recording, replaying;
  char *record_file_name;
  statefile_t record_file;
  struct Event current_event;
  
  uint32_t rom_size;
  uint32_t exrom_size;
  char *rom_name;
  char *exrom_name;

  Ring<int> *cmd_queue;
  bool emulation_stopped;
};

#endif
