/*
 * iface.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "iface.h"
#include "debug.h"
#include "os.h"
#include "serial.h"
#include <stdlib.h>
#include <string.h>

#define EXPECT_PROMPT(msg) \
  if (os_serial_expect(sh, '>', 10)) { \
    DEBUG(WARN, "IFACE no prompt on issuing %s\n", msg); \
  }
#define EXPECT_PROMPT_HARD(msg) \
  if (os_serial_expect(sh, '>', 10)) { \
    ERROR("IFACE fatal: no prompt on issuing %s\n", msg); \
    exit(1); \
  }

int IfaceELM::msgThreadRunner(void *data)
{
  IfaceELM *iface = (IfaceELM *)data;
  for (;;) {
    if (iface->msg_thread_quit)
      break;
    if (iface->msg_thread_idle) {
      os_msleep(10);
      continue;
    }
    iface->sendObdMessageThread();
    iface->msg_thread_idle = true;
  }
  return 0;
}

IfaceELM::IfaceELM(Cpu *c, UI *ui, const char *tty) : Interface(ui)
{
  sh = os_serial_open(tty);
  if (sh < 0) {
    ERROR("failed to open tty %s\n", tty);
    exit(1);
  }
  os_serial_send(sh, "ATZ\r");
  EXPECT_PROMPT_HARD("ATZ")
  os_serial_send(sh, "ATE0\r");
  EXPECT_PROMPT("ATE0")
  os_serial_send(sh, "ATH1\r");
  EXPECT_PROMPT("ATH1")
  os_serial_send(sh, "ATSW00\r"); /* disable wakeup */
  EXPECT_PROMPT("ATSW00")
  os_serial_send(sh, "ATTP3\r"); /* temp switch to ISO9141-2 */
  EXPECT_PROMPT("ATTP3")
  os_serial_send(sh, "ATKW0\r"); /* ignore funky keywords */
  EXPECT_PROMPT("ATKW0")

  delay = c->getCycles();
  obd_ptr = 0;
  astate = ASTATE_IDLE;
  in_buf_start = in_buf_end = 0;
  cpu = c;
  
  msg_thread_idle = true;
  msg_thread_quit = false;
  msg_thread = os_create_thread(msgThreadRunner, this);
}

IfaceELM::~IfaceELM()
{
  msg_thread_quit = true;
  os_wait_thread(msg_thread, NULL);
  os_serial_send(sh, "ATPC\r");
  os_serial_close(sh);
}

void IfaceELM::sendObdMessage()
{
  while (!msg_thread_idle) {
    DEBUG(WARN, "IFACE BAD THING: blocking for previous job to finish\n");
    os_msleep(1);
  }
  msg_thread_idle = false;
}

void IfaceELM::sendObdMessageThread()
{
  DEBUG(IFACE, "IFACE transmitting OBD message\n");
  int *x;
  int count = 0;
  for (x = obd_message; *x != -1; x++)
    count++;
  if (count < 4) {
    ERROR("IFACE short OBD message\n");
    exit(1);
  }
  else {
    DEBUG(IFACE, "IFACE counted %d OBD message bytes\n", count);
  }
  char *header = (char *)malloc(8 /* header bytes with WS */ + 5 /* "ATSH " */ + 2 /* CR, 0 */);
  sprintf(header, "ATSH %02X %02X %02X\r", obd_message[0], obd_message[1], obd_message[2]);
  os_serial_flush(sh);
  os_serial_send(sh, header);
  EXPECT_PROMPT(header)
  free(header);
  x = &obd_message[3];
  count -= 4;
  char *msg = (char *)malloc(count * 3);
  char *mm = msg;
  while (count) {
    mm += sprintf(mm, "%02X ", *x++);
    count--;
  }
  mm[-1] = '\r';
  mm[0] = 0;
  os_serial_send(sh, msg);
  in_buf_start = in_buf_end;
  free(msg);
}

static int reply_buf[100];

