/*
 * os_sdl.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include <SDL/SDL.h>
#include "os.h"

void os_msleep(int ms)
{
  SDL_Delay(ms);
}

unsigned int os_mtime(void)
{
  return SDL_GetTicks();
}

void *os_create_thread(int (*fn)(void *), void *data)
{
  return (void *)SDL_CreateThread(fn, data);
}

void os_wait_thread(void *thread, int *status)
{
  SDL_WaitThread((SDL_Thread *)thread, status);
}
