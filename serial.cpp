/*
 * serial.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "serial.h"
#include "debug.h"
#include "iface.h"
#include <stdlib.h>
#include "os.h"
#include "hints.h"

Serial::Serial(Cpu *cpu, Interface *iface, UI *ui, Hints *hints)
{
  stat = SERSTAT_RI;
  si_state = SI_NORMAL;
  baudrate = baudrate_pos = 0;
  specified_baudrate = 0;
  this->iface = iface;
  rx_buf = new Ring<uint8_t>(256);
  this->cpu = cpu;
  this->ui = ui;
  iface->setSerial(this);

  serial_bitbang_bit_pos = 0;
  serial_bitbang_last = 0;
  serial_bitbang_enabled = false;
  serial_bitbang_last_bit_sent = 0;
  
  force_baud_rate = false;
  forced_baud_rate_no = 0;
  bitbang_reads_after_slow_init = 0;
  
  reset();
  
  read_after_write = true;
  enable_echo = true;
  echo_back_on_time = 0;
  setBaudrateMethod(BR_AUTO);
  fixed_baudrate = 0;
  
  control = 0;
  slow_init_target = 0;
  slow_init_count = 0;
  
  comm_line = 7;
  
  this->hints = hints;
}

void Serial::reset()
{
  ti_set_time = 0;
  ti_set_byte = 0;
  ri_set_time = 0;
  last_slow_init = 0;
}

Serial::~Serial()
{
  delete rx_buf;
}

void Serial::setBaudrate(uint8_t divisor)
{
  /* disable speed detection workaround; see slowInit()
     We assume that the software doesn't reset the baud rate between
     attempts to connect to an ISO9141 ECU; checked with 10B0 for Audi
     and BMW
  */
  force_baud_rate = false;
  bitbang_reads_after_slow_init = 0;
  
  if (!baudrate_pos) {
    baudrate = (baudrate & 0xff00) | divisor;
    baudrate_pos++;
  }
  else {
    baudrate = (divisor << 8) | (baudrate & 0xff);
    specified_baudrate = baudrate;
    baudrate_pos--;
    if (baudrate_method == BR_FORCE) {
      baudrate = cpu->serBaudToDiv(fixed_baudrate);
      DEBUG(SERIAL, "SERIAL baudrate %04X ignored and forced to %04X\n", specified_baudrate, baudrate);
    }
    else {
      DEBUG(SERIAL, "SERIAL baudrate set to %04X (specified %04X)\n", baudrate, specified_baudrate);
    }

    if (!cpu->isReplaying())
      iface->setBaudDivisor(baudrate & 0x7fff);
    else
      ui->setBaudrate(0, cpu->serDivToBaud(baudrate));

    hints->baudrate(specified_baudrate, baudrate);
  }
  
  /* flushing the serial buffer here is reasonable for real life,
     but bad for the fake interface (and ELM, I suppose) because
     it might flush our carefully crafted slow init response,
     so we leave it to the interface implementation whether
     to flush it or not */
}

uint16_t Serial::getBaudDivisor(void)
{
  return baudrate & 0x7fff;
}

void Serial::setControl(uint8_t val)
{
  DEBUG(SERIAL, "SERIAL set control %02X\n", val);
  control = val;
  /* no idea what this is supposed to achieve */
  //stat |= SERSTAT_RI;
  si_state = SI_NORMAL;
  
  if (!(val & 8)) {
    if (val == 0x01) {
      /* serial port on, but RX off -> this program does not want any echos! */
      setEcho(false);
      /* let's keep it off for at least one second */
      echo_back_on_time = cpu->getCycles() + 10000000;
    }
    else {
      /* UART receive off, we're expecting bitbanging read */
      serial_bitbang_bit_pos = 0;
      serial_bitbang_enabled = true;
      ui->setLED(LED_SERIAL_ENABLE, false);
    }
  }
  else {
    serial_bitbang_enabled = false;
    ui->setLED(LED_SERIAL_ENABLE, true);
    /* return to default echo setting (on) */
    if (cpu->getCycles() >= echo_back_on_time)
      setEcho(true);
  }
  iface->setRxBitbang(serial_bitbang_enabled);
  /* and make sure it doesn't immediately get turned off again */
  read_after_write = true;
}

void Serial::setStat(uint8_t val)
{
  DEBUG(SERIAL, "SERIAL set stat %02X\n", val);
  /* XXX really? */
  stat = val;
}

