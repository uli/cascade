/*
 * eeprom.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "eeprom.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

Eeprom::Eeprom(Cpu *cpu, UI *ui)
{
  enable = false;
  clock = true;
  bit_count = 0;
  cmd = 0;
  data = 0;
  addr = 0;
  mode = EEPROM_CMD;
  memset(mem, 0, sizeof(mem));
  this->ui = ui;
  this->cpu = cpu;
  filename = NULL;
}

Eeprom::~Eeprom()
{
  if (!cpu->isReplaying() && filename) {
    DEBUG(WARN, "writing EEPROM contents to %s\n", filename);
    FILE *fp = fopen(filename, "w");
    if (fp) {
      fwrite(mem, 128, 1, fp);
      fclose(fp);
    }
    else {
      DEBUG(WARN, "failed to write EEPROM data: %s\n", strerror(errno));
    }
    free(filename);
  }
}

void Eeprom::toggleInputs(bool ena, bool clk, bool bit)
{
  if (!enable && ena) {
    /* bitbang on */
    DEBUG(IO, "bitbang on\n");
    mode = EEPROM_CMD;
    cmd = 0;
    bit_count = 3;
    ui->setLED(LED_EEPROM, true);
  }
  else if (enable && !ena) {
    /* bitbang off */
    enable = false;
    DEBUG(IO, "bitbang off\n");
    ui->setLED(LED_EEPROM, false);
  }
  else if (enable && clock && !clk) {
    /* clock falling edge */
    DEBUG(IO, "bitbang clock out %d\n", bit);
    switch (mode) {
      case EEPROM_CMD:
        bit_count--;
        if (bit)
          cmd |= 1 << bit_count;
        if (!bit_count) {
          switch (cmd) {
            case 6:
              mode = EEPROM_ADDR_READ;
              bit_count = 8;
              addr = 0;
              DEBUG(IO, "EEPROM read\n");
              break;
            case 5:
              mode = EEPROM_ADDR_WRITE;
              bit_count = 8;
              addr = 0;
              DEBUG(IO, "EEPROM write\n");
              break;
            case 4:
              /* not sure what this is, we are sent 8 bits, probably some
                 sort of configuration command */
              mode = EEPROM_UNKNOWN;
              bit_count = 8;
              addr = 0;
              DEBUG(IO, "EEPROM unknown op\n");
              break;
            default:
              ERROR("unknown EEPROM command 0x%x\n", cmd);
              exit(1);
          }
        }
        break;
      case EEPROM_ADDR_READ:
      case EEPROM_ADDR_WRITE:
        bit_count--;
        if (bit)
          addr |= 1 << bit_count;
        if (!bit_count) {
          DEBUG(IO, "EEPROM address 0x%x\n", addr);
          if (mode == EEPROM_ADDR_READ) {
            mode = EEPROM_DATA_READ;
            bit_count = 16;
            data = mem[addr];
          }
          else {
            mode = EEPROM_DATA_WRITE;
            bit_count = 16;
            data = 0;
          }
        }
        break;
      case EEPROM_DATA_READ:
        if (bit_count)
          bit_count--;
        /* no need to do anything else, the requested byte will be returned
           by readData() */
        break;
      case EEPROM_DATA_WRITE:
        if (bit_count) {
          bit_count--;
          if (bit)
            data |= (1 << bit_count);
        }
        if (!bit_count) {
          DEBUG(IO, "EEPROM wrote 0x%x to 0x%x\n", data, addr);
          mem[addr] = data;
        }
        break;
      case EEPROM_UNKNOWN:
        if (bit_count)
          bit_count--;
        /* XXX we should probably act upon this stuff... */
        break;
      default:
        ERROR("unknown EEPROM mode %d\n", mode);
        exit(1);
    };
  }
  enable = ena;
  clock = clk;
}

bool Eeprom::readData()
{
  if (cpu->isReplaying())
    return cpu->retrieveEventValue(EVENT_EEPROMREAD);
  
  bool ret = data & (1 << bit_count);
  DEBUG(IO, "EEPROM reading bit %d from 0x%x: %d\n", bit_count, data, ret);
  cpu->recordEvent(EVENT_EEPROMREAD, ret);
  return ret;
}

void Eeprom::setFilename(const char *name)
{
  if (name != filename) {
    FILE *fp = fopen(name, "r+");
    if (fp) {
      DEBUG(WARN, "loading EEPROM contents from %s\n", name);
      (void)fread(mem, 128, 1, fp);
      fclose(fp);
    }
    else {
      DEBUG(WARN, "failed to load EEPROM contents from %s\n", name);
    }
  }
  
  filename = strdup(name);
}

#include "state.h"

void Eeprom::loadSaveState(statefile_t fp, bool write)
{
  STATE_RW(enable); STATE_RW(clock);
  STATE_RW(bit_count);
  STATE_RW(cmd);
  STATE_RW(data);
  STATE_RW(addr);
  STATE_RW(mode);
  
  STATE_RWBUF(mem, 128);
  
  STATE_RWSTRING(filename);
}

void Eeprom::erase()
{
  memset(mem, 0, 128);
}
