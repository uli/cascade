/*
 * iface_kl_ftdi.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "iface_kl.h"
#include <ftdi.h>

class IfaceKLFTDI : public IfaceKL {
public:
  IfaceKLFTDI(Cpu *p, UI *ui, bool sampling);
  virtual ~IfaceKLFTDI();

  virtual void setBaudDivisor(int divisor);
  
  virtual void checkInput();
  virtual void sendByte(uint8_t byte);
  
  virtual void slowInitImminent();
  
  virtual bool sendSlowInitBitwise(uint8_t bit);

  virtual int getRxState();
  
  virtual void setRxBitbang(bool);
  
protected:
  virtual void setBitbang(bool);
  
private:
  void resetMode();
  
  static int readThreadRunner(void *data);

  int sh;
  bool ignore_breaks;

  struct ftdi_context ftdic;
  
  int slow_init;
  uint64_t last_slow_init;
  
  bool option_sampling_enabled;
  bool enable_read_thread;
  bool read_thread_quit;
  bool enable_sampling_read; /* do RX bit sampling instead of reading bytes */
  void *read_thread;
  bool bitbang_enabled; /* bitbanging mode enabled (for slow init or RX sampling) */
  
  uint32_t sample_ptr;
  uint64_t sample_start_time;
  static const uint32_t sample_size = 1024;
  
  /* CPU clocks per sample for 20 MHz CPU and 262144 Hz sampling rate */
  static const uint32_t sample_clocks = 38;
  
  uint8_t sample_buf[2048 * 1024];
};
