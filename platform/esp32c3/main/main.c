/*
 * Copyright (c) 2025, rt-claw Development Team
 * SPDX-License-Identifier: MIT
 *
 * Platform entry for ESP32-C3 / ESP-IDF + FreeRTOS.
 */

#include "claw_os.h"
#include "claw_init.h"

void app_main(void)
{
    claw_init();

    /* Main task keeps running — services are in their own threads */
    while (1) {
        claw_thread_delay_ms(1000);
    }
}
