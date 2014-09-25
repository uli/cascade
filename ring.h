/*
 * ring.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

#ifndef _RING_H
#define _RING_H

#include "state.h"

template <class T>
class Ring {
public:
  Ring(int s) {
    size = s;
    start = end = 0;
    ring = new T[size];
  }
  ~Ring() {
    delete ring;
  }
  
  void add(T data) {
    ring[end] = data;
    end = (end + 1) % size;
  }
  void prepend(T data) {
    if (!start)
      start = size - 1;
    else
      start--;
    ring[start] = data;
  }
  
  T consume() {
    T ret = ring[start];
    start = (start + 1) % size;
    return ret;
  }
  T snoop() {
    return ring[start];
  }
  void flush() {
    start = end;
  }
  bool empty() {
    return start == end;
  }
  int count() {
    if (start > end)
      return size - end + start;
    else
      return end - start;
  }
  
  void loadSaveState(statefile_t fp, bool write) {
    STATE_RW(start);
    STATE_RW(end);
    STATE_RWBUF(ring, sizeof(T) * size);
  }

private:
  T *ring;
  int size;
  int start, end;
};

#endif
  