void Serial::checkInput()
{
  if (!cpu->isReplaying()) {
    iface->checkInput();
  }
}

uint8_t Serial::readStat()
{
  if (cpu->isReplaying())
    return cpu->retrieveEventValue(EVENT_SERIALSTAT);

  if (ti_set_time && cpu->getCycles() >= ti_set_time) {
    /* 10 bits worth of time have passed, it's time to set the TI flag */
    ti_set_time = 0;
    stat |= SERSTAT_TI;
  }
  
  uint8_t ret = stat;
  stat &= ~(SERSTAT_OE | SERSTAT_FE | SERSTAT_TI | SERSTAT_RI | SERSTAT_RPERB8);

  if (si_state == SI_SPAM)
    ret |= SERSTAT_RI;

  if (!ti_set_time) {
    /* no byte in transfer */
    stat |= SERSTAT_TXE;
    /* seems like we're waiting for something... */
    checkInput();
  }

  if (!rxBufEmpty() && cpu->getCycles() >= ri_set_time)
    ret |= SERSTAT_RI;
  
  if (ret && ret != 0x08 /* TXE and nothing else */ && ret != 0x40 /* RI and nothing else, i.e. waiting for TX to finish */) {
    DEBUG(SERIAL, "%llu SERIAL read stat %02X\n", (unsigned long long)cpu->getCycles(), ret);
  }

  cpu->recordEvent(EVENT_SERIALSTAT, ret);
  return ret;
}

uint8_t Serial::readControl()
{
  DEBUG(SERIAL, "SERIAL read control %02X\n", control);
  return control;
}

uint8_t Serial::readData()
{
  uint8_t ret;
  if (cpu->isReplaying())
    ret = cpu->retrieveEventValue(EVENT_SERIALRX);
  else {
    ret = retrieveRxData();
    cpu->recordEvent(EVENT_SERIALRX, ret);
  }
  hints->byteReceived(ret);
  read_after_write = true;
  DEBUG(SERIAL, "SERIAL read data %02X\n", ret);
  ui->setLED(LED_SERIAL_RX, true);
  return ret;
}

void Serial::writeData(uint8_t data)
{
  DEBUG(SERIAL, "%llu SERIAL write data %02X in si_state %d, time %d\n", (unsigned long long)cpu->getCycles(), data, si_state, os_mtime());
  hints->byteSent(data);

  if (!read_after_write && enable_echo) {
    /* while echo is on no byte has been read since the last write
       under these circumstances we assume the software does not want
       an echo, so we turn it off */
    setEcho(false);
    if (!ti_set_time && !rxBufEmpty() && snoopByte() == ti_set_byte) {
      /* already echoed the previously written byte, remove it again */
      skipByte();
    }
  }
  read_after_write = false;

  ti_set_time = cpu->getCycles() + 8 * (baudrate & 0x7fff) * 10 /* bits */;
  /* Set the earliest time a reply should be dispensed. It is important to do
     this here and not in readStat() when ti_set_time is reached. Due to the
     asynchronous way we do serial I/O, a reply may arrive before that time,
     and it would be dispensed because ri_set_time has not been set yet. */
  if (!enable_echo)
    ri_set_time = ti_set_time + 7000;
  else
    ri_set_time = ti_set_time;
  ti_set_byte = data;
  
  /* Whatever was in the buffer we're not likely to be interested in anymore.
     Helps(?) recover from communication hickups when talking to VAG ECUs.
     We do it before sending the data to make sure we don't race with the
     serial input runner. */
  if (!rxBufEmpty()) {
    DEBUG(SERIAL, "flushing rx buf before write (count %d)\n", rx_buf->count());
    flushRxBuf();
  }

  stat &= ~(SERSTAT_TXE | SERSTAT_TI);
  
  cpu->sync(true);
  DEBUG(SERIAL, "writeData() time after sync %d\n", os_mtime());
  
  if (!cpu->isReplaying())
    iface->sendByte(data);

  /* Add the echo to the input buffer. It's safe to do this here because
     ri_set_time makes sure it won't be dispensed until its time has come.
     Since we're doing serial I/O asynchronously, real data from the interface
     may have arrived already, so we make sure the echo comes first by tacking
     it on at the beginning of the buffer. This case is frequently encountered
     with VAG ECUs, where every single byte is acknowledged. */
  if (enable_echo)
    prependRxData(ti_set_byte);

  ui->setLED(LED_SERIAL_TX, true);
}

