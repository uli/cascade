/*
 * iface_kl_tty.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "iface_kl_tty.h"
#include "os.h"
#include "debug.h"
#include <stdlib.h>
#include "serial.h"
#include "cpu.h"
#include "autotty.h"

IfaceKLTTY::IfaceKLTTY(Cpu *cpu, UI *ui, const char *driver) : IfaceKL(ui)
{
  DEBUG(IFACE, "initializing KL interface\n");
  if (!driver) {
    ERROR("KL interface requires a driver\n");
    exit(1);
  }
  destroy_atty_in_dtor = true;
  atty = new AutoTTY(ui, driver);

  /* disable CAN mode if we have a K+CAN */
  atty->clearDTR();
  atty->clearRTS();

  this->cpu = cpu;
  init();
}

IfaceKLTTY::IfaceKLTTY(Cpu *cpu, UI *ui, AutoTTY* atty) : IfaceKL(ui)
{
  this->atty = atty;
  destroy_atty_in_dtor = false;
  this->cpu = cpu;
  init();
}

void IfaceKLTTY::init()
{
  baudrate = 10400;
  atty->setBaudrate(10400);
  ignore_breaks = false;
  last_check_input = 0;
  
  enable_read_thread = true;
  read_thread_quit = false;
  read_thread = os_create_thread(readThreadRunner, this);
  if (!read_thread) {
    ERROR("failed to create TTY read thread\n");
    exit(1);
  }
}

IfaceKLTTY::~IfaceKLTTY()
{
  DEBUG(OS, "tty dtor telling thread to quit\n");
  read_thread_quit = true;
  DEBUG(OS, "tty waiting for thread\n");
  /* have to give it the rough treatment because it is likely to block on wait */
  os_kill_thread(read_thread, NULL);
  DEBUG(OS, "tty thread stopped\n");
  if (destroy_atty_in_dtor)
    delete atty;
}

void IfaceKLTTY::setBaudDivisor(int divisor)
{
  int target_baud = cpu->serDivToBaud(divisor);
  int old_baudrate = baudrate;
  if (divisor == 0x77 || divisor == 0x76)
    baudrate = 10400;
  else {
    baudrate = target_baud;
    DEBUG(WARN, "unknown baud divisor 0x%x, setting to %d baud\n", divisor, baudrate);
  }
  if (old_baudrate != baudrate) {
    DEBUG(IFACE, "KL setting baudrate to %d\n", baudrate);
    atty->setBaudrate(baudrate);
    /* lots of flushing... */
    atty->flush();
    serial->flushRxBuf();	/* serial input buffer */
    echo_buf->flush();	/* interface echo buffer */
    /* the echo buffer must be flushed because we may have flushed an
       echo that was still in the serial queues, which wreaks havoc
       on our echo cancellation method */
    DEBUG(IFACE, "KL setting baudrate done\n");
  }
  ui->setBaudrate(baudrate, target_baud);
}

int IfaceKLTTY::readThreadRunner(void *data)
{
  IfaceKLTTY *iface = (IfaceKLTTY *)data;
  int count;
  uint8_t byte;
  for (;;) {
    if (iface->read_thread_quit)
      break;
    if (!iface->enable_read_thread) {
      os_msleep(1);
      continue;
    }
    count = iface->atty->readToBuffer(&byte, 1);
    if (count == 1) {
      DEBUG(IFACE, "runner found a byte: %02X\n", byte);
      if (!iface->echo_buf->empty() && iface->echo_buf->snoop() == byte) {
        /* remove this byte from the echo buffer and ignore it */
        iface->echo_buf->consume();
        DEBUG(IFACE, "ignoring echo %02X\n", byte);
      }
      else if (!iface->ignore_breaks || byte != 0) {
        iface->serial->addRxData(byte);
        DEBUG(IFACE, "byte received: %02X", byte);
        if (!iface->echo_buf->empty()) {
          DEBUG(IFACE, " (echo buf %02X)", iface->echo_buf->snoop());
        }
        DEBUG(IFACE, "\n");
        /* assuming that the hardware echo is reliable, we should not flush
           the echo buffer here, because the echo will eventually arrive,
           and it will mess up the communication if we don't detect it as such */
      }
      else {
        DEBUG(IFACE, "ignoring break received: %02X\n", byte);
      }
    }
  }
  return 0;
}

void IfaceKLTTY::checkInput()
{
}

void IfaceKLTTY::sendByte(uint8_t byte)
{
  ignore_breaks = false;
  DEBUG(IFACE, "byte sent: %02X\n", byte);
  atty->sendByte(byte);
  /* add this byte to the echo buffer so it can be properly ignored */
  /* XXX: potential race condition: runner may find a reply while we're
     stuck right here and fail to recognize it as an echo; caused actual
     disruption in the FTDI interface, but so far hasn't done any harm
     here... */
  echo_buf->add(byte);
}

void IfaceKLTTY::slowInitImminent()
{
  ignore_breaks = true;
}

bool IfaceKLTTY::sendSlowInitBitwise(uint8_t bit)
{
  ignore_breaks = true;
  if (bit)
    atty->clearBreak();
  else
    atty->setBreak();
  return true;
}

void IfaceKLTTY::setL(uint8_t bit)
{
  if (bit)
    atty->setRTS();
  else
    atty->clearRTS();
}
