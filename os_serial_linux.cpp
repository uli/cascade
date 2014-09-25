/*
 * os_serial_linux.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "os.h"
#include "debug.h"
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/serial.h>
#include <dirent.h>

//#define DRYDOCK

int os_serial_open(const char *tty, bool nonblock)
{
#ifdef DRYDOCK
  return 2;	/* stderr */
#else
  int fd;
  
  if (nonblock)
    fd = open(tty, O_RDWR|O_NOCTTY|O_NDELAY);
  else
    fd = open(tty, O_RDWR|O_NOCTTY);
  
  if (fd < 0) {
    ERROR("OS failed to open serial device %s: %s\n", tty, strerror(errno));
    return -1;
  }
  
  /* put TTY in raw mode */
  struct termios tio;
  tcgetattr(fd, &tio);
  cfmakeraw(&tio);
  tcsetattr(fd, TCSANOW, &tio);
  DEBUG(OS, "OS serial flush after open\n");
  tcflush(fd, TCIOFLUSH);	/* flush stale serial buffers */
  
  return fd;
#endif
}

int os_serial_close(int handle)
{
  return close(handle);
}

int os_serial_send(int handle, const char *msg)
{
  DEBUG(OS, "OS serial send %s\n", msg);
  return write(handle, msg, strlen(msg));
}

int os_serial_send_buf(int handle, const unsigned char *buf, int len)
{
  DEBUG(OS, "OS serial send buffer, length %d\n", len);
  return write(handle, buf, len);
}

int os_serial_send_byte(int handle, char byte)
{
  DEBUG(OS, "OS serial send byte %02X\n", byte);
  return write(handle, &byte, 1);
}

int os_serial_bytes_received(int fd)
{
  int bytes = 0;
  ioctl(fd, FIONREAD, &bytes);
  return bytes;
}

int os_serial_read_to_buffer(int fd, void *buf, int count)
{
  return read(fd, buf, count);
}

int os_serial_flush(int fd)
{
  DEBUG(OS, "OS serial flush\n");
  return tcflush(fd, TCIOFLUSH);
}

int os_serial_set_baudrate(int fd, int baudrate)
{
  struct termios tios;
  if (tcgetattr(fd, &tios) < 0) {
    ERROR("tcgetattr failed\n");
    return -1;
  }
  struct serial_struct ser;
  if (ioctl(fd, TIOCGSERIAL, &ser)) {
    ERROR("TIOCGSERIAL failed\n");
    return -1;
  }
  
  ser.custom_divisor = ser.baud_base / baudrate;
  ser.flags &= ~ASYNC_SPD_MASK;
  ser.flags |= ASYNC_SPD_CUST | ASYNC_LOW_LATENCY;
  
  tios.c_cflag &= ~CBAUD;
  tios.c_cflag |= B38400;
  
  if (ioctl(fd, TIOCSSERIAL, &ser)) {
    ERROR("TIOCSSERIAL failed\n");
    return -1;
  }
  if (ioctl(fd, TIOCGSERIAL, &ser)) {
    ERROR("TIOCGSERIAL failed\n");
    return -1;
  }
  if (ser.custom_divisor != ser.baud_base / baudrate) {
    ERROR("failed to set baudrate divisor, is %d, should be %d\n", ser.custom_divisor, ser.baud_base / baudrate);
  }
  if (tcsetattr(fd, TCSANOW, &tios) < 0) {
    ERROR("tcsetattr failed\n");
    return -1;
  }
  return 0;
}

int os_serial_clear_break(int fd)
{
  if (ioctl(fd, TIOCCBRK) < 0) {
    ERROR("TIOCCBRK failed\n");
    return -1;
  }
  return 0;
}

int os_serial_set_break(int fd)
{
  if (ioctl(fd, TIOCSBRK) < 0) {
    ERROR("TIOCSBRK failed\n");
    return -1;
  }
  return 0;
}

int os_serial_set_rts(int fd)
{
  int flags;
  if (ioctl(fd, TIOCMGET, &flags)) {
    ERROR("CMBIC: %s\n", strerror(errno));
    return -1;
  }
  flags |= TIOCM_RTS;
  return ioctl(fd, TIOCMSET, &flags);
}

int os_serial_clear_rts(int fd)
{
  int flags;
  if (ioctl(fd, TIOCMGET, &flags)) {
    ERROR("CMBIC: %s\n", strerror(errno));
    return -1;
  }
  flags &= ~TIOCM_RTS;
  return ioctl(fd, TIOCMSET, &flags);
}

int os_serial_set_dtr(int fd)
{
  int flags;
  if (ioctl(fd, TIOCMGET, &flags)) {
    ERROR("CMBIC: %s\n", strerror(errno));
    return -1;
  }
  flags |= TIOCM_DTR;
  return ioctl(fd, TIOCMSET, &flags);
}

int os_serial_clear_dtr(int fd)
{
  int flags;
  if (ioctl(fd, TIOCMGET, &flags)) {
    ERROR("CMBIC: %s\n", strerror(errno));
    return -1;
  }
  flags &= ~TIOCM_DTR;
  return ioctl(fd, TIOCMSET, &flags);
}

const char *os_serial_get_error(void)
{
  return strerror(errno);
}

#define SYS_DRIVER_PATH "/sys/bus/usb-serial/drivers/"

int os_serial_find_port(const char *driver, char **tty_found)
{
  int sh;
  char *sysfs_dir = new char[sizeof(SYS_DRIVER_PATH) + strlen(driver)];
  sprintf(sysfs_dir, SYS_DRIVER_PATH "%s", driver);
  DEBUG(IFACE, "checking %s for tty\n", sysfs_dir);

  for (;;) {
    DIR *dir = opendir(sysfs_dir);
    if (!dir) {
      os_msleep(300);
      continue;
    }

    struct dirent *ent;
    while ((ent = readdir(dir))) {
      if (!strncmp("ttyUSB", ent->d_name, 6)) {
        char *tty = new char[sizeof("/dev/") + strlen(ent->d_name)];
        sprintf(tty, "/dev/%s", ent->d_name);
        DEBUG(IFACE, "trying to open %s\n", tty);
        sh = os_serial_open(tty, false);
        if (sh >= 0 && tty_found)
        	*tty_found = strdup(tty);
        delete tty;
        if (sh >= 0) {
          closedir(dir);
          delete sysfs_dir;
          return sh;
        }
      }
    }
    closedir(dir);
    os_msleep(300);
  }
}
