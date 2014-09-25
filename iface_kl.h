/*
 * iface_kl.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _IFACE_KL_H
#define _IFACE_KL_H

#include "interface.h"
#include "ring.h"

class Cpu;

class IfaceKL : public Interface {
public:
  IfaceKL(UI *ui) : Interface(ui) {
    echo_buf = new Ring<uint8_t>(64);
  }
  virtual ~IfaceKL() {
    delete echo_buf;
  }
  
  virtual void setBaudDivisor(int divisor) = 0;
  
  virtual void checkInput() = 0;
  virtual void sendByte(uint8_t byte) = 0;
  
  virtual void slowInitImminent() = 0;
  
  virtual bool sendSlowInitBitwise(uint8_t bit) = 0;

protected:
  int baudrate;
  
  Cpu *cpu;
  
  /* echo cancellation ring buffer */
  Ring<uint8_t> *echo_buf;
};

#endif
