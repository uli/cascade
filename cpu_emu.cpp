/*
 * cpu_emu.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "cpu.h"
#include "os.h"
#include "ui.h"
#include "lcd.h"
#include "keypad.h"
#include "hsio.h"
#include "eeprom.h"

int Cpu::emulate(void)
{
  uint8_t imm8;
  uint16_t imm16;
  int16_t simm16;
  uint32_t imm32;
  uint16_t target;
  uint8_t res8;
  uint16_t res16;
  int8_t sres8;
  int16_t sres16;
  int32_t sres32;
  int64_t sres64;
  uint32_t res32;
  uint64_t res64;
  uint8_t val8, val8_2;
  int8_t sval8;
  uint16_t val16; 
  int16_t sval16;
  uint32_t val32;
  int32_t sval32;
  int8_t rel8;
  int16_t rel16;
  uint8_t addr8, addr8_2;
  uint16_t addr16;
  
  if (!rom_name)
    ui->loadRom();
  else {
    resume();
    ui->machineRunning();
  }

#ifndef NDEBUG
  int abridging = 0;
#endif

  for(;;) {
    uint8_t opcode, eopcode;
    uint32_t passedcycles = cycles - oldcycles;
    timer2 += passedcycles * timer2_inc_factor;
    oldcycles = cycles;

    if (cycles >= next_sampling) {
      next_sampling += 65536;
      sync(false);
    }
    
#if !defined(NDEBUG) || defined(BENCHMARK)
    if (getCycles() > end_cycles) {
      DEBUG(WARN, "maximum cycles exceeded\n");
      dumpMem();
      ERROR("%llu state times in %u ms, %llu Hz\n", (unsigned long long)getCycles(), nowtime - starttime, (unsigned long long)getCycles() * 1000 * 2 / (nowtime - starttime));
      return 0;
    }
#endif
    
    if ((int_mask & (1 << 5)) && (psw & PSW_INTE)) {
      for (int i = 0; i < 4; i++) {
        /* doing this the obvious way ("getTimer1() - passedcycles / 8")
           results in a loss of interactivity; I could imagine it to be due
           to small "interrupt storms" becuase "old timer equals new timer"
           repeatedly triggers an interrupt in Hsio::swtInterrupt() when
           there are fast instructions (less than 8 state times) coming up
           that are too short to change TIMER1 before the next check.
           maybe implementing proper cycle counting for interrupts would
           help; compiling with NDEBUG should, too :) */
        if (hsi->swtInterrupt((getCycles() - passedcycles) / 8 + timer1_offset, getTimer1(), i)) {
          DEBUG(IO, "TIMER INTERRUPT SWT%d at %lld cycles!!!\n", i, (unsigned long long)getCycles());
          ios1 |= 1 << i;
          push16(pc);
          pc = memRead16(0x200a);
          /* XXX: cycles? */
        }
      }
    }
    
    if (cycles >= next_lcd_update) {
      lcd->update();
#ifdef BENCHMARK
      next_lcd_update += 52428800;
#else
      next_lcd_update += 524288;
#endif
    }
      
    if (cycles >= next_event_pumping) {
      bool remember_to_reset_machine_state_in_ui = false;
      if (emulation_stopped) {
        ui->machineStopped();
        remember_to_reset_machine_state_in_ui = true;
      }
      /* In case the emulation has been stopped, we have to loop here
         to parse commands from the UI. */
      do {
        keypad->update();
        while (cmd_queue->count()) {
          int cmd = cmd_queue->consume();
          switch (cmd) {
            case CPU_CMD_EXIT:
              DEBUG(KEY, "exiting by user request\n");
#ifndef NDEBUG
              dumpMem();
#endif
              ui->quit();
#ifndef NDEBUG
              if (nowtime - starttime)
                ERROR("%llu state times in %u ms, %llu Hz\n", (unsigned long long)getCycles(), nowtime - starttime, (unsigned long long)getCycles() * 1000 * 2 / (nowtime - starttime));
#endif
              return 0;
            case CPU_CMD_TOGGLE_ECHO:
              serial->setEcho(!serial->getEcho());
              break;
            case CPU_CMD_SAVE: {
                const char *state_name = ui->getStateName(true);
                if (state_name)
                  loadSaveState(state_name, true);
                lcd->redraw();
                break;
              }
            case CPU_CMD_LOAD: {
                const char *state_name = ui->getStateName(false);
                if (state_name)
                  loadSaveState(state_name, false);
                lcd->redraw();
                break;
              }
            case CPU_CMD_RECORD: {
                const char *rec_name = ui->getStateName(true, "rec", "rec");
                if (rec_name)
                  enableRecording(rec_name);
                lcd->redraw();
                break;
              }
            case CPU_CMD_PLAY: {
                const char *play_name = ui->getStateName(false, "rec", "rec");
                if (play_name)
                  enableReplaying(play_name);
                lcd->redraw();
                break;
              }
            case CPU_CMD_STOP_RECPLAY:
              disableRecording();
              break;
            case CPU_CMD_RESET:
              if (ui->askUser("Reset", "Do you want to reset the scanner?"))
                reset();
              break;
            case CPU_CMD_FRESET:
              if (ui->askUser("Factory Reset", "Do you want to reset the scanner to factory default?")) {
                eeprom->erase();
                reset();
              }
              break;
            case CPU_CMD_LOAD_ROM:
              ui->loadRom();
              break;
            default:
              break;
          }
        }
        if (emulation_stopped)
          os_msleep(100);
      } while (emulation_stopped);
      if (remember_to_reset_machine_state_in_ui)
        ui->machineRunning();
      next_event_pumping += 131072;
    }
    
