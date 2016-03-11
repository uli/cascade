/*
 * main.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifndef DIST_WINDOWS

#include <unistd.h>
#endif //DIST_WINDOWS

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include "os.h"
#include "lcd.h"
#include "serial.h"
#include "debug.h"
#include "keypad.h"
#include "cpu.h"
#include "eeprom.h"
#include "iface.h"
#include "iface_kl_tty.h"
#ifdef HAVE_FTDI
#include "iface_kl_ftdi.h"
#endif
#include "iface_fake.h"
#include "iface_kcan.h"

uint32_t debug_level;
uint32_t debug_level_unabridged;

int runEmu(void *cpu) {
  DEBUG(OS, "running Cpu::emulate()\n");
  int ret = ((Cpu *)cpu)->emulate();
  DEBUG(OS, "Cpu::emulate() ended\n");
  return ret;
}

FILE *win_stderr;

int main(int argc, char **argv)
{
#ifdef __MINGW32__
  win_stderr = fopen("stderr.out", "w");
#endif

  UI::initToolkit(argv[0]);
  UI ui;

  Cpu cpu(&ui);
  ui.setCpu(&cpu);


  const char *tty = NULL;
  
#define IFACE_ELM 0
#define IFACE_KL 1  
#define IFACE_FTDI 2
#define IFACE_FAKE 3
#define IFACE_KCAN 4
#if defined(NDEBUG) && (defined(__MINGW32__)|| defined (DIST_WINDOWS))
  int iface_type = IFACE_KCAN;
#else
  int iface_type = IFACE_FAKE;
#endif
  bool expect_echo = false;
  bool ftdi_sampling_enabled = false;

  debug_level = DEBUG_DEFAULT;
#ifndef NDEBUG
  uint32_t trigger = 0;
#endif
#if !defined(DIST_WINDOWS)
  signed char c;
  while ((c = getopt (argc, argv, "d:t:w:s:m:r:p:i:ex:v:S")) != -1) {
    switch (c) {
      case 'd':
        {
          struct {
            const char *name;
            uint32_t value;
          } debug_options[] = {
            {"all", 0xffffffffUL},
            {"io", DEBUG_IO},
            {"mem", DEBUG_MEM},
            {"warn", DEBUG_WARN},
            {"trace", DEBUG_TRACE},
            {"op", DEBUG_OP},
            {"int", DEBUG_INT},
            {"lcd", DEBUG_LCD},
            {"serial", DEBUG_SERIAL},
            {"abridged", DEBUG_ABRIDGED},
            {"iface", DEBUG_IFACE},
            {"os", DEBUG_OS},
            {"key", DEBUG_KEY},
            {"hsio", DEBUG_HSIO},
            {"event", DEBUG_EVENT},
            {"ui", DEBUG_UI},
            {"hints", DEBUG_HINTS},
            {NULL, 0},
          };
          char *d = strtok(optarg, ",");
          do {
            int i;
            for (i = 0; debug_options[i].name; i++) {
              if (d[0] == '-' && !strcmp(debug_options[i].name, d + 1)) {
                debug_level &= ~debug_options[i].value;
                break;
              }
              else if (!strcmp(debug_options[i].name, d)) {
                debug_level |= debug_options[i].value;
                break;
              }
            }
          } while ((d = strtok(NULL, ",")));
        }
        break;
#ifndef NDEBUG
      case 't':
        trigger = strtol(optarg, NULL, 0);
        break;
      case 'w':
        {
          char *a = strtok(optarg, ",");
          char *b = strtok(NULL, ",");
          cpu.setWatchpoint(strtol(a, NULL, 0),
                            strtol(b, NULL, 0)
                           );
        }
        break;
#endif
      case 's':
        tty = strdup(optarg);
        break;
      case 'm':
        cpu.setMaximumCycles(strtol(optarg, NULL, 0));
        break;
      case 'r':
        cpu.enableRecording(optarg);
        break;
      case 'p':
        cpu.enableReplaying(optarg);
        break;
      case 'i':
        if (!strcmp(optarg, "elm"))
          iface_type = IFACE_ELM;
        else if (!strcmp(optarg, "kl"))
          iface_type = IFACE_KL;
#ifdef HAVE_FTDI
        else if (!strcmp(optarg, "ftdi"))
          iface_type = IFACE_FTDI;
#endif
        else if (!strcmp(optarg, "kcan"))
          iface_type = IFACE_KCAN;
        else if (!strcmp(optarg, "fake"))
          iface_type = IFACE_FAKE;
        else {
          ERROR("unknown interface type %s specified\n", optarg);
          exit(1);
        }
        break;
      case 'e':
        expect_echo = true;
        break;
      case 'x':
        if (!cpu.loadExtendedRom(optarg)) {
          ERROR("failed to load extended ROM image\n");
          exit(1);
        }
        break;
      case 'v':
        cpu.setSlowDown(strtod(optarg, NULL));
        break;
      case 'S':
        ftdi_sampling_enabled = true;
        break;
      default:
        ERROR("bad shit\n");
        exit(1);
    };
  }
  argc -= optind;
  argv += optind;
#endif //DIST_WINDOWS  

#ifndef NDEBUG
  if (trigger) {
    cpu.setDebugTrigger(trigger, debug_level);
    debug_level = DEBUG_DEFAULT;
  }
#endif

#if !defined(DIST_WINDOWS)
  if (argc >= 1) {
    if (!cpu.loadRom(argv[0])) {
      ERROR("failed to load ROM image\n");
      exit(1);
    }
  }
#else
  if (argc > 1) {
      if (!cpu.loadRom(argv[1])) {
          ERROR("failed to load ROM image\n");
          exit(1);
      }
  }
#endif

  Interface *iface;
  switch (iface_type) {
    case IFACE_KL:
      iface = new IfaceKLTTY(&cpu, &ui, tty);
      break;
#ifdef HAVE_FTDI
    case IFACE_FTDI:
      iface = new IfaceKLFTDI(&cpu, &ui, ftdi_sampling_enabled);
      break;
#endif
    case IFACE_ELM:
      iface = new IfaceELM(&cpu, &ui, tty);
      break;
    case IFACE_FAKE:
      iface = new IfaceFake(&cpu, &ui);
      break;
    case IFACE_KCAN:
      iface = new IfaceKCAN(&cpu, &ui, tty);
      break;
    default:
      ERROR("internal error");
      exit(1);
  }
  cpu.setSerial(iface, expect_echo);

  void *emu = os_create_thread(runEmu, &cpu);

  DEBUG(OS, "UI::run() start\n");
  ui.run();
  DEBUG(OS, "UI::run() ended\n");

  int ret;
  os_wait_thread(emu, &ret);
  DEBUG(OS, "emu thread finished\n");

  delete iface;
  DEBUG(OS, "iface deleted\n");

#ifdef __MINGW32__
  fclose(win_stderr);
#endif
  return ret;
}
