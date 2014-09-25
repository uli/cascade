/*
 * os.h
 *
 * (C) Copyright 2014 Ulrich Hecht
 *
 * This file is part of CASCADE.  CASCADE is almost free software; you can
 * redistribute it and/or modify it under the terms of the Cascade Public
 * License 1.0.  Read the file "LICENSE" for details.
 */

int os_serial_open(const char *tty, bool nonblock = true);
int os_serial_close(int handle);
int os_serial_send(int handle, const char *msg);
int os_serial_send_buf(int handle, const unsigned char *buf, int len);
int os_serial_send_byte(int handle, char byte);
int os_serial_send_byte_5baud(int handle, char byte);
char *os_serial_read(int handle, int timeout, int eot);
int os_serial_expect(int handle, int prompt, int timeout);
int os_serial_flush(int handle);
int os_serial_bytes_received(int handle);
int os_serial_read_to_buffer(int handle, void *buf, int count);
int os_serial_set_baudrate(int handle, int baudrate);
const char *os_serial_get_error(void);
int os_serial_find_port(const char *driver, char **tty);

void os_msleep(int ms);
unsigned int os_mtime(void);
void *os_create_thread(int (*fn)(void *), void *data);
void os_wait_thread(void *thread, int *status);
void os_kill_thread(void *thread, int *status);

int os_serial_set_break(int fd);
int os_serial_clear_break(int fd);
int os_serial_set_rts(int fd);
int os_serial_clear_rts(int fd);
int os_serial_set_dtr(int fd);
int os_serial_clear_dtr(int fd);