#ifdef NDEBUG
    for (int i = 10; i; i--) {
#endif

#if !defined(NDEBUG) || defined(LATENCY)
    opc = pc;
#endif
#ifndef NDEBUG

    if (virtToPhys(pc, 1) == trigger)
      debug_level = trigger_level;
    
    debug_level_unabridged = debug_level;
    if ((debug_level & DEBUG_ABRIDGED)) {
      if (mem_profile[virtToPhys(pc, 1)] > 20) {
        if (!abridging) {
          DEBUG(TRACE, "%04X (%08X) ...", opc, virtToPhys(opc, 1));
          abridging = 1;
        }
        debug_level &= ~(DEBUG_TRACE | DEBUG_MEM | DEBUG_OP);
      }
      else {
        if (abridging)
          DEBUG(TRACE, "... %04X (%08X)\n", opc, virtToPhys(opc, 1));
        abridging = 0;
        if (debug_level & DEBUG_TRACE)
          mem_profile[virtToPhys(pc, 1)]++;
      }
    }
    uint32_t old_debug_level = debug_level;
    debug_level &= ~DEBUG_MEM;
      
    DEBUG(TRACE, "PC %04X (%08X) AX %04X BX %04X CX %04X DX %04X A0 %04X A2 %04X A6 %04X E0 %04X E2 %04X E4 %04X E8 %04X PSW %02X SP %04X 25E %02X\n",
            pc, virtToPhys(pc, 1), memRead16(0x50), memRead16(0x52),
            memRead16(0x54), memRead16(0x56), memRead16(0xa0), memRead16(0xa2), memRead16(0xa6), memRead16(0xe0),
            memRead16(0xe2), memRead16(0xe4), memRead16(0xe8), psw, ram[0x18] | (ram[0x19] << 8), ram[0x25e]);
    
    if (debug_level & DEBUG_TRACE)
      DEBUG(TRACE, "   %s\n", disassemble());
#endif
    
#ifdef LATENCY
    uint64_t lat_old_cycles = 0;
    if (do_latency) {
      lat_old_cycles = getCycles();
      gettimeofday(&tv, NULL);
    }
#endif
    opcode = fetch();
#ifndef NDEBUG
    debug_level = old_debug_level;
#endif
    switch (opcode) {
      case 0x00: /* skip a8 */
        fetch();
        cycle(3);
        break;
      case 0x01: /* clr a8 */
        memWrite16(fetch(), 0);
        psw &= ~(PSW_N|PSW_C|PSW_V);
        psw |= PSW_Z;
        cycle(3);
        break;
      case 0x02: /* not a8 */
        addr8 = fetch();
        res16 = ~memRead16(addr8);
        memWrite16(addr8, res16);
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (!res16)
          psw |= PSW_Z;
        if (res16 & 0x8000)
          psw |= PSW_N;
        cycle(3);
        break;
      case 0x03: /* neg a8 */
        addr8 = fetch();
        sres16 = -(int16_t)memRead16(addr8);
        memWrite16(addr8, sres16);
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (!sres16) {
          psw |= PSW_Z;
          /* apparently, this is not a subtraction but a logical operation
             followed by the addition of 1 */
          psw |= PSW_C;
        }
        if (sres16 < 0) {
          psw |= PSW_N;
          psw |= PSW_V;	/* XXX: is that correct? */
          psw |= PSW_VT;
        }
        cycle(3);
        break;
      /* XXX: 04 XCH */
      case 0x05: /* dec a8 */
        target = fetch();
        val16 = memRead16(target);
        res32 = val16 - 1;
        memWrite16(target, res32 & 0xffff);
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        /* no clue if this makes any sense */
        if ((val16 & 0x8000) == 0 && (res32 & 0x8000) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        if (val16)
          psw |= PSW_C;	/* no borrow */
        if (!res32)
          psw |= PSW_Z;
        if (res32 & 0x8000)
          psw |= PSW_N;
        cycle(3);
        break;
      case 0x06: /* ext a8 */
        addr8 = fetch();
        sres32 = (int32_t)(int16_t)memRead16(addr8);
        memWrite32(addr8, sres32);
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (!sres32)
          psw |= PSW_Z;
        if (sres32 < 0)
          psw |= PSW_N;
        cycle(4);
        break;
      case 0x07: /* inc a8 */
        target = fetch();
        val16 = memRead16(target);
        res32 = val16 + 1;
        memWrite16(target, res32 & 0xffff);
        /* docs say to clear PSW_ST; looks fishy to me, though */
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V|PSW_ST);
        if ((val16 & 0x8000) == 0 && (res32 & 0x8000) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        if (val16 == 0xffff)
          psw |= PSW_C;
        if (!res32)
          psw |= PSW_Z;
        if (res32 & 0x8000)
          psw |= PSW_N;
        cycle(3);
        break;
      case 0x08: /* shr b8, imm8/a8 */
        imm8 = fetch();
        if (imm8 > 15)
          imm8 = memRead8(imm8);
        addr8 = fetch();
        val16 = memRead16(addr8);
        res32 = ((uint32_t)val16 << 16) >> imm8;
        memWrite16(addr8, res32 >> 16);
        cycleShift(6, imm8);

        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V|PSW_ST);
        if (!(res32 >> 16))
          psw |= PSW_Z;
        if (res32 & 0x8000)	/* one has been shifted put last */
          psw |= PSW_C;
        if (res32 & 0x7fff)	/* one has been shifted out of the carry */
          psw |= PSW_ST;
        break;
      case 0x09: /* shl b8, imm8/a8 */
        imm8 = fetch();
        if (imm8 > 15)
          imm8 = memRead8(imm8);
        addr8 = fetch();
        val16 = memRead16(addr8);
        res32 = (uint32_t)val16 << imm8;
        memWrite16(addr8, res32 & 0xffff);
        cycleShift(6, imm8);

        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (!(res32 & 0xffff))
          psw |= PSW_Z;
        if (res32 & 0x8000)
          psw |= PSW_N;
        if (res32 & 0x10000)
          psw |= PSW_C;
        if ((res32 & 0x8000) != (val16 & 0x8000)) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        break;
      case 0x0a: /* shra b8, imm8/a8 */
        imm8 = fetch();
        if (imm8 > 15)
          imm8 = memRead8(imm8);
        addr8 = fetch();
        val16 = memRead16(addr8);
        sres32 = (((int32_t)(int16_t)val16) << 16) >> imm8;
        memWrite16(addr8, sres32 >> 16);
        res32 = (uint32_t)sres32;
        cycleShift(6, imm8);

        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V|PSW_ST);
        if (!(res32 >> 16))
          psw |= PSW_Z;
        if (res32 & 0x8000)	/* one has been shifted put last */
          psw |= PSW_C;
        if (res32 & 0x80000000UL)
          psw |= PSW_N;
        if (res32 & 0x7fff)	/* one has been shifted out of the carry */
          psw |= PSW_ST;
        break;
      /* XXX: 0B XCH */
      case 0x0c: /* shrl b8, imm8/a8 */
        imm8 = fetch();
        if (imm8 > 15)
          imm8 = memRead8(imm8);
        addr8 = fetch();
        val32 = memRead32(addr8);
        res64 = ((uint64_t)val32 << 32) >> imm8;
        memWrite32(addr8, res64 >> 32);
        cycleShift(7, imm8);

        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V|PSW_ST);
        if (!(res64 >> 32))
          psw |= PSW_Z;
        if (res64 & 0x80000000UL)	/* one has been shifted out last */
          psw |= PSW_C;
        if (res64 & 0x7fffffffUL)	/* one has been shifted out of the carry */
          psw |= PSW_ST;
        break;
      case 0x0d: /* shll b8, imm8/a8 */
        imm8 = fetch();
        if (imm8 > 15) /* yes, the specs say 15, even though this is a 32-bit shift */
          imm8 = memRead8(imm8);
        addr8 = fetch();
        val32 = memRead32(addr8);
        res64 = ((uint64_t)val32) << imm8;
        memWrite32(addr8, res64 & 0xffffffffUL);
        /* timing not documented, guess based on timing for shrl */
        cycleShift(7, imm8);

        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (!(res64 & 0xffffffffUL))
          psw |= PSW_Z;
        if (res64 & 0x80000000UL)
          psw |= PSW_N;
        if (res64 & 0x100000000ULL)
          psw |= PSW_C;
        /* XXX overflow? */
        break;
      case 0x0e: /* shral b8, imm8/a8 */
        imm8 = fetch();
        if (imm8 > 15)
          imm8 = memRead8(imm8);
        addr8 = fetch();
        val32 = memRead32(addr8);
        sres64 = (((int64_t)(int32_t)val32) << 32) >> imm8;
        memWrite32(addr8, sres64 >> 32);
        res64 = (uint64_t)sres64;
        cycleShift(7, imm8);

        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V|PSW_ST);
        if (!(res64 >> 32))
          psw |= PSW_Z;
        if (res64 & 0x80000000UL)	/* one has been shifted out last */
          psw |= PSW_C;
        if (res64 & 0x8000000000000000ULL)
          psw |= PSW_N;
        if (res64 & 0x7fffffffUL)	/* one has been shifted out of the carry */
          psw |= PSW_ST;
        break;
      case 0x0f: /* norml b8, a8 */
        addr8 = fetch();
        addr8_2 = fetch();
        val32 = memRead32(addr8_2);
        val8 = 0;
        while (val8 < 31 && !(val32 & 0x80000000UL)) {
          val32 <<= 1;
          val8++;
        }
        memWrite32(addr8_2, val32);
        memWrite8(addr8, val8);
        psw &= ~(PSW_Z|PSW_C);
        /* docs: N flag undefined */
        if (val8 == 31 && !(val32 & 0x80000000UL))
          psw |= PSW_Z;
        cycleShift(8, val8);
        break;
      case 0x11: /* clrb a8 */
        memWrite8(fetch(), 0);
        psw &= ~(PSW_N|PSW_C|PSW_V);
        psw |= PSW_Z;
        cycle(3);
        break;
      case 0x12: /* notb a8 */
        addr8 = fetch();
        res8 = ~memRead8(addr8);
        memWrite8(addr8, res8);
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (!res8)
          psw |= PSW_Z;
        if (res8 & 0x80)
          psw |= PSW_N;
        cycle(3);
        break;
      case 0x13: /* negb a8 */
        addr8 = fetch();
        sres8 = -(int8_t)memRead8(addr8);
        memWrite8(addr8, sres8);
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (!sres8) {
          psw |= PSW_Z;
          psw |= PSW_C; /* see neg a8 */
        }
        if (sres8 < 0) {
          psw |= PSW_N;
          psw |= PSW_V;	/* XXX: is this correct? */
          psw |= PSW_VT;
        }
        cycle(3);
        break;
      case 0x14: /* xchb b8, a8 */
        addr8 = fetch();
        val8 = memRead16(addr8);
        addr8_2 = fetch();
        val8_2 = memRead16(addr8_2);
        memWrite8(addr8, val8_2);
        memWrite8(addr8_2, val8);
        cycle(5);
        break;
      case 0x15: /* decb a8 */
        target = fetch();
        val8 = memRead8(target);
        res16 = val8 - 1;
        memWrite8(target, res16 & 0xff);
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        /* no clue if this makes any sense */
        if ((val8 & 0x80) == 0 && (res16 & 0x80) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        if (val8)
          psw |= PSW_C;
        if (!res16)
          psw |= PSW_Z;
        if (res16 & 0x80)
          psw |= PSW_N;
        cycle(3);
        break;
      case 0x17: /* incb a8 */
        addr8 = fetch();
        val8 = memRead8(addr8);
        res8 = val8 + 1;
        memWrite8(addr8, res8);
        cycle(3);
        goto set_psw_add8;
      case 0x18: /* shrb b8, imm8/a8 */
        imm8 = fetch();
        if (imm8 > 15)
          imm8 = memRead8(imm8);
        addr8 = fetch();
        val8 = memRead8(addr8);
        res16 = (((uint16_t)val8) << 8) >> imm8;
        memWrite8(addr8, res16 >> 8);
        cycleShift(6, imm8);

        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V|PSW_ST);
        if (!(res16 >> 8))
          psw |= PSW_Z;
        if (res16 & 0x80)
          psw |= PSW_C;
        if (res16 & 0x7f)
          psw |= PSW_ST;
        break;
      case 0x19: /* shlb b8, imm8/a8 */
        imm8 = fetch();
        if (imm8 > 15)
          imm8 = memRead8(imm8);
        addr8 = fetch();
        val8 = memRead8(addr8);
        res16 = (uint16_t)val8 << imm8;
        memWrite8(addr8, res16 & 0xff);
        cycleShift(6, imm8);

        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (!(res16 & 0xff))
          psw |= PSW_Z;
        if (res16 & 0x80)
          psw |= PSW_N;
        if (res16 & 0x100)
          psw |= PSW_C;
        if ((res16 & 0x80) != (val8 & 0x80)) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        break;
      case 0x20:
      case 0x21:
      case 0x22:
      case 0x23:
      case 0x24:
      case 0x25:
      case 0x26:
      case 0x27:
    //  case 0x20 ... 0x27: /* sjmp rel11 */
        rel16 = ((int16_t)((fetch() | ((opcode & 0x7) << 8)) << 5)) >> 5;
        target = pc + rel16;
        if (target == pc - 2) {
          ERROR("ENDLESS LOOP at %04X (%08X) at %lld cycles!\n", pc - 2, virtToPhys(pc - 2, 1), (long long)cycles);
          ui->showWarning("Endless loop, resetting.");
          DEBUG(WARN, "resetting\n");
          reset();
          break;
        }
        pc = target;
        cycle(7);
        break;
      case 0x28:
      case 0x29:
      case 0x2A:
      case 0x2B:
      case 0x2C:
      case 0x2D:
      case 0x2E:
      case 0x2F:
    //  case 0x28 ... 0x2f: /* scall rel11 */
        val8 = fetch();
        rel16 = ((int16_t)((val8 | ((opcode & 0x7) << 8)) << 5)) >> 5;
        target = pc + rel16;
        push16(pc);
        pc = target;
        cycle(11);
        break;
      case 0x30:
      case 0x31:
      case 0x32:
      case 0x33:
      case 0x34:
      case 0x35:
      case 0x36:
      case 0x37:
//      case 0x30 ... 0x37: /* jbc rel8 */
        imm8 = 1 << (opcode & 0x7);
        addr8 = fetch();
        sval8 = (int8_t)fetch();
        target = pc + sval8;
        if (!(imm8 & memRead8(addr8))) {
          pc = target;
          DEBUG(OP, "JBC taken from %04X to %04X\n", opc, pc);
          cycle(4);
        }
        cycle(5);
        break;  
      case 0x38:
      case 0x39:
      case 0x3A:
      case 0x3B:
      case 0x3C:
      case 0x3D:
      case 0x3E:
      case 0x3F:
          //      case 0x38 ... 0x3f: /* jbs rel8 */
        imm8 = 1 << (opcode & 0x7);
        addr8 = fetch();
        sval8 = (int8_t)fetch();
        target = pc + sval8;
        if (imm8 & memRead8(addr8)) {
          pc = target;
          DEBUG(OP, "JBS taken from %04X to %04X\n", opc, target);
          cycle(4);
        }
        cycle(5);
        break;
      case 0x40: /* and c8, b8, a8 */
      case 0x41: /* and c8, b8, imm16 */
      case 0x42: /* and c8, b8, [a8](+) */
      case 0x43: /* and c8, b8, imm8/16[a8] */
        if (opcode == 0x40) {
          val16 = memRead16(fetch());
          cycle(5);
        }
        else if (opcode == 0x41) {
          val16 = fetch16();
          cycle(6);
        }
        else if (opcode == 0x42) {
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          val16 = memRead16(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 2);
            cycle(1);
          }
          cycleRM3(7, addr16);
        }
        else {
          addr16 = indexedAddr();
          val16 = memRead16(addr16);
          cycleRM3(7, addr16);
        }
        res16 = val16 & memRead16(fetch());
        memWrite16(fetch(), res16);
        goto set_psw_logical16;
      case 0x44: /* add c8, b8, a8 */
        val16 = memRead16(fetch());
        cycle(5);