void Serial::slowInit(uint8_t bit)
{
  /* slow init always wants an echo */
  setEcho(true);
  /* when doing slow init, we're not interested any more about what happened before */
  flushRxBuf();
  hints->slowInit(bit);
  
  /* Workaround for cars requiring speed detection (VAG, BMW, possibly others)
     The problem with this is that bitwise reading at a speed fast enough to
     detect high baud rates (e.g. 9600 or 10400) is tricky (FTDI) to impossible
     (everything else) to implement, so we employ a trick instead:
     If the last slow init was not long ago and followed by a lot of
     bitbanging reads on the serial port we assume that it failed and that we
     need to try it again at another baudrate. We maintain a list of common
     baud rates and try them one after the other in order.
  */
  uint64_t time_since_last_call = cpu->getCycles() - last_slow_init;
  if (baudrate_method == BR_AUTOPLUS &&
      (force_baud_rate || bitbang_reads_after_slow_init > 100000) &&
      time_since_last_call > 1000 * 10000
     ) {
    /* more than 1s since last slow init, plus many bitbanging reads or forcing already enabled */
    force_baud_rate = true;	/* in case it isn't */
    /* remember it for ourselves (getRxState() dishes out bits at baudrate speed */
    baudrate = forced_baud_rates[forced_baud_rate_no];
    /* set the hardware interface accordingly */
    iface->setBaudDivisor(baudrate & 0x7fff);
    DEBUG(SERIAL, "SERIAL forced baud rate to %04X\n", baudrate);
    /* advance to next baudrate */
    forced_baud_rate_no = (forced_baud_rate_no + 1) % NUM_FORCED_BAUD_RATES;
  }
  bitbang_reads_after_slow_init = 0;
  last_slow_init = cpu->getCycles();
  
  ui->setLED(LED_SERIAL_BREAK, !bit);
  
  cpu->sync(true);
  DEBUG(SERIAL, "slowInit() time after sync %d\n", os_mtime());
    
  if (iface->sendSlowInitBitwise(bit))
    return;
  
  if (si_state != SI_ISO_SLOW_INIT) {
    /* start bit must be 0 */
    if (bit)
      return;
    si_state = SI_ISO_SLOW_INIT;
    slow_init_count = 0;
    slow_init_target = 0;
    if (!cpu->isReplaying())
      iface->slowInitImminent();
  }
  else {
    if (slow_init_count < 8) {
      slow_init_target = (slow_init_target >> 1) | (bit ? 0x80 : 0);
      slow_init_count++;
    }
    else {
      /* stop bit must be 1 */
      if (!bit) {
        si_state = SI_NORMAL;
        return;
      }
      DEBUG(SERIAL, "SERIAL slow init to %02X complete\n", slow_init_target);
      flushRxBuf();
      if (!cpu->isReplaying())
        iface->sendSlowInit(slow_init_target);
      si_state = SI_NORMAL;
    }
  }
}

void Serial::addRxData(const int *data)
{
  int i;
  for (i = 0; data[i] != -1; i++) {
    rx_buf->add(data[i]);
    DEBUG(SERIAL, "SERIAL RX %02X at %lld\n", data[i], (unsigned long long)cpu->getCycles());
  }
}

void Serial::addRxData(uint8_t byte)
{
  rx_buf->add(byte);
  DEBUG(SERIAL, "SERIAL RX %02X at %lld\n", byte, (unsigned long long)cpu->getCycles());
}

void Serial::prependRxData(uint8_t byte)
{
  rx_buf->prepend(byte);
  DEBUG(SERIAL, "SERIAL ECHO %02X at %lld\n", byte, (unsigned long long)cpu->getCycles());
}

int Serial::retrieveRxData()
{
  if (rx_buf->empty())
    return -1;
  else {
    return rx_buf->consume();
  }
}

bool Serial::rxBufEmpty()
{
  return rx_buf->empty();
}

void Serial::flushRxBuf()
{
  DEBUG(SERIAL, "flushing serial RX buffer\n");
  rx_buf->flush();
}

int Serial::snoopByte(void)
{
  if (rxBufEmpty())
    return -1;
  else
    return rx_buf->snoop();
}

void Serial::skipByte(void)
{
  if (rxBufEmpty()) {
    ERROR("skipByte: none in buffer\n");
    exit(1);
  }
  rx_buf->consume();
}

