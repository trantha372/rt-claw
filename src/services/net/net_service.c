/*
 * Copyright (c) 2025, rt-claw Development Team
 * SPDX-License-Identifier: MIT
 */

#include "claw_os.h"
#include "net_service.h"

#define TAG "net"

int net_service_init(void)
{
    CLAW_LOGI(TAG, "service initialized (network backend pending)");
    return CLAW_OK;
}
