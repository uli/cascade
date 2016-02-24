/*
 * ui.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "ui.h"
#include "lcd.h"
#include "serial.h"
#include <QtGui>
#ifdef Q_WS_QWS
#include <QDirectPainter>
#endif
#include <QTextEdit>

#include "version.h"

class KeypadButton : public QPushButton {
public:
  KeypadButton(UI *ui, QString text, UIKey key) : QPushButton(text, ui) {
    this->key = key;
    setFixedSize(50, 25);
    connect(this, SIGNAL(pressed()), ui, SLOT(keypadDownSlot()));
    connect(this, SIGNAL(released()), ui, SLOT(keypadUpSlot()));
    show();
  }
  UIKey key;
};

/* Don't forget to update enum in ui.h! */
static const char *led_names[] __attribute__((used)) = {
  "serial",
  "data rx",
  "data tx",
  "break",
  "eeprom",
  "echo",
  "beep",
  "CAN",
  "iface",
  "rec",
  "play",
};

LED::LED(QWidget *parent, int x, int y, QColor c, QString name) : QWidget(parent) {
  led_label = new QLabel(name, this);
  led_label->move(LED_WIDTH + 5, 0);
  led_label->show();
  led = new QWidget(this);
  led->setAutoFillBackground(true);
  led->setGeometry(0, (led_label->height() - LED_HEIGHT) / 2, LED_WIDTH, LED_HEIGHT);
  color = c;
  prev_state = true;
  set(false);
  redraw();
  led->show();
  setGeometry(x, y, led_label->x() + led_label->width(), led_label->height());
}

#ifndef Q_WS_QWS
/* SlowPaint serves as a replacement for QDirectPaint which is only
   available in embedded builds of Qt. */
SlowPaint::SlowPaint(QWidget *parent, int x, int y) : QWidget(parent)
{
  resize(x, y);
  image = new QImage(QSize(x, y), QImage::Format_RGB16);
}
SlowPaint::~SlowPaint()
{
  delete image;
}
void SlowPaint::paintEvent(QPaintEvent *pev)
{
  QPainter p(this);
  p.drawImage(QPoint(0, 0), *image);
}
#endif

