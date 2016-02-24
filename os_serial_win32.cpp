/*
 * os_serial_win32.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include <windows.h>
#include "ftd2xx_win32/ftd2xx.h"
#include <QList>
#include <QtDebug>
#include "os.h"
#include "debug.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifndef DIST_WINDOWS

#include <unistd.h>
#include <dirent.h>
#endif //DIST_WINDOW



//#define DRYDOCK

#define CHECK(fun, ...) { \
  FT_STATUS ret; \
  /* DEBUG(OS, "doing " #fun " now\n"); */ \
  if ((ret = fun(__VA_ARGS__)) != FT_OK) { \
    DEBUG(OS, #fun " failed: %ld\n", ret); \
    return -1; \
  } \
  /* DEBUG(OS, "done with " #fun " now\n"); */ \
}

/* XXX: global variables? ewww! */
HANDLE hEvent = (HANDLE)0;

int os_serial_open(const char *tty, bool nonblock)
{
#ifdef DRYDOCK
  return 2;	/* stderr */
#else
  FT_HANDLE handle;
  DEBUG(OS, "opening FTDI device\n");
  CHECK(FT_Open, strtoul(tty, NULL, 0), &handle);
  CHECK(FT_SetDataCharacteristics, handle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
  CHECK(FT_SetTimeouts, handle, 5000, 1000);
  CHECK(FT_SetFlowControl, handle, FT_FLOW_NONE, 0, 0);
  //CHECK(FT_SetDeadmanTimeout, handle, 500);
  CHECK(FT_Purge, handle, FT_PURGE_RX | FT_PURGE_TX);
  ERROR("go open tty %ld handle %ld!\n", strtoul(tty, NULL, 0), (int)handle);
  if (!(hEvent = CreateEventA(NULL, false, false, ""))) {
    ERROR("CreateEvent failed %ld\n", GetLastError());
    return -1;
  }
  CHECK(FT_SetEventNotification, handle, FT_EVENT_RXCHAR, hEvent);
  return (int)handle;
#endif
}

int os_serial_close(int handle)
{
  CHECK(FT_Close, (FT_HANDLE)handle);
  return 0;
}

int os_serial_send(int handle, const char *msg)
{
  DEBUG(OS, "OS serial send %s\n", msg);
  DWORD written;
  CHECK(FT_Write, (FT_HANDLE)handle, (char *)msg, strlen(msg), &written);
  return written;
}

int os_serial_send_buf(int handle, const unsigned char *buf, int len)
{
  DEBUG(OS, "OS serial send buf len %d\n", len);
  DWORD written;
  CHECK(FT_Write, (FT_HANDLE)handle, (char *)buf, len, &written);
  return written;
}

int os_serial_send_byte(int handle, char byte)
{
  DWORD written;
  CHECK(FT_Write, (FT_HANDLE)handle, &byte, 1, &written);
  DEBUG(OS, "OS serial sent byte %02X (%ld written)\n", byte, written);
  return written;
}


int os_serial_bytes_received(int fd)
{
  DWORD bytes = 0;
  CHECK(FT_GetQueueStatus, (FT_HANDLE)fd, &bytes);
  return bytes;
}

int os_serial_read_to_buffer(int fd, void *buf, int count)
{
  static int pending = 0;
  DWORD bytes_read = 0;
  while (pending < count) {
    DWORD ret;
    DEBUG(OS, "waiting for object\n");
    if ((ret = WaitForSingleObject(hEvent, INFINITE)) != 0) {
      ERROR("WaitForSingleObject returned %ld, error %ld\n", ret, GetLastError());
      return -1;
    }
    DEBUG(OS, "got signal\n");
    DWORD EventWord;
    DWORD RxBytes;
    DWORD TxBytes;
    CHECK(FT_GetStatus, (HANDLE)fd, &RxBytes, &TxBytes, &EventWord);
    if (EventWord & FT_EVENT_RXCHAR) {
      pending = RxBytes;
    }
    else {
      DEBUG(WARN, "unknown FT event %ld, ignoring\n", EventWord);
      //return -1;
    }
  }
  DEBUG(OS, "reading pending bytes\n");
  CHECK(FT_Read, (FT_HANDLE)fd, buf, count, &bytes_read);
  pending -= bytes_read;
  return bytes_read;
}

int os_serial_flush(int fd)
{
  DEBUG(OS, "OS serial flush\n");
  CHECK(FT_Purge, (FT_HANDLE)fd, FT_PURGE_RX | FT_PURGE_TX);
  return 0;
}

int os_serial_set_baudrate(int fd, int baudrate)
{
  CHECK(FT_SetBaudRate, (FT_HANDLE)fd, baudrate);
  DEBUG(OS, "OS set baudrate %d\n", baudrate);
  return 0;
}

int os_serial_clear_break(int fd)
{
  CHECK(FT_SetBreakOff, (FT_HANDLE)fd);
  DEBUG(OS, "break off at %d\n", os_mtime());
  return 0;
}

int os_serial_set_break(int fd)
{
  CHECK(FT_SetBreakOn, (FT_HANDLE)fd);
  DEBUG(OS, "break on at %d\n", os_mtime());
  return 0;
}

int os_serial_set_rts(int fd)
{
  CHECK(FT_SetRts, (FT_HANDLE)fd);
  return 0;
}

int os_serial_clear_rts(int fd)
{
  CHECK(FT_ClrRts, (FT_HANDLE)fd);
  return 0;
}

int os_serial_set_dtr(int fd)
{
  CHECK(FT_SetDtr, (FT_HANDLE)fd);
  return 0;
}

int os_serial_clear_dtr(int fd)
{
  CHECK(FT_ClrDtr, (FT_HANDLE)fd);
  return 0;
}

const char *os_serial_get_error(void)
{
  return "unknown";	/* XXX: better idea? */
}

int os_serial_find_port(const char *driver, char **tty_found)
{
  (void)driver;	/* There's only FTDI. */
  DWORD num = 0;
  while (!num) {
    CHECK(FT_CreateDeviceInfoList, &num);
    for (DWORD i = 0; i < num; i++) {
      FT_HANDLE handle;
      char foo[2] = {i + '0', 0};
      if ((handle = (FT_HANDLE)os_serial_open(foo, false)) >= 0) {
        DWORD flags, type, id, locid;
        char serial[16];
        char description[64];
        FT_HANDLE tmphandle; /* WTF is this? */
        CHECK(FT_GetDeviceInfoDetail, i, &flags, &type, &id, &locid, serial,
              description, &tmphandle);
        *tty_found = strdup(description);
        return (int)handle;
      }
    }
    os_msleep(500);
  }
  
  return -1;
}
