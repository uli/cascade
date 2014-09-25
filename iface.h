/*
 * iface.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _IFACE_H_
#define _IFACE_H_

#include <stdint.h>
#include "cpu.h"
#include "interface.h"

typedef enum {
  ASTATE_IDLE,
  ASTATE_OBD_COMPOSING,
  ASTATE_OBD_ANSWERING,
  ASTATE_SLOW_INIT_KW_SENT,
  ASTATE_SLOW_INIT_WAIT_FOR_CHECK_BYTE,
  ASTATE_SLOW_INIT_CHECK_BYTE_RECEIVED,
  ASTATE_SLOW_INIT_PAUSE_AFTER_55,
} adapter_state_t;

class Serial;
class Cpu;

class IfaceELM : public Interface {
public:
  IfaceELM(Cpu *p, UI *ui, const char *tty);
  virtual ~IfaceELM();
  
  virtual void setBaudDivisor(int divisor);
  
  virtual void checkInput();
  virtual void sendByte(uint8_t byte);
  
  virtual void sendSlowInit(uint8_t target);
  virtual void slowInitImminent();

protected:
  int *getObdReply();
  void sendObdMessage();
  void sendObdMessageThread();

private:
  bool isInInputBuffer(uint8_t byte);
  char *getInputBuffer();
  void fillInputBuffer();

  int *obd_request;
  int sh;

  adapter_state_t astate;
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
  
  static int msgThreadRunner(void * data);
  void *msg_thread;
  bool msg_thread_idle;
  bool msg_thread_quit;
};

#endif