UI::UI(QWidget *parent) : QWidget(parent)
{
  // ============ UI Widgets ================

/* whole screen percent to coords */
#define SCX(x) (width() * (x) / 100)
#define SCY(y) (height() * (y) / 100)
/* right side percent to coords */
#define SCRX(x) ((width() - DST_WIDTH) * (x) / 100)
#define SCRY(y) SCY(y)
/* bottom side percent to coords */
#define SCBX(x) SCX(x)
#define SCBY(y) ((height() - DST_HEIGHT) * (y) / 100)

#ifdef Q_WS_QWS
  setWindowFlags(Qt::FramelessWindowHint);
#else
  /* windowed, have to set size manually */
  setFixedSize(QSize(DISPLAY_X, DISPLAY_Y));
  QString title = QString(APP_NAME) + QString(" ") + QString(VERSION);
  setWindowTitle(title);
#endif
  setContentsMargins(0, 0, 0, 0);
  grabKeyboard();
#ifdef Q_WS_QWS
  showFullScreen();
#else
  show();
#endif
  
  /* Menu bar */
  QMenuBar *menu = new QMenuBar(this);
#ifdef Q_WS_QWS
  menu->setStyle(new QWindowsStyle);
#endif
  menu->resize(width(), 20);

  /* File menu */
  QMenu *file_menu = new QMenu("&File");
  connect(file_menu, SIGNAL(aboutToShow()), this, SLOT(disablePaintingSlot()));
  connect(file_menu, SIGNAL(aboutToHide()), this, SLOT(enablePaintingSlot()));

  /* Load ROM */
  QAction *load_action = file_menu->addAction("Load ROM");
  connect(load_action, SIGNAL(triggered(bool)), this, SLOT(menuLoadRomSlot()));
  file_menu->addSeparator();
  /* Load State */
  QAction *load_state_action = file_menu->addAction("Load State");
  connect(load_state_action, SIGNAL(triggered(bool)), this, SLOT(loadStateSlot()));
  /* Save State */
  QAction *save_state_action = file_menu->addAction("Save State");
  connect(save_state_action, SIGNAL(triggered(bool)), this, SLOT(saveStateSlot()));

  file_menu->addSeparator();

  /* Quit */
  QAction *quit_action = file_menu->addAction("&Quit");
  connect(quit_action, SIGNAL(triggered(bool)), this, SLOT(menuQuitSlot()));
  menu->addMenu(file_menu);

  /* Machine menu */
  QMenu *machine_menu = new QMenu("Machine");
  connect(machine_menu, SIGNAL(aboutToShow()), this, SLOT(disablePaintingSlot()));
  connect(machine_menu, SIGNAL(aboutToHide()), this, SLOT(enablePaintingSlot()));

  /* Reset */
  QAction *reset_action = machine_menu->addAction("Reset");
  connect(reset_action, SIGNAL(triggered(bool)), this, SLOT(resetSlot()));
  /* Factory Reset */
  QAction *factory_reset_action = machine_menu->addAction("Factory Reset");
  connect(factory_reset_action, SIGNAL(triggered(bool)), this, SLOT(factoryResetSlot()));
  menu->addMenu(machine_menu);

  /* Help Menu */
  QMenu *help_menu = new QMenu("Help");
  connect(help_menu, SIGNAL(aboutToShow()), this, SLOT(disablePaintingSlot()));
  connect(help_menu, SIGNAL(aboutToHide()), this, SLOT(enablePaintingSlot()));
#ifndef Q_WS_QWS
  QAction *help_action = help_menu->addAction("Online Help");
  connect(help_action, SIGNAL(triggered(bool)), this, SLOT(onlineHelpSlot()));
#endif
  /* About */
  QAction *about_action = help_menu->addAction("About");
  connect(about_action, SIGNAL(triggered(bool)), this, SLOT(menuAboutSlot()));
  menu->addMenu(help_menu);

  menu->show();
  
  /* Scanner keypad: Function keys */
  for (int i = 0 ; i < 6 ; i++) {
    KeypadButton *fkey = new KeypadButton(this, QString("F") + QString::number(i + 1), (UIKey)((int)UIKEY_F1 + i));
    fkey->setFixedSize(40, 25);
    fkey->move(40 + i * ((DST_WIDTH - 45) / 6), menu->height() + DST_HEIGHT);
    fkey->show();
  }
  
  /* Scanner keypad: random keys */
  KeypadButton *onoff = new KeypadButton(this, "ON/OFF", UIKEY_F10);
  onoff->move(DST_WIDTH + SCRX(24) - onoff->width(), menu->height() + SCRY(18));
  KeypadButton *bright = new KeypadButton(this, "*", UIKEY_b);
  bright->move(onoff->x() + onoff->width() + SCRX(2), onoff->y());
  KeypadButton *shift = new KeypadButton(this, "SHIFT", UIKEY_LSHIFT);
  shift->move(onoff->x(), onoff->y() + onoff->height() + SCRY(2));
  KeypadButton *help = new KeypadButton(this, "HELP", UIKEY_F12);
  help->move(shift->x() + shift->width() + SCRX(2), shift->y());
  
  /* Scanner keypad: arrow keys */
  KeypadButton *up = new KeypadButton(this, "^", UIKEY_UP);
  up->setFixedSize(40, 25);
  up->move(DST_WIDTH + SCRX(75) - up->width() / 2, onoff->y() + SCRY(1));
  KeypadButton *left = new KeypadButton(this, "<-", UIKEY_LEFT);
  left->setFixedSize(30, 50);
  left->move(up->x() - SCRX(2) - left->width(), up->y() + SCRY(1));
  KeypadButton *down = new KeypadButton(this, "v", UIKEY_DOWN);
  down->setFixedSize(40, 25);
  down->move(up->x(), left->y() + left->height() - down->height() + SCRY(1));
  KeypadButton *right = new KeypadButton(this, "->", UIKEY_RIGHT);
  right->setFixedSize(30, 50);
  right->move(up->x() + up->width() + SCRX(2), left->y());
  
  /* Scanner keys: numerical keypad, yes, no */
  KeypadButton *b;
  for (int i = 0; i < 12; i++) {
    QString t = QString::number(i+1);
    UIKey k = (UIKey)((int)UIKEY_0 + i + 1);
    switch (i) {
      case 9: t = QString("NO"); k = UIKEY_n; break;
      case 10: t = QString("0"); k = UIKEY_0; break;
      case 11: t = QString("YES"); k = UIKEY_y; break;
      default: break;
    }
    b = new KeypadButton(this, t, k);
    b->setFixedSize(40, 25);
    b->move(DST_WIDTH + SCRX(10 + i % 3 * 15) - b->width() / 2, shift->y() + shift->height() + SCY(3) + i / 3 * (SCY(2) + b->height()));
  }

  /* Scanner keypad: more random keys */
  KeypadButton *esc = new KeypadButton(this, "ESC", UIKEY_ESCAPE);
  esc->move(DST_WIDTH + SCRX(75) - esc->width() / 2, shift->y() + shift->height() + SCY(3));
  KeypadButton *undo = new KeypadButton(this, "UNDO", UIKEY_BACKSPACE);
  undo->move(esc->x(), esc->y() + esc->height() + SCRY(2));
  KeypadButton *enter = new KeypadButton(this, "ENTER", UIKEY_RETURN);
  enter->move(undo->x(), b->y());

  /* Hint box */
  hint_label = new QLabel("", this);
  hint_label->setAutoFillBackground(true);
  QPalette palette = hint_label->palette();
  palette.setColor(QPalette::Window, Qt::darkGray);
  hint_label->setPalette(palette);
  QFont font = hint_label->font();
  font.setPixelSize(SCBY(21));
  font.setBold(true);
  hint_label->setFont(font);
  hint_label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  hint_label->setAlignment(Qt::AlignCenter);
  hint_label->setGeometry(SCBX(2), menu->height() + DST_HEIGHT + SCBY(28), DST_WIDTH - SCBX(4), SCBY(25));
  hint_label->show();
  
  connect(this, SIGNAL(setHintSignal(int)), this, SLOT(setHintSlot(int)), Qt::QueuedConnection);
  connect(this, SIGNAL(clearHintSignal(int)), this, SLOT(clearHintSlot(int)), Qt::QueuedConnection);
  connect(this, SIGNAL(showWarningSignal(QString)), this, SLOT(showWarningSlot(QString)), Qt::BlockingQueuedConnection);

  /* Recording buttons */
  QPushButton *rec = new QPushButton("REC", this);
  rec->setGeometry(hint_label->x(), hint_label->y() + hint_label->height() + SCBY(4), SCBX(6), SCBY(20));
  rec->show();
  connect(rec, SIGNAL(clicked()), this, SLOT(recordSlot()));
  QPushButton *play = new QPushButton("PLAY", this);
  play->setGeometry(rec->x() + rec->width() + SCBX(1), rec->y(), SCBX(6), rec->height());
  play->show();
  connect(play, SIGNAL(clicked()), this, SLOT(playSlot()));
  QPushButton *stop = new QPushButton("STOP", this);
  stop->setGeometry(play->x() + play->width() + SCBX(1), rec->y(), SCBX(6), rec->height());
  stop->show();
  connect(stop, SIGNAL(clicked()), this, SLOT(stopSlot()));
  
  /* Machine status */
  machine_state = new QLabel(this);
  machine_state->setGeometry(DST_WIDTH - SCBX(9), rec->y(), SCBX(8), rec->height());
  machineStoppedSlot();
  machine_state->show();
  connect(this, SIGNAL(machineStoppedSignal()), this, SLOT(machineStoppedSlot()), Qt::QueuedConnection);
  connect(this, SIGNAL(machineRunningSignal()), this, SLOT(machineRunningSlot()), Qt::QueuedConnection);

  /* Keyboard grab */
  QCheckBox *key_grab = new QCheckBox("Grab Keyboard", this);
  key_grab->setGeometry(machine_state->x() - SCBX(17), machine_state->y(), SCBX(16), machine_state->height());
  key_grab->setCheckState(Qt::Checked);
  key_grab->show();
  connect(key_grab, SIGNAL(stateChanged(int)), this, SLOT(grabKeyboardSlot(int)));

  /* Signals for UI dialogs.
     This whole signalling business is necessary because Qt is only partially
     thread-safe. Particularly, the UI stuff is not. This is why we cannot
     call UI methods from the emulation thread and have to send a signal
     to the UI thread whenever we want anything GUIish to happen. */
  connect(this, SIGNAL(getStateNameSignal(bool, const char *, const char *)), this, SLOT(getStateNameSlot(bool, const char *, const char *)), Qt::BlockingQueuedConnection);
  connect(this, SIGNAL(loadRomSignal()), this, SLOT(loadRomSlot()), Qt::BlockingQueuedConnection);
  connect(this, SIGNAL(askUserSignal(const char *, const char *, const char *, const char *)), this, SLOT(askUserSlot(const char *, const char *, const char *, const char *)), Qt::BlockingQueuedConnection);
  connect(this, SIGNAL(fatalErrorSignal(const char *, const char *, const char *, const char *, const char *)), this, SLOT(fatalErrorSlot(const char *, const char *, const char *, const char *, const char*)), Qt::BlockingQueuedConnection);
  
  /* Baud rate control box */
  QGroupBox *gb = new QGroupBox("Baud Rate Control", this);
  gb->move(DST_WIDTH + SCRX(2), enter->y() + enter->height() + SCRY(2));
  gb->resize(SCRX(46), SCRY(17));
  /* Automatic */
  QRadioButton *rb_auto = new QRadioButton("Automatic", gb);
  rb_auto->move(SCRX(3), SCRY(3));
  rb_auto->setChecked(true);
  connect(rb_auto, SIGNAL(clicked()), this, SLOT(baudrateAutoSlot()));
  /* Incremental */
  QRadioButton *rb_inc = new QRadioButton("Incremental", gb);
  rb_inc->move(rb_auto->x(), rb_auto->y() + SCRY(4));
  connect(rb_inc, SIGNAL(clicked()), this, SLOT(baudrateIncSlot()));
  /* Fixed */
  QRadioButton *rb_fixed = new QRadioButton(" ", gb);
  rb_fixed->move(rb_inc->x(), rb_inc->y() + SCRY(4));
  connect(rb_fixed, SIGNAL(toggled(bool)), this, SLOT(baudrateFixedSlot(bool)));
  fixed_br = new QComboBox(gb);
  fixed_br->move(rb_fixed->x() + SCRX(6), rb_fixed->y());
  fixed_br->setEditable(true);
  fixed_br->lineEdit()->setValidator(new QIntValidator(5, 115200, this));
  fixed_br->addItem("15625");
  fixed_br->addItem("10400");
  fixed_br->addItem("9600");
  fixed_br->addItem("4800");
  fixed_br->addItem("1940");
  fixed_br->lineEdit()->setText("9600");
  fixed_br->setEnabled(false);
  connect(fixed_br, SIGNAL(editTextChanged(const QString &)), this, SLOT(baudrateFixedChangeSlot(const QString &)));
  QLabel *baudlabel = new QLabel("Baud", gb);
  baudlabel->move(fixed_br->x() + fixed_br->width() + SCRX(1), rb_fixed->y() + SCRY(1) - 1);
  gb->show();
  
  connect(this, SIGNAL(setBaudrateSignal(int, int)), this, SLOT(setBaudrateSlot(int, int)), Qt::QueuedConnection);

  /* Current baud rate box */
  QGroupBox *cb = new QGroupBox("Current Baud Rate", this);
  cb->setGeometry(gb->x() + SCRX(50), gb->y(), gb->width(), gb->height());
  QGridLayout *gl = new QGridLayout();
  QLabel *act_lbl = new QLabel("Actual");
  QLabel *req_lbl = new QLabel("Requested");
  req_lbl->setEnabled(false);
  act_br = new QLabel("foo", cb);
  act_br->setAlignment(Qt::AlignRight);
  act_br->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  act_br->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  req_br = new QLabel("bar", cb);
  req_br->setAlignment(Qt::AlignRight);
  req_br->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  req_br->setEnabled(false);
  gl->addWidget(act_lbl, 0, 0);
  gl->addWidget(req_lbl, 1, 0);
  gl->addWidget(act_br, 0, 1);
  gl->addWidget(req_br, 1, 1);
  cb->setLayout(gl);
  cb->show();
  
  /* Comm line indicator */
  QGroupBox *lineb = new QGroupBox("K Line", this);
  lineb->move(gb->x(), cb->y() + cb->height() + SCRY(2));
  lineb->resize(SCRX(96), SCRY(11));
  lineb->show();
  
  QHBoxLayout *hl = new QHBoxLayout();
  /* the OBD connector has 16 pins, but pin 16 is battery plus, so it's
     not likely that anyone would want to connect the K line there... */
  for (int i = 0; i < 15; i++) {
    line_led[i] = new QLabel(QString::number(i+1), lineb);
    line_led[i]->setAutoFillBackground(true);
    line_led[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    line_led[i]->setAlignment(Qt::AlignCenter);
    line_led[i]->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    hl->addWidget(line_led[i]);
  }
  lineb->setLayout(hl);
  cur_line = 1;
  setCommLineSlot(7);
  connect(this, SIGNAL(setCommLineSignal(int)), this, SLOT(setCommLineSlot(int)), Qt::QueuedConnection);
  
#ifndef NDEBUG
  /* Timing accuracy indicator */
  /* XXX: Maybe this should be converted to an LED? */
  QLabel *tl = new QLabel("Time", this);
  tl->move(lineb->x(), lineb->y() + lineb->height() + SCRY(1));
  time = new QLabel(QString::number(0), this);
  time->move(tl->x() + tl->width(), tl->y());
  tl->show();
  time->show();
#endif
  
  /* Current interface port */
  QLabel *ttyl = new QLabel("Interface", this);
  ttyl->move(lineb->x() + SCRX(50), lineb->y() + lineb->height() + SCRY(1));
  ttyl->show();
  port = new QLabel(this);
  port->move(ttyl->x() + ttyl->width() + SCRX(2), ttyl->y());
  port->resize(SCRX(46 - 2) - ttyl->width(), SCRY(3));
  port->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  font = port->font();
  font.setBold(true);
  port->setFont(font);
  port->show();
  
  // ============ End UI Widgets ===============

  /* Top left of LCD screen */
  screen_x = 0;
  screen_y = menu->height();
#ifdef Q_WS_QWS
  painter = new QDirectPainter(this, QDirectPainter::Reserved);
  region = QRegion(screen_x, screen_y, DST_WIDTH, DST_HEIGHT);
  painter->setRegion(region);
#else
  painter = new SlowPaint(this, DST_WIDTH, DST_HEIGHT);
  painter->move(screen_x, screen_y);
  painter->show();
#endif

  connect(this, SIGNAL(startPaintSignal()), this, SLOT(startPaintSlot()), Qt::QueuedConnection);
  connect(this, SIGNAL(endPaintSignal()), this, SLOT(endPaintSlot()), Qt::QueuedConnection);
  
  connect(this, SIGNAL(quitSignal()), this, SLOT(quitSlot()), Qt::BlockingQueuedConnection);
  
  /* Status LEDs */
  for (int i = 0; i < NUM_LEDS; i++) {
    int r, g, b;
    switch (i) {
      case LED_SERIAL_ENABLE: r = 0; g = 255; b = 0; break;
      case LED_SERIAL_RX: r = 255; g = 255; b = 0; break;
      case LED_SERIAL_TX: r = 255; g = 255; b = 0; break;
      case LED_SERIAL_BREAK: r = 255; g = 0; b = 0; break;
      case LED_EEPROM: r = 0; g = 255; b = 0; break;
      case LED_ECHO: r = 0; g = 0; b = 255; break;
      case LED_BEEP: r = 255; g = 0; b = 0; break;
      case LED_REC: r = 255; g = 0 ; b = 0; break;
      case LED_PLAY: r = 0; g = 255; b = 0; break;
      default: r = 0 + i * 30; g = 255 - i * 20; b = 0; break;
    }
    leds[i] = new LED(this, DST_WIDTH + SCRX(3) + i / 3 * SCRX(25),
                      menu->height() + SCRY(2) + i % 3 * SCRY(4), QColor(r, g, b),
                      led_names[i]);
    leds[i]->show();
  }
  connect(this, SIGNAL(setLEDSignal(int)), this, SLOT(setLEDSlot(int)), Qt::QueuedConnection);
  setBaudrateSlot(0, 0);
  
  /* Painting flags */
  dirty = true;
  led_dirty = true;
#ifdef Q_WS_QWS
  paint_disabled = false;
#endif
  hint_set = 0;
  
  /* Initialize keypad state */
  for (int i = 0; i < UIKEY_MAX; i++) {
    key_down[i] = key_up[i] = false;
  }
  
  serial = NULL;
  current_state_name = NULL;
}

UI::~UI()
{
  if (current_state_name)
    free(current_state_name);
}

void UI::setSerial(Serial *s)
{
  serial = s;
}

int UI::run()
{
  return qApp->exec();
}

bool UI::pollEvent(struct Event &ev)
{
  for (int i = 0; i < UIKEY_MAX; i++) {
    if (key_down[i]) {
      key_down[i] = false;
      ev.type = EVENT_KEYDOWN;
      ev.value = i;
      return true;
    }
    if (key_up[i]) {
      key_up[i] = false;
      ev.type = EVENT_KEYUP;
      ev.value = i;
      return true;
    }
  }
  return false;
}

void UI::setLED(int led, bool on)
{
  leds[led]->set(on);
  if (leds[led]->changed()) {
    led_dirty = true;
    emit setLEDSignal(led);
  }
}

void UI::setLEDSlot(int led)
{
  leds[led]->redraw();
}

void UI::flip() {
  if (led_dirty) {
    setLED(LED_SERIAL_RX, false);
    setLED(LED_SERIAL_TX, false);
    led_dirty = false;
  }
  if (dirty) {
    dirty = false;
    emit endPaintSignal();
  }
}

void UI::startPaintSlot()
{
  painter->startPainting(true);
}

void UI::endPaintSlot()
{
  painter->endPainting(region);
}

void UI::setBaudrate(int actual_baud, int target_baud)
{
  emit setBaudrateSignal(actual_baud, target_baud);
}

void UI::setBaudrateSlot(int actual_baud, int target_baud)
{
  act_br->setText(QString::number(actual_baud));
  req_br->setText(QString::number(target_baud));
}

void UI::setCommLine(int line)
{
  emit setCommLineSignal(line);
}

/* line negative -> unknown */
void UI::setCommLineSlot(int line)
{
  DEBUG(UI, "setCommLineSlot %d\n", line);
  if (line == cur_line)
    return;
  
  if (cur_line > 0 && cur_line < 16) {
    QPalette pal = palette();
    line_led[cur_line - 1]->setPalette(pal);
  }
  else if (cur_line == 42) {
    DEBUG(UI, "CAN LED off\n");
    setLED(LED_CAN, false);
  }
  
  cur_line = line;
  
  if (line == 42) {
    DEBUG(UI, "CAN LED on\n");
    setLED(LED_CAN, true);
  }
  else if (line > 0 && line < 16) {
    QPalette pal = line_led[line - 1]->palette();
    pal.setColor(QPalette::Window, Qt::green);
    line_led[line - 1]->setPalette(pal);
  }
}

void UI::setPort(const char *tty)
{
	port->setText(tty);
}

void UI::setHint(int hint)
{
  emit setHintSignal(hint);
}

void UI::setHintSlot(int hint)
{
  hint_set |= (1 << hint);
  updateHint();
}

void UI::updateHint()
{
  QString hint_text = "";
  int hint_count = 0;

  //hint_set |= (1 << HINT_TEST);

  for (int i = 0; i < NUM_HINTS; i++) {
    if (hint_set & (1 << i)) {
      if (hint_count)
        hint_text += " / ";
      hint_count++;
      switch (i) {
        case HINT_IGNITION:
          hint_text += "<font color=\"yellow\">Ignition on/off!</font>";
          break;
        case HINT_VAG_BREAKDOWN:
          hint_text += "<font color=\"green\">Communications breakdown.</font>";
          break;
        case HINT_MITSUBISHI_DIAG:
          hint_text += "<font color=\"red\">Connect diag pin to ground!</font>";
          break;
        case HINT_TEST:
          hint_text += "Test hint";
          break;
        default:
          abort();
      }
    }
  }

  hint_label->setText(hint_text);
}

void UI::clearHint(int hint)
{
  emit clearHintSignal(hint);
}

void UI::clearHintSlot(int hint)
{
  if (hint_set & (1 << hint)) {
    hint_set &= ~(1 << hint);
    updateHint();
  }
}

#include "state.h"

void UI::loadSaveState(statefile_t fp, bool write)
{
  for (int i = 0; i < NUM_LEDS; i++) {
    STATE_RW(leds[i]->state);
    if (!write)
      emit(setLEDSignal(i));
  }
  STATE_RW(hint_set);
  if (!write)
    dirty = true;
}

void UI::keypadDownSlot()
{
  KeypadButton *b = (KeypadButton *)sender();
  ERROR("keyDown()\n");
  key_down[b->key] = true;
}

void UI::keypadUpSlot()
{
  KeypadButton *b = (KeypadButton *)sender();
  ERROR("keyUp()\n");
  key_up[b->key] = true;
}

void UI::initToolkit()
{
  int qt_argc = 2;
  static char *qt_argv[] = {(char *)"emu", (char *)"-qws"};
  new QApplication(qt_argc, qt_argv);
}

void UI::quitSlot()
{
  qApp->quit();
}

void UI::quit()
{
  emit quitSignal();
}

void UI::baudrateAutoSlot()
{
  serial->setBaudrateMethod(BR_AUTO);
}

void UI::baudrateIncSlot()
{
  serial->setBaudrateMethod(BR_AUTOPLUS);
}

void UI::baudrateFixedChangeSlot(const QString &value)
{
  int br = value.toInt();
  DEBUG(UI, "fixed serial baud %d\n", br);
  serial->setFixedBaudrate(br);
  serial->setBaudrateMethod(BR_FORCE);
}

void UI::baudrateFixedSlot(bool checked)
{
  if (checked) {
    fixed_br->setEnabled(true);
    baudrateFixedChangeSlot(fixed_br->lineEdit()->text());
  }
  else {
    fixed_br->setEnabled(false);
  }
}

#ifndef NDEBUG
void UI::updateTime(int ms)
{
  time->setText(QString::number(ms));
}
#endif

void UI::showWarning(const char *text, const char *arg0, const char *arg1, const char *arg2)
{
  QString msg;
  if (arg2)
    msg.sprintf(text, arg0, arg1, arg2);
  else if (arg1)
    msg.sprintf(text, arg0, arg1);
  else if (arg0)
    msg.sprintf(text, arg0);
  else
    msg = QString(text);
  
  emit showWarningSignal(msg);
}

void UI::showWarningSlot(QString text)
{
  disablePaintingSlot();
  QMessageBox::warning(this, "Warning", text);
  enablePaintingSlot();
}

void UI::keyEvent(QKeyEvent *event, bool *key_event)
{
  if (event->isAutoRepeat())
    return;
  DEBUG(UI, "key pressed: 0x%x\n", (uint32_t)event->key());
  switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
      key_event[UIKEY_RETURN] = true; break;
    case Qt::Key_Left:
      key_event[UIKEY_LEFT] = true; break;
    case Qt::Key_Up:
      key_event[UIKEY_UP] = true; break;
    case Qt::Key_Right:
      key_event[UIKEY_RIGHT] = true; break;
    case Qt::Key_Down:
      key_event[UIKEY_DOWN] = true; break;
    case Qt::Key_F10:
      key_event[UIKEY_F10] = true; break;
    case Qt::Key_Backspace:
      key_event[UIKEY_BACKSPACE] = true; break;
    case Qt::Key_Y:
      key_event[UIKEY_y] = true; break;
    case Qt::Key_N:
      key_event[UIKEY_n] = true; break;
    case Qt::Key_0:
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
    case Qt::Key_5:
    case Qt::Key_6:
    case Qt::Key_7:
    case Qt::Key_8:
    case Qt::Key_9:
      key_event[event->key() - Qt::Key_0 + UIKEY_0] = true; break;
    case Qt::Key_F1:
    case Qt::Key_F2:
    case Qt::Key_F3:
    case Qt::Key_F4:
    case Qt::Key_F5:
    case Qt::Key_F6:
    case Qt::Key_F7:
    case Qt::Key_F8:
    case Qt::Key_F9:

      key_event[event->key() - Qt::Key_F1 + UIKEY_F1] = true; break;
    case Qt::Key_F12:
      key_event[UIKEY_F12] = true; break;
    case Qt::Key_B:
      key_event[UIKEY_b] = true; break;
    case Qt::Key_Escape:
      key_event[UIKEY_ESCAPE] = true; break;
    default:
      QWidget::keyPressEvent(event);
      break;
  }
}
void UI::keyPressEvent(QKeyEvent *event)
{
  switch (event->key()) {
    case Qt::Key_S:
      saveScreenshot();
      break;
    default:
      keyEvent(event, key_down);
      break;
  }
}

