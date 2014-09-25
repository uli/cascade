/*
 * hints.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "serial.h"
#include "cpu.h"

class Hints {
public:
  Hints(Cpu *cpu, UI *ui);
  
  void setSerial(Serial *serial);
  
  void byteReceived(uint8_t byte);
  void byteSent(uint8_t byte);
  void baudrate(uint16_t specified, uint16_t actual);
  void slowInit(uint8_t bit);
  void commLine(int line);
  
  void beep();
  
  void port254(uint8_t value);
  
private:
  int vag_04_counter;

  Cpu *cpu;
  UI *ui;
  Serial *serial;
};
