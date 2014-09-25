/*
 * iface_fake.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _IFACE_FAKE_H_
#define _IFACE_FAKE_H_

#include <stdint.h>
#include "cpu.h"
#include "interface.h"

typedef enum {
  FAKE_ASTATE_IDLE,
  FAKE_ASTATE_OBD_COMPOSING,
  FAKE_ASTATE_OBD_ANSWERING,
  FAKE_ASTATE_SLOW_INIT_KW_SENT,
  FAKE_ASTATE_SLOW_INIT_WAIT_FOR_CHECK_BYTE,
  FAKE_ASTATE_SLOW_INIT_CHECK_BYTE_RECEIVED,
  FAKE_ASTATE_SLOW_INIT_PAUSE_AFTER_55,
  FAKE_ASTATE_CAN_START,
  FAKE_ASTATE_CAN_LENGTH,
  FAKE_ASTATE_CAN_COMPOSING,
  FAKE_ASTATE_CAN_ANSWERING,
} fake_adapter_state_t;

class Serial;
class Cpu;

class IfaceFake : public Interface {
public:
  IfaceFake(Cpu *p, UI *ui);
  
  virtual void setBaudDivisor(int divisor);
  
  virtual void checkInput();
  virtual void sendByte(uint8_t byte);
  
  virtual void sendSlowInit(uint8_t target);
  virtual void slowInitImminent();

private:
  bool isInInputBuffer(uint8_t byte);
  char *getInputBuffer();

  int *obd_request;

  fake_adapter_state_t astate;
  int obd_ptr;
  int obd_message[128];
  const int *obd_reply;
  uint64_t delay;
  uint8_t slow_init_target;
  uint8_t slow_init_check_byte;
  
  int baud_divisor;
  
  static const int in_buf_size = 256;
  char in_buf[in_buf_size];
  int in_buf_start;
  int in_buf_end;
  Cpu *cpu;
  
  bool hyundai;	/* Hyundai (true) or Audi (false)? */
  
  int can_mode;
  int can_length;
  int can_ptr;
  int can_message[128];
};

#endif
