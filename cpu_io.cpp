/*
 * cpu_io.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "cpu.h"
#include "os.h"
#include "serial.h"
#include "eeprom.h"
#include "lcd.h"
#include "hsio.h"
#include "keypad.h"
#include "hints.h"

#ifndef NDEBUG
#define REG(x) reg = x
#else
#define REG(x)
#endif  

const uint8_t extint_queue[8] = {0, 0xf1, 0xf7, 0x01, 0x42, 0, 0, 0};
static int extint_pos = 0;

static uint8_t printable_char(uint8_t c)
{
  if (c > ' ' && c < 0x80)
    return c;
  else
    return '?';
}

uint8_t Cpu::ioRead8(uint16_t addr)
{
  uint8_t ret;
#ifndef NDEBUG
  const char* reg;
#endif

  switch(addr) {
    case 0x02:  /* AD_RESULT (LO, 0) AD_COMMAND (15) */
      switch (wsr) {
        case 0:
          REG("AD_RESULT (LO)");
          ret = (ad_command & 0x7 /* A/D channel */) |
                (ad_command & 0x10 /* A/D mode */);
          break;
        case 15:
          REG("AD_COMMAND");
          ret = ad_command;
          break;
        default: goto fail;
      };
      break;
    case 0x03: /* AD_RESULT (HI, 0) AD_TIME (1) HSI_MODE (15) */
      switch (wsr) {
        case 0:
          REG("AD_RESULT (HI)");
          ret = ad_result >> 8; /* better for battery */
#if 0
          /* "ABNORMAL VEHICLE POWER" */
          if (ad_result)
            ad_result -= 0x100;
#endif
          break;
        case 1: REG("AD_TIME"); ret = 0xff; break;
        case 15: REG("HSI_MODE"); ret = hsi->getMode(); break;
        default: goto fail;
      };
      break;
    case 0x04: /* HSI_TIME(LO) (0) PTSSEL(LO) (x) HSO_TIME(LO) (15) */
      switch (wsr) {
        case 0:
          REG("HSI_TIME(LO)"); ret = hsi->getTime(HSI_TIME_LO); break;
        case 1:
          REG("PTSSEL(LO)"); ret = ptssel & 0xff; break;
        case 15:
          REG("HSO_TIME(LO)"); ret = hsi->getTime(HSO_TIME_LO); break;
        default: goto fail;
      };
      break;
    case 0x05: /* HSI_TIME(HI) (0) PTSSEL(HI) (x) HSO_TIME(HI) (15) */
      switch (wsr) {
        case 0:
          REG("HSI_TIME(HI)"); ret = hsi->getTime(HSI_TIME_HI); break;
        case 1:
          REG("PTSSEL(HI)"); ret = ptssel >> 8; break;
        case 15:
          REG("HSO_TIME(HI)"); ret = hsi->getTime(HSO_TIME_HI); break;
        default: goto fail;
      };
      break;
    case 0x07:  /* SBUF(RX) (0) SBUF(TX) (15) */
      switch (wsr) {
        case 0:
          REG("SBUF(RX)");
          if (ioc1 & 0x20)
            ret = serial->readData();
          else
            ret = 0xff;
          //if (ret == 0x48) debug_level |= DEBUG_TRACE | DEBUG_MEM;
          break;
        case 15:
          REG("SBUF(TX)");
          ret = 0xff;
          break;
        default:
          goto fail;
      };
      break;
    case 0x08:	/* INT_MASK */
      REG("INT_MASK");
      ret = int_mask;
      break;
    case 0x0a: /* TIMER1 (LO, 0) WATCHDOG (15) */
      switch (wsr) {
        case 0:
          REG("TIMER1 (LO)");
          ret = getTimer1() & 0xff;
          break;
        case 15: 
          REG("WATCHDOG");
          ret = 0xff;
          break;
        default: goto fail;
      };
      break;
    case 0x0b: /* TIMER1 (HI, 0) IOC2 (not bit 7; 15) */
      switch (wsr) {
        case 0:
          REG("TIMER1 (HI)");
          ret = (getTimer1() >> 8) & 0xff;
          break;
        case 15:
          REG("IOC2 except bit 7");
          ret = 0xff;
          break;
        default: goto fail;
      };
      break;
    case 0x0c: /* TIMER2(LO) (0) IOC3 (1) T2CAPTURE(LO) (15) */
      switch (wsr) {
        case 0:
          REG("TIMER2(LO)");
          /* value in timer2 is 8 times the value we should return;
             this choice was made to allow easy implementation of
             both fast (1 per state time) and slow (1 per 8 state
             times) increment */
          ret = (timer2 / 8) & 0xff;
          break;
        case 1:
          REG("IOC3");
          ret = 0xff;
          break;
        default:
          goto fail;
      }
      break;
    case 0x0d: /* TIMER2(HI) (0) T2CAPTURE(HI) (15) */
      switch (wsr) {
        case 0:
          REG("TIMER2(HI)");
          /* see TIMER2(LO) */
          ret = (timer2 / 8) >> 8;
          break;
        default:
          goto fail;
      }
      break;
    case 0x0e:	/* IOPORT0 */
      REG("IOPORT0");
      ret = 0xff;
      break;
    case 0x0f:	/* IOPORT1 */
      REG("IOPORT1");
      ioport1 &= ~(1 << 4);
      if (eeprom->readData())
        ioport1 |= (1 << 4);
      ret = ioport1;
      break;
    case 0x10:	/* IOPORT2 */
      REG("IOPORT2");
      ret = (ioport2 & 0xfd) | (serial->getRxState() << 1);
      ioport2 = ret;
      break;
    case 0x11: /* SP_STAT (0) SP_CON (15) */
      switch (wsr) {
        case 0:
          REG("SP_STAT");
          if (ioc1 & 0x20)
            ret = serial->readStat();
          else
            ret = 0;
          break;
        case 15:
          REG("SP_CON");
          ret = serial->readControl();
          break;
        default:
          goto fail;
      };
      break;
    case 0x12:  /* INT_PEND1 */
      REG("INT_PEND1");
      ret = 0xff;
      break;
    case 0x13:	/* INT_MASK1 */
      REG("INT_MASK1");
      ret = int_mask1;
      break;
    case 0x15: /* IOS0 (0) IOC0 (15; bit 1 RAO) */
      switch (wsr) {
        case 0:
          REG("IOS0");
          ret = ios0;
          break;
        case 15:
          REG("IOC0");
          ret = ioc0 | 2;
          break;
        default:
          goto fail;
      };
      break;
    case 0x16: /* IOS1 (0) IOC1 (15) */
      switch(wsr) {
        case 0:
          REG("IOS1");

          /* TIMER1 overflow bit */
          if (last_ios1_read > getTimer1())
            ios1 |= (1 << 5);
          
          ret = ios1;
          ios1 &= 0xc0; /* "reading IOS1 clears bits IOS1.0 - IOS1.5" */
          last_ios1_read = getTimer1();
          break;
        case 1:
          REG("PWM2_CONTROL") ; ret = 0xff; break;
        case 15:
          REG("IOC1"); ret = ioc1; break;
        default:
          goto fail;
      };
      break;
    case 0x17: /* IOS2(0) PWM1_CONTROL (1) PWM0_CONTROL (15) */
      switch (wsr) {
        case 0:
          REG("IOS2"); ret = 0xff; break;
        case 1:
          REG("PWM1_CONTROL"); ret = 0xff; break;
        case 15:
          REG("PWM0_CONTROL"); ret = 0xff; break;
        default:
          goto fail;
      };
      break;
    case 0x210:
      REG("LCD_READ_STATUS");
      ret = lcd->read(LCD_READ_STATUS);
      break;
    case 0x212:
      REG("LCD_READ_DATA");
      ret = lcd->read(LCD_READ_DATA);
      break;
    case 0x200:
      REG("IO200");
      switch (ram[0x202]) {
        case 0xe0:
          ret = keypad->getLine(0);
          break;
        case 0xe5:
          ret = keypad->getLine(1);
          break;
        case 0xe6:
          ret = keypad->getLine(2);
          break;
        case 0xec:
          ret = keypad->getLine(3);
          break;
        default:
          ret = 0x7f;
          break;
      }
      break;
    case 0x236:
      REG("IO236");
      ret = (rand() % 0xff) & 0xef; //| 0x10;
      break;
    case 0x201:
      REG("IO201");
      ret = (rand() % 0xff) | 0x08;
      break;
    case 0x240:
      REG("IO240");
      ret = extint_queue[extint_pos];
      extint_pos = (extint_pos + 1) % 8;
      break;
    case 0x270:
      return code_lo;
      /* well understood, no debug output */
    case 0x271:
      REG("CODEMAP_HI");
      ret = code_hi;
      break;
    case 0x272:
      REG("DATAMAP_LO");
      ret = data_lo;
      break;
    case 0x273:
      REG("DATAMAP_HI");
      ret = data_hi;
      break;

    default:
     if ((addr>= 0x203 && addr <= 0x20f) ||
         (addr>= 0x211 && addr <= 0x211) ||
         (addr>= 0x213 && addr <= 0x235) ||
         (addr>= 0x237 && addr <= 0x23f) ||
         (addr>= 0x241 && addr <= 0x26f) ||
         (addr>= 0x274 && addr <= 0x2ff) 
         )
     {

         REG("IO2XX");
         DEBUG(IO, "%04X: IO2XX from %04X (%02X, '%c')\n", opc, addr, ram[addr], printable_char(ram[addr]));
         if (addr == 0x274)
             ret = 0x00;
         else
             ret = ram[addr];


         goto jump_good;
     }
