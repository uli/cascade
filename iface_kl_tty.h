/*
 * iface_kl_tty.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "iface_kl.h"

class AutoTTY;

class IfaceKLTTY : public IfaceKL {
public:
  IfaceKLTTY(Cpu *p, UI *ui, const char *driver);
  IfaceKLTTY(Cpu *p, UI *ui, AutoTTY* atty);
  ~IfaceKLTTY();

  virtual void setBaudDivisor(int divisor);
  
  virtual void checkInput();
  virtual void sendByte(uint8_t byte);
  
  virtual void slowInitImminent();
  
  virtual bool sendSlowInitBitwise(uint8_t bit);
  
  virtual void setL(uint8_t bit);

private:
  void init();
  static int readThreadRunner(void *data);
  
  AutoTTY *atty;
  bool destroy_atty_in_dtor;
  bool ignore_breaks;
  uint64_t last_check_input;
  
  bool enable_read_thread;
  bool read_thread_quit;
  void *read_thread;
};
