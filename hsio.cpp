/*
 * hsio.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "hsio.h"
#include <stdlib.h>
#include "debug.h"

Hsio::Hsio()
{
  hsi_mode = 0xff;
  hso_swt_time[0] = hso_swt_time[1] = hso_swt_time[2] = hso_swt_time[3] = 0;
  hso_command = 0;
  hso_swt_command[0] = hso_swt_command[1] = hso_swt_command[2] = hso_swt_command[3] = 0;
}

uint8_t Hsio::getMode()
{
  DEBUG(HSIO, "HSIO get mode (%02X)\n", hsi_mode);
  return hsi_mode;
}

uint8_t Hsio::getTime(int which)
{
  switch (which) {
    case HSI_TIME_LO:
    case HSI_TIME_HI:
    case HSO_TIME_LO:
    case HSO_TIME_HI:
      DEBUG(WARN, "HSIO fake HSI time %d\n", which);
      return 0xff;
    default:
      ERROR("unknown HSIO time %d\n", which);
      exit(1);
  }
}

void Hsio::setMode(uint8_t mode)
{
  DEBUG(HSIO, "HSIO set mode %02X\n", mode);
  hsi_mode = mode;
}

void Hsio::setTime(int which, uint8_t value)
{
  int swt;
  switch (which) {
    case HSO_TIME_LO:
    case HSO_TIME_HI:
      if ((hso_command & 0xf0) != 0x30) {
        ERROR("HSO unimp1\n");
        //exit(1);
        return;
      }
      swt = (hso_command & 0xf) - 8;
      DEBUG(TRACE, "HSIO setting SWT%d time %d to %02X\n", swt, which, value);
      if (which == HSO_TIME_LO)
        hso_swt_time[swt] = (hso_swt_time[swt] & 0xff00) | value;
      else
        hso_swt_time[swt] = (hso_swt_time[swt] & 0xff) | (value << 8);
      break;
    case HSI_TIME_LO:
    case HSI_TIME_HI:
      ERROR("HSI time write unimplemented\n");
      exit(1);
    default:
      ERROR("HSO unimp2\n");
      exit(1);
  }
}

void Hsio::setCommand(int which, uint8_t cmd)
{
  switch (which) {
    case HSO_CMD:
      hso_command = cmd;
      if ((cmd & 0xf0) != 0x30) {
        ERROR("HSO unimp5\n");
        //exit(1);
        return;
      }
      switch (cmd & 0xf) {
        case 0x8:
        case 0x9:
        case 0xa:
        case 0xb:
          hso_swt_command[(cmd & 0xf) - 8] = cmd;
          break;
        default:
          ERROR("HSIO internal error (cmd %02X)\n", cmd);
          exit(1);
      }
      break;
    default:
      ERROR("unimplemented HSIO cmd for %d\n", which);
      exit(1);
  }
}

void Hsio::setStatus(int which, uint8_t value)
{
  DEBUG(HSIO, "HSIO setStatus(%d, %d) unimplemented\n", which, value);
}

#include "state.h"

void Hsio::loadSaveState(statefile_t fp, bool write)
{
  STATE_RW(hsi_mode);
  STATE_RWBUF(hso_swt_time, 4 * sizeof(uint16_t));
  STATE_RW(hso_command);
  STATE_RWBUF(hso_swt_command, 4 * sizeof(uint8_t));
}
