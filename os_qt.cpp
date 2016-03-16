/*
 * os_qt.cpp
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#include "os.h"
#include <QThread>

class OsThread : public QThread
{
public:
  OsThread(int (*fun)(void *), void *data) : QThread() {
    this->fun = fun;
    this->data = data;
  }
  void run() {
    res = fun(data);
  }
  int result() {
    return res;
  }
    
private:
  int (*fun)(void *);
  void *data;
  int res;
};

void *os_create_thread(int (*fn)(void *), void *data)
{
  OsThread *thr = new OsThread(fn, data);
  thr->start();
  return (void *)thr;
}

void os_wait_thread(void *thread, int *status)
{
  OsThread *thr = (OsThread *)thread;
  thr->wait();
  if (status)
    *status = thr->result();
}

void os_kill_thread(void *thread, int *status)
{
  OsThread *thr = (OsThread *)thread;
  thr->terminate();
  thr->wait();
  if (status)
    *status = thr->result();
}