int *IfaceELM::getObdReply()
{
  char* reply = getInputBuffer();
  char* oreply = reply;
  DEBUG(IFACE, "IFACE got reply %s from interface\n", reply);
  if (!strncmp(reply, "BUS INIT: ...OK\r", 16))
    reply += 16;
  int *rb = reply_buf;
  while (sscanf(reply, "%02X", rb) == 1) {
    DEBUG(IFACE, "IFACE reply byte %02X\n", *rb);
    reply += 2;
    rb++;
    while (*reply == ' ' || *reply == '\n') {
      DEBUG(IFACE, "IFACE skipping whitespace\n");
      reply++;
    }
    if (*reply == 0 || *reply == '>') {
      DEBUG(IFACE, "IFACE end of reply\n");
      break;
    }
  }
  *rb = -1;
  free(oreply);
  return reply_buf;
}

void IfaceELM::sendSlowInit(uint8_t target)
{
  DEBUG(IFACE, "IFACE sending slow init to %02X\n", target);
  cpu->setSlowDown(8);
  /* XXX: this ought to be non-blocking */
  os_serial_flush(sh);
  os_serial_send(sh, "ATPC\r");	/* hang up */
  EXPECT_PROMPT("ATPC")
  char t[9];
  sprintf(t, "ATIIA%02X\r", target);	/* set slow init target */
  os_serial_send(sh, t);
  EXPECT_PROMPT(t)
  /* XXX: detect ELM327 >= 1.4 and do an "ATSI" instead */
  os_serial_send(sh, "0101\r");
  /* XXX: parse buffer dump and send actual reply received */
  /* XXX combo: only works when ATSI is used; "01 01" overwrites
     the input buffer with the reply from the ECU */
  astate = ASTATE_SLOW_INIT_KW_SENT;
  delay = cpu->getCycles() +
          100 * 10000 +	/* standard says 30 - 200 ms, we use 100 ms */
          200 * 10000; 	/* we also have to take the time for the last bit of
                           the slow init into account; yes, the CARB OBD-II
                           code _is_ that picky... */
  slow_init_target = target;
}

void IfaceELM::setBaudDivisor(int divisor)
{
  baud_divisor = divisor;
  /* no serial buffer flushing, it might contain our crafted response */
  ui->setBaudrate(10400, cpu->serDivToBaud(divisor));
}

void IfaceELM::sendByte(uint8_t byte)
{
  if (astate == ASTATE_IDLE && baud_divisor == 0x77) {
    DEBUG(IFACE, "IFACE starting OBD message\n");
    astate = ASTATE_OBD_COMPOSING;
    obd_ptr = 0;
    obd_message[obd_ptr++] = byte;
    delay = cpu->getCycles() + 1000; /* guess */
    cpu->setSlowDown(.5);
  }
  else if (astate == ASTATE_OBD_COMPOSING) {
    obd_message[obd_ptr++] = byte;
    delay = cpu->getCycles() + 1000;
  }
  else if (astate == ASTATE_SLOW_INIT_WAIT_FOR_CHECK_BYTE) {
    if (byte == slow_init_check_byte) {
      DEBUG(IFACE, "IFACE slow init: got expected byte %02X\n", byte);
    }
    else {
      DEBUG(IFACE, "IFACE slow init: unexpected byte %02X\n", byte);
    }
    astate = ASTATE_SLOW_INIT_CHECK_BYTE_RECEIVED;
    delay = cpu->getCycles() + 1000;
  }
}

