/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Console output with optional capture for IM routing.
 */

#include "osal/claw_os.h"
#include "claw/core/console.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ---- Capture state ---- */

static struct {
    char  *buf;
    size_t size;
    size_t pos;
} s_cap;

static struct claw_mutex *s_cap_lock;

static void cap_init_once(void)
{
    if (!s_cap_lock) {
        s_cap_lock = claw_mutex_create("claw_pf");
    }
}

void claw_printf_capture_start(char *buf, size_t size)
{
    cap_init_once();
    claw_mutex_lock(s_cap_lock, CLAW_WAIT_FOREVER);
    s_cap.buf = buf;
    s_cap.size = size;
    s_cap.pos = 0;
    if (buf && size > 0) {
        buf[0] = '\0';
    }
}

size_t claw_printf_capture_stop(void)
{
    size_t n = s_cap.pos;
    s_cap.buf = NULL;
    s_cap.size = 0;
    s_cap.pos = 0;
    if (s_cap_lock) {
        claw_mutex_unlock(s_cap_lock);
    }
    return n;
}

/* Strip ANSI escape sequences (\033[...m) from src into dst */
static size_t strip_ansi(const char *src, size_t src_len,
                         char *dst, size_t dst_size)
{
    size_t di = 0;
    size_t si = 0;

    while (si < src_len && di < dst_size - 1) {
        if (src[si] == '\033' && si + 1 < src_len &&
            src[si + 1] == '[') {
            si += 2;
            while (si < src_len && src[si] != 'm') {
                si++;
            }
            if (si < src_len) {
                si++;
            }
        } else {
            dst[di++] = src[si++];
        }
    }
    dst[di] = '\0';
    return di;
}

int claw_printf(const char *fmt, ...)
{
    va_list ap;
    int ret;

    /* Always write to stdout (serial console) */
    va_start(ap, fmt);
    ret = vprintf(fmt, ap);
    va_end(ap);

    /* Append to capture buffer if active */
    if (s_cap.buf && s_cap.pos < s_cap.size - 1) {
        char tmp[512];
        va_start(ap, fmt);
        int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
        va_end(ap);

        if (n > 0) {
            size_t avail = s_cap.size - 1 - s_cap.pos;
            size_t written = strip_ansi(tmp, (size_t)n,
                                        s_cap.buf + s_cap.pos,
                                        avail + 1);
            s_cap.pos += written;
        }
    }

    return ret;
}
