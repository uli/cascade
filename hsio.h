/*
 * hsio.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _HSIO_H
#define _HSIO_H

#include <stdint.h>
#include "debug.h"
#include "state.h"

#define HSI_TIME_LO 0
#define HSI_TIME_HI 1
#define HSO_TIME_LO 2
#define HSO_TIME_HI 3

#define HSO_CMD 0
#define HSI_CMD 1

#define HSI_STAT 0
#define HSO_STAT 1

class Hsio {
public:
  Hsio();
  
  uint8_t getMode();
  void setMode(uint8_t mode);
  uint8_t getTime(int which);
  void setTime(int which, uint8_t value);
  void setCommand(int which, uint8_t cmd);
  void setStatus(int which, uint8_t value);
  
  inline bool swtInterrupt(uint16_t timer1_old, uint16_t timer1_new, int which)
  {
    if (hso_swt_command[which] &&
        ((timer1_new > timer1_old && hso_swt_time[which] > timer1_old && hso_swt_time[which] <= timer1_new) ||
         (timer1_new < timer1_old && (hso_swt_time[which] > timer1_old || hso_swt_time[which] <= timer1_new))
        )
       ) {
        hso_swt_command[which] = 0;
        DEBUG(TRACE, "HSIO SWT%d interrupt\n", which);
        return true;
    }
    else
      return false;
  }
  
  void loadSaveState(statefile_t fp, bool write);
  
private:
  uint8_t hsi_mode;
  uint16_t hso_swt_time[4];
  uint8_t hso_command;
  uint8_t hso_swt_command[4];
};

#endif
