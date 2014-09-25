/*
 * iface_kcan.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _IFACE_KCAN_H_
#define _IFACE_KCAN_H_

#include "interface.h"

class Cpu;
class UI;
class AutoTTY;

class IfaceKCAN : public Interface {
public:
  IfaceKCAN(Cpu *c, UI *ui, const char *tty);
  ~IfaceKCAN();
  virtual void setCAN(bool onoff);
  virtual void setSerial(Serial * s);

  virtual void setBaudDivisor(int divisor);
  
  virtual void checkInput();
  virtual void sendByte(uint8_t byte);
  
  virtual void slowInitImminent();
  
  virtual bool sendSlowInitBitwise(uint8_t bit);
  
  virtual void setL(uint8_t bit) {
    /* The mythical ability to toggle the L line that some KL interfaces
       are rumored to have is definitely absent on the K+CAN */
  }

private:
  bool can_enabled;
  Cpu *cpu;
  AutoTTY *atty;
  Interface *iface;
};

#endif