void UI::keyReleaseEvent(QKeyEvent *event)
{
  keyEvent(event, key_up);
}

void UI::getStateNameSlot(bool save, const char *dir, const char *ext)
{
  DEBUG(UI, "getStateNameSlot() start\n");
  disablePaintingSlot();

  QString caption;
  if (save)
    caption = "Save State";
  else
    caption = "Load State";
  
  QString filter = QString("*.") + QString(ext) + QString(";;*");
  QFileDialog fd(0, caption, dir, filter);
  if (save) {
    fd.setAcceptMode(QFileDialog::AcceptSave);
    fd.setFileMode(QFileDialog::AnyFile);
  }
  else {
    fd.setAcceptMode(QFileDialog::AcceptOpen);
    fd.setFileMode(QFileDialog::ExistingFile);
  }
  fd.setConfirmOverwrite(true);
  fd.setDefaultSuffix(ext);
  
  if (current_state_name) {
    free(current_state_name);
    current_state_name = NULL;
  }
  if (fd.exec()) {
    QStringList files = fd.selectedFiles();
    if (files.size() == 1)
      current_state_name = strdup(files[0].toLocal8Bit().data());
    else
      current_state_name = NULL;
  }

  enablePaintingSlot();
  DEBUG(UI, "getStateNameSlot() end, current_state_name %s\n", current_state_name);
}

