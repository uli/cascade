/*
 * eeprom.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _EEPROM_H
#define _EEPROM_H

#include "ui.h"

#include <stdint.h>

#define EEPROM_CMD 0
#define EEPROM_ADDR_WRITE 1
#define EEPROM_DATA_WRITE 2
#define EEPROM_ADDR_READ 3
#define EEPROM_DATA_READ 4
#define EEPROM_UNKNOWN 5

class Cpu;
class UI;

class Eeprom {
public:
  Eeprom(Cpu *cpu, UI *ui);
  ~Eeprom();
  
  void toggleInputs(bool enable, bool clock, bool data);
  bool readData();
  
  void setFilename(const char *name);
  
  void loadSaveState(statefile_t fp, bool write);
  
  void erase();
  
private:
  bool enable, clock;
  int bit_count;
  uint8_t cmd;
  uint32_t data;
  uint16_t addr;
  int mode;
  
  UI *ui;
  Cpu *cpu;
  
  char *filename;

  uint16_t mem[128];
};

#endif
