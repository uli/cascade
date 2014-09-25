/*
 * keypad.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "keypad.h"
#include "ui.h"

Keypad::Keypad(Cpu *cpu, UI *ui)
{
  this->cpu = cpu;
  this->ui = ui;
  key[0] = key[1] = key[2] = key[3] = 0x7f;
}

void Keypad::update()
{
  struct Event e;
  for (;;) {
    if (cpu->isReplaying()) {
      bool found_event = false;
      for (;;) {
        e = cpu->retrieveEvent(EVENT_KEYDOWN);
        if (e.type == EVENT_INVALID) {
          e = cpu->retrieveEvent(EVENT_KEYUP);
          if (e.type == EVENT_INVALID)
            break;
        }
        if (e.type == EVENT_KEYDOWN) {
          found_event = true;
          break;
        }
        if (e.type == EVENT_KEYUP) {
          found_event = true;
          break;
        }
      }
      if (!found_event && !ui->pollEvent(e))
        break;
    }
    else {
      if (!ui->pollEvent(e))
        break;
    }
    if (e.type == EVENT_KEYDOWN) {
      DEBUG(KEY, "%016llu KEY pressed: %d\n", (unsigned long long)cpu->getCycles(), e.value);
      cpu->recordEvent(EVENT_KEYDOWN, e.value);
      switch (e.value) {
        case UIKEY_m:
          DEBUG(WARN, "--------- MARK -------------\n");
          break;
        case UIKEY_t:
          debug_level |= DEBUG_DEFAULT | DEBUG_TRACE | DEBUG_MEM | DEBUG_OP | DEBUG_IO;
          //debug_level &= ~DEBUG_ABRIDGED;
          break;
        case UIKEY_z:
          DEBUG(WARN, "NMI triggered!\n");
          cpu->push16(cpu->pc);
          cpu->pc = cpu->memRead16(0x203e);
          //debug_level |= DEBUG_TRACE | DEBUG_MEM;
          break;
        case UIKEY_LSHIFT:
        case UIKEY_RSHIFT:	SET_KEY(HI_SHIFT); break;
        case UIKEY_KP_ENTER:
        case UIKEY_RETURN:	SET_KEY(HI_RETURN); break;
        case UIKEY_ESCAPE:	SET_KEY(HI_ESCAPE); break;
        case UIKEY_UP:	SET_KEY(HI_UP); break;
        case UIKEY_DOWN:	SET_KEY(HI_DOWN); break;
        case UIKEY_LEFT:	SET_KEY(HI_LEFT); break;
        case UIKEY_RIGHT:	SET_KEY(HI_RIGHT); break;
        case UIKEY_BACKSPACE: SET_KEY(HI_UNDO); break;
        case UIKEY_n:	SET_KEY(HI_NO); break;
        case UIKEY_y:	SET_KEY(HI_YES); break;
        case UIKEY_b:	SET_KEY(HI_BACKLIGHT); break;
        case UIKEY_F1:	SET_KEY(HI_F1); break;
        case UIKEY_F2:	SET_KEY(HI_F2); break;
        case UIKEY_F3:	SET_KEY(HI_F3); break;
        case UIKEY_F4:	SET_KEY(HI_F4); break;
        case UIKEY_F5:	SET_KEY(HI_F5); break;
        case UIKEY_F6:	SET_KEY(HI_F6); break;
        case UIKEY_F12:	SET_KEY(HI_HELP); break;
        case UIKEY_0:
        case UIKEY_KP0:	SET_KEY(HI_0); break;
        case UIKEY_1:
        case UIKEY_KP1:	SET_KEY(HI_1); break;
        case UIKEY_2:
        case UIKEY_KP2:	SET_KEY(HI_2); break;
        case UIKEY_3:
        case UIKEY_KP3:	SET_KEY(HI_3); break;
        case UIKEY_4:
        case UIKEY_KP4:	SET_KEY(HI_4); break;
        case UIKEY_5:
        case UIKEY_KP5:	SET_KEY(HI_5); break;
        case UIKEY_6:
        case UIKEY_KP6:	SET_KEY(HI_6); break;
        case UIKEY_7:
        case UIKEY_KP7:	SET_KEY(HI_7); break;
        case UIKEY_8:
        case UIKEY_KP8:	SET_KEY(HI_8); break;
        case UIKEY_9:
        case UIKEY_KP9:	SET_KEY(HI_9); break;
#if 0
        case UIKEY_0 ... UIKEY_7:
          {
            int irq = e.value - UIKEY_0;
            DEBUG(WARN, "IRQ hi %d triggered!\n", irq);
            push16(pc);
            pc = memRead16(0x2030 + irq * 2);
          }
          break;
        case UIKEY_a ... UIKEY_h:
          {
            int irq = e.value - UIKEY_a;
            if (int_mask & (1 << irq)) {
              DEBUG(WARN, "IRQ lo %d triggered!\n", irq);
              push16(pc);
              pc = memRead16(0x2000 + irq * 2);
              //debug_level |= DEBUG_TRACE | DEBUG_MEM;
            }
            else {
              DEBUG(WARN, "IRQ lo %d disabled, not triggered!\n", irq);
            }
          }
          break;
#endif
        case UIKEY_F10:
          cpu->sendCommand(CPU_CMD_EXIT); break;
        case UIKEY_s:
          cpu->sendCommand(CPU_CMD_SAVE); break;
        case UIKEY_l:
          cpu->sendCommand(CPU_CMD_LOAD); break;
        case UIKEY_r:
          cpu->sendCommand(CPU_CMD_RESET); break;
        case UIKEY_f:
          cpu->sendCommand(CPU_CMD_FRESET); break;
        case UIKEY_F7:
          cpu->sendCommand(CPU_CMD_RECORD); break;
        case UIKEY_F8:
          cpu->sendCommand(CPU_CMD_PLAY); break;
        case UIKEY_F9:
          cpu->sendCommand(CPU_CMD_STOP_RECPLAY); break;
#ifdef LATENCY
        case UIKEY_l:
          cpu->do_latency = true;
          ui->writeText(660, 300, "LATENCY");
          break;
#endif
        case UIKEY_e:
          cpu->sendCommand(CPU_CMD_TOGGLE_ECHO); break;
        default:
          break;
      }
    }
    else if (e.type == EVENT_KEYUP) {
      DEBUG(KEY, "%016llu KEY releasd: %d\n", (unsigned long long)cpu->getCycles(), e.value);
      cpu->recordEvent(EVENT_KEYUP, e.value);
      switch (e.value) {
        case UIKEY_LSHIFT:
        case UIKEY_RSHIFT:	CLEAR_KEY(HI_SHIFT); break;
        case UIKEY_KP_ENTER:
        case UIKEY_RETURN:	CLEAR_KEY(HI_RETURN); break;
        case UIKEY_ESCAPE:	CLEAR_KEY(HI_ESCAPE); break;
        case UIKEY_UP:	CLEAR_KEY(HI_UP); break;
        case UIKEY_DOWN:	CLEAR_KEY(HI_DOWN); break;
        case UIKEY_LEFT:	CLEAR_KEY(HI_LEFT); break;
        case UIKEY_RIGHT:	CLEAR_KEY(HI_RIGHT); break;
        case UIKEY_BACKSPACE: CLEAR_KEY(HI_UNDO); break;
        case UIKEY_n:	CLEAR_KEY(HI_NO); break;
        case UIKEY_y:	CLEAR_KEY(HI_YES); break;
        case UIKEY_b:	CLEAR_KEY(HI_BACKLIGHT); break;
        case UIKEY_F1:	CLEAR_KEY(HI_F1); break;
        case UIKEY_F2:	CLEAR_KEY(HI_F2); break;
        case UIKEY_F3:	CLEAR_KEY(HI_F3); break;
        case UIKEY_F4:	CLEAR_KEY(HI_F4); break;
        case UIKEY_F5:	CLEAR_KEY(HI_F5); break;
        case UIKEY_F6:	CLEAR_KEY(HI_F6); break;
        case UIKEY_F12:	CLEAR_KEY(HI_HELP); break;
        case UIKEY_0:
        case UIKEY_KP0:	CLEAR_KEY(HI_0); break;
        case UIKEY_1:
        case UIKEY_KP1:	CLEAR_KEY(HI_1); break;
        case UIKEY_2:
        case UIKEY_KP2:	CLEAR_KEY(HI_2); break;
        case UIKEY_3:
        case UIKEY_KP3:	CLEAR_KEY(HI_3); break;
        case UIKEY_4:
        case UIKEY_KP4:	CLEAR_KEY(HI_4); break;
        case UIKEY_5:
        case UIKEY_KP5:	CLEAR_KEY(HI_5); break;
        case UIKEY_6:
        case UIKEY_KP6:	CLEAR_KEY(HI_6); break;
        case UIKEY_7:
        case UIKEY_KP7:	CLEAR_KEY(HI_7); break;
        case UIKEY_8:
        case UIKEY_KP8:	CLEAR_KEY(HI_8); break;
        case UIKEY_9:
        case UIKEY_KP9:	CLEAR_KEY(HI_9); break;
        default:
          break;
      }
    }
  }
}

uint8_t Keypad::getLine(int line)
{
  return key[line];
}

#include "state.h"

void Keypad::loadSaveState(statefile_t fp, bool write)
{
  STATE_RWBUF(key, 4);
}