const char *UI::getStateName(bool save, const char *dir, const char *ext)
{
  emit getStateNameSignal(save, dir, ext);
  return current_state_name;
}

void UI::loadStateSlot()
{
  key_down[UIKEY_l] = true;
}

void UI::saveStateSlot()
{
  key_down[UIKEY_s] = true;
}

void UI::resetSlot()
{
  key_down[UIKEY_r] = true;
}

void UI::factoryResetSlot()
{
  key_down[UIKEY_f] = true;
}

void UI::recordSlot()
{
  key_down[UIKEY_F7] = true;
}

void UI::playSlot()
{
  key_down[UIKEY_F8] = true;
}

void UI::stopSlot()
{
  key_down[UIKEY_F9] = true;
}

bool UI::askUser(const char *caption, const char *question, const char *button1, const char *button2)
{
  emit askUserSignal(caption, question, button1, button2);
  return ask_user_result;
}

void UI::askUserSlot(const char *caption, const char *question, const char *button1, const char *button2)
{
  DEBUG(UI, "user question: %s/%s => %s/%s\n", caption, question, button1, button2);
  disablePaintingSlot();
  QMessageBox qbox(QMessageBox::Information, caption, question);
  QPushButton *eins = qbox.addButton(button1, QMessageBox::AcceptRole);
  QPushButton *zwei = qbox.addButton(button2, QMessageBox::RejectRole);
  qbox.exec();
  if (qbox.clickedButton() == eins)
    ask_user_result = true;
  else
    ask_user_result = false;
  enablePaintingSlot();
}