do_add3:
        addr8 = fetch();
        res16 = memRead16(addr8) + val16;
        memWrite16(fetch(), res16);
        goto set_psw_add16;
      case 0x45: /* add c8, b8, imm16 */
        val16 = fetch16();
        cycle(6);
        goto do_add3;
      /* XXX: 46 ADD indirect */
      case 0x47: /* add c8, b8, imm8/16[a8] */
        addr16 = indexedAddr();
        val16 = memRead16(addr16);
        cycleRM3(7, addr16);
        goto do_add3;
      case 0x48: /* sub c8, b8, a8 */
      /* XXX: 49 SUB immediate */
      /* XXX: 4A SUB indirect */
      case 0x4b: /* sub c8, b8, imm8/16[a8] */
        if (opcode == 0x48) {
          imm16 = memRead16(fetch());
          cycle(5);
        }
        else {
          addr16 = indexedAddr();
          imm16 = memRead16(addr16);
          cycleRM3(7, addr16);
        }
        val16 = memRead16(fetch());
        res16 = val16 - imm16;
        memWrite16(fetch(), res16);
        goto set_psw_sub16;
      case 0x4c: /* mulu c8, b8, a8 */
      case 0x4d: /* mulu c8, b8, imm16 */
      /* XXX: 4E MULU indirect */
      case 0x4f: /* mulu c8, b8, imm8/16[a8] */
        if (opcode == 0x4c) {
          imm16 = memRead16(fetch());
          cycle(14);
        }
        else if (opcode == 0x4d) {
          imm16 = fetch16();
          cycle(15);
        }
        else {
          addr16 = indexedAddr();
          imm16 = memRead16(addr16);
          cycleRM3(17, addr16);
        }
        addr8 = fetch();
        addr8_2 = fetch();
        res32 = (uint32_t)memRead16(addr8) * (uint32_t)imm16;
        memWrite32(addr8_2, res32);
        /* docs: ST undefined */
        break;
      case 0x50: /* andb c8, b8, a8 */
      case 0x51: /* andb c8, b8, imm8 */
      /* XXX: 52 ANDB indirect */
      /* XXX: 53 ANDB indexed */
        if (opcode == 0x50) {
          imm8 = memRead8(fetch());
          cycle(5);
        }
        else {
          imm8 = fetch();
          cycle(6);
        }
        res8 = memRead8(fetch()) & imm8;
        target = fetch();
        memWrite8(target, res8);
        goto set_psw_logical8;
      case 0x54: /* addb c8, b8, a8 */
      case 0x55: /* addb c8, b8, imm8 */
      /* XXX: 56 ADDB indirect */
      case 0x57: /* addb c8, b8, imm8/16[a8] */
        if (opcode == 0x54) {
          val8_2 = memRead8(fetch());
          cycle(5);
        }
        else if (opcode == 0x55) {
          val8_2 = fetch();
          cycle(6);
        }
        else {
          addr16 = indexedAddr();
          val8_2 = memRead8(addr16);
          cycleRM3(7, addr16);
        }
        val8 = memRead8(fetch());
        addr8 = fetch();
        res8 = val8 + val8_2;
        memWrite8(addr8, res8);
set_psw_add8:
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if ((val8 & 0x80) == 0 && (res8 & 0x80) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        if (res8 < val8)
          psw |= PSW_C;
        if (!res8)
          psw |= PSW_Z;
        if (res8 >= 0x80)
          psw |= PSW_N;
        break;
      case 0x58: /* subb c8, b8, a8 */
      /* XXX: 59 SUBB immediate */
      /* XXX: 5A SUBB indirect */
      case 0x5b: /* subb c8, b8, imm8/16[a8] */
        if (opcode == 0x58) {
          imm8 = memRead8(fetch());
          cycle(5);
        }
        else {
          addr16 = indexedAddr();
          imm8 = memRead8(addr16);
          cycleRM3(7, addr16);
        }
        val8 = memRead8(fetch());
        res8 = val8 - imm8;
        memWrite8(fetch(), res8);
        goto set_psw_sub8;
      case 0x5c: /* mulub c8, b8, a8 */
      case 0x5d: /* mulub c8, b8, imm8 */
      /* XXX: 5E MULUB indirect */
      /* XXX: 5F MULUB indexed */
        if (opcode == 0x5c) {
          imm8 = memRead8(fetch());
          cycle(10);
        }
        else {
          imm8 = fetch();
          cycle(10);
        }
        addr8 = fetch();
        addr8_2 = fetch();
        res16 = (uint16_t)memRead8(addr8) * (uint16_t)imm8;
        memWrite16(addr8_2, res16);
        /* docs: ST undefined */
        break;
      case 0x60: /* and b8, a8 */
      case 0x61: /* and b8, imm16 */
      /* XXX: 62 AND indirect */
      case 0x63: /* and b8, [a8](+) */
        switch (opcode & 3) {
          case 0: 
            imm16 = memRead16(fetch());
            cycle(4);
            break;
          case 1:
            imm16 = fetch16();
            cycle(5);
            break;
          case 3:
            addr8 = fetch();
            addr16 = memRead16(addr8 & 0xfe);
            imm16 = memRead16(addr16);
            if (addr8 & 1) {
              memWrite16(addr8 & 0xfe, addr16 + 2);
              cycle(1);
            }
            cycle(6);
            break;
          default: goto illegal;
        }
        addr8 = fetch();
        res16 = imm16 & memRead16(addr8);
        memWrite16(addr8, res16);
        goto set_psw_logical16;
      case 0x64: /* add b8, a8 */
      case 0x65: /* add b8, imm16 */
      case 0x66: /* add b8, [a8](+) */
        if (opcode == 0x64) {
          val16 = memRead16(fetch());
          cycle(4);
        }
        else if (opcode == 0x65) {
          val16 = fetch16();
          cycle(5);
        }
        else {
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          val16 = memRead16(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 2);
            cycle(1);
          }
          cycle(6);
        }
        addr8 = fetch();
        res16 = memRead16(addr8) + val16;
        memWrite16(addr8, res16);
set_psw_add16:
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if ((val16 & 0x8000) == 0 && (res16 & 0x8000) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        if (res16 < val16)
          psw |= PSW_C;
        if (!res16)
          psw |= PSW_Z;
        if (res16 >= 0x8000)
          psw |= PSW_N;
        break;
      case 0x67: /* add c8, imm8/16[a8] */
        target = indexedAddr();
        val16 = memRead16(target);
        addr8 = fetch();
        res16 = memRead16(addr8) + val16;
        memWrite16(addr8, res16);
        cycleRM2(6, target);
        goto set_psw_add16;
      case 0x68: /* sub b8, a8 */
      case 0x69: /* sub b8, imm16 */
        if (opcode == 0x68) {
          imm16 = memRead16(fetch());
          cycle(4);
        }
        else {
          imm16 = fetch16();
          cycle(5);
        }
        addr8 = fetch();
        val16 = memRead16(addr8);
        res16 = val16 - imm16;
        memWrite16(addr8, res16);
        goto set_psw_sub16;
      case 0x6a: /* sub b8, [a8](+) */
      case 0x8a: /* cmp b8, [a8](+) */
        addr8 = fetch();
        addr16 = memRead16(addr8 & 0xfe);
        imm16 = memRead16(addr16);
        target = fetch();
        val16 = memRead16(target);
        res16 = val16 - imm16;
        if (opcode == 0x6a) { /* sub */
          memWrite16(target, res16);
        }
        if (addr8 & 1) {
          memWrite16(addr8 & 0xfe, addr16 + 2);
          cycle(1);
        }
        cycleRM2(6, addr16);
        goto set_psw_sub16;
      case 0x6b: /* sub b8, imm8/16[a8] */
        addr16 = indexedAddr();
        imm16 = memRead16(addr16);
        target = fetch();
        val16 = memRead16(target);
        res16 = val16 - imm16;
        memWrite16(target, res16);
        cycleRM2(6, addr16);
        goto set_psw_sub16;
      case 0x6c: /* mulu b8, a8 */
      case 0x6d: /* mulu b8, imm16 */
      case 0x6e: /* mulu b8, [a8](+) */
      case 0x6f: /* mulu b8, imm8/16[a8] */
        if (opcode == 0x6c) {
          imm16 = memRead16(fetch());
          cycle(14);
        }
        else if (opcode == 0x6d) {
          imm16 = fetch16();
          cycle(15);
        }
        else if (opcode == 0x6e) {
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          imm16 = memRead16(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 2);
            cycle(1);
          }
          cycleRM3(16, addr16); /* 3 cycles even though 2-op insn */
        }
        else {
          addr16 = indexedAddr();
          imm16 = memRead16(addr16);
          cycleRM3(17, addr16);
        }
        addr8 = fetch();
        res32 = (uint32_t)imm16 * (uint32_t)memRead16(addr8);
        memWrite32(addr8, res32);
        /* docs: ST undefined */
        break;
      case 0x70: /* andb b8, a8 */
      case 0x71: /* andb b8, imm8 */
      case 0x72: /* andb b8, [a8](+) */
      case 0x73: /* andb b8, imm8/16[a8] */
        if (opcode == 0x73) {
          addr16 = indexedAddr();
          imm8 = memRead8(addr16);
          cycleRM2(6, addr16);
        }
        else if (opcode == 0x72) {
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          imm8 = memRead8(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 1);
            cycle(1);
          }
          cycleRM2(6, addr16);
        }
        else if (opcode == 0x71) {
          imm8 = fetch();
          cycle(5);
        }
        else {
          imm8 = memRead8(fetch());
          cycle(4);
        }
        target = fetch();
        res8 = memRead8(target) & imm8;
        memWrite8(target, res8);
