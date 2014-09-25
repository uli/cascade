#include "debug.h"
#include "os.h"
#include <ftdi.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

uint32_t debug_level = 0xffffffff;

int main(int argc, char **argv)
{
  struct ftdi_context ftdic;
  if (ftdi_init(&ftdic) < 0) {
    ERROR("ftdi initialization failed\n");
    exit(1);
  }
  if (ftdi_usb_open(&ftdic, 0x0403, 0x6001) < 0) {
    ERROR("unable to open FTDI device: %s\n", ftdi_get_error_string(&ftdic));
    exit(1);
  }
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

  /* TX line is bit 0, RTS is bit 2, DTR is bit 4 */
  if (ftdi_set_bitmode(&ftdic, 0x15, BITMODE_BITBANG) < 0) {
    ERROR("ftdi_set_bitmode failed: %s\n", ftdi_get_error_string(&ftdic));
  }
  /* just set it to something fast, it will keep state until
     the next byte is written */
  int ret;
  if (argc > 2)
    ret = ftdi_set_baudrate(&ftdic, strtol(argv[2], 0, 0));
  else
    ret = ftdi_set_baudrate(&ftdic, 20000000 / 2 / 8 / 32 /* 800000/32 */);
  if (ret < 0) {
    ERROR("ftdi_set_baudrate() failed: %s\n", ftdi_get_error_string(&ftdic));
  }
  DEBUG(IFACE, "FTDI baudrate %d\n", ftdic.baudrate);
  uint8_t buf[65536];
  
  if (argc > 1) {
    uint8_t byte = 1;
    ftdi_write_data(&ftdic, &byte, 1);
    os_msleep(500);
    byte = 0;
    ftdi_write_data(&ftdic, &byte, 1);
    os_msleep(200);

    uint8_t si = strtol(argv[1], 0, 0);
    for (int i = 0; i < 8; i++) {
      byte = (si >> i) & 1;
      ftdi_write_data(&ftdic, &byte, 1);
      os_msleep(200);
      DEBUG(IFACE, "slow init %d\n", byte);
    }
    byte = 1;
    ftdi_write_data(&ftdic, &byte, 1);
    //os_msleep(200);
    ERROR("si done\n");
  }
  uint8_t last = 0xff;
  uint32_t pause = 0;
  for (;;) {
    int count = ftdi_read_data(&ftdic, buf, 65536);
    fprintf(stderr, "%d read\n", count);
    for (int i = 0; i < count; i++) {
      if (buf[i] != last) {
       fprintf(stderr, "%02X after %d\n", buf[i], pause);
       last = buf[i];
       pause = 0;
      }
      else {
        pause++;
      }
    }
  }
}
