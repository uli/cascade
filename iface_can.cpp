/*
 * iface_can.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "iface_can.h"
#include "os.h"
#include "serial.h"
#include "autotty.h"

IfaceCAN::IfaceCAN(Cpu *c, UI *ui, AutoTTY* atty) : Interface(ui)
{
  DEBUG(IFACE, "CAN iface coming up\n");
  atty->assertInterface();
  atty->setBaudrate(833333);
  atty->clearRTS();
  atty->clearDTR();
  os_msleep(125);
  atty->setDTR();
  os_msleep(500);
  atty->clearDTR();
  atty->setRTS();
  this->atty = atty;
  this->cpu = c;
  read_thread = os_create_thread(readThreadRunner, this);
  if (!read_thread) {
    ERROR("failed to create TTY read thread\n");
    exit(1);
  }
  tx_state = TXST_IDLE;
  DEBUG(IFACE, "CAN iface up\n");
}

IfaceCAN::~IfaceCAN()
{
  atty->assertInterface();

  /* turn off CAN mode */
  atty->clearRTS();
  atty->clearDTR();
  os_msleep(300);	/* tried sleeping 500 ms first, but 300 ms works
                           reliably at least for VAG; no sleeping clearly
                           doesn't */

  os_kill_thread(read_thread, NULL);
  DEBUG(IFACE, "CAN reader thread stopped\n");
}

void IfaceCAN::sendByte(uint8_t byte)
{
  DEBUG(IFACE, "CAN sendByte %02X\n", byte);
  if (tx_state == TXST_IDLE && byte == 0xf1) {
    DEBUG(IFACE, "CAN cmd start\n");
    /* cmd start */
    tx_state = TXST_EXPECT_CMD;
  }
  else if (tx_state == TXST_EXPECT_CMD) {
    DEBUG(IFACE, "CAN cmd\n"); 
    tx_cmd = byte;
    tx_state = TXST_EXPECT_LENGTH;
  }
  else if (tx_state == TXST_EXPECT_LENGTH) {
    tx_len = byte;
    tx_state = TXST_EXPECT_DATA;
    tx_msg_ptr = 0;
    if (!tx_len) {
      msgOut();
      tx_state = TXST_IDLE;
    }
  }
  else if (tx_state == TXST_EXPECT_DATA) {
    tx_msg[tx_msg_ptr++] = byte;
    if (tx_msg_ptr == tx_len) {
      msgOut();
      tx_state = TXST_IDLE;
    }
  }
}

int IfaceCAN::readThreadRunner(void *data)
{
  IfaceCAN *iface = (IfaceCAN *)data;
  int state = RDST_IDLE;
  uint8_t byte;
  int bytes_to_get = 0;
  int rx_data_ptr = 0;
  for (;;) {
    int count = iface->atty->readToBuffer(&byte, 1);
    if (count == 1) {
      if (state == RDST_IDLE) {
        DEBUG(IFACE, "rx msg type %02X\n", byte);
        iface->rx_type = byte;
        state = RDST_EXPECT_LENGTH;
      }
      else if (state == RDST_EXPECT_LENGTH) {
        iface->rx_len = byte;
        bytes_to_get = byte;
        rx_data_ptr = 0;
        if (bytes_to_get == 0) {
          state = RDST_IDLE;
          iface->msgIn();
        }
        else {
          state = RDST_EXPECT_DATA;
        }
      }
      else if (state == RDST_EXPECT_DATA) {
        iface->rx_msg[rx_data_ptr++] = byte;
        bytes_to_get--;
        if (!bytes_to_get) {
          state = RDST_IDLE;
          iface->msgIn();
        }
      }
    }
  }
  return 0;
}