set_psw_logical8:
        psw &= ~(PSW_C|PSW_V|PSW_N|PSW_Z);
        if (res8 & 0x80)
          psw |= PSW_N;
        if (res8 == 0)
          psw |= PSW_Z;
        break;
      case 0x74: /* addb b8, a8 */
      case 0x75: /* addb b8, imm8 */
      case 0x76: /* addb b8, [a8](+) */
      case 0x77: /* addb b8, imm8/16[a8] */
        if (opcode == 0x74) {
          imm8 = memRead8(fetch());
          cycle(4);
        }
        else if (opcode == 0x75) {
          imm8 = fetch();
          cycle(5);
        }
        else if (opcode == 0x76) {
          addr8_2 = fetch();
          addr16 = memRead16(addr8_2 & 0xfe);
          imm8 = memRead8(addr16);
          if (addr8_2 & 1) {
            memWrite16(addr8_2 & 0xfe, addr16 + 1);
            cycle(1);
          }
          cycleRM2(6, addr16);
        }
        else {
          addr16 = indexedAddr();
          imm8 = memRead8(addr16);
          cycleRM2(6, addr16);
        }
        addr8 = fetch();
        val8 = memRead8(addr8);
        res8 = val8 + imm8;
        memWrite8(addr8, res8);
        goto set_psw_add8;
      case 0x78: /* subb b8, a8 */
      case 0x79: /* subb b8, imm8 */
      case 0x7a: /* subb b8, [a8](+) */
      case 0x7b: /* subb b8, imm8/16[a8] */
        if (opcode == 0x78) {
          imm8 = memRead8(fetch());
          cycle(4);
        }
        else if (opcode == 0x79) {
          imm8 = fetch();
          cycle(5);
        }
        else if (opcode == 0x7a) {
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          imm8 = memRead8(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 1);
            cycle(1);
          }
          cycleRM2(6, addr16);
        }
        else {
          addr16 = indexedAddr();
          imm8 = memRead8(addr16);
          cycleRM2(6, addr16);
        }
        addr8 = fetch();
        val8 = memRead8(addr8);
        res8 = val8 - imm8;
        memWrite8(addr8, res8);
        goto set_psw_sub8;
      /* XXX: 7C MULUB direct */
      case 0x7d: /* mulub b8, imm8 */
      /* XXX: 7E MULUB indirect */
      /* XXX: 7F MULUB indexed */
        {
          imm8 = fetch();
          target = fetch();
          res16 = (uint16_t)memRead8(target) * (uint16_t)imm8;
          memWrite16(target, res16);
          cycle(10);
          /* docs: ST undefined */
          break;
        }
      case 0x80: /* or b8, a8 */
      case 0x81: /* or b8, imm16 */
      /* XXX: 82 OR indirect */
      case 0x83: /* or b8, imm8/16[a8] */
        if (opcode == 0x80) {
          val16 = memRead16(fetch());
          cycle(4);
        }
        else if (opcode == 0x81) {
          val16 = fetch16();
          cycle(5);
        }
        else {
          addr16 = indexedAddr();
          val16 = memRead16(addr16);
          cycleRM2(6, addr16);
        }
        addr8 = fetch();
        res16 = val16 | memRead16(addr8);
        memWrite16(addr8, res16);
set_psw_logical16:
        psw &= ~(PSW_C|PSW_V|PSW_N|PSW_Z);
        if (res16 & 0x8000)
          psw |= PSW_N;
        if (res16 == 0)
          psw |= PSW_Z;
        break;
      case 0x84: /* xor b8, a8 */
      /* XXX: 85 XOR immediate */
      /* XXX: 86 XOR indirect */
      case 0x87: /* xor b8, imm8/16[a8] */
        if (opcode == 0x84) {
          val16 = memRead16(fetch());
          cycle(4);
        }
        else {
          addr16 = indexedAddr();
          val16 = memRead16(addr16);
          cycleRM2(6, addr16);
        }
        addr8 = fetch();
        res16 = val16 ^ memRead16(addr8);
        memWrite16(addr8, res16);
        goto set_psw_logical16;
      case 0x88: /* cmp b8, a8 */
        imm16 = memRead16(fetch());
        val16 = memRead16(fetch());
        res16 = val16 - imm16;
        DEBUG(OP, "comparing %04X and %04X -> %d\n", val16, imm16, res16);
        cycle(4);
        goto set_psw_sub16;
      case 0x89: /* cmp b8, imm16 */
        imm16 = fetch16();
        val16 = memRead16(fetch());
        res16 = val16 - imm16;
        DEBUG(OP, "comparing %04X and %04X -> %d\n", val16, imm16, res16);
        cycle(5);
set_psw_sub16:
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (val16 >= imm16)
          psw |= PSW_C;	/* no borrow */
        if (!res16)
          psw |= PSW_Z;
        if (res16 >= 0x8000)
          psw |= PSW_N;
        if ((val16 & 0x8000) == 0 && (res16 & 0x8000) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        break;
      /* XXX: 8A CMP indirect */
      case 0x8b: /* cmp c8, imm8/16[a8] */
        target = indexedAddr();
        imm16 = memRead16(target);
        val16 = memRead16(fetch());
        res16 = val16 - imm16;
        DEBUG(OP, "comparing %04X and %04X -> %d\n", val16, imm16, res16);
        cycleRM2(6, target);
        goto set_psw_sub16;
      case 0x8c: /* divu b8, a8 */
      case 0x8d: /* divu b8, imm16 */
      /* XXX: 8E DIVU indirect */
      case 0x8f: /* divu b8, imm8/16[a8] */
        if (opcode == 0x8c) {
          val16 = memRead16(fetch());
          cycle(24);
        }
        else if (opcode == 0x8d) {
          val16 = fetch16();
          cycle(25);
        }
        else {
          addr16 = indexedAddr();
          val16 = memRead16(addr16);
          cycleRM3(26, addr16);
        }
        target = fetch();
        val32 = memRead32(target);
        if (!val16)
          val16 = 1;
        memWrite16(target, val32 / val16);
        memWrite16(target + 2, val32 % val16);
        /* XXX: overflow? */
        break;
      case 0x90: /* orb b8, a8 */
      case 0x91: /* orb b8, imm8 */
      case 0x92: /* orb b8, [a8](+) */
      case 0x93: /* orb b8, imm8/16[a8] */
        if (opcode == 0x93) { /* indexed */
          addr16 = indexedAddr();
          imm8 = memRead8(addr16);
          cycleRM2(6, addr16);
        }
        else if (opcode == 0x92) { /* indirect */ 
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          imm8 = memRead8(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 1);
            cycle(1);
          }
          cycleRM2(6, addr16);
        }
        else if (opcode == 0x90) {
          imm8 = memRead8(fetch());
          cycle(4);
        }
        else {
          imm8 = fetch();
          cycle(5);
        }
        target = fetch();
        res8 = memRead8(target) | imm8;
        memWrite8(target, res8);
        goto set_psw_logical8;
      case 0x94: /* xorb b8, a8 */
      case 0x95: /* xorb b8, imm8 */
      case 0x96: /* xorb b8, [a8](+) */
      case 0x97: /* xorb b8, imm8/16[a8] */
        if (opcode == 0x97) {
          addr16 = indexedAddr();
          imm8 = memRead8(addr16);
          cycleRM2(6, addr16);
        }
        else if (opcode == 0x94) {
          imm8 = memRead8(fetch());
          cycle(4);
        }
        else if (opcode == 0x95) {
          imm8 = fetch();
          cycle(5);
        }
        else {
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          imm8 = memRead8(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 1);
            cycle(1);
          }
          cycleRM2(6, addr16);
        }
        target = fetch();
        res8 = memRead8(target) ^ imm8;
        memWrite8(target, res8);
        goto set_psw_logical8;
      case 0x98: /* cmpb b8, a8 */
      case 0x99: /* cmpb b8, imm8 */
        if (opcode == 0x98) {
          imm8 = memRead8(fetch());
          cycle(4);
        }
        else {
          imm8 = fetch();
          cycle(5);
        }
        val8 = memRead8(fetch());
        res8 = val8 - imm8;
        DEBUG(OP, "comparing %04X and %04X -> %d\n", val8, imm8, res8);
