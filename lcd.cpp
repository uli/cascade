/*
 * lcd.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "lcd.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>

#define LAYER2_OFFSET 0x2580

#ifndef NDEBUG
static int coords_x(uint16_t cursor)
{
  if (cursor >= LAYER2_OFFSET)
    cursor -= LAYER2_OFFSET;
  return cursor * 8 % SRC_WIDTH;
}

static int coords_y(uint16_t cursor)
{
  if (cursor >= LAYER2_OFFSET)
    cursor -= LAYER2_OFFSET;
  return cursor * 8 / SRC_WIDTH;
}
#endif

Lcd::Lcd(UI *ui)
{
  mem = (uint8_t *)malloc(65536);
  this->ui = ui;
  reset();
}

Lcd::~Lcd()
{
  free(mem);
}

void Lcd::reset()
{
  memset(mem, 0, 65536);
  state = CMD_IDLE;
  next_param = 0;
  cursor = 0;
  dirty = true;
}

uint8_t Lcd::read(int a0)
{
  uint8_t ret = 0xff;
  if (a0 == LCD_READ_STATUS) {
    ERROR("LCD unimplemented status read\n");
    exit(1);
  }
  else {
    switch (state) {
      case CMD_MREAD:
        ret = mem[cursor];
        DEBUG(LCD, "LCD memread %02X ('%c') <- %04X (%d/%d)\n", ret, ret, cursor, coords_x(cursor), coords_y(cursor));
        cursor++;
        break;
      default:
        ERROR("LCD unknown state when reading\n");
        exit(1);
        break;
    };
  }
  DEBUG(LCD, "LCD read %d: %02X\n", a0, ret);
  return ret;
}

void Lcd::write(int a0, uint8_t val)
{
  DEBUG(LCD, "LCD write %d: %02X\n", a0, val);
  if (a0 == LCD_WRITE_COMMAND) {
    switch (val) {
      case 0x42:
        state = CMD_MWRITE;
        break;
      case 0x43:
        state = CMD_MREAD;
        break;
      case 0x46:
        state = CMD_CSRW;
        next_param = 0;
        break;
      case 0x58:
      case 0x59:
        DEBUG(LCD, "LCD display %s\n", (val & 1) ? "on" : "off");
        display_on = val & 1;
        state = CMD_DISPONOFF;
        next_param = 0;
        break;
      case 0x5b:
        state = CMD_OVLAY;
        next_param = 0;
        break;
      default:
        ERROR("LCD unimplemented command %02X\n", val);
        exit(1);
        break;
    };
  }
  else {
    switch (state) {
      case CMD_MWRITE:
        DEBUG(LCD, "LCD memwrite %02X ('%c') -> %04X (%d/%d)\n", val, val, cursor, coords_x(cursor), coords_y(cursor));
        mem[cursor] = val;
        dirty = true;
        cursor++;
        break;
      case CMD_CSRW:
        if (next_param == 0)
          cursor = val;
        else if (next_param == 1) {
          cursor |= val << 8;
          DEBUG(LCD, "LCD cursor write %04X (%d/%d)\n", cursor, coords_x(cursor), coords_y(cursor));
        }
        else {
          ERROR("LCD error in CSRW\n");
          exit(1);
        }
        next_param++;
        break;
      case CMD_OVLAY:
        if (next_param == 0) {
          mx = val & 3;
          /* dm is actually two bits, but they are required to be
             the same, so we just use one */
          dm = (val >> 2) & 1;
          ov = (val >> 4) & 1;
          DEBUG(LCD, "LCD overlay mx %d dm %d ov %d\n", mx, dm, ov);
        }
        else {
          ERROR("LCD error in OVLAY\n");
          exit(1);
        }
        next_param++;
        break;
      case CMD_DISPONOFF:
        if (next_param == 0) {
          fc = val & 3;
          fp1 = (val >> 2) & 3; /* attributes first block */
          fp2_4 = (val >> 4) & 3; /* second, fourth block */
          fp3 = (val >> 6) & 3; /* third block */
          DEBUG(LCD, "LCD fc %02X fp1 %02X fp2/4 %02X fp3 %02X\n", fc, fp1, fp2_4, fp3);
        }
        else {
          ERROR("LCD error in DISP ON/OFF\n");
          exit(1);
        }
        break;
      default:
        ERROR("LCD unknown state %d when writing %02X to %d\n",
                 state, val, a0);
        exit(1);
        break;
    };
  }
}

void Lcd::update()
{
  if (dirty) {
    uint16_t *pp = (uint16_t *)ui->getPixels();
    if (!pp)	/* painting disabled */
      return;
    dirty = false;
    
    int i, j, lc;
#ifdef SCALE_SCREEN
    int line = 0;
#endif
    uint8_t pixels;
    int display_x = ui->screenStep();
    pp += ui->screenX();
    pp += ui->screenY() * display_x;
    for (i = 0, lc = 0; i < SRC_WIDTH*SRC_HEIGHT; i+=8, lc+=8) {
      if (lc == SRC_WIDTH) {
        pp += display_x - DST_WIDTH;
#ifdef SCALE_SCREEN
        if (line % DOUBLE_Y_EVERY == (DOUBLE_Y_EVERY - 1)) {
          /* copy previous line */
          memcpy(pp, pp - display_x, DST_WIDTH * 2);
          pp += display_x;
        }
        line++;
#endif
        lc = 0;
      }
      pixels = (mem[i/8]) ^ (mem[LAYER2_OFFSET + i/8]);
      for (j = 0; j < 8; j++) {
        *pp++ = (pixels >> 7) - 1;
#ifdef SCALE_SCREEN
        if (j % DOUBLE_X_EVERY == DOUBLE_X_EVERY - 1)
          *pp++ = (pixels >> 7) - 1;
#endif
        pixels <<= 1;
      }
    }
#ifdef SCALE_SCREEN
    /* copy previous line */
    memcpy(pp, pp - display_x, DST_WIDTH * 2);
#endif
    ui->setDirty();
  }
  ui->flip();
}

#include "state.h"

void Lcd::loadSaveState(statefile_t fp, bool write)
{
  STATE_RWBUF(mem, 65536);
  STATE_RW(state);
  STATE_RW(next_param);
  STATE_RW(cursor);
  STATE_RW(mx);
  STATE_RW(dm);
  STATE_RW(ov);
  STATE_RW(display_on);
  STATE_RW(fc);
  STATE_RW(fp1); STATE_RW(fp2_4); STATE_RW(fp3);
}

void Lcd::redraw()
{
  dirty = true;
  update();
}
