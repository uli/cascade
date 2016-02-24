/*
 * cpu.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "cpu.h"
#include "debug.h"
#include "os.h"
#include "eeprom.h"
#include "ui.h"
#include "lcd.h"
#include "keypad.h"
#include "hsio.h"
#include "hints.h"
#include <string.h>

Cpu::Cpu(UI *ui)
{
  end_cycles = (uint64_t)-1LL;
  
  recording = replaying = false;
  record_file = NULL;
  record_file_name = NULL;
  
  this->ui = ui;
  keypad = new Keypad(this, ui);
  lcd = new Lcd(ui);
  eeprom = new Eeprom(this, ui);
  hsi = new Hsio();
  this->serial = NULL;

  rom = NULL;
  rom_name = NULL;
  rom_size = 0;
  exrom = NULL;
  exrom_name = NULL;
  exrom_size = 0;
  ram = new uint8_t[0xc000];
  memset(ram, 0, 0xc000);
#ifndef NDEBUG
  mem_profile = NULL;
#endif
#ifdef LATENCY
  mem_latency = NULL;
  do_latency = false;
#endif
  record_file = NULL;
  mapped_ram = NULL;
  
  current_event.cycles = 0;
  current_event.type = EVENT_INVALID;
  current_event.value = 0;
  
  hints = new Hints(this, ui);
  
  cmd_queue = new Ring<int>(10);

  emulation_stopped = true;

  reset();
}

void Cpu::reset()
{
  pc = 0x2080;
  psw = 0;
  cycles = 0;
  int_mask = 0;
  int_mask1 = 0;
  ios0 = ios1 = ioc0 = ioc1 = 0;
  last_ios1_read = 0;
  
  clock = oclock = 20000000;
  slowdown = 1;

  timer1_offset = 0;
  timer2_inc_factor = 1;
  timer2 = 0;
  
#ifndef NDEBUG
  watchpoint_lo = 0;
  trigger = 0;
#endif
  
  ad_command = 0;
  ad_result = 0xff00;
  
  code_lo = 0;
  code_hi = 0;
  data_lo = 0;
  data_hi = 0;
  
  code_ptr = rom;
  data_ptr = (uint8_t *)rom;
  wsr = 0;
  wsr1 = 0;

  ioport1 = ioport2 = 0;
  
  ptssrv = 0;
  ptssel = 0;

  oldcycles = cycles;
  starttime = oldtime = os_mtime();
  nowtime = oldtime;
  next_sampling = 0;
  next_lcd_update = 0;
  next_event_pumping = 0;
  
  lcd->reset();
  if (serial)
    serial->reset();
}

Cpu::~Cpu()
{
  delete hsi;
  delete eeprom;
  delete lcd;
  delete keypad;
  if (rom)
    free((uint8_t *)rom);
  if (rom_name)
    free(rom_name);
  if (exrom)
    free((uint8_t *)exrom);
  if (exrom_name)
    free(exrom_name);
  delete ram;
#ifndef NDEBUG
  if (mem_profile)
    free(mem_profile);
#endif
#ifdef LATENCY
  if (mem_latency)
    free(mem_latency);
#endif
  if (serial)
    delete serial;
  if (hints)
    delete hints;
  if (record_file)
    state_close(record_file);
  if (record_file_name)
    free(record_file_name);
  if (mapped_ram)
    free(mapped_ram);
  delete cmd_queue;
}

void Cpu::setRomSize(size_t size)
{
  if (rom)
    free((uint8_t *)rom);
  
  rom = (uint8_t *)malloc(size);
#ifndef NDEBUG
  if (mem_profile)
    free(mem_profile);
  mem_profile = (uint8_t *)calloc(1, size);
#endif
#ifdef LATENCY
  if (mem_latency)
    free(mem_latency);
  mem_latency = (struct latency_t *)calloc(sizeof(struct latency_t), size);
  for (uint32_t i = 0; i < size; i++)
    mem_latency[i].min = 0xffff;
#endif
  rom_size = size;
  code_ptr = rom;
  data_ptr = (uint8_t *)rom;	/* data_ptr can't be const, might point to RAM */
}

void Cpu::setMappedRamSize(size_t size)
{
  if (mapped_ram)
    free(mapped_ram);
  mapped_ram = (uint8_t *)calloc(1, size);
}

void Cpu::enableRecording(const char *rname)
{
  disableRecording();
  record_file_name = strdup(rname);

  record_file = state_open(rname, "wb");

  if (!record_file) {
    ERROR("could not open %s for writing\n", rname);
    ui->fatalError("Could not open %s for writing.", NULL, rname);
    return;
  }
  if (!loadSaveState(record_file, true)) {
    recording = true;
    ui->setLED(LED_REC, true);
  }
}