int Serial::getRxState(void)
{
  if (cpu->isReplaying())
    return cpu->retrieveEventValue(EVENT_SERIALRXBIT);
  
  /* speed detection workaround (see slowInit()) */
  bitbang_reads_after_slow_init++;
  
  int bit = iface->getRxState();
  if (bit >= 0) {
    cpu->recordEvent(EVENT_SERIALRXBIT, bit);
    return bit;
  }

  /* interface doesn't know how to read RX state, we have to fake it */  

  checkInput();
  if (!serial_bitbang_enabled || snoopByte() == -1) {
    cpu->recordEvent(EVENT_SERIALRXBIT, 1);
    return 1; /* RX idle */
  }
  
  /* only get a new bit if enough time has passed */
  uint8_t byte = 0;
  if (cpu->getCycles() - serial_bitbang_last > getBaudDivisor() * 8) {
    switch (serial_bitbang_bit_pos) {
      case 0: /* start bit */
        serial_bitbang_last_bit_sent = 0;
        break;
//      case 1 ... 8: /* data bits */
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
          /* data bits */
        byte = snoopByte();
        serial_bitbang_last_bit_sent = (byte >> (serial_bitbang_bit_pos - 1)) & 1;
        DEBUG(SERIAL, "issuing bit %d of %02X at %llu\n",
              serial_bitbang_last_bit_sent, byte,
              (unsigned long long)serial_bitbang_last);
        break;
      case 9: /* stop bit */
        serial_bitbang_last_bit_sent = 1;
        break;
      default:
        DEBUG(SERIAL, "internal error\n");
        exit(1);
    }
    serial_bitbang_bit_pos++;
    serial_bitbang_last = cpu->getCycles();
  }
  /* even once we've sent an entire byte, the software still expects it
     in the serial buffer, so we don't remove it */
  /* actually, it seems to expect _something_ in the serial buffer,
     no matter what... */
  /* actually actually, it expects _nothing_ in the buffer; if they
     bytes after 0x55 are sufficiently delayed (as they would be on a
     real ECU), everything is fine, so we skip the issued byte */
  if (serial_bitbang_bit_pos == 10) {
    serial_bitbang_bit_pos = 0;
    DEBUG(SERIAL, "%llu issued serial byte through bitbang input\n", (unsigned long long)cpu->getCycles());
    skipByte();
  }
  cpu->recordEvent(EVENT_SERIALRXBIT, serial_bitbang_last_bit_sent);
  return serial_bitbang_last_bit_sent;
}

void Serial::setEcho(bool echo)
{
  DEBUG(SERIAL, "serial echo %d\n", echo);
  if (enable_echo != echo) {
    enable_echo = echo;
    ui->setLED(LED_ECHO, echo);
  }
}

bool Serial::getEcho()
{
  return enable_echo;
}

void Serial::setBaudrateMethod(baudrate_method_t n)
{
  baudrate_method = n;
}

void Serial::setFixedBaudrate(int baudrate)
{
  fixed_baudrate = baudrate;
}

void Serial::setCommLine(int line)
{
  if (line != comm_line) {
    iface->checkInput();
    flushRxBuf();
    ui->setCommLine(line);
  }
  if (line != comm_line && (line == 42 || comm_line == 42)) {
    iface->setCAN(line == 42);
  }
  comm_line = line;
  hints->commLine(line);
}

#include "state.h"

void Serial::loadSaveState(statefile_t fp, bool write)
{
  STATE_RW(baudrate);
  STATE_RW(specified_baudrate);
  STATE_RW(baudrate_method);
  STATE_RW(comm_line);
  STATE_RW(control);
  STATE_RW(stat);
  STATE_RW(baudrate_pos);
  STATE_RW(slow_init_target);
  STATE_RW(slow_init_count);
  STATE_RW(si_state);
  
  STATE_RW(ti_set_time);
  STATE_RW(read_after_write);
  STATE_RW(enable_echo);
  STATE_RW(echo_back_on_time);
  STATE_RW(ti_set_byte);
  
  STATE_RW(force_baud_rate);
  STATE_RW(bitbang_reads_after_slow_init);
  STATE_RW(forced_baud_rate_no);
  STATE_RW(last_slow_init);
  
  STATE_RW(serial_bitbang_enabled);
  STATE_RW(serial_bitbang_bit_pos);
  STATE_RW(serial_bitbang_last);
  STATE_RW(serial_bitbang_last_bit_sent);
  
  rx_buf->loadSaveState(fp, write);
}

void Serial::setL(uint8_t bit)
{
  iface->setL(bit);
}