void UI::fatalError(const char *error, const char *detail, const char *arg0, const char *arg1, const char *arg2)
{
  emit fatalErrorSignal(error, detail, arg0, arg1, arg2);
}

void UI::fatalErrorSlot(const char *error, const char *detail, const char *arg0, const char *arg1, const char *arg2)
{
  disablePaintingSlot();

  /* fuck varargs */
  QString msg;
  if (arg2)
    msg.sprintf(error, arg0, arg1, arg2);
  else if (arg1)
    msg.sprintf(error, arg0, arg1);
  else if (arg0)
    msg.sprintf(error, arg0);
  else
    msg = QString(error);
    
  QMessageBox box(QMessageBox::Critical, "Fatal Error", msg);
  if (detail)
    box.setDetailedText(detail);
  box.exec();
  enablePaintingSlot();
}

void UI::loadRom()
{
  emit loadRomSignal();
  if (load_rom_ok) {
    if (!cpu->loadRom(load_rom_name.toLocal8Bit().data())) {
      cpu->stop();
      machineStopped();
    }
    else {
      cpu->reset();
      cpu->resume();
      machineRunning();
    }
  }
}

void UI::loadRomSlot()
{
  DEBUG(UI, "loadRomSlot() start\n");
  load_rom_ok = false;
  disablePaintingSlot();

  QString caption = "Load ROM";
  
  QFileDialog fd(0, caption, "roms", "*.dat *.exe *.lha *.bin;;*");
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::ExistingFile);
  
  if (fd.exec()) {
    QStringList files = fd.selectedFiles();
    if (files.size() == 1) {
      load_rom_ok = true;
      load_rom_name = files[0];
    }
  }

  enablePaintingSlot();
  DEBUG(UI, "loadRomSlot() end\n");
}