void Cpu::disableRecording()
{
  recording = replaying = false;
  ui->setLED(LED_REC, false);
  ui->setLED(LED_PLAY, false);
  if (record_file_name) {
    free(record_file_name);
    record_file_name = NULL;
  }
  if (record_file) {
    state_close(record_file);
    record_file = NULL;
  }
}

void Cpu::enableReplaying(const char *rname)
{
  disableRecording();
  /* keep current name if rname == NULL
     used by state loading, which changes record_file_name directly */
  if (rname) {
    record_file_name = strdup(rname);
  }

  record_file = state_open(record_file_name, "rb");
  if (!record_file) {
    ERROR("could not open %s for reading\n", record_file_name);
    exit(1);
  }
  if (!loadSaveState(record_file, false)) {
    replaying = true;
    ui->setLED(LED_PLAY, true);
  }
}

void Cpu::setSerial(Interface *iface, bool expect_echo)
{
  serial = new Serial(this, iface, ui, hints);
  hints->setSerial(serial);
  ui->setSerial(serial);
  serial->setEcho(expect_echo);
}

#ifndef NDEBUG
void Cpu::setWatchpoint(uint16_t lo, uint16_t hi)
{
  watchpoint_lo = lo;
  watchpoint_hi = hi;
}
#endif

void Cpu::setMaximumCycles(uint64_t max)
{
  end_cycles = max;
}

#ifndef NDEBUG
void Cpu::setDebugTrigger(uint32_t trigger, uint32_t level)
{
  this->trigger = trigger;
  trigger_level = level;
}
#endif

uint32_t Cpu::virtToPhysSlow(uint16_t addr, int fetch)
{
  uint8_t map_lo, map_hi;  
  if (fetch) {
    map_lo = code_lo;
    map_hi = code_hi;
  }
  else {
    map_lo = data_lo;
    map_hi = data_hi;
  }
  
  if (map_hi == 9)
    return 0xc000 + (map_lo - 6) * 0x4000 + (addr - 0xc000);
  else if (map_hi == 0)
    return /* 0x10000 + */ map_lo * 0x4000 + (addr - 0xc000);
  else if (map_hi >= 1 && map_hi <= 6)
    return 0x400000 * map_hi + map_lo * 0x4000 + (addr - 0xc000);
  else if (map_hi == 8 && map_lo <= 0x1f) { // (map_lo == 0x1e || map_lo == 0x1f || map_lo == 3 || map_lo == 5 || map_lo == 4)) {
    return 0xcaf00000UL + map_lo * 0x4000 + (addr - 0xc000);
  }
  else if (map_hi == 0x10  ) {
    if (exrom)
      return 0xbab00000UL + map_lo * 0x4000 + (addr - 0xc000);
    else
      return map_lo * 0x4000 + (addr - 0xc000);
  }
  else if (map_hi == 7 || map_hi == 0x1f || map_hi == 0x1e) {
    return 0xcaf00000UL + (map_lo % 0x20) * 0x4000 + (addr - 0xc000);
  }
  else {
    char buf[256];
    sprintf(buf, "Unimplemented memory mapping. (%02X/%02X)\nThe system will reset now.", map_hi, map_lo);
    ERROR(buf);
#ifndef NDEBUG
    dumpMem();
    exit(1);
#else
    ui->fatalError(buf);
    reset();
#endif
    return 0;
  }
}

uint8_t Cpu::mappedRead8(uint16_t addr, int fetch)
{
  uint32_t effective_addr = virtToPhys(addr, fetch);
  if (!fetch) {
    DEBUG(MEM, "MAPPED READ bank %02X/%02X virtual %04X real %08X\n", data_lo, data_hi, addr, effective_addr);
  }
  if (effective_addr >= 0xcaf00000UL)
    return mapped_ram[effective_addr - 0xcaf00000UL];
  else if (effective_addr >= 0xbab00000UL)
    return exrom[effective_addr];
  else
    return rom[effective_addr];
}

uint8_t Cpu::memRead8Bus(uint16_t addr, int fetch)
{
  uint8_t ret;
  if (addr >= 0xc000)
    ret = mappedRead8(addr, fetch);
  else if (addr < 0x18 ||
#ifndef NDEBUG
      (addr >= 0x1f00 && addr < 0x2000) ||
      (addr >= 0x2014 && addr < 0x2030) ||
      (addr >= 0x205e && addr < 0x2080) ||
#endif
      (addr >= 0x200 && addr < 0x2ff))
    ret = ioRead8(addr);
  else
    ret = ram[addr];

#ifndef NDEBUG
  if (!fetch) DEBUG(MEM, "READ %02X <- %04X\n", ret, addr);
  if (watchpoint_lo && watchpoint_lo <= addr && watchpoint_hi >= addr)
    DEBUG(WARN, "%04X/%08X: WATCH READ %04X: %02X\n", opc, virtToPhys(opc, 1), addr, ret);
#endif

  return ret;
}

