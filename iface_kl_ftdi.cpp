/*
 * iface_kl_ftdi.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "iface_kl_ftdi.h"
#include "debug.h"
#include <stdlib.h>
#include "serial.h"
#include "cpu.h"
#include "os.h"

IfaceKLFTDI::IfaceKLFTDI(Cpu *cpu, UI *ui, bool sampling) : IfaceKL(ui)
{
  DEBUG(IFACE, "initializing KL FTDI interface\n");
  baudrate = 10400;
  this->cpu = cpu;
  if (ftdi_init(&ftdic) < 0) {
    ERROR("ftdi initialization failed\n");
    exit(1);
  }
  if (ftdi_usb_open(&ftdic, 0x0403, 0x6001) < 0) {
    ERROR("unable to open FTDI device: %s\n", ftdi_get_error_string(&ftdic));
    exit(1);
  }
  setBitbang(false);
  ftdi_set_line_property(&ftdic, BITS_8, STOP_BIT_1, NONE);

  /* these just seem like a good idea */
  ftdi_setflowctrl(&ftdic, SIO_DISABLE_FLOW_CTRL);
  ftdi_setdtr_rts(&ftdic, 1, 1);

  ftdi_set_latency_timer(&ftdic, 2);

  unsigned int chipid = -1;
  ftdi_read_chipid(&ftdic, &chipid);
  DEBUG(IFACE, "FTDI chipid: %X\n", chipid);

  /* single-figure timeouts cause lost bytes; haven't tried anything
     in-between yet */
  ftdic.usb_read_timeout = 100;

  slow_init = -1;
  last_slow_init = 0;
  
  option_sampling_enabled = sampling;
  enable_read_thread = true;
  enable_sampling_read = false;
  read_thread_quit = false;
  bitbang_enabled = false;
  read_thread = os_create_thread(readThreadRunner, this);
  if (!read_thread) {
    ERROR("failed to create FTDI read thread\n");
    exit(1);
  }
}

IfaceKLFTDI::~IfaceKLFTDI()
{
  read_thread_quit = true;
  DEBUG(IFACE, "deiniting FTDI\n");
#if 0
  /* Workaround for handing back the interface to the kernel driver, which
     libftdi is supposed to do IMO, but doesn't.
     Probably worked with exactly one combination of libftdi and libusb, and
     I forgot which one that was...
  */
  if (libusb_release_interface(ftdic.usb_dev, ftdic.interface) < 0)
    perror("release interface");
  if (libusb_attach_kernel_driver(ftdic.usb_dev, ftdic.interface) != 0)
    perror("attach kernel");
  libusb_close(ftdic.usb_dev);
  ftdic.usb_dev = NULL;
#endif
  ftdi_deinit(&ftdic);
}

void IfaceKLFTDI::setBaudDivisor(int divisor)
{
  int target_baud = cpu->serDivToBaud(divisor);
  int old_baudrate = baudrate;
  if (divisor >= 0x80 && divisor <= 0x86) {
    DEBUG(IFACE, "divisor 0x%x in 9600 baud bracket\n", divisor);
    baudrate = 9600;
  }
  else if (divisor == 0x77 || divisor == 0x76) {
    DEBUG(IFACE, "divisor 0x%x in 10400 baud bracket\n", divisor);
    baudrate = 10400;
  }
  else if (divisor >= 0x4d && divisor <= 0x51) {
    DEBUG(IFACE, "divisor 0x%x in 15625 baud bracket\n", divisor);
    baudrate = 15625;
  }
  else {
    baudrate = target_baud;
    DEBUG(WARN, "unknown baud divisor 0x%x, setting to %d baud\n", divisor, baudrate);
  }
  if (old_baudrate != baudrate) {
    DEBUG(IFACE, "KL setting baudrate to %d\n", baudrate);
    setBitbang(false); /* also sets baudrate */
    /* lots of flushing */
    ftdi_usb_purge_buffers(&ftdic);	/* serial port */
    serial->flushRxBuf();		/* serial input buffer */
    echo_buf->flush();			/* interface echo buffer */
    /* the echo buffer must be flushed because we may have flushed an
       echo that was still in the serial queues, which wreaks havoc
       on our echo cancellation method */
  }
  ui->setBaudrate(baudrate, target_baud);
}

