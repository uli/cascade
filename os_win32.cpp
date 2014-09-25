/*
 * os_win32.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "os.h"
#include <windows.h>
#include "debug.h"

void os_msleep(int ms)
{
  Sleep(ms);
}

LARGE_INTEGER freq;
unsigned int os_mtime()
{
  static bool have_freq = false;
  if (!have_freq) {
    if (!QueryPerformanceFrequency(&freq)) {
      ERROR("failed to query performance frequency\n");
      abort();
    }
    have_freq = true;
  }
  LARGE_INTEGER time;
  if (!QueryPerformanceCounter(&time)) {
    ERROR("failed to query performance counter\n");
    abort();
  }
  return time.QuadPart * 1000 / freq.QuadPart;
}
