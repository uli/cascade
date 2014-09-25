/*
 * ui.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _UI_H
#define _UI_H

#include "serial.h"
#include "cpu.h"
#include <QWidget>
#include <QLabel>
#ifdef Q_WS_QWS
#include <QDirectPainter>
#else
#include <QPainter>
#endif
#include <QComboBox>
#include <QtConcurrentRun>

/* host display size */
#define DISPLAY_X 800
#define DISPLAY_Y 480

/* LED geometry */
#define LED_X 660
#define LED_Y 100
#define LED_SPACING 16
#define LED_HEIGHT 4
#define LED_WIDTH 10

/* serial port speed pos */
#define SERIAL_X 660
#define SERIAL_Y 304

/* serial port data line */
#define LINE_X 660
#define LINE_Y 384

/* baudrate method */
#define BR_X 660
#define BR_Y 430

/* Don't forget to update led_names in ui.cpp! */
enum {
  LED_SERIAL_ENABLE = 0,
  LED_SERIAL_RX,
  LED_SERIAL_TX,
  LED_SERIAL_BREAK,
  LED_EEPROM,
  LED_ECHO,
  LED_BEEP,
  LED_CAN,
  LED_IFACE,
  LED_REC,
  LED_PLAY,
  NUM_LEDS
};

enum {
  HINT_IGNITION = 0,
  HINT_VAG_BREAKDOWN,
  HINT_MITSUBISHI_DIAG,
  HINT_TEST,
  NUM_HINTS
};

#define HINT_X 400
#define HINT_Y 432

enum UIKey {
  UIKEY_UNKNOWN = 0,
  UIKEY_LSHIFT,
  UIKEY_RSHIFT,
  UIKEY_KP_ENTER,
  UIKEY_RETURN,
  UIKEY_ESCAPE,
  UIKEY_UP,
  UIKEY_DOWN,
  UIKEY_LEFT,
  UIKEY_RIGHT,
  UIKEY_BACKSPACE,
  UIKEY_0,
  UIKEY_1,
  UIKEY_2,
  UIKEY_3,
  UIKEY_4,
  UIKEY_5,
  UIKEY_6,
  UIKEY_7,
  UIKEY_8,
  UIKEY_9,
  UIKEY_KP0,
  UIKEY_KP1,
  UIKEY_KP2,
  UIKEY_KP3,
  UIKEY_KP4,
  UIKEY_KP5,
  UIKEY_KP6,
  UIKEY_KP7,
  UIKEY_KP8,
  UIKEY_KP9,
  UIKEY_a,
  UIKEY_b,
  UIKEY_c,
  UIKEY_d,
  UIKEY_e,
  UIKEY_f,
  UIKEY_g,
  UIKEY_h,
  UIKEY_l,
  UIKEY_m,
  UIKEY_n,
  UIKEY_r,
  UIKEY_s,
  UIKEY_t,
  UIKEY_y,
  UIKEY_z,
  UIKEY_F1,
  UIKEY_F2,
  UIKEY_F3,
  UIKEY_F4,
  UIKEY_F5,
  UIKEY_F6,
  UIKEY_F7,
  UIKEY_F8,
  UIKEY_F9,
  UIKEY_F10,
  UIKEY_F11,
  UIKEY_F12,
  UIKEY_MAX
};

class LED : public QWidget {
  Q_OBJECT
public:
  LED(QWidget *parent, int x, int y, QColor c, QString name);

  void set(bool on) {
    state = on;
  }
  
  void redraw() {
    if (state != prev_state) {
      QPalette p = led->palette();
      if (!state)
        p.setColor(QPalette::Window, Qt::black);
      else
        p.setColor(QPalette::Window, color);
      led->setPalette(p);
      prev_state = state;
    }
  }

  bool changed() {
    return state != prev_state;
  }
  
private:
  QLabel *led_label;
  QWidget *led;
  QColor color;
public:
  bool state, prev_state;
};
  
class Serial;

#ifndef Q_WS_QWS
/* For Qt builds without QDirectPainter */
class SlowPaint : public QWidget {
  Q_OBJECT
public:
  SlowPaint(QWidget *parent, int x, int y);
  ~SlowPaint();
  void startPainting(bool) {}
  void endPainting(QRegion &) { update(); }
  QImage *image;

protected:
  void paintEvent(QPaintEvent *event);

private:
};
#endif

class UI : public QWidget {
  Q_OBJECT
  
public:
  static void initToolkit();
  
  UI(QWidget *parent = 0);
  ~UI();
  
  void setSerial(Serial *s);
  
  int run();
  
  bool pollEvent(struct Event &e);
  
  void flip();
  void setDirty() {
    dirty = true;
  }

