/*
 * keypad.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _KEYPAD_H
#define _KEYPAD_H

#include <stdint.h>
#include "cpu.h"

/* key[3] */
#define HI_F1		0x301
#define HI_F2		0x302
#define HI_F3		0x304
#define HI_F4		0x308
#define HI_F5		0x310
#define HI_F6		0x320
#define HI_ESCAPE	0x340

/* key[2] */
#define HI_1		0x201
#define HI_2		0x202
#define HI_3		0x204
#define HI_4		0x208
#define HI_5		0x210
#define HI_6		0x220
#define HI_7		0x240

/* key[1] */
#define HI_8		0x101
#define HI_9		0x102
#define HI_0		0x104
#define HI_UP		0x108
#define HI_DOWN		0x110
#define HI_LEFT		0x120
#define HI_RIGHT	0x140

/* key[0] */
#define HI_HELP		0x001
#define HI_SHIFT	0x002
#define HI_UNDO		0x004
#define HI_NO		0x008
#define HI_YES		0x010
#define HI_BACKLIGHT	0x020
#define HI_RETURN	0x040

#define SET_KEY(a) (key[(a) >> 8] &= ~((a) & 0xff))
#define CLEAR_KEY(a) (key[(a) >> 8] |= ((a) & 0xff))

class Cpu;
class UI;

class Keypad {
public:
  Keypad(Cpu *cpu, UI *ui);
  void update();
  uint8_t getLine(int line);
  
  void loadSaveState(statefile_t fp, bool write);

private:
  uint8_t key[4];
  Cpu *cpu;
  UI *ui;
};

#endif
