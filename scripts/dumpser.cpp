#include "os.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

uint32_t debug_level = 0xffffffff;

int main(int argc, char **argv)
{
  uint8_t byte;
  uint32_t pause;
  uint8_t send = 0xfe;
  const char *serial_port = getenv("SERIAL");
  if (!serial_port)
    serial_port = "/dev/ttyUSB4";
  int sh = os_serial_open(serial_port);
  os_serial_set_baudrate(sh, strtol(argv[1], 0, 0));
  sleep(1);
  if (argc > 3) {
    os_serial_set_break(sh);
    os_msleep(25);
    os_serial_clear_break(sh);
    os_msleep(25);
    uint8_t format = strtol(argv[2], 0, 0);
    uint8_t target = strtol(argv[3], 0, 0);
    os_serial_send_byte(sh, format);
    os_serial_send_byte(sh, target);
    os_serial_send_byte(sh, 0xf1);
    os_serial_send_byte(sh, 0x81);
    os_serial_send_byte(sh, format + target + 0xf1 + 0x81);
  }
  else if (argc > 2)
    os_serial_send_byte_5baud(sh, strtol(argv[2], 0, 0));
  for(;;) {
    if (os_serial_read_to_buffer(sh, &byte, 1) == 1) {
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