void Cpu::memWrite8Slow(uint16_t addr, uint8_t value)
{
  DEBUG(MEM, "WRITE %02X -> %04X\n", value, addr);

#ifndef NDEBUG
  if (watchpoint_lo && addr >= watchpoint_lo && addr <= watchpoint_hi)
    DEBUG(WARN, "%04X/%08X: WATCH %04X: %02X -> %02X\n", opc, virtToPhys(opc, 1), addr, memRead8(addr), value);
#endif
    
  uint32_t effective_addr = virtToPhys(addr, 0);
  if (effective_addr >= 0xcaf00000UL) {
    DEBUG(MEM, "RAM write(!) of %02X to %04X not ignored\n", value, addr);
    mapped_ram[effective_addr - 0xcaf00000UL] = value;
  }
  else
    ram[effective_addr] = value;
}


void Cpu::dumpMem()
{
  FILE *fp = fopen("dump", "w");
  if (!fp) {
    perror("dump");
    exit(1);
  }
  fwrite(ram, 0xc000, 1, fp);
  fclose(fp);
  fp = fopen("dump.ram", "w");
  fwrite(mapped_ram, 524288, 1, fp);
  fclose(fp);
#ifdef LATENCY
  fp = fopen("dump.lat", "w");
  fwrite(mem_latency, mem_size, sizeof(struct latency_t), fp);
  fclose(fp);
#endif
}

void Cpu::resetTiming()
{
  /* set the start time used for timing calculations (oldtime) in such a
     way that it seems like we're right on time */
  oldtime = nowtime - cycles * 2 * 1000 / clock;
}

void Cpu::setSlowDown(float factor)
{
  if (factor != slowdown) {
    slowdown = factor;
    clock = oclock / factor;
    resetTiming();
    DEBUG(WARN, "slowing down factor %f, now clocking at %llu Hz\n", factor, (unsigned long long)clock);
  }
}

