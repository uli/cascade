/*
 * lcd.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef __LCD_H
#define __LCD_H

#include "ui.h"

#include <stdint.h>
#include <stdio.h>

#define LCD_WRITE_DATA 0
#define LCD_WRITE_COMMAND 1

#define LCD_READ_DATA 1
#define LCD_READ_STATUS 0

typedef enum {
  CMD_IDLE = 0,
  CMD_SYSTEM_SET,
  CMD_MWRITE,
  CMD_MREAD,
  CMD_CSRW,
  CMD_CSRR,
  CMD_OVLAY,
  CMD_DISPONOFF,
} state_t;

/* layer mixing */
#define MX_OR 0		/* L1 or L2 or L3 */
#define MX_XOR 1	/* L1 xor L2 or L3 */
#define MX_AND 2	/* L1 and L2 or L3 */
#define MX_POR 3	/* priority OR (L1 above L2 above L3) */

/* display mode (blocks 1 and 3) */
#define DM_TEXT 0
#define DM_GRAPHICS 1

/* composition mode */
#define OV_TWO_LAYERS 0
#define OV_THREE_LAYERS 1

/* screen block attributes */
#define FP_OFF 0
#define FP_ON 1
#define FP_ON_FLASH_2HZ 2
#define FP_ON_FLASH_16HZ 3

/* emulated device screen resolution, fixed */
#define SRC_WIDTH 320
#define SRC_HEIGHT 240

/* define this to enable display scaling */
#define SCALE_SCREEN

#ifdef SCALE_SCREEN
/* duplicate every nth pixel
   nearest neighbour scaling looks like ass, especially for a value of 2
   (which, unfortunately, would give us the most useful screen geometry)
 */
/* only works with powers of 2, useful values are 1, 2 and 4 */
#define DOUBLE_X_EVERY 2
/* works with any positive integer */
#define DOUBLE_Y_EVERY 2

/* calculate the screen height from the scaling coefficients */
#define DST_WIDTH (SRC_WIDTH + (SRC_WIDTH / DOUBLE_X_EVERY))
#define DST_HEIGHT (SRC_HEIGHT + (SRC_HEIGHT / DOUBLE_Y_EVERY))

#else /* SCALE_SCREEN */
#define DST_WIDTH SRC_WIDTH
#define DST_HEIGHT SRC_HEIGHT
#endif


class UI;

class Lcd {
public:
  Lcd(UI *ui);
  ~Lcd();

  void reset();
  
  uint8_t read(int a0);
  void write(int a0, uint8_t val);

  void update();
  void redraw();

  void loadSaveState(statefile_t fp, bool write);
  
private:
  uint8_t *mem;
  state_t state;
  int next_param;
  uint16_t cursor;
  int mx; /* mixing */
  int dm; /* display mode */
  int ov; /* three layers? */
  int display_on;
  int fc; /* cursor mode */
  int fp1, fp2_4, fp3; /* screen block attributes */
  
  bool dirty;
  
  UI *ui;
};

#endif
