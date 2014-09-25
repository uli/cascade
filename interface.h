/*
 * interface.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _INTERFACE_H
#define _INTERFACE_H

#include <stdint.h>
#include "debug.h"
#include <stdlib.h>

class Serial;
class UI;

class Interface {
public:
  Interface(UI *ui) {
    serial = NULL;
    this->ui = ui;
  }
  virtual ~Interface() {
  }
  
  virtual void setBaudDivisor(int divisor) = 0;
  
  virtual void checkInput() = 0;
  virtual void sendByte(uint8_t byte) = 0;
  
  virtual void sendSlowInit(uint8_t target) {
  }
  
  virtual void slowInitImminent() = 0;

  virtual void setSerial(Serial *s) {
    serial = s;
  };
  
  virtual bool sendSlowInitBitwise(uint8_t bit) {
    return false;
  }
  
  virtual void setL(uint8_t bit) {
    DEBUG(WARN, "setting L line unimplemented\n");
  }
  
  virtual void setCAN(bool onoff) {
    DEBUG(WARN, "This interface does not support CAN\n");
  }
  
  virtual int getRxState() {
    return -1; /* not implemented */
  }
  
  virtual void setRxBitbang(bool) {
  }
  
protected:
  Serial *serial;
  bool expect_echo;
  UI *ui;
};

#endif
