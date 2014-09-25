/*
 * iface_can.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _IFACE_CAN_H
#define _IFACE_CAN_H

#include "interface.h"

class Cpu;
class UI;
class AutoTTY;

enum {
  RDST_IDLE = 0,
  RDST_EXPECT_LENGTH,
  RDST_EXPECT_DATA,
};

enum {
  TXST_IDLE = 0,
  TXST_EXPECT_CMD,
  TXST_EXPECT_LENGTH,
  TXST_EXPECT_DATA,
};

class IfaceCAN : public Interface {
public:
  IfaceCAN(Cpu *c, UI *ui, AutoTTY *atty);
  ~IfaceCAN();
  virtual void setBaudDivisor(int divisor) {}
  virtual void checkInput() {}
  virtual void slowInitImminent() {}
  
  virtual void sendByte(uint8_t byte);

private:
  void msgIn();
  void msgOut();
  
  Cpu *cpu;
  static int readThreadRunner(void *data);
  void *read_thread;
  AutoTTY *atty;
  
  int rx_type;
  int rx_len;
  uint8_t rx_msg[0xff];
  
  int tx_cmd;
  int tx_len;
  int tx_state;
  int tx_msg_ptr;
  uint8_t tx_msg[0xff];
};

#endif