void UI::saveScreenshot(const char *prefix)
{
#ifdef Q_WS_QWS
  uchar *pixels = QDirectPainter::frameBuffer();
  QImage::Format format;
  switch (QDirectPainter::screenDepth()) {
    case 16:
      format = QImage::Format_RGB16;
      break;
    default:
      DEBUG(WARN, "screenshot: bit depth not supported\n");
      return;
  }
  QImage img(pixels, width(), height(), format);
  QString name;
  for (int screenshot_count = 0;; screenshot_count++) {
    name.sprintf("%s%03d.png", prefix ? prefix : "scr/screen_", screenshot_count);
    if (!QFileInfo(name).exists())
      break;
  }
  img.save(name);
  DEBUG(UI, "saved screenshot as %s\n", name.toLocal8Bit().data());
#else
  DEBUG(WARN, "screenshots not implemented\n");
#endif
}

void UI::closeEvent(QCloseEvent *event)
{
  key_down[UIKEY_F10] = true;
  event->ignore();
}

void UI::menuQuitSlot()
{
  key_down[UIKEY_F10] = true;
}

void UI::setCpu(Cpu *cpu)
{
  this->cpu = cpu;
}

void UI::menuLoadRomSlot()
{
  cpu->sendCommand(CPU_CMD_LOAD_ROM);
}