fail:
      ERROR("I/O byte read from 0x%x unimplemented at %04X\n", addr, opc);
      dumpMem();
      exit(1);
  };
jump_good:
  DEBUG(IO, "%04X (%08X/%8lld): IOR %s (%02X): %02X ('%c')\n", opc, virtToPhys(opc, 1), (long long)getCycles(), reg, addr, ret, printable_char(ret));
  return ret;
}

#define IGNORE(name) DEBUG(IO, "ignoring %02X -> %s\n", value, name);

void Cpu::ioWrite8(uint16_t addr, uint8_t value)
{
#ifndef NDEBUG
  const char *reg;
#endif
  switch (addr) {
    case 0x00:
    case 0x01:
      return;
    case 0x02:
      switch(wsr) {
        case 0:
          REG("AD_COMMAND");
          ad_command = value;
          break;
        case 15:
          REG("AD_RESULT (LO)");
          IGNORE(reg)
          break;
        default: goto fail;
      };
      break;
    case 0x03:
      switch (wsr) {
        case 0:
          REG("HSI_MODE");
          hsi->setMode(value);
          break;
        case 1:
          REG("AD_TIME");
          IGNORE(reg)
          break;
        case 15:
          REG("AD_RESULT(HI)");
          IGNORE(reg)
          break;
        default: goto fail;
      };
      break;
    case 0x04:
      switch(wsr) {
        case 0:
          REG("HSO_TIME (LO)");
          hsi->setTime(HSO_TIME_LO, value);
          break;
        case 1:
          REG("PTSSEL (LO)");
          ptssel = (ptssel & 0xff00) | value;
          break;
        case 15:
          REG("HSI_TIME (LO)");
          hsi->setTime(HSI_TIME_LO, value);
          break;
        default: goto fail;
      };
      break;
    case 0x05:
      switch (wsr) {
        case 0:
          REG("HSO_TIME (HI)");
          hsi->setTime(HSO_TIME_HI, value);
          break;
        case 1:
          REG("PTSSEL (HI)");
          ptssel = (ptssel & 0xff) | (value << 8);
          break;
        case 15:
          REG("HSI_TIME (HI)");
          hsi->setTime(HSI_TIME_HI, value);
          break;
        default: goto fail;
      };
      break;
    case 0x06: /* HSO_COMMAND (0) PTSSRV (LO, 1) HSI_STATUS 0246 (15) */
      switch(wsr) {
        case 0:
          REG("HSO_COMMAND");
          hsi->setCommand(HSO_CMD, value);
          break;
        case 1:
          REG("PTSSRV (LO)");
          ptssrv = (ptssrv & 0xff00) | value;
          break;
        case 15:
          REG("HSI_STATUS bits 0, 2, 4, 6");
          hsi->setStatus(HSI_STAT, value);
          break;
        default: goto fail;
      };
      break;
    case 0x07: /* SBUF(TX) (0) SBUF(RX) (15) */
      switch (wsr) {
        case 0: REG("SBUF(TX)"); serial->writeData(value); break;
        case 1: REG("PTSSRV(HI)"); ptssrv = (ptssrv & 0xff) | (value << 8); break;
        case 15: REG("SBUF(RX)"); IGNORE(reg); break;
        default: goto fail;
      };
      break;
    case 0x08: /* INT_MASK */
      REG("INT_MASK");
      int_mask = value;
      break;
    case 0x09: /* INT_PEND */
      REG("INT_PEND");
      IGNORE(reg)
      break;
    case 0x0a: /* WATCHDOG (0) TIMER1(LO) (15) */
      switch (wsr) {
        case 0:
          REG("WATCHDOG");
          IGNORE(reg);
          break;
        case 15:
          REG("TIMER1(LO)");
          timer1_offset &= 0xff00;
          timer1_offset |= (value - getCycles() / 8) & 0xff;
          DEBUG(IO, "TIMER1 now %04X\n", getTimer1());
          break;
        default:
          goto fail;
      }
      break;
    case 0x0b: /* IOC2 (0) TIMER1(HI) (15) */
      if (wsr == 0) {
        REG("IOC2");
        if (value & 1)	/* fast increment */
          timer2_inc_factor = 8;
        else		/* slow increment */
          timer2_inc_factor = 1;
        if (value & 2) {
          /* count down, not up */
          timer2_inc_factor = -timer2_inc_factor;
        }
      }
      else if (wsr == 15) {
        REG("TIMER1(HI)");
        timer1_offset &= 0xff;
        timer1_offset |= (value - ((getCycles() / 8) >> 8)) << 8;
        DEBUG(IO, "TIMER1 now %04X\n", getTimer1());
      }
      else
        goto fail;
      break;
    case 0x0c: /* TIMER2(LO) (0) IOC3 (1) */
      if (wsr == 0) {
        REG("TIMER2(LO)");
        timer2 = (((timer2 / 8) & 0xff00) | value) * 8;
      }
      else if (wsr == 1) {
        REG("IOC3");
        IGNORE(reg)
      }
      else
        goto fail;
      break;
    case 0x0d: /* TIMER2(HI) (0) T2CAPTURE(HI) (15) */
      if (wsr == 0) {
        REG("TIMER2(HI)");
        timer2 = (((timer2 / 8) & 0xff) | (value << 8)) * 8;
      }
      else if (wsr == 15) {
        REG("T2CAPTURE(HI)");
        IGNORE(reg)
      }
      else
        goto fail;
      break;
    case 0x0e: /* BAUD_RATE (0) */
      REG("BAUD_RATE");
      DEBUG(IO, "I/O setting baud rate to 0x%x\n", value);
      serial->setBaudrate(value);
      break;
    case 0x0f: /* IOPORT1 */
      REG("IOPORT1");
#define SET(a, bit) ((a) & (1 << (bit)))
#if 0
      if (SET(value, 7) && !SET(ioport1, 7)) {
        DEBUG(IO, "IOPORT1 bitbang on\n");
      }
      if (!SET(value, 7) && SET(ioport1, 7)) {
        DEBUG(IO, "IOPORT1 bitbang off\n");
      }
      if (SET(value, 7) && SET(value, 6) && !SET(ioport1, 6)) {
        DEBUG(IO, "IOPORT1 bitbang clock out %d\n", (value & 0x20) >> 5);
      }
#endif
      eeprom->toggleInputs(SET(value, 7), SET(value, 6), SET(value, 5));
      ioport1 = value;
#undef SET
      break;
    case 0x10: /* IOPORT2 */
      REG("IOPORT2");
      if ((value & 0xfe) == (ioport2 & 0xfe)) {
        DEBUG(WARN, "SERIAL somebody is manually hammering the TXD pin (%d)\n", value & 1);
        serial->slowInit(value & 1);
      }
      ioport2 = value;
      break;
    case 0x11: /* SP_CON (0) SP_STAT (15) */
      if (wsr == 0) {
        REG("SP_CON");
        serial->setControl(value);
      }
      else if (wsr == 15) {
        REG("SP_STAT");
        serial->setStat(value);
      }
      else goto fail;
      break;
    case 0x12: /* INT_PEND1 */
      REG("INT_PEND1");
      IGNORE(reg)
      break;
    case 0x13: /* INT_MASK1 */
      REG("INT_MASK1");
      int_mask1 = value;
      break;
    case 0x14: /* WSR */
      REG("WSR");
      DEBUG(IO, "setting WSR to %02X\n", value);
      wsr = value;
      break;
    case 0x15: /* WSR1 */
      REG("WSR1");
      DEBUG(IO, "setting WSR1 to %02X\n", value);
      wsr1 = value;
      break;
    case 0x16: /* IOC1 (0) PWM1_CONTROL (1) IOS1 (15) */
      if (wsr == 0) {
        REG("IOC1");
        ioc1 = value;
      }
      else if (wsr == 1) {
        REG("PWM1_CONTROL");
        IGNORE(reg)
      }
      else if (wsr == 15) {
        REG("IOS1");
        ios1 = value;
      }
      else
        goto fail;
      break;
    case 0x17: /* PWM0_CONTROL (0) PWM2_CONTROL (1) IOS2 (15) */
      switch (wsr) {
        case 0: REG("PWM0_CONTROL"); break;
        case 1: REG("PWM2_CONTROL"); break;
        case 15: REG("IOS2"); break;
        default: goto fail;
      };
      IGNORE(reg)
      break;
    case 0x210:
      REG("LCD_WRITE_DATA");
      lcd->write(LCD_WRITE_DATA, value);
      return; /* no debug output, well understood */
      break;
    case 0x212:
      REG("LCD_WRITE_COMMAND");
      lcd->write(LCD_WRITE_COMMAND, value);
      return; /* no debug output, well understood */
      break;
    case 0x202:
      REG("IO202");
      ram[addr] = value;
      break;
    case 0x250: /* serial line switch? */
      REG("IO250");
      {
        int line = -(~value & 0xff);	/* UI::setCommline() interprets negative value as unknown line */
        if (is_hiscan) {
          switch (value) {
            case 0xbf:
              DEBUG(WARN, "IO serial line switched to ECU/AT (pin 7?)\n");
              line = 7;
              break;
            case 0xf7:
              DEBUG(WARN, "IO serial line switched to ABS/TCS (pin 8)\n");
              line = 8;
              break;
            case 0xfb:
              DEBUG(WARN, "IO serial line switched to CC(ElantraRD) (pin 9)\n");
              line = 9;
              break;
            case 0xef:
              DEBUG(WARN, "IO serial line switched to SRS (pin 12)\n");
              line = 12;
              break;
            case 0xfe:
              DEBUG(WARN, "IO serial line switched to EPS(SonataEF) (pin 13)\n");
              line = 12;
              break;
            case 0xfd:
              DEBUG(WARN, "IO serial line switched to AT(ElantraRD old) (pin 2 or 10)\n");
              line = 10;
              break;
            case 0xdf:
              DEBUG(WARN, "IO serial line switched to CAN\n");
              line = 42;
              break;
            default:
              DEBUG(WARN, "IO serial line switched to unknown line (%02X)\n", (~value) & 0xff);
              break;
          }
        }
        else {
          switch (value) {
            case 0xff:
              DEBUG(WARN, "IO serial line switched to ECU/AT (pin 7?), I\n");
              line = 7;
              break;
            case 0x7f:
              DEBUG(WARN, "IO serial line switched to ECU/AT (pin 7?), II\n");
              line = 7;
              break;
            case 0xf7:
              DEBUG(WARN, "IO serial line switched to ABS/TCS/TPMS (pin 8), perhaps SRS(Civic) (pin 14)\n");
              line = 8;
              break;
            case 0xfb:
              DEBUG(WARN, "IO serial line switched to CC(ElantraRD) (pin 9), perhaps SRS(Civic) (pin 14)\n");
              line = 9;
              break;
            case 0xbf:
              DEBUG(WARN, "IO serial line switched to AT(ElantraRD old) (pin 2 or 10)\n");
              line = 10;
              break;
            case 0xfe:
              DEBUG(WARN, "IO serial line switched to SRS (pin 12)\n");
              line = 12;
              break;
            case 0xdf:
              DEBUG(WARN, "IO serial line switched to CAN\n");
              line = 42;
              break;
            default:
              DEBUG(WARN, "IO serial line switched to unknown line (%02X)\n", (~value) & 0xff);
              break;
          }
        }
        serial->setCommLine(line);
      }
      break;
    case 0x254: /* another kind of serial port control */
      REG("IO254");
      if (value == 0x23 && ram[addr] != 0x23) {
        DEBUG(WARN, "Mitsubishi diag pin low (0x23)\n");
        serial->setL(0);
      }
      else if (ram[addr] == 0x23 && value != 0x23) {
        DEBUG(WARN, "Mitsubishi diag pin high (not 0x23)\n");
        serial->setL(1);
      }
      else if (value == 0x33 && ram[addr] != 0x33) {
        DEBUG(WARN, "Mitsubishi diag pin low (0x33)\n");
        serial->setL(0);
      }
      else if (ram[addr] == 0x33 && value != 0x33) {
        DEBUG(WARN, "Mitsubishi diag pin high (not 0x33)\n");
        serial->setL(1);
      }
      else if ((value & 0x02) != (ram[addr] & 0x02)) {
        switch (value >> 4) {
          case 0:
          case 3:
            /* fast init, K line (or both K and L?) */
            DEBUG(SERIAL, "somebody is hammering the TXD pin via 0x254 (value %02X, bit %d)\n", value, (value >> 1) & 1);
            serial->slowInit((value >> 1) & 1);
            break;
          case 7:
            /* Mitsubishi diag pin (pin 1?) */
            DEBUG(WARN, "Mitsubishi diag pin set to %d\n", (value >> 1) & 1);
            serial->setL((value >> 1) & 1);
            break;
          default:
            ERROR("unhandled port 254h value\n");
            exit(1);
            break;
        }
      }
      DEBUG(WARN, "IO254 %02X -> %02X at %d\n", ram[addr], value, os_mtime());
      hints->port254(value);
      ram[addr] = value;
      break;
    case 0x25e: /* some LED? */
      REG("IO25E");
      ram[addr] = value;
      ui->setLED(LED_BEEP, !(value & 2));
      hints->beep();
      break;
    case 0x270:
      code_lo = value;
      code_ptr = &rom[virtToPhysSlow(0xc000, 1)];
      return; /* well understood, no debug output */
    case 0x271:
      REG("CODEMAP_HI");
      code_hi = value;
      code_ptr = &rom[virtToPhysSlow(0xc000, 1)];
      break;
    case 0x272:
      REG("DATAMAP_LO");
      data_lo = value;
      {
        uint32_t phys = virtToPhysSlow(0xc000, 0);
        //ERROR("phys 0x%x, rom_size %d (0x%x)\n", phys, rom_size, rom_size);
        if (phys >= 0xcaf00000UL)
          data_ptr = &mapped_ram[phys - 0xcaf00000UL];
        else if (phys >= 0xbab00000UL)
          data_ptr = (uint8_t *)&exrom[phys - 0xbab00000UL];
        else if (phys <= rom_size - 0xc000)
          data_ptr = (uint8_t *)&rom[phys];
        else {
          DEBUG(WARN, "invalid ROM data mapping 0x%x\n", phys);
          data_ptr = (uint8_t *)rom;
        }
      }
#if 0
      if (value == 0x9a) {
        debug_level |= DEBUG_TRACE|DEBUG_OP|DEBUG_MEM;
        debug_level_unabridged |= DEBUG_TRACE|DEBUG_OP|DEBUG_MEM;
      }
#endif
      break;
    case 0x273:
      REG("DATAMAP_HI");
      data_hi = value;
      {
        uint32_t phys = virtToPhysSlow(0xc000, 0);
        if (phys >= 0xcaf00000UL)
          data_ptr = &mapped_ram[phys - 0xcaf00000UL];
        else if (phys >= 0xbab00000UL)
          data_ptr = (uint8_t *)&exrom[phys - 0xbab00000UL];
        else
          data_ptr = (uint8_t *)&rom[phys];
      }
      break;

    default:
        if ((addr>= 0x200 && addr <= 0x201) ||
            (addr>= 0x203 && addr <= 0x20f) ||
            (addr>= 0x211 && addr <= 0x211) ||
            (addr>= 0x213 && addr <= 0x24f) ||
            (addr>= 0x251 && addr <= 0x253) ||
            (addr>= 0x255 && addr <= 0x25d) ||
            (addr>= 0x25f && addr <= 0x26f) ||
            (addr>= 0x274 && addr <= 0x2ff) 
            )
        {
            REG("IO2XX");
            DEBUG(IO, "%04X: IO2XX %02X ('%c') to %04X\n", opc, value, printable_char(value), addr);
            ram[addr] = value;

            goto jump_good;
        }

fail:
      ERROR("%04X/%08X: I/O byte write of 0x%x ('%c') to 0x%x (WSR %02X) unimplemented\n", opc, virtToPhys(opc, 1), value, printable_char(value), addr, wsr);
      dumpMem();
      exit(1);
  };
jump_good:
  DEBUG(IO, "%04X (%08X/%8lld): IOW %s (%02X/'%c') -> %02X\n", opc, virtToPhys(opc, 1), (long long)getCycles(), reg, value, printable_char(value), addr);
}

