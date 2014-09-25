/*
 * iface_kcan.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "iface_kcan.h"
#include "iface_kl_tty.h"
#include "iface_can.h"
#include "os.h"
#include "autotty.h"

IfaceKCAN::IfaceKCAN(Cpu *c, UI* ui, const char *driver) : Interface(ui)
{
  cpu = c;
  can_enabled = false;
  atty = new AutoTTY(ui, driver);
  iface = new IfaceKLTTY(c, ui, atty);
  iface->setSerial(serial);
}

void IfaceKCAN::setSerial(Serial *s)
{
  serial = s;
  iface->setSerial(serial);
}

IfaceKCAN::~IfaceKCAN()
{
  delete iface;
  delete atty;
}

void IfaceKCAN::setCAN(bool onoff)
{
  DEBUG(IFACE, "KCAN setCAN %d\n", onoff);
  if (onoff != can_enabled) {
    delete iface;
    if(onoff) 
      iface = new IfaceCAN(cpu, ui, atty);
    else
      iface = new IfaceKLTTY(cpu, ui, atty);
    iface->setSerial(serial);
  }
  can_enabled = onoff;
}

void IfaceKCAN::setBaudDivisor(int divisor)
{
  iface->setBaudDivisor(divisor);
}

void IfaceKCAN::checkInput()
{
  iface->checkInput();
}

void IfaceKCAN::sendByte(uint8_t byte)
{
  iface->sendByte(byte);
}

void IfaceKCAN::slowInitImminent()
{
  iface->slowInitImminent();
}

bool IfaceKCAN::sendSlowInitBitwise(uint8_t bit)
{
  return iface->sendSlowInitBitwise(bit);
}