set_psw_sub8:
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V);
        if (val8 >= imm8)
          psw |= PSW_C;	/* no borrow */
        if (!res8)
          psw |= PSW_Z;
        if (res8 >= 0x80)
          psw |= PSW_N;
        if ((val8 & 0x80) == 0 && (res8 & 0x80) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        break;
      case 0x9a: /* cmpb b8, [a8](+) */
        addr8 = fetch();
        addr16 = memRead16(addr8 & 0xfe);
        imm8 = memRead8(addr16);
        val8 = memRead8(fetch());
        if (addr8 & 1) {
          memWrite16(addr8 & 0xfe, addr16 + 1);
          cycle(1);
        }
        cycleRM2(6, addr16);
        res8 = val8 - imm8;
        DEBUG(OP, "comparing %04X and %04X -> %d\n", val8, imm8, res8);
        goto set_psw_sub8;
      case 0x9b: /* cmpb b8, imm8/16[a8] */
        target = indexedAddr();
        imm8 = memRead8(target);
        addr8 = fetch();
        val8 = memRead8(addr8);
        res8 = val8 - imm8;
        DEBUG(OP, "comparing %04X and %04X -> %d\n", val8, imm8, res8);
        cycleRM2(6, target);
        goto set_psw_sub8;
      /* XXX: 9C DIVUB direct */
      case 0x9d: /* divub b8, imm8 */
      /* XXX: 9E DIVUB immediate */
      case 0x9f: /* divub b8, imm8/16[a8] */
        if (opcode == 0x9d) {
          imm8 = fetch();
          cycle(16);
        }
        else {
          target = indexedAddr();
          imm8 = memRead8(target);
          cycleRM3(19, target);
        }
        addr8 = fetch();
        val16 = memRead16(addr8);
        if (!imm8)
          imm8 = 1;
        memWrite8(addr8, val16 / imm8);
        memWrite8(addr8 + 1, val16 % imm8);
        /* XXX overflow */
        break;
      case 0xa0: /* ld b8, a8 */
        addr8 = fetch();
        addr8_2 = fetch();
        memWrite16(addr8_2, memRead16(addr8));
        cycle(4);
        break;
      case 0xa1: /* ld b8, imm16 */
        imm16 = fetch16();
        memWrite16(fetch(), imm16);
        cycle(5);
        break;
      case 0xa2: /* ld b8, [a8](+) */
        addr8 = fetch();
        addr16 = memRead16(addr8 & 0xfe);
        val16 = memRead16(addr16);
        memWrite16(fetch(), val16);
        if (addr8 & 1) {
          memWrite16(addr8 & 0xfe, addr16 + 2);
          cycle(1);
        }
        cycleRM3(5, addr16);
        break;
      case 0xa3: /* ld c8, imm8/16[a8] */
        addr16 = indexedAddr();
        memWrite16(fetch(), memRead16(addr16));
        cycleRM3(6, addr16);
        break;
      case 0xa4: /* addc b8, a8 */
      case 0xa5: /* addc b8, imm16 */
      /* XXX: A6 ADDC indirect */
      case 0xa7: /* addc c8, imm8/16[a8] */
        if (opcode == 0xa4) {
          val16 = memRead16(fetch());
          cycle(4);
        }
        else if (opcode == 0xa5) {
          val16 = fetch16();
          cycle(5);
        }
        else {
          addr16 = indexedAddr();
          val16 = memRead16(addr16);
          cycleRM2(6, addr16);
        }
        addr8 = fetch();
        res16 = memRead16(addr8) + val16;
        if (psw & PSW_C)
          res16++;
        memWrite16(addr8, res16);

        psw &= ~(PSW_N|PSW_V);
        if ((val16 & 0x8000) == 0 && (res16 & 0x8000) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        if (res16 < val16 || (res16 == val16 && (psw & PSW_C)))
          psw |= PSW_C;
        else
          psw &= ~PSW_C;
        /* addition with carry only clears the zero flag if appropriate, but
           it never sets it */
        if (res16)
          psw &= ~PSW_Z;
        if (res16 >= 0x8000)
          psw |= PSW_N;
        break;
      case 0xa8: /* subc b8, a8 */
      case 0xa9: /* subc b8, imm16 */
      case 0xaa: /* subc b8, [a8](+) */
      /* XXX: AB SUBC indexed */
        if (opcode == 0xa8) {
          imm16 = memRead16(fetch());
          cycle(4);
        }
        else if (opcode == 0xa9) {
          imm16 = fetch16();
          cycle(5);
        }
        else {
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          imm16 = memRead16(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 2);
            cycle(1);
          }
          cycleRM2(6, addr16);
        }
        addr8 = fetch();
        val16 = memRead16(addr8);
        res16 = val16 - imm16;
        if (!(psw & PSW_C))
          res16--;
        memWrite16(addr8, res16);

        if (val16 > imm16 || (val16 == imm16 && (psw & PSW_C)))
          psw |= PSW_C;	/* no borrow */
        else
          psw &= ~PSW_C; /* borrow */
        psw &= ~(PSW_N|PSW_V);
        /* subc only clears the zero flag if appropriate, it never sets it */
        if (res16)
          psw &= ~PSW_Z;
        if (res16 >= 0x8000)
          psw |= PSW_N;
        if ((val16 & 0x8000) == 0 && (res16 & 0x8000) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        break;
      case 0xac: /* ldbze b8, a8 */
      case 0xad: /* ldbze b8, imm8 */
      case 0xae: /* ldbze b8, [a8](+) */
      case 0xaf: /* ldbze c8, imm8/16[a8] */
        if (opcode == 0xac) {
          val16 = (uint16_t)memRead8(fetch());
          cycle(4);
        }
        else if (opcode == 0xad) {
          val16 = (uint16_t)fetch();
          cycle(4);
        }
        else if (opcode == 0xae) {
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          val16 = (uint16_t)memRead8(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 1);
            cycle(1);
          }
          cycleRM3(5, addr16);
        }
        else {
          addr16 = indexedAddr();
          val16 = (uint16_t)memRead8(addr16);
          cycleRM3(6, addr16);
        }
        memWrite16(fetch(), val16);
        break;
      case 0xb0: /* ldb b8, a8 */
        addr8 = fetch();
        memWrite8(fetch(), memRead8(addr8));
        cycle(4);
        break;
      case 0xb1: /* ldb b8, imm8 */
        imm8 = fetch();
        memWrite8(fetch(), imm8);
        cycle(5);
        break;
      case 0xb2: /* ldb b8, [a8](+) */
        addr8 = fetch();
        addr16 = memRead16(addr8 & 0xfe);
        memWrite8(fetch(), memRead8(addr16));
        if (addr8 & 1) {
          memWrite16(addr8 & 0xfe, addr16 + 1);
          cycle(1);
        }
        cycleRM3(5, addr16);
        break;
      case 0xb3: /* ldb c8, imm8/16[a8] */
        target = indexedAddr();
        imm8 = memRead8(target);
        memWrite8(fetch(), imm8);
        cycleRM3(6, target);
        break;
      case 0xb4: /* addcb b8, a8 */
      case 0xb5: /* addcb b8, imm8 */
      /* XXX: B6 ADDCB indirect */
      /* XXX: B7 ADDCB indexed */
        if (opcode == 0xb4) {
          imm8 = memRead8(fetch());
          cycle(4);
        }
        else {
          imm8 = fetch();
          cycle(5);
        }
        addr8 = fetch();
        val8 = memRead8(addr8);
        res8 = val8 + imm8;
        if (psw & PSW_C)
          res8++;
        memWrite8(addr8, res8);

        psw &= ~(PSW_N|PSW_V);
        if ((val8 & 0x80) == 0 && (res8 & 0x80) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        if (res8 < val8 || (res8 == val8 && (psw & PSW_C)))
          psw |= PSW_C;
        else
          psw &= ~PSW_C;
        /* addition with carry only clears the zero flag if appropriate, but
           it never sets it */
        if (res8)
          psw &= ~PSW_Z;
        if (res8 >= 0x80)
          psw |= PSW_N;
        break;
      case 0xb8: /* subcb b8, a8 */
      case 0xb9: /* subcb b8, imm8 */
      case 0xba: /* subcb b8, [a8](+) */
      /* XXX: BB SUBCB indexed */
        if (opcode == 0xb8) {
          imm8 = memRead8(fetch());
          cycle(4);
        }
        else if (opcode == 0xb9) {
          imm8 = fetch();
          cycle(5);
        }
        else {
          addr8 = fetch();
          addr16 = memRead16(addr8 & 0xfe);
          imm8 = memRead8(addr16);
          if (addr8 & 1) {
            memWrite16(addr8 & 0xfe, addr16 + 1);
            cycle(1);
          }
          cycleRM2(6, addr16);
        }
        addr8 = fetch();
        val8 = memRead8(addr8);
        res8 = val8 - imm8;
        if (!(psw & PSW_C))
          res8--;
        memWrite8(addr8, res8);

        if (val8 > imm8 || (val8 == imm8 && (psw & PSW_C)))
          psw |= PSW_C;	/* no borrow */
        else
          psw &= ~PSW_C;	/* borrow */
        psw &= ~(PSW_N|PSW_V);
        /* subc only clears the zero flag if appropriate, it never sets it */
        if (res8)
          psw &= ~PSW_Z;
        if (res8 >= 0x80)
          psw |= PSW_N;
        if ((val8 & 0x80) == 0 && (res8 & 0x80) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        break;
      case 0xbc: /* ldbse b8, a8 */
      case 0xbd: /* ldbse b8, imm8 */
      /* XXX: BE LDBSE indirect */
      /* XXX: BF LDBSE indexed */
        if (opcode == 0xbc) {
          val16 = (int16_t)(int8_t)memRead8(fetch());
        }
        else {
          val16 = (int16_t)(int8_t)fetch();
        }
        memWrite16(fetch(), val16);
        cycle(4);
        break;
      case 0xc0: /* st b8, a8 */
        addr8 = fetch();
        memWrite16(addr8, memRead16(fetch()));
        cycle(4);
        break;
      /* XXX: C1 BMOV */
      case 0xc2: /* st b8, [a8](+) */
        addr8 = fetch();
        addr8_2 = fetch();
        val16 = memRead16(addr8_2);
        addr16 = memRead16(addr8 & 0xfe);
        memWrite16(addr16, val16);
        if (addr8 & 1) {
          memWrite16(addr8 & 0xfe, addr16 + 2);
          cycle(1);
        }
        cycleRM3(5, addr16);
        break;
      case 0xc3: /* st c8, imm8/16[a8] */
        target = indexedAddr();
        memWrite16(target, memRead16(fetch()));
        cycleRM3(6, target);
        break;
      case 0xc4: /* stb b8, a8 */
        addr8 = fetch();
        memWrite8(addr8, memRead8(fetch()));
        cycle(4);
        break;
      case 0xc5: /* cmpl b8, a8 */
        addr8 = fetch();
        if (!addr8) {
          /* even though there's nothing about this in the docs,
             this is the only behavior that makes sense; reading
             random I/O registers certainly doesn't */
          imm32 = 0;
        }
        else {
          imm32 = memRead32(addr8);
        }
        addr8 = fetch();
        if (!addr8) {
          /* even though there's nothing about this in the docs,
             this is the only behavior that makes sense; reading
             random I/O registers certainly doesn't */
          val32 = 0;
        }
        else {
          val32 = memRead32(addr8);
        }
        res32 = val32 - imm32;
        DEBUG(OP, "comparing long %08X and %08X -> %d\n", val32, imm32, res32);
        cycle(7);

        /* docs are not very explicit about it, but it seems CMPL handles VT
           differently from all other insns, so we clear it, too */
        psw &= ~(PSW_Z|PSW_N|PSW_C|PSW_V|PSW_VT);
        if (val32 >= imm32)
          psw |= PSW_C;	/* no borrow */
        if (!res32)
          psw |= PSW_Z;
        if (res32 >= 0x80000000UL)
          psw |= PSW_N;
        if ((val32 & 0x80000000UL) == 0 && (res32 & 0x80000000UL) == 1) {
          psw |= PSW_V;
          psw |= PSW_VT;
        }
        break;
      case 0xc6: /* stb b8, [a8](+) */
        addr8 = fetch();
        addr8_2 = fetch();
        addr16 = memRead16(addr8 & 0xfe);
        val8 = memRead8(addr8_2);
        memWrite8(addr16, val8);
        if (addr8 & 1) {
          memWrite16(addr8 & 0xfe, addr16 + 1);
          cycle(1);
        }
        cycleRM3(5, addr16);
        break;
      case 0xc7: /* stb c8, imm8/16[a8] */
        target = indexedAddr();
        val8 = memRead8(fetch());
        memWrite8(target, val8);
        cycleRM3(6, target);
        break;
      case 0xc8: /* push a8 */
        addr8 = fetch();
        push16(memRead16(addr8));
        cycle(8);
        break;
      case 0xc9: /* push imm16 */
        imm16 = fetch16();
        push16(imm16);
        cycle(9);
        break;
      case 0xca: /* push [a8](+?) */
        addr8 = fetch();
        addr16 = memRead16(addr8 & 0xfe);
        push16(memRead16(addr16));
        if (addr8 & 1) {
          memWrite16(addr8 & 0xfe, addr16 + 2);
          cycle(1);
        }
        cycleRM3(11, addr16);
        break;
      case 0xcb: /* push imm8/16[a8] */
        target = indexedAddr();
        push16(memRead16(target));
        cycleRM3(12, target);
        break;
      case 0xcc: /* pop a8 */
        addr8 = fetch();
        memWrite16(addr8, pop16());
        cycle(11);
        break;
      case 0xcd: /* bmovi b8, a8 */ {
          /* XXX: interruptible? */
          uint16_t count = memRead16(fetch());
          uint8_t ptrs = fetch();
          uint16_t src = memRead16(ptrs);
          uint16_t dest = memRead16(ptrs + 2);
          int cycle_inc = 8;
          if (src > 0x200)
            cycle_inc += 3;
          if (dest > 0x200)
            cycle_inc += 3;
          for (; count; src+=2, dest+=2, count--) {
            memWrite16(dest, memRead16(src));
            cycle(cycle_inc);
          }
          memWrite16(ptrs, src);
          memWrite16(ptrs + 2, dest);
          cycle(7);
        }
        break;
      /* XXX: CE POP indirect */
      /* XXX: CF POP indexed */
      case 0xd0: /* jnst rel8 */
        rel8 = (int8_t)fetch();
        if (!(psw & PSW_ST)) {
          target = pc + rel8;
          DEBUG(OP, "JNST taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      case 0xd1: /* jnh rel8 */
        rel8 = (int8_t)fetch();
        if ((psw & PSW_Z) || !(psw & PSW_C)) {
          target = pc + rel8;
          DEBUG(OP, "JNH taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      case 0xd2: /* jgt rel8 */
        rel8 = (int8_t)fetch();
        if (!(psw & PSW_N) && !(psw & PSW_Z)) {
          target = pc + rel8;
          DEBUG(OP, "JGT taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      case 0xd3: /* jnc rel8 */
        rel8 = (int8_t)fetch();
        if (!(psw & PSW_C)) {
          target = pc + rel8;
          DEBUG(OP, "JNC taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      /* XXX: D4 JNVT */
      /* XXX: D5 JNV */
      case 0xd6: /* jge rel8 */
        rel8 = (int8_t)fetch();
        if (!(psw & PSW_N)) {
          target = pc + rel8;
          DEBUG(OP, "JGE taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      case 0xd7: /* jne rel8 */
        rel8 = (int8_t)fetch();
        if (!(psw & PSW_Z)) {
          target = pc + rel8;
          DEBUG(OP, "JNE taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      /* XXX: D8 JST */
      case 0xd9: /* jh rel8 */
        rel8 = (int8_t)fetch();
        if (!(psw & PSW_Z) && (psw & PSW_C)) {
          target = pc + rel8;
          DEBUG(OP, "JH taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      case 0xda: /* jle rel8 */
        rel8 = (int8_t)fetch();
        if ((psw & PSW_N) || (psw & PSW_Z)) {
          target = pc + rel8;
          DEBUG(OP, "JLE taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      case 0xdb: /* jc rel8 */
        rel8 = (int8_t)fetch();
        if (psw & PSW_C) {
          target = pc + rel8;
          DEBUG(OP, "JC taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      /* XXX: DC JVT */
      /* XXX: DD JV */
      case 0xde: /* jlt rel8 */
        rel8 = (int8_t)fetch();
        if (psw & PSW_N) {
          target = pc + rel8;
          DEBUG(OP, "JLT taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      case 0xdf: /* je rel8 */
        rel8 = (int8_t)fetch();
        if (psw & PSW_Z) {
          target = pc + rel8;
          DEBUG(OP, "JE taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(4);
        break;
      case 0xe0: /* djnz a8, rel8 */
        addr8 = fetch();
        val8 = memRead8(addr8) - 1;
        memWrite8(addr8, val8);
        rel8 = (int8_t)fetch();
        if (val8) {
          target = pc + rel8;
          DEBUG(OP, "DJNZ taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(5);
        break;
      case 0xe1: /* djnzw a8, rel8 */
        addr8 = fetch();
        val16 = memRead16(addr8) - 1;
        memWrite16(addr8, val16);
        rel8 = (int8_t)fetch();
        if (val16) {
          target = pc + rel8;
          DEBUG(OP, "DJNZW taken from %04X to %04X\n", opc, target);
          pc = target;
          cycle(4);
        }
        cycle(6); /* maybe 7, docs are not clear */
        break;
      /* XXX: E2 TIJMP */
      case 0xe3: /* br [a8] */
        target = memRead16(fetch());
        pc = target;
        cycle(7);
        break;
      case 0xe7: /* ljmp rel16 */
        sval16 = (int16_t)fetch16();
        target = pc + sval16;
        DEBUG(OP, "LJMP from %04X to %04X\n", opc, target);
        pc = target;
        cycle(7);
        break;
      case 0xec: /* dpts */
        psw &= ~PSW_PTSE;
        cycle(2);
        break;
      case 0xed: /* epts */
        psw |= PSW_PTSE;
        cycle(2);
        break;
      case 0xef: /* lcall rel16 */
        sval16 = (int16_t)fetch16();
        target = pc + sval16;
        DEBUG(OP, "LCALL %04X\n", target);
        push16(pc);
        pc = target;
        cycle(13);
        break;
      case 0xf0: /* ret */
        pc = pop16();
        cycle(14);
        break;
      case 0xf2: /* pushf */
        push16((psw << 8) | int_mask);
        psw = int_mask = 0;
        cycle(8);
        break;
      /* XXX: F3 POPF */
      case 0xf4: /* pusha */
        push16((psw << 8) | int_mask);
        push16((int_mask1 << 8) | wsr);
        psw = int_mask = int_mask1 = 0;
        cycle(18);
        break;
      case 0xf5: /* popa */
        val16 = pop16();
        int_mask1 = val16 >> 8;
        wsr = val16 & 0xff;
        val16 = pop16();
        psw = val16 >> 8;
        int_mask = val16 & 0xff;
        cycle(18);
        break;
      /* XXX: F6 IDLPD */
      /* XXX: F7 TRAP */
      case 0xf8: /* clrc */
        psw &= ~PSW_C;
        cycle(2);
        break;
      case 0xf9: /* setc */
        psw |= PSW_C;
        cycle(2);
        break;
      case 0xfa: /* di */
        DEBUG(OP, "INTERRUPTS disabled\n");
        psw &= ~PSW_INTE;
        cycle(2);
        break;
      case 0xfb: /* ei */
        DEBUG(OP, "INTERRUPTS enabled\n");
        psw |= PSW_INTE;
        cycle(2);
        break;
      case 0xfc: /* clrvt */
        psw &= ~PSW_VT;
        cycle(2);
        break;
      /* XXX: FD NOP */
      case 0xfe:
        eopcode = fetch();
        switch (eopcode) {
          /* XXX: signed multiplications */
          /* XXX: DIVB */
          case 0x8c: /* div b8, a8 */
          case 0x8d: /* div b8, imm16 */
          /* XXX: FE 8E DIV indirect */
          case 0x8f: /* div b8, imm8/16[a8] */
            if (eopcode == 0x8c) {
              simm16 = (int16_t)memRead16(fetch());
              cycle(26);
            }
            else if (eopcode == 0x8d) {
              simm16 = (int16_t)fetch16();
              cycle(27);
            }
            else {
              addr16 = indexedAddr();
              simm16 = (int16_t)memRead16(addr16);
              cycleRM3(29, addr16);
            }
            addr8 = fetch();
            sval32 = (int32_t)memRead32(addr8);
            if (!simm16)
              simm16 = 1;
            memWrite16(addr8, sval32 / simm16);
            memWrite16(addr8 + 2, sval32 % simm16);
            psw &= ~PSW_V;
            /* XXX overflow? */
            break;
          default:
            ERROR("ILLEGAL OPCODE %02X %02X at %04X (%08X)\n", opcode, eopcode, opc, virtToPhys(opc, 1));
            goto illegal_out;
        };
        break;
      default:
illegal:
        ERROR("ILLEGAL OPCODE %02X at %04X (%08X)\n", opcode, opc, virtToPhys(opc, 1));
illegal_out:
#ifdef NDEBUG
        char iop[40];
        sprintf(iop, "Illegal opcode %02X at %04X", opcode, opc);
        ui->fatalError(iop);
        reset();
        break;
#else
        dumpMem();
        ui->quit();
        return 1;
#endif
    };
#ifdef LATENCY
    if (do_latency) {
      gettimeofday(&tv2, NULL);
      uint32_t latency = (tv2.tv_sec - tv.tv_sec) * 1000000 + ((int)tv2.tv_usec - (int)tv.tv_usec);
      uint32_t ppc = virtToPhys(opc, 1);
      if (latency > mem_latency[ppc].max)
        mem_latency[ppc].max = latency;
      if (latency < mem_latency[ppc].min)
        mem_latency[ppc].min = latency;
      mem_latency[ppc].total += latency;
      mem_latency[ppc].count++;
      mem_latency[ppc].cycles = (uint8_t)(getCycles() - lat_old_cycles);
      mem_latency[ppc].opcode = opcode;
    }
#endif
#ifdef NDEBUG
    }
#endif
#ifndef NDEBUG
    debug_level = debug_level_unabridged;
#endif
  }
}