void IfaceCAN::msgIn()
{
  DEBUG(IFACE, "msgIn(), type %02X:", rx_type);
  for (int i = 0; i < rx_len; i++) {
    DEBUG(WARN, " %02X", rx_msg[i]);
  }
  DEBUG(WARN, "\n");

  /* S-System seems to always discard the first byte, so we add a decoy. */
  /* XXX: Why does it do that? */
  /* XXX: I guess we shouldn't do that if there is nothing to send to begin
     with. */
  if (cpu->is_hiscan)
    serial->addRxData(0x42);

  switch (rx_type) {
    case 0x50:	/* supposedly init 500 kpbs ACK, but it doesn't seem to be sent */
      serial->addRxData(0xf2);
      serial->addRxData(0xf6);
      serial->addRxData((uint8_t)0x00);	/* C++ is the stupidest language ever */
      break;
    case 0x53:	/* filter setting ACK */
    case 0x60:	/* data send ACK */
      /* NOP, CS doesn't expect ACKs here */
      break;
    case 0x70: {	/* receive data */
        serial->addRxData(0xf2);
        serial->addRxData(0xfc);
        serial->addRxData(rx_len - 2); /* CS expects 16-bit address */
        int can_addr_in = (rx_msg[0] << 3) | (rx_msg[1] >> 2);
        serial->addRxData(can_addr_in >> 3);
        serial->addRxData((can_addr_in & 7) << 5);
        for (int i = 4 ; i < rx_len; i++) {
          serial->addRxData(rx_msg[i]);
        }
        break;
      }
    default:
      DEBUG(WARN, "unknown reply %02X from CAN interface\n", rx_type);
      break;
  }
}

void IfaceCAN::msgOut()
{
  DEBUG(IFACE, "msgOut(), cmd %02X:", tx_cmd);
  for (int i = 0; i < tx_len; i++) {
    DEBUG(WARN, " %02X", tx_msg[i]);
  }
  DEBUG(WARN, "\n");

  /* 34 is the length of the longest fixed-size command (set filter),
     and the rest depends on tx_len, so this ought to be enough. */
  uint8_t *out = (uint8_t *)alloca(34 + tx_len);
  int outp = 0;

  switch (tx_cmd) {
    case 0xfa: /* init */
      out[outp++] = 0x50; /* 0x51 for 250kbps */
      out[outp++] = 0x00;
      atty->sendBuf(out, outp);
      /* 0x50 (unlike 0x51) doesn't yield a reply, so we fake one */
      rx_type = 0x50;
      rx_len = 0;
      msgIn();
      break;
    case 0xfb: { /* filter setup */
        out[outp++] = 0x53;
        out[outp++] = 0x20;
        int can_addr_out = (tx_msg[8] << 3) | (tx_msg[9] >> 5);
        DEBUG(IFACE, "setting outward filter to %03X\n", can_addr_out);
        out[outp++] = can_addr_out >> 3;
        out[outp++] = (can_addr_out & 7) << 2;
        out[outp++] = 0x00;
        out[outp++] = 0x00;
        for (int i = 0; i < 3; i++) {
          out[outp++] = 0xff;
          out[outp++] = 0x1c;
          out[outp++] = 0x00;
          out[outp++] = 0x00;
        }
        /* incoming filter */
        /* CS sets this to 0xffff, presumably meaning "no filtering", but I
           don't know how to do this for K+CAN */
        /* with the test GW (1K0907530F), both 0x00000000 and 0x00001cff work */
        out[outp++] = 0x00;
        out[outp++] = 0x00;
        //out[outp++] = 0xff;
        //out[outp++] = 0x1c;
        out[outp++] = 0x00;
        out[outp++] = 0x00;
        /* outward filter again */
        out[outp++] = can_addr_out >> 3;
        out[outp++] = (can_addr_out & 7) << 2;
        out[outp++] = 0x00;
        out[outp++] = 0x00;
        /* dunno what this is */
        out[outp++] = 0xff;
        out[outp++] = 0x1c;
        out[outp++] = 0x00;
        out[outp++] = 0xff;
        /* and an empty one again */
        out[outp++] = 0xff;
        out[outp++] = 0x1c;
        out[outp++] = 0x00;
        out[outp++] = 0x00;
        atty->sendBuf(out, outp);
        break;
      }
    case 0xfc: { /* send data */
        out[outp++] = 0x60;
        out[outp++] = tx_len + 2; /* K+CAN expects 32-bit addresses */
        int can_addr_out = (tx_msg[0] << 3) | (tx_msg[1] >> 5);
        out[outp++] = can_addr_out >> 3;
        out[outp++] = (can_addr_out & 7) << 2;
        out[outp++] = 0x00;
        out[outp++] = 0x00;
        for (int i = 2; i < tx_len; i++) {
          out[outp++] = tx_msg[i];
        }
        atty->sendBuf(out, outp);
        break;
      }
    default:
      DEBUG(WARN, "unknown CAN command %02X\n", tx_cmd);
      break;
  }
}