/* makes sure we switch back to UART mode if no slow initialization
   is taking place (any more) */
void IfaceKLFTDI::resetMode()
{
  if (slow_init != -1 && cpu->getCycles() - last_slow_init > 5000000) {
    slow_init = -1;
    setBitbang(false); /* XXX flushes buffer, do we want that? */
  }
}

int IfaceKLFTDI::readThreadRunner(void *data)
{
  IfaceKLFTDI *iface = (IfaceKLFTDI *)data;
  int count;
  uint8_t byte;
  for (;;) {
    if (iface->read_thread_quit)
      break;
    if (!iface->enable_read_thread) {
      os_msleep(1);	/* XXX: maybe 10 is OK? */
      continue;
    }
    if (iface->enable_sampling_read) {
      /* interface has been put into bitbang mode, we are supposed to sample
         the inputs */
      /* check for sample buffer overflow */
      if (iface->sample_ptr >= sizeof(iface->sample_buf) - (sample_size - 1)) {
        DEBUG(IFACE, "sample buffer overflow\n");
        iface->sample_ptr = 0;
        iface->sample_start_time = iface->cpu->getCycles();
      }
      count = ftdi_read_data(&iface->ftdic, iface->sample_buf + iface->sample_ptr, sample_size);
      //DEBUG(IFACE, "runner sampled %d bits\n", count);
      iface->sample_ptr += count;
    }
    else {
      /* regular byte-wise serial port'ing */
      count = ftdi_read_data(&iface->ftdic, &byte, 1);
      if (count == 1) {
        DEBUG(IFACE, "runner found a byte: %02X\n", byte);
        /* only add it if we haven't been disabled in the meantime */
        if (iface->enable_read_thread) {
          if (!iface->echo_buf->empty() && iface->echo_buf->snoop() == byte) {
            /* remove this byte from the echo buffer and ignore it */
            iface->echo_buf->consume();
            DEBUG(IFACE, "ignoring echo %02X\n", byte);
          }
          else {
            iface->serial->addRxData(byte);
            DEBUG(IFACE, "byte received: %02X", byte);
            if (!iface->echo_buf->empty())
              DEBUG(IFACE, " (echo buf %02X)", iface->echo_buf->snoop());
            DEBUG(IFACE, "\n");
            /* assuming that the hardware echo is reliable, we should not flush
               the echo buffer here, because the echo will eventually arrive,
               and it will mess up the communication if we don't detect it as such */
          }
        }
      }
      else {
        //DEBUG(IFACE, "runner timeout\n");
      }
    }
  }
  return 0;
}

void IfaceKLFTDI::checkInput()
{
  resetMode();
  /* read thread runs permanently (unless explicitly disabled), so
     no need to do anything else here */
}

void IfaceKLFTDI::sendByte(uint8_t byte)
{
  resetMode();
  /* add this byte to the echo buffer so it can be properly ignored */
  echo_buf->add(byte);
  if (ftdi_write_data(&ftdic, &byte, 1) < 0) {
    ERROR("FTDI write failed\n");
    exit(1);
  }
}

void IfaceKLFTDI::slowInitImminent()
{
  /* not used */
}