void UI::menuAboutSlot()
{
  disablePaintingSlot();
  QString text("<center><strong>" APP_NAME "</strong> " VERSION "</center><br/>\n");
  text += COPYRIGHT;
  text += "<center><a href=\"http://cascade.fishpondstudios.com/\">Visit the web site for more information.</a></center>";

  QDialog box;
  box.setWindowTitle("About this Program");
  box.resize(400, 300);
  QVBoxLayout *layout = new QVBoxLayout();
  QTextEdit *text_label = new QTextEdit(text, &box);
  text_label->setReadOnly(true);
  text_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
  layout->addWidget(text_label);

  QPushButton *ok = new QPushButton("OK", &box);
  layout->addWidget(ok, 0, Qt::AlignHCenter);
  connect(ok, SIGNAL(clicked()), &box, SLOT(accept()));
  box.setLayout(layout);
  box.exec();
  enablePaintingSlot();
}

void UI::disablePaintingSlot()
{
  releaseKeyboard();
#ifdef Q_WS_QWS
  painter->lower();
  paint_disabled = true;
#endif
}

void UI::enablePaintingSlot()
{
#ifdef Q_WS_QWS
  painter->raise();
  paint_disabled = false;
  cpu->getLcd()->redraw();
#endif
  grabKeyboard();
}

void UI::machineStopped()
{
  emit machineStoppedSignal();
}

void UI::machineStoppedSlot()
{
  machine_state->setText("<font color=\"red\">STOPPED</font>");
}

void UI::machineRunning()
{
  emit machineRunningSignal();
}

void UI::machineRunningSlot()
{
  machine_state->setText("<font color=\"green\">RUNNING</font>");
}

#ifndef Q_WS_QWS
void UI::onlineHelpSlot()
{
  QDesktopServices::openUrl(QUrl("http://cascade.fishpondstudios.com/manual/"));
}
#endif

void UI::grabKeyboardSlot(int state)
{
  if (state == Qt::Checked)
    grabKeyboard();
  else
    releaseKeyboard();
}
