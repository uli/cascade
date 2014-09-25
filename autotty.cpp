/*
 * autotty.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "autotty.h"
#include "os.h"
#include "ui.h"
#include "debug.h"

AutoTTY::AutoTTY(UI *ui, const char *driver)
{
  this->ui = ui;
  this->driver = strdup(driver);
  sh = -1;
  plugged_in = false;
  ui->setLED(LED_IFACE, false);
  ui->setPort("none");
  saved_baudrate = 9600;
  saved_break = saved_dtr = saved_rts = false;
  shown_iface_warning = false;
}

AutoTTY::~AutoTTY()
{
  os_serial_close(sh);
}

void AutoTTY::setBaudrate(int baudrate)
{
  saved_baudrate = baudrate;
  if (plugged_in && os_serial_set_baudrate(sh, baudrate) < 0)
    down("setbaud");
}

void AutoTTY::clearBreak()
{
  saved_break = false;
  if (plugged_in && os_serial_clear_break(sh) < 0)
    down("clearbreak");
}

void AutoTTY::clearDTR()
{
  saved_dtr = false;
  if (plugged_in && os_serial_clear_dtr(sh) < 0)
    down("cleardtr");
  else
    DEBUG(IFACE, "clearDTR at %d\n", os_mtime());
}

void AutoTTY::clearRTS()
{
  saved_rts = false;
  if (plugged_in && os_serial_clear_rts(sh) < 0)
    down("clearrts");
  else
    DEBUG(IFACE, "clearRTS at %d\n", os_mtime());
}

void AutoTTY::setBreak()
{
  saved_break = true;
  if (plugged_in && os_serial_set_break(sh) < 0)
    down("setbreak");
}

void AutoTTY::setDTR()
{
  saved_dtr = true;
  if (plugged_in && os_serial_set_dtr(sh) < 0)
    down("setdtr");
  else
    DEBUG(IFACE, "setDTR at %d\n", os_mtime());
}

void AutoTTY::setRTS()
{
  saved_rts = true;
  if (plugged_in && os_serial_set_rts(sh) < 0)
    down("setrts");
  else
    DEBUG(IFACE, "setRTS at %d\n", os_mtime());
}

int AutoTTY::readToBuffer(void *buf, int count)
{
  if (!plugged_in)
    searchTTY();
  int ret = os_serial_read_to_buffer(sh, buf, count);
  if (ret < 0)
    down("read");
  if (ret > 0)
    DEBUG(IFACE, "readToBuffer got %d bytes\n", ret);
  return ret;
}

void AutoTTY::sendByte(char byte)
{
  if (plugged_in) {
    int ret =  os_serial_send_byte(sh, byte);
    if (ret < 0)
      down("sendbyte");
    if (ret == 0)
      DEBUG(IFACE, "byte not sent!\n");
  }
}

void AutoTTY::sendBuf(uint8_t *buf, int len)
{
  if (plugged_in) {
    int ret = os_serial_send_buf(sh, buf, len);
    if (ret < 0)
      down("sendbuf");
    if (ret < len)
      DEBUG(IFACE, "buf send incomplete, %d/%d bytes sent\n", ret, len);
  }
}

void AutoTTY::flush()
{
  if (plugged_in && os_serial_flush(sh) < 0)
    down("flush");
}

void AutoTTY::assertInterface()
{
  if (plugged_in)
    return;
  if (!shown_iface_warning) {
    ui->showWarning("No interface has been found yet.<br>"
                    "Please connect a KL or K+CAN interface to your computer. If you "
                    "have already connected an interface, please make sure that the "
                    "correct drivers have been installed.");
    shown_iface_warning = true;
  }
}

void AutoTTY::down(const char *reason)
{
  DEBUG(IFACE, "tty down, fd %d: %s (%s)\n", sh, reason, os_serial_get_error());
  plugged_in = false;
  ui->setLED(LED_IFACE, false);
  ui->setPort("none");
  if (sh >= 0) {
    os_serial_close(sh);
    sh = -1;
  }
}

void AutoTTY::searchTTY()
{
  char *tty;
  sh = os_serial_find_port(driver, &tty);
  if (sh < 0) {
    ERROR("os_serial_find_port returned error\n");
    ui->fatalError("Interface auto-detection routine failed.");
    return;
  }
  /* found a working TTY */
  DEBUG(IFACE, "working TTY %s found, fd %d\n", tty, sh);
  ui->setPort(tty);
  free(tty);
  plugged_in = true;
  ui->setLED(LED_IFACE, true);

  /* re-set to desired configuration */
  setBaudrate(saved_baudrate);
  if (saved_break)
    setBreak();
  else
    clearBreak();
  if (saved_rts)
    setRTS();
  else
    clearRTS();
  if (saved_dtr)
    setDTR();
  else
    clearDTR();
}