#ifndef NDEBUG
static char buf[80];
const char *Cpu::disassemble()
{
  uint8_t opcode = peek(0);

#define OPUNIMP(x) sprintf(buf, x " undecoded");

#define OP0(x) sprintf(buf, x);
#define OP1(x) sprintf(buf, x " %02Xh", peek(1));
#define OP1IM(x) sprintf(buf, x " #%04Xh", peek16(1));
#define OP1IN(x) sprintf(buf, x " [%02Xh]%s", peek(1) & 0xfe, (peek(1) & 1) ? "+" : "");
#define OP1IX(x) \
  if (peek(1) & 1) \
    sprintf(buf, x " %04Xh[%02Xh]", peek16(2), peek(1) & 0xfe); \
  else \
    sprintf(buf, x " %02Xh[%02Xh]", peek(2), peek(1));

#define OP2D(x) sprintf(buf, x " %02Xh, %02Xh", peek(2), peek(1));

#define OP2IM(x, byte) \
  if (byte) \
    sprintf(buf, x " %02Xh, #%02Xh", peek(2), peek(1)); \
  else \
    sprintf(buf, x " %02Xh, #%04Xh", peek(3), peek16(1)); \

#define OP2IX(x) \
  if (peek(1) & 1) \
    sprintf(buf, x " %02Xh, %04Xh[%02Xh]", peek(4), peek16(2), peek(1) & 0xfe); \
  else \
    sprintf(buf, x " %02Xh, %02Xh[%02Xh]", peek(3), peek(2), peek(1));

#define OP2SH(x) \
  if (peek(1) > 15) \
    sprintf(buf, x " %02Xh, #%02Xh", peek(2), peek(1)); \
  else \
    sprintf(buf, x " %02Xh, %02Xh", peek(2), peek(1));

#define OPJ8(x) sprintf(buf, x " %04Xh (%02Xh)", opc + (int8_t)peek(1) + 2, peek(1));
#define OPJ16(x) sprintf(buf, x " %04Xh (%04Xh)", (uint16_t)(opc + (int16_t)peek16(1) + 3), peek16(1));
#define OPDJ8(x) sprintf(buf, x " %02Xh, %04Xh (%02Xh)", peek(1), opc + (int8_t)peek(2) + 3, peek(2));

#define OPJ11(x) \
        sprintf(buf, x " %04Xh", opc + (((int16_t)((peek(1) | ((peek(0) & 0x7) << 8)) << 5)) >> 5) + 2);

#define OPJBIT(x) \
        sprintf(buf, x " %02Xh, %u, %04Xh", peek(1), opcode & 7, opc + (int8_t)peek(2) + 3);

#define OP3E(x, byte) \
  switch (peek(0) & 3) { \
    case 0: \
      sprintf(buf, x " %02Xh, %02Xh, %02Xh", peek(3), peek(2), peek(1)); \
      break; \
    case 1: \
      if (byte) \
        sprintf(buf, x " %02Xh, %02Xh, #%02Xh", peek(3), peek(2), peek(1)); \
      else \
        sprintf(buf, x " %02Xh, %02Xh, #%04Xh", peek(4), peek(3), peek16(1)); \
      break; \
    case 2: \
      sprintf(buf, x " %02Xh, %02Xh, [%02Xh]%s", peek(3), peek(2), peek(1) & 0xfe, (peek(1) & 1) ? "+" : ""); \
      break; \
    case 3: \
      if (peek(1) & 1) \
        sprintf(buf, x " %02Xh, %02Xh, %04Xh[%02Xh]", peek(5), peek(4), peek16(2), peek(1) & 0xfe); \
      else \
        sprintf(buf, x " %02Xh, %02Xh, %02Xh[%02Xh]", peek(4), peek(3), peek(2), peek(1)); \
      break; \
  }
#define OP3(x) OP3E(x, 0)
#define OP3B(x) OP3E(x, 1)

#define OP2E(x, byte) \
  switch (peek(0) & 3) { \
    case 0: \
      OP2D(x); \
      break; \
    case 1: \
      OP2IM(x, byte); \
      break; \
    case 2: \
      sprintf(buf, x " %02Xh, [%02Xh]%s", peek(2), peek(1) & 0xfe, (peek(1) & 1) ? "+" : ""); \
      break; \
    case 3: \
      OP2IX(x); \
      break; \
  }

#define OP2(x) OP2E(x, 0)
#define OP2B(x) OP2E(x, 1)

  switch (opcode) {
    case 0x00: OP1("SKIP"); break;
    case 0x01: OP1("CLR"); break;
    case 0x02: OP1("NOT"); break;
    case 0x03: OP1("NEG"); break;
    case 0x04: OP2D("XCH"); break;
    case 0x05: OP1("DEC"); break;
    case 0x06: OP1("EXT"); break;
    case 0x07: OP1("INC"); break;
    case 0x08: OP2SH("SHR"); break;
    case 0x09: OP2SH("SHL"); break;
    case 0x0a: OP2SH("SHRA"); break;
    case 0x0b: OP2IX("XCH"); break;
    case 0x0c: OP2SH("SHRL"); break;
    case 0x0d: OP2SH("SHLL"); break;
    case 0x0e: OP2SH("SHRAL"); break;
    case 0x0f: OP2SH("NORML"); break;
    case 0x10: OP0("RESERVED"); break;
    case 0x11: OP1("CLRB"); break;
    case 0x12: OP1("NOTB"); break;
    case 0x13: OP1("NEGB"); break;
    case 0x14: OP2D("XCHB"); break;
    case 0x15: OP1("DECB"); break;
    case 0x16: OP1("EXTB"); break;
    case 0x17: OP1("INCB"); break;
    case 0x18: OP2SH("SHRB"); break;
    case 0x19: OP2SH("SHLB"); break;
    case 0x1a: OP2SH("SHRAB"); break;
    case 0x1b: OP2IX("XCHB"); break;
 /*   case 0x1c ... 0x1f: OP0("RESERVED"); break;
    case 0x20 ... 0x27: OPJ11("SJMP"); break;
    case 0x28 ... 0x2f: OPJ11("SCALL"); break;
    case 0x30 ... 0x37: OPJBIT("JBC"); break;
    case 0x38 ... 0x3f: OPJBIT("JBS"); break;
    case 0x40 ... 0x43: OP3("AND"); break;
    case 0x44 ... 0x47: OP3("ADD"); break;
    case 0x48 ... 0x4b: OP3("SUB"); break;
    case 0x4c ... 0x4f: OP3("MULU"); break;
    case 0x50 ... 0x53: OP3B("ANDB"); break;
    case 0x54 ... 0x57: OP3B("ADDB"); break;
    case 0x58 ... 0x5b: OP3B("SUBB"); break;
    case 0x5c ... 0x5f: OP3B("MULUB"); break;
    case 0x60 ... 0x63: OP2("AND"); break;
    case 0x64 ... 0x67: OP2("ADD"); break;
    case 0x68 ... 0x6b: OP2("SUB"); break;
    case 0x6c ... 0x6f: OP2("MULU"); break;
    case 0x70 ... 0x73: OP2B("ANDB"); break;
    case 0x74 ... 0x77: OP2B("ADDB"); break;
    case 0x78 ... 0x7b: OP2B("SUBB"); break;
    case 0x7c ... 0x7f: OP2B("MULUB"); break;
    case 0x80 ... 0x83: OP2("OR"); break;
    case 0x84 ... 0x87: OP2("XOR"); break;
    case 0x88 ... 0x8b: OP2("CMP"); break;
    case 0x8c ... 0x8f: OP2("DIVU"); break;
    case 0x90 ... 0x93: OP2B("ORB"); break;
    case 0x94 ... 0x97: OP2B("XORB"); break;
    case 0x98 ... 0x9b: OP2B("CMPB"); break;
    case 0x9c ... 0x9f: OP2B("DIVUB"); break;
    case 0xa0 ... 0xa3: OP2("LD"); break;
    case 0xa4 ... 0xa7: OP2("ADDC"); break;
    case 0xa8 ... 0xab: OP2("SUBC"); break;
    case 0xac ... 0xaf: OP2("LDBZE"); break;
    case 0xb0 ... 0xb3: OP2("LDB"); break;
    case 0xb4 ... 0xb7: OP2("ADDCB"); break;
    case 0xb8 ... 0xbb: OP2("SUBCB"); break;
    case 0xbc ... 0xbf: OP2("LDBSE"); break;*/
    case 0xc0: OP2("ST"); break;
    case 0xc1: OP2D("BMOV"); break;
    case 0xc2: OP2("ST"); break;
    case 0xc3: OP2("ST"); break;
    case 0xc4: OP2B("STB"); break;
    case 0xc5: OP2D("CMPL"); break;
    case 0xc6: OP2B("STB"); break;
    case 0xc7: OP2B("STB"); break;
    case 0xc8: OP1("PUSH"); break;
    case 0xc9: OP1IM("PUSH"); break;
    case 0xca: OP1IN("PUSH"); break;
    case 0xcb: OP1IX("PUSH"); break;
    case 0xcc: OP1("POP"); break;
    case 0xcd: OP2D("BMOVI"); break;
    case 0xce: OP1IN("POP"); break;
    case 0xcf: OP1IX("POP"); break;
    case 0xd0: OPJ8("JNST"); break;
    case 0xd1: OPJ8("JNH"); break;
    case 0xd2: OPJ8("JGT"); break;
    case 0xd3: OPJ8("JNC"); break;
    case 0xd4: OPJ8("JNVT"); break;
    case 0xd5: OPJ8("JNV"); break;
    case 0xd6: OPJ8("JGE"); break;
    case 0xd7: OPJ8("JNE"); break;
    case 0xd8: OPJ8("JST"); break;
    case 0xd9: OPJ8("JH"); break;
    case 0xda: OPJ8("JLE"); break;
    case 0xdb: OPJ8("JC"); break;
    case 0xdc: OPJ8("JVT"); break;
    case 0xdd: OPJ8("JV"); break;
    case 0xde: OPJ8("JLT"); break;
    case 0xdf: OPJ8("JE"); break;
    case 0xe0: OPDJ8("DJNZ"); break;
    case 0xe1: OPDJ8("DJNZW"); break;
    case 0xe2: OPUNIMP("TIJMP"); break;
    case 0xe3: sprintf(buf, "BR [%02X]", peek(1)); break;
   // case 0xe4 ... 0xe6: OP0("RESERVED"); break;
    case 0xe7: OPJ16("LJMP"); break;
  //  case 0xe8 ... 0xeb: OP0("RESERVED"); break;
    case 0xec: OPUNIMP("DPTS"); break;
    case 0xed: OPUNIMP("EPTS"); break;
    case 0xee: OP0("RESERVED NOP"); break;
    case 0xef: OPJ16("LCALL"); break;
    case 0xf0: OP0("RET"); break;
    case 0xf1: OP0("RESERVED"); break;
    case 0xf2: OP0("PUSHF"); break;
    case 0xf3: OP0("POPF"); break;
    case 0xf4: OP0("PUSHA"); break;
    case 0xf5: OP0("POPA"); break;
    case 0xf6: OPUNIMP("IDLPD"); break;
    case 0xf7: OPUNIMP("TRAP"); break;
    case 0xf8: OP0("CLRC"); break;
    case 0xf9: OP0("SETC"); break;
    case 0xfa: OP0("DI"); break;
    case 0xfb: OP0("EI"); break;
    case 0xfc: OP0("CLRVT"); break;
    case 0xfd: OP0("NOP"); break;
    case 0xfe: OPUNIMP("signed multiply/divide"); break;
    case 0xff: OP0("RST"); break;
    default:
      sprintf(buf, "(UNHANDLED)");
  }
  return buf;
}
#endif

