/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Console output with optional capture for IM routing.
 *
 * Always writes to stdout (serial console).  When capture is active,
 * output is also appended to a buffer with ANSI codes stripped.
 */

#ifndef CLAW_CORE_CONSOLE_H
#define CLAW_CORE_CONSOLE_H

#include <stddef.h>

/**
 * Printf wrapper.  Output goes to stdout, and also to the capture
 * buffer if claw_printf_capture_start() was called.
 */
int claw_printf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/**
 * Start capturing claw_printf output into buf.
 * Thread-safe: only one capture active at a time (mutex-protected).
 */
void claw_printf_capture_start(char *buf, size_t size);

/**
 * Stop capturing.  Returns bytes written (excluding NUL).
 */
size_t claw_printf_capture_stop(void);

#endif /* CLAW_CORE_CONSOLE_H */
