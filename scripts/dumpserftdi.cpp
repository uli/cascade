#include "debug.h"
#include "os.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ftdi.h>

uint32_t debug_level = 0xffffffff;

int baudrate;
struct ftdi_context ftdic;

void setBitbang(bool bb)
{
  if (bb) {
    DEBUG(IFACE, "setBitbang on\n");
    /* TX line is bit 0, RTS is bit 2, DTR is bit 4 */
    if (ftdi_set_bitmode(&ftdic, 0x15, BITMODE_BITBANG) < 0) {
      ERROR("ftdi_set_bitmode failed: %s\n", ftdi_get_error_string(&ftdic));
    }
    /* just set it to something fast, it will keep state until
       the next byte is written */
    if (ftdi_set_baudrate(&ftdic, 115200 / 16) < 0) {
      ERROR("ftdi_set_baudrate() failed: %s\n", ftdi_get_error_string(&ftdic));
    }
  }
  else {
    DEBUG(IFACE, "setBitbang off\n");
    if (ftdi_set_bitmode(&ftdic, 0xff, BITMODE_RESET) < 0) {
      ERROR("ftdi bitmode reset failed: %s\n", ftdi_get_error_string(&ftdic));
    }
    if (ftdi_set_baudrate(&ftdic, baudrate) < 0) {
      ERROR("FTDI set baudrate after reset failed: %s\n", ftdi_get_error_string(&ftdic));
    }
    ftdi_usb_purge_buffers(&ftdic);
  }
}

int main(int argc, char **argv)
{
  uint8_t byte;
  uint32_t pause;
  uint8_t send = 0xfe;
  baudrate = strtol(argv[1], 0, 0);
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
  ftdi_set_baudrate(&ftdic, baudrate);
  sleep(1);
  if (argc > 2) {
    setBitbang(true);
    byte = 1;
    ftdi_write_data(&ftdic, &byte, 1);
    os_msleep(500);
    byte = 0;
    ftdi_write_data(&ftdic, &byte, 1);
    os_msleep(200);

    uint8_t si = strtol(argv[2], 0, 0);
    for (int i = 0; i < 8; i++) {
      byte = (si >> i) & 1;
      ftdi_write_data(&ftdic, &byte, 1);
      os_msleep(200);
      DEBUG(IFACE, "slow init %d\n", byte);
    }
    byte = 1;
    ftdi_write_data(&ftdic, &byte, 1);
    //os_msleep(200);
    setBitbang(false);
    ERROR("si done\n");
  }
  for(;;) {
    if (ftdi_read_data(&ftdic, &byte, 1) == 1) {
      fprintf(stderr, "%02X\n", byte);
      if (byte != (send ^ 1)) {
        pause = 0;
      }
      else {
        fprintf(stderr, "^ echo\n");
      }
    }
    else {
      pause++;
      if (pause > 100000) {
        //fprintf(stderr, "sending %02X\n", send);
        //os_serial_send_byte(sh, send);
        send++;
        //if (send == 0xff) pause = 900000;
        //else 
          pause = 0;
      }
    }
  }
}