void Cpu::recordEvent(int type, int value)
{
  if (!recording)
    return;
  
  DEBUG(EVENT, "record   %d at %llu\n", type, (unsigned long long)getCycles());

  struct Event e;
  e.cycles = getCycles();
  e.type = type;
  e.value = value;
#ifdef EVENT_COMPRESSED
  gzwrite(record_file, &e, sizeof(struct Event));
#else
  fwrite(&e, sizeof(struct Event), 1, record_file);
#endif
}

struct Event Cpu::retrieveEvent(int type)
{
  static const struct Event event_none = {0, EVENT_INVALID, 0};
  
  DEBUG(EVENT, "retrieve %d at %llu\n", type, (unsigned long long)getCycles());

  // Get next event if:
  // - we don't have one yet, or
  // - previous event has been returned already, or
  // - we are past the previous event, and
  // - there is a next event to begin with
  if ((current_event.type == EVENT_INVALID || current_event.cycles < getCycles())) {
    if (record_file) {
      if (!state_eof(record_file))
#ifdef EVENT_COMPRESSED
        gzread(record_file, &current_event, sizeof(struct Event));
#else
        fread(&current_event, sizeof(struct Event), 1, record_file);
#endif
      else
        disableRecording();
    }
  }
  
  if (type == current_event.type && getCycles() == current_event.cycles) {
    DEBUG(EVENT, "found event type %d val %d at %llu\n", current_event.type, current_event.value, (unsigned long long)current_event.cycles);
    struct Event ret = current_event;
    current_event.type = EVENT_INVALID;
    return ret;
  }
  else
    return event_none;
}