bool IfaceKLFTDI::sendSlowInitBitwise(uint8_t bit)
{
  if (slow_init == -1 || cpu->getCycles() - last_slow_init > 5000000) {
    /* initialize for slow init */
    DEBUG(IFACE, "FTDI bitbang on\n");
    slow_init = 0;
    setBitbang(true);
  }
  uint8_t byte = bit? 1 : 0;
  if (ftdi_write_data(&ftdic, &byte, 1) < 0) {
    ERROR("ftdi_write_data() failed\n");
    exit(1);
  }
  slow_init++;
  last_slow_init = cpu->getCycles();
  if (slow_init == 10) {
    /* slow init complete, go back to UART mode */
    DEBUG(IFACE, "FTDI slow init done, bitbang off\n");
    setBitbang(false);
    slow_init = -1;
  }
  return true;
}

int IfaceKLFTDI::getRxState()
{
  if (!option_sampling_enabled)
    return -1;
  
  if (!enable_sampling_read)
    return 1;	/* idle */

  /* find which sample's turn it is */
  uint32_t sample_no = (cpu->getCycles() - sample_start_time) / sample_clocks;
  
  /* since we sample in blocks of many bits, the wanted sample may not have
     arrived yet */
  while (sample_ptr < sample_no) {
    /* sample_start_time may have changed, so we have to recalculate */
    sample_no = (cpu->getCycles() - sample_start_time) / sample_clocks;
    //DEBUG(IFACE, "looking for sample %d, now at %d\n", sample_no, sample_ptr);
    os_msleep(1);
  }
  /* RX is bit 1 */
  uint8_t bit = (sample_buf[sample_no] >> 1) & 1;
  //DEBUG(IFACE, "sample no %d: %d (%02X)\n", sample_no, bit, sample_buf[sample_no]);
  return bit;
}

void IfaceKLFTDI::setBitbang(bool bb)
{
  if (bb && !bitbang_enabled) {
    enable_read_thread = false;
    DEBUG(IFACE, "setBitbang on %d\n", os_mtime());
    /* TX line is bit 0, RTS is bit 2, DTR is bit 4 */
    if (ftdi_set_bitmode(&ftdic, 0x15, BITMODE_BITBANG) < 0) {
      ERROR("ftdi_set_bitmode failed: %s\n", ftdi_get_error_string(&ftdic));
    }

    /* tried to use arithmetic here to pick an even fraction of the CPU
       clock, but it seems to be very advisable to use a hand-picked power
       of two, because otherwise inaccuracies accumulate quickly that make
       the results worthless */
    if (ftdi_set_baudrate(&ftdic, 8192 /* 262144 Hz (I think...) */) < 0) {
      ERROR("ftdi_set_baudrate() failed: %s\n", ftdi_get_error_string(&ftdic));
    }
    ftdi_usb_purge_buffers(&ftdic);
    enable_sampling_read = false;
    bitbang_enabled = true;
    enable_read_thread = false;
  }
  else if (!bb && bitbang_enabled) {
    enable_read_thread = false;
    enable_sampling_read = false;
    DEBUG(IFACE, "setBitbang off %d, sample_ptr %d\n", os_mtime(), sample_ptr);
    if (ftdi_set_bitmode(&ftdic, 0xff, BITMODE_RESET) < 0) {
      ERROR("ftdi bitmode reset failed: %s\n", ftdi_get_error_string(&ftdic));
    }
    DEBUG(IFACE, "ftdi setting baudrate %d\n", baudrate);
    if (ftdi_set_baudrate(&ftdic, baudrate) < 0) {
      ERROR("FTDI set baudrate after reset failed: %s\n", ftdi_get_error_string(&ftdic));
    }
    ftdi_usb_purge_buffers(&ftdic);
    bitbang_enabled = false;
    enable_read_thread = true;
  }
}

void IfaceKLFTDI::setRxBitbang(bool bb)
{
  if (!option_sampling_enabled)
    return;
  
  setBitbang(bb);
  /* this method is called by Serial if it can be assumed that the switch
     to bit-banging mode happens in order to sample the RX pin, so we
     initiate sampling from here */
  if (bb) {
    sample_ptr = 0;
    sample_start_time = cpu->getCycles();
    enable_sampling_read = true;
    enable_read_thread = true;
  }
}
