/*
 * hints.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "hints.h"
#include "debug.h"

Hints::Hints(Cpu *cpu, UI *ui)
{
  this->cpu = cpu;
  this->ui = ui;
  vag_04_counter = 0;
}

void Hints::setSerial(Serial *serial)
{
  this->serial = serial;
}

void Hints::byteReceived(uint8_t byte)
{
  if (byte != 0x04) {
    DEBUG(HINTS, "HINTS reset read\n");
    if (vag_04_counter > 20)
      ui->clearHint(HINT_VAG_BREAKDOWN);
    vag_04_counter = 0;
  }
}

void Hints::byteSent(uint8_t byte)
{
  if (byte == 0x04) {
    vag_04_counter++;
    DEBUG(HINTS, "HINTS count %d\n", vag_04_counter);
    if (vag_04_counter > 20) {
      ui->setHint(HINT_VAG_BREAKDOWN);
    }
  }
  else {
    vag_04_counter = 0;
    DEBUG(HINTS, "HINTS reset write\n");
  }
  /* data out on the serial port, ignition toggling won't help anymore */
  ui->clearHint(HINT_IGNITION);
}

void Hints::beep()
{
  /* After an unsuccessful connection attempt, the serial port is left
     untouched, and/or the serial line is set to 0xff (K Line for CS
     software), so we can't turn off the ignition hint from there.
     Instead, we use the error beep and do it from here. */
  ui->clearHint(HINT_IGNITION);
}

void Hints::baudrate(uint16_t specified, uint16_t actual)
{
  /* change of baudrate -> something's afoot, better reset the ECU */
  /* special Mitsubishi hack: resetting the ECU before the 67 baud connect
     attempt will cause spurious data on the serial port that is misinterpreted
     as a reply, which will prevent the software from trying other methods */
  if (specified == 0xc8df /* 67 Baud */)
    ui->clearHint(HINT_IGNITION);
  else
    ui->setHint(HINT_IGNITION);
}

void Hints::slowInit(uint8_t bit)
{
  /* data out on the serial port, ignition toggling won't help anymore */
  ui->clearHint(HINT_IGNITION);
}

void Hints::commLine(int line)
{
  if (line > 0)
    ui->setHint(HINT_IGNITION);
  else
    ui->clearHint(HINT_IGNITION);
}

void Hints::port254(uint8_t value)
{
  if (value == 0x23 || value == 0x33 || value == 0x73) {
    ui->setHint(HINT_MITSUBISHI_DIAG);
  }
  else if (value == 0x3f) {
    ui->clearHint(HINT_MITSUBISHI_DIAG);
  }
}