int Cpu::retrieveEventValue(int type)
{
  struct Event ev;
  ev = retrieveEvent(type);
  if (ev.type == EVENT_INVALID) {
    ERROR("event type %d not found at %llu\n", type, (unsigned long long)getCycles());
    disableRecording();
  }
  return ev.value;
}

#include <sys/stat.h>
bool Cpu::loadRom(const char *name)
{
  /* We might have to unpack several layers of archives, but we want to keep
     reference to the original file.  */
  const char *orig_name = name;
  /* If we redirect "name" to a temporary file after unpacking, we set this
     flag to remind us to clean up after loading. */
  bool unlink_me = false;
  /* If we have to write a temporary file, this will hold its name. */
  char new_name[1024];
  /* This is easier than finding out what today's maximum path length macro
     is. */
  if (name && strlen(name) > 1020) {
    ui->fatalError("file name too long", NULL);
    return false;
  }

  /* We return back here if we have created a temp file that needs to be
     parsed again. */
start_over:
  /* If a name has been specified, open that file; otherwise, open the
     last ROM read. */
  FILE *fp = fopen(name ? name : rom_name, "rb");
  if (!fp) {
    ERROR("failed to open %s\n", name ? name : rom_name);
    return false;
  }

  /* If a name has been given, we set it as the new ROM name. */
  if (name) {
    if (rom_name)
      free(rom_name);
    rom_name = strdup(name);
  }

  /* Check if this is a funny archive file. File formats encountered so far:
     1. Plain ROM image.
     2. ROM image in an LHA archive.
     3. ROM image in an LHA archive in a self-extracting RAR archive.
     We try to unfold this mess until we hit a plain ROM image. */
  char ident_buf[5] = {0};
  size_t s = fread(ident_buf, 1, 5, fp);
  if (s == 5) {
    if (ident_buf[0] == 'M' && ident_buf[1] == 'Z') {
      /* EXE, most likely a self-extracting RAR */
      DEBUG(OS, "extracting RAR file %s\n", rom_name);
      fclose(fp);
      sprintf(new_name, "%s.lha", rom_name);
      char cmd[strlen(rom_name) + strlen(new_name) + 50];
      sprintf(cmd, "unrar p -inul \"%s\" >\"%s\"", rom_name, new_name);
      if (system(cmd)) {
        ERROR("failed to unpack %s to %s\n", rom_name, new_name);
        ui->fatalError("failed to unpack %s to %s using unrar", NULL, rom_name, new_name);
        /* If the source has already been a temp file, delete it. */
        if (unlink_me)
          unlink(rom_name);
        /* Delete the broken temp file just created. */
        unlink(new_name);
        return false;
      }
      if (unlink_me) {
        unlink(rom_name);
      }
      /* Redirect the filename to load, set the unlink flag and start over. */
      name = new_name;
      unlink_me = true;
      goto start_over;
    }
    else if (ident_buf[2] == '-' && ident_buf[3] == 'l' && ident_buf[4] == 'h') {
      /* LHA */
      DEBUG(OS, "extracting LHA file %s\n", rom_name);
      fclose(fp);
      char *stripname = strdup(rom_name);
      char *r;
      if ((r = strrchr(stripname, '.')))
        *r = 0;
      if ((r = strrchr(stripname, '.')))
        *r = 0;
      sprintf(new_name, "%s.bin", stripname);
      free(stripname);
      char cmd[strlen(rom_name) + strlen(new_name) + 50];
      sprintf(cmd, "lha p -q \"%s\" >\"%s\"", rom_name, new_name);
      if (system(cmd)) {
        ERROR("failed to unpack %s to %s\n", rom_name, new_name);
        ui->fatalError("failed to unpack %s to %s using lha", NULL, rom_name, new_name);
        if (unlink_me)
          unlink(rom_name);
        unlink(new_name);
        return false;
      }
      if (unlink_me)
        unlink(rom_name);
      /* We'll keep this one. */
      unlink_me = false;
      name = new_name;
      orig_name = name;
      goto start_over;
    }
  }

  fseek(fp, 0, SEEK_SET);
  
  /* Find out how big this thing is and dimension our memory accordingly. */
  struct stat st;
  fstat(fileno(fp), &st);
  setRomSize(st.st_size);
  setMappedRamSize(524288);
  
  DEBUG(MEM, "allocated %lld bytes for ROM image\n", (long long)st.st_size);
  /* Win32 workaround; you can't just load a whole SEVERAL MEGABYTES in ONE
     GO, what do you think this is, the 21st century?!?  */
  uint8_t *rrom = (uint8_t *)rom;
  while (st.st_size) {
    size_t bread = fread(rrom, 1, st.st_size > 4096 ? 4096 : st.st_size, fp);
    st.st_size -= bread;
    rrom += bread;
  }
  fclose(fp);
  /* If we just read from a temp file, remove it. */
  if (unlink_me)
    unlink(rom_name);

  /* restore original file name if necessary */
  if (orig_name && strcmp(rom_name, orig_name)) {
    free(rom_name);
    rom_name = strdup(orig_name);
  }

  memcpy(ram, rom, 0xc000);
  
  char eename[strlen(rom_name) + 4 + 1];
  sprintf(eename, "%s.eep", rom_name);
  eeprom->setFilename(eename);

  /* Hi-Scan vs CarmanScan detection */
  is_hiscan = true;
  const char ident[] = "CARMAN";
  const char *ip = ident;
  const uint8_t *rr = rom;
  size_t size = rom_size;
  while (size) {
    if (*rr == *ip) {
      ip++;
      if (!*ip) {
        is_hiscan = false;
        break;
      }
    }
    else
      ip = ident;
    rr++;
    size--;
  }
  
  return true;
}