void IfaceELM::checkInput()
{
  if (cpu->getCycles() < delay)
    return;

  if (!msg_thread_idle)
    return;

  fillInputBuffer();
  
  if (astate == ASTATE_OBD_COMPOSING) {
    DEBUG(IFACE, "IFACE OBD message complete, %d bytes: ", obd_ptr);
    int i;
    for (i = 0; i < obd_ptr; i++)
      DEBUG(IFACE, "%02X ", obd_message[i]);
    DEBUG(IFACE, "\n");
    
    astate = ASTATE_OBD_ANSWERING;
    obd_message[obd_ptr] = -1;
    sendObdMessage();
    cpu->setSlowDown(8);
  }
  else if (astate == ASTATE_OBD_ANSWERING) {
    /* don't block if protocol interpreter hasn't replied yet */
    if (!isInInputBuffer('>'))
      return;
    obd_reply = getObdReply();
    serial->addRxData(obd_reply);
    astate = ASTATE_IDLE;
    cpu->setSlowDown(1);
  }
  else if (astate == ASTATE_SLOW_INIT_KW_SENT) {
    /* don't answer until we have heard from the protocol interpreter */
    if (!isInInputBuffer('>'))
      return;
    DEBUG(IFACE, "IFACE adding slow init msg\n");
    /* XXX: parse buffer dump and send actual reply received */
    serial->addRxData(0x55);
    astate = ASTATE_SLOW_INIT_PAUSE_AFTER_55;
#define HYUNDAI
#ifndef HYUNDAI
    delay = cpu->getCycles() + 5 * 10000;	/* standard says wait 5 ms - 20 ms */
#endif
    cpu->setSlowDown(1);
  }
  else if (astate == ASTATE_SLOW_INIT_PAUSE_AFTER_55) {
#ifdef HYUNDAI
    static const int slow_init_msg[] = {0x08, 0x08, -1};
#else
    static const int slow_init_msg[] = {0x01, 0x8a, -1};
#endif
    astate = ASTATE_SLOW_INIT_WAIT_FOR_CHECK_BYTE;
    serial->addRxData(slow_init_msg);
    slow_init_check_byte = (~slow_init_msg[2]) & 0xff;
  }
  else if (astate == ASTATE_SLOW_INIT_CHECK_BYTE_RECEIVED) {
#ifdef HYUNDAI
    /* AFAIK we should respond with 0xcc, but S-System expects two bytes */
    int slow_init_epilog[] = {0xcc, 0x42, -1};
    slow_init_epilog[0] = (~slow_init_target) & 0xff;
#else
    int slow_init_epilog[] = {0x0f, 0xf0, 0x01, 0xfe, 0xf6, 0x09, 0x30, 0xcf,
                              0x33, 0xcc, 0x30, 0xcf, 0x39, 0xc6, 0x30, 0xcf,
                              0x36, 0xc9, 0x30, 0xcf, 0x33, 0xcc, 0x32, 0xcd,
                              0x45, 0xba, 0x20, 0xdf, 0x20, 0xdf, 0x03, -1};
#endif
    serial->addRxData(slow_init_epilog);
    astate = ASTATE_IDLE;
  }
}

bool IfaceELM::isInInputBuffer(uint8_t byte)
{
  int i = 0;
  for (i = in_buf_start; i != in_buf_end; i = (i + 1) % in_buf_size)
    if (in_buf[i] == byte)
      return true;
  return false;
}

char *IfaceELM::getInputBuffer()
{
  int current_buf_size = in_buf_end - in_buf_start;
  if (current_buf_size < 0)
    current_buf_size += in_buf_size;
  char *buf = (char *)malloc(current_buf_size + 1);
  char *ret = buf;
  for (; in_buf_start != in_buf_end; in_buf_start = (in_buf_start + 1) % in_buf_size)
    *buf++ = in_buf[in_buf_start];
  *buf = 0;
  return ret;
}

void IfaceELM::fillInputBuffer()
{
  if (os_serial_bytes_received(sh) > 0) {
    int count = os_serial_bytes_received(sh);
    DEBUG(IFACE, "IFACE got %d bytes on serial: ", count);
    while (count) {
      os_serial_read_to_buffer(sh, &in_buf[in_buf_end], 1);
      DEBUG(IFACE, "%c", in_buf[in_buf_end]);
      count--;
      in_buf_end = (in_buf_end + 1) % in_buf_size;
    }
    DEBUG(IFACE, "\n");
  }
}

void IfaceELM::slowInitImminent()
{
  cpu->setSlowDown(.25);
}
