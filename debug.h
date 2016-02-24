/*
 * debug.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include <stdio.h>
#include <stdint.h>

#define DEBUG_WARN    0x00000010UL
#define DEBUG_IO      0x00000100UL
#define DEBUG_TRACE   0x00001000UL
#define DEBUG_MEM     0x00010000UL
#define DEBUG_OP      0x00100000UL
#define DEBUG_INT     0x01000000UL
#define DEBUG_LCD     0x10000000UL
#define DEBUG_SERIAL  0x20000000UL
#define DEBUG_ABRIDGED 0x80000000UL
#define DEBUG_IFACE   0x00000002UL
#define DEBUG_OS      0x00000004UL
#define DEBUG_KEY     0x00000008UL
#define DEBUG_HSIO    0x00000020UL
#define DEBUG_EVENT   0x00000040UL
#define DEBUG_HINTS   0x00000080UL
#define DEBUG_UI      0x00000200UL

#define DEBUG_DEFAULT (DEBUG_WARN | DEBUG_SERIAL | DEBUG_ABRIDGED | DEBUG_IFACE | DEBUG_OS | DEBUG_KEY | DEBUG_HSIO | DEBUG_HINTS | DEBUG_UI)
#ifdef _MSC_VER 
#define unlikely(x) (x)
#else
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif // MSVC_VER

#ifdef __MINGW32__
extern FILE *win_stderr;
#endif

#ifdef NDEBUG
#define DEBUG(level, ...) do {} while(0)
#else
#ifdef __MINGW32__
#define DEBUG(level, ...) \
  do { if (unlikely(debug_level & DEBUG_ ##level)) __mingw_fprintf(win_stderr, __VA_ARGS__); /* fflush(win_stderr); */ } while(0)
#else
#define DEBUG(level, ...) \
  do { if (unlikely(debug_level & DEBUG_ ##level)) fprintf(stderr, __VA_ARGS__); } while(0)
#endif
#endif
#ifdef __MINGW32__
#undef ERROR
#define ERROR(...) do { __mingw_fprintf(win_stderr, __VA_ARGS__); fflush(win_stderr); } while(0)
#else
#define ERROR(...) fprintf(stderr, __VA_ARGS__)
#endif

extern uint32_t debug_level;