bool Cpu::loadExtendedRom(const char *name)
{
  FILE *fp = fopen(name ? name : exrom_name, "r");
  if (!fp) {
    ERROR("failed to open %s\n", name ? name : exrom_name);
    return false;
  }
  
  if (name) {
    if (exrom_name)
      free(exrom_name);
    exrom_name = strdup(name);
  }
  
  struct stat st;
  fstat(fileno(fp), &st);
  exrom_size = st.st_size;
  
  exrom = (const uint8_t *)calloc(st.st_size, 1);
  DEBUG(MEM, "allocated %lld bytes for extended ROM image\n", (long long)st.st_size);
  /* Win32 workaround */
  uint8_t *rrom = (uint8_t *)exrom;
  while (st.st_size) {
    size_t bread = fread(rrom, 1, st.st_size > 4096 ? 4096 : st.st_size, fp);
    st.st_size -= bread;
    rrom += bread;
  }
  fclose(fp);
  return true;
}

#include "state.h"

bool Cpu::loadSaveState(const char *name, bool write)
{
  statefile_t fp;
  
  if (write)
    fp = state_open(name, "wb");
  else
    fp = state_open(name, "rb");
  bool ret = loadSaveState(fp, write);
  state_close(fp);
  return ret;
}
bool Cpu::loadSaveState(statefile_t fp, bool write)
{
  /* load/save the CPU state */    
  STATE_RW(clock);
  STATE_RW(oclock);
  
  STATE_RW(pc);
  STATE_RW(opc);
  STATE_RW(psw);
  STATE_RW(code_hi); STATE_RW(code_lo);
  STATE_RW(data_hi); STATE_RW(data_lo);
  STATE_RW(wsr);
  STATE_RW(wsr1);
  STATE_RW(int_mask); STATE_RW(int_mask1);
  STATE_RW(ad_command);
  
  STATE_RW(cycles);
  STATE_RW(end_cycles);
  
  STATE_RW(ioc0); STATE_RW(ioc1); STATE_RW(ios0); STATE_RW(ios1);
  STATE_RW(last_ios1_read);
  
  STATE_RW(ioport1); STATE_RW(ioport2);
  STATE_RW(ad_result);
  STATE_RW(timer1_offset);
  STATE_RW(timer2);
  STATE_RW(timer2_inc_factor);
  
  /* load/save timing information */
  STATE_RW(starttime);
  STATE_RW(oldtime);
  uint32_t old_nowtime = nowtime;
  STATE_RW(nowtime);
  /* when reading, we have to compensate for the time that has passed since
     the state had been written */
  if (!write) {
    int32_t timediff = old_nowtime - nowtime;
    starttime += timediff;
    oldtime += timediff;
    nowtime += timediff;
  }

  STATE_RW(next_sampling);
  STATE_RW(next_lcd_update);
  STATE_RW(next_event_pumping);

  STATE_RW(slowdown);

  /* load/save peripheral states (except EEPROM, see below) */
  lcd->loadSaveState(fp, write);
  serial->loadSaveState(fp, write);
  hsi->loadSaveState(fp, write);
  keypad->loadSaveState(fp, write);
  ui->loadSaveState(fp, write);

  /* load/save ROM */
  /* we only save the name(s) of the ROM(s), not the entire contents */
  /* this is a bit shitty, we need to make a copy of rom_name because
     loadRom() deletes it when called ... */
  STATE_RWSTRING(rom_name);
  if (!write) {
    if (!loadRom(NULL)) {
      ui->fatalError("Failed to load ROM file '%s'.", NULL, rom_name);
      stop();
      reset();
      return true;
    }
  }
  STATE_RWSTRING(exrom_name);
  if (!write && exrom_name) {
    if (!loadExtendedRom(NULL)) {
      ui->fatalError("Failed to load extended ROM file '%s'.", NULL, exrom_name);
      stop();
      reset();
      return true;
    }
  }
  
  /* load/save event replaying state */
  
  if (!write && recording) {
    DEBUG(WARN, "loading state, recording aborted\n");
    disableRecording();	/* looks like the only sensible thing to do to me */
  }

  STATE_RWSTRING(record_file_name);

  /* need to save this in a temporary because we need the current state of
     affairs later */
  bool repl = replaying;
  STATE_RW(repl);
  
  STATE_RW(current_event);

  uint32_t pos = 0;
  if (write && replaying) {
    /* writing: get current position in file */
    pos = state_tell(record_file);
  }

  /* save/load position in the event stream */
  STATE_RW(pos);
  
  if (!write && repl) {
    /* reading: set current position in file */
    enableReplaying(NULL);
    state_seek(record_file, pos, SEEK_SET);
  }
  
  if (!write) {
    /* set data_ptr, code_ptr */
    ioWrite8(0x270, code_lo);
    ioWrite8(0x271, code_hi);
    ioWrite8(0x272, data_lo);
    ioWrite8(0x273, data_hi);
  }
  
  /* this is all reset by loadRom(), so we do it after reloading */
  STATE_RWBUF(ram, 0xc000);
  STATE_RWBUF(mapped_ram, 524288);
  eeprom->loadSaveState(fp, write);

  resume();
  ui->machineRunning();

  return false;
}

void Cpu::sync(bool exact)
{
  static uint64_t last_diff_report = 0;
  nowtime = os_mtime();
  uint32_t passedtime = nowtime - oldtime;
  uint32_t targettime = cycles * 2 * 1000 / clock;
  int32_t diff = targettime - passedtime;

#ifndef NDEBUG
  if (cycles % (1048576 * 2) < 1000) {
    ui->updateTime(diff);
  }
#endif

  if (diff < -50 && getCycles() - last_diff_report > 500000) {
    DEBUG(WARN, "too slow (%d) at 0x%x\n", diff, virtToPhys(pc, 1));
    last_diff_report = getCycles();
    resetTiming();
  }
#if 0
  if (cycles % 100000 < 4) {
    ERROR("TIMING passed %u should be %u, we're %d ms fast\n", passedtime, targettime, diff);
  }
#endif
#ifndef BENCHMARK
  if (exact && diff > 0) {
    while(os_mtime() - oldtime < targettime) {}
  }
  else if (diff > 5)
    os_msleep(diff - 1);
#endif
}

void Cpu::sendCommand(int cmd)
{
  cmd_queue->add(cmd);
}
