/*
 * Copyright (c) 2025, rt-claw Development Team
 * SPDX-License-Identifier: MIT
 *
 * Message gateway - central message routing and channel management.
 */

#ifndef CLAW_CORE_GATEWAY_H
#define CLAW_CORE_GATEWAY_H

#include "claw_os.h"
#include "claw_config.h"

enum gateway_msg_type {
    GW_MSG_DATA = 0,
    GW_MSG_CMD,
    GW_MSG_EVENT,
    GW_MSG_SWARM,
};

struct gateway_msg {
    enum gateway_msg_type type;
    uint16_t src_channel;
    uint16_t dst_channel;
    uint16_t len;
    uint8_t  payload[CLAW_GW_MSG_MAX_LEN];
};

int gateway_init(void);
int gateway_send(struct gateway_msg *msg);

#endif /* CLAW_CORE_GATEWAY_H */
