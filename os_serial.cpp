/*
 * os_serial.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "os.h"
#include "debug.h"
#include <stdlib.h>

#ifndef DRYDOCK
/** Convert carriage return to line feed.
    @param buf data to be converted
    @param size size of buf
 */
static void crtolf(char *buf, size_t size)
{
  size_t i;
  for (i = 0; i < size; i++) {
    if (buf[i] == '\r')
      buf[i] = '\n';
  }
}
#endif

/** Get response from serial port .
    @param fd serial port file descriptor
    @param buf buffer the input data will be written to
    @param size size of buf
    @param timeout timeout (seconds) before aborting read
    @param expect character signaling end of transmission
    @return number of bytes read or -1 on error
 */
char *os_serial_read(int fd, int timeout, int expect)
{
  int size = 9000;
  char *buf = (char *)calloc(1, size);
#ifdef DRYDOCK
  gets(buf);
  return buf;
#else
  int rc;
  char *obuf = buf;
  timeout *= 1000;
  while (size > 0) {
    /* check if there is data to read */
    int bytes;
    for (; timeout > 0;) {
      bytes = os_serial_bytes_received(fd);
      if (bytes > 0) break;
      os_msleep(1); timeout--;
    }
    if (timeout <= 0) {
      /* read timeout */
      ERROR("OS timeout reading from serial port\n");
      free(obuf);
      return NULL;
    }
    
    rc = os_serial_read_to_buffer(fd, buf, bytes > size ? size : bytes);
    if (rc < 0) {
      ERROR("OS I/O error reading from serial port\n");
      free(obuf);
      return NULL;
    }
    crtolf(buf, rc);
    
    size -= rc;
    buf += rc;

    /* check for end-of-transmission character */
    if (expect && rc >= 1 && buf[-1] == expect) {
      DEBUG(OS, "OS serial read: %s\n", obuf);
      return obuf;
    }
  }

  DEBUG(OS, "OS serial read: %s\n", obuf);
  return obuf;
#endif /* DRYDOCK */
}

int os_serial_expect(int fd, int prompt, int timeout)
{
  char *x;
  if (!(x = os_serial_read(fd, timeout, prompt)))
    return 1;
  else {
    free(x);
    return 0;
  }
}

static void breakbit(int fd, int bit)
{
  if (bit)
    os_serial_clear_break(fd);
  else
    os_serial_set_break(fd);
}

int os_serial_send_byte_5baud(int fd, char byte)
{
  int flags;
  DEBUG(OS, "OS serial flush before 5 baud send\n");
  if (os_serial_flush(fd) < 0)
    return -1;
#if 1
  /* turn off CAN on K+CAN */
  os_serial_clear_dtr(fd);
  os_serial_clear_rts(fd);
  os_msleep(500);
#endif
  breakbit(fd, 1);
  os_msleep(500);
  breakbit(fd, 0); // start bit
#if 0
  os_serial_clear_rts(fd);
#endif
  os_msleep(200);
  for (int i = 0; i < 8; i++) {
    DEBUG(WARN, "bit %d\n", !!(byte & (1 << i)));
    breakbit(fd, byte & (1 << i));
#if 0
    if (byte & (1 << i)) {
      os_serial_set_rts(fd);
    }
    else {
      os_serial_clear_rts(fd);
    }
#endif
    os_msleep(200);
    DEBUG(WARN, "time %d\n", os_mtime());
  }
#if 0
  os_serial_set_rts(fd);
#endif
  breakbit(fd, 1);
  //os_serial_flush(fd);
  return 0;
}