  void *getPixels() {
#ifdef Q_WS_QWS
    if (paint_disabled)
      return NULL;
#endif
    /* XXX: either do locking, or do away with this completetly,
       coz it's pointless anyway on a real FB device */
    emit startPaintSignal();
#ifdef Q_WS_QWS
    return QDirectPainter::frameBuffer();
#else
    return painter->image->bits();
#endif
  }
  
  int screenX() {
#ifdef Q_WS_QWS
    return screen_x;
#else
    return 0;
#endif
  }
  int screenY() {
#ifdef Q_WS_QWS
    return screen_y;
#else
    return 0;
#endif
  }
  int screenStep() {
#ifdef Q_WS_QWS
    return QDirectPainter::screenWidth();
#else
    return painter->image->width();
#endif
  }

  void setLED(int led, bool on);
  void setBaudrate(int actual_baud, int target_baud);
  void setCommLine(int line);
  void setPort(const char *tty);
    
  void setHint(int hint);
  void clearHint(int hint);
  
  
  void loadSaveState(statefile_t fp, bool write);
  
  void quit();
  
#ifndef NDEBUG
  void updateTime(int ms);
#endif
  
  const char *getStateName(bool save, const char *dir = "save", const char *ext = "sav");
  
  bool askUser(const char *caption, const char *question, const char *button1 = "OK", const char *button2 = "Cancel");
  void fatalError(const char *error, const char *detail = NULL, const char *arg0 = NULL, const char *arg1 = NULL, const char *arg2 = NULL);
  void showWarning(const char *text, const char *arg0 = NULL, const char *arg1 = NULL, const char *arg2 = NULL);

  void loadRom();

  void saveScreenshot(const char *prefix = 0);

  void setCpu(Cpu *cpu);

  void machineStopped();
  void machineRunning();

protected:
  void updateHint();
  
  void keyPressEvent(QKeyEvent *event);
  void keyReleaseEvent(QKeyEvent *event);
  void keyEvent(QKeyEvent *event, bool *key_event);
  
  void closeEvent(QCloseEvent *event);
  
private:
#ifdef Q_WS_QWS
  QDirectPainter *painter;
#else
  SlowPaint *painter;
#endif
  QRegion region;
  int screen_x, screen_y;
  
  bool dirty;
  bool led_dirty;
#ifdef Q_WS_QWS
  bool paint_disabled;
#endif
  
  LED *leds[NUM_LEDS];
  
  uint32_t hint_set;
  QLabel *hint_label;

  bool key_down[UIKEY_MAX];
  bool key_up[UIKEY_MAX];

  QComboBox *fixed_br;
  QLabel *act_br;
  QLabel *req_br;
  
  Serial *serial;
  
  QLabel *line_led[15];
  int cur_line;

  QLabel *time;
  QLabel *port;

  char *current_state_name;
  bool ask_user_result;
  bool load_rom_ok;
  QString load_rom_name;

  Cpu *cpu;

  QLabel *machine_state;

private slots:
  void keypadDownSlot();
  void keypadUpSlot();
  void setLEDSlot(int num);
  void startPaintSlot();
  void endPaintSlot();
  void setHintSlot(int hint);
  void clearHintSlot(int hint);
  void baudrateAutoSlot();
  void baudrateIncSlot();
  void baudrateFixedSlot(bool checked);
  void baudrateFixedChangeSlot(const QString &value);
  void setBaudrateSlot(int actual, int target);
  void setCommLineSlot(int line);
  void showWarningSlot(QString text);
  void loadStateSlot();
  void saveStateSlot();
  void getStateNameSlot(bool save, const char *dir, const char *ext);
  void quitSlot();
  void resetSlot();
  void recordSlot();
  void playSlot();
  void stopSlot();
  void factoryResetSlot();
  void askUserSlot(const char *caption, const char *question, const char *button1, const char *button2);
  void fatalErrorSlot(const char *error, const char *detail, const char *arg0, const char *arg1, const char *arg2);
  void loadRomSlot();
  void menuQuitSlot();
  void menuLoadRomSlot();
  void menuAboutSlot();
  void disablePaintingSlot();
  void enablePaintingSlot();
  void machineStoppedSlot();
  void machineRunningSlot();
#ifndef Q_WS_QWS
  void onlineHelpSlot();
#endif
  void grabKeyboardSlot(int state);

signals:
  void setLEDSignal(int num);
  void startPaintSignal();
  void endPaintSignal();
  void setHintSignal(int hint);
  void clearHintSignal(int hint);
  void setBaudrateSignal(int actual, int target);
  void setCommLineSignal(int line);
  void showWarningSignal(QString text);
  void getStateNameSignal(bool save, const char *dir, const char *ext);
  void quitSignal();
  void askUserSignal(const char *caption, const char *question, const char *button1, const char *button2);
  void fatalErrorSignal(const char *error, const char *detail, const char *arg0, const char *arg1, const char *arg2);
  void loadRomSignal();
  void machineStoppedSignal();
  void machineRunningSignal();
};

#endif
