/*
 * Copyright (c) 2025, rt-claw Development Team
 * SPDX-License-Identifier: MIT
 *
 * Global compile-time configuration for rt-claw.
 */

#ifndef __CLAW_CONFIG_H__
#define __CLAW_CONFIG_H__

#define RT_CLAW_VERSION         "0.1.0"

/* Gateway */
#define CLAW_GW_MSG_POOL_SIZE   16
#define CLAW_GW_MSG_MAX_LEN     256
#define CLAW_GW_THREAD_STACK    4096
#define CLAW_GW_THREAD_PRIO     15

/* Swarm */
#define CLAW_SWARM_MAX_NODES    32
#define CLAW_SWARM_HEARTBEAT_MS 5000
#define CLAW_SWARM_TIMEOUT_MS   15000

#endif /* __CLAW_CONFIG_H__ */
