/*
 * state.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _STATE_H
#define _STATE_H

#include <stdio.h>
#include "debug.h"

#ifdef EVENT_COMPRESSED
#include <zlib.h>
#endif

/* any attempt to make this more ceeplusplussy bloated it with needless complexity,
   so we let the preprocessor do the job instead */
#ifdef EVENT_COMPRESSED
typedef gzFile statefile_t;
#define state_open gzopen
#define state_tell gztell
#define state_seek gzseek
#define state_eof gzeof
#define state_close gzclose
#define STATE_RW(x) write ? gzwrite(fp, &x, sizeof(x)) : gzread(fp, &x, sizeof(x))
#define STATE_RWBUF(x, n) write ? gzwrite(fp, x, n) : gzread(fp, x, n)
#else
typedef FILE* statefile_t;
#define state_open fopen
#define state_tell ftell
#define state_seek fseek
#define state_eof feof
#define state_close fclose
#define STATE_RW(x) write ? fwrite(&x, sizeof(x), 1, fp) : fread(&x, sizeof(x), 1, fp)
#define STATE_RWBUF(x, n) write ? fwrite(x, n, 1, fp) : fread(x, n, 1, fp)
#endif

#ifdef EVENT_COMPRESSED
#define STATE_RWSTRING(s) { \
   if (write) { \
      if (s) gzputs(fp, s); \
      gzputc(fp, '\n'); \
   } \
   else { \
      char n[256]; \
      gzgets(fp, n, 256); \
      n[strlen(n)-1] = 0; /* strip LF */ \
      if (s) \
        free(s); \
      if (strlen(n)) \
        s = strdup(n); \
      else \
        s = NULL; \
      DEBUG(WARN, "rwstring read %s\n", s); \
   } \
}
#else
#define STATE_RWSTRING(s) { \
   if (write) { \
      if (s) fputs(s, fp); \
      putc('\n', fp); \
   } \
   else { \
      char n[256]; \
      fgets(n, 256, fp); \
      n[strlen(n)-1] = 0; /* strip LF */ \
      if (s) \
        free(s); \
      if (strlen(n)) \
        s = strdup(n); \
      else \
        s = NULL; \
      DEBUG(WARN, "rwstring read %s\n", s); \
   } \
}
#endif

#endif
