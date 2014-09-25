/*
 * autotty.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include <stdint.h>

class UI;

class AutoTTY {
public:
  AutoTTY(UI *ui, const char *driver);
  ~AutoTTY();
  
  void setBaudrate(int baudrate);

  void clearBreak();
  void clearDTR();
  void clearRTS();
  void setBreak();
  void setDTR();
  void setRTS();

  int readToBuffer(void *buf, int count);
  void sendByte(char byte);
  void sendBuf(uint8_t *buf, int len);

  void flush();

  void assertInterface();

private:
  void down(const char *reason = 0);
  void searchTTY();

  bool plugged_in;

  int saved_baudrate;
  bool saved_break, saved_dtr, saved_rts;
  
  bool shown_iface_warning;

  UI *ui;
  char *driver;
  int sh;
};
