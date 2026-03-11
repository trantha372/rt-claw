/*
 * Copyright (c) 2025, rt-claw Development Team
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "claw_os.h"
#include "swarm.h"

#define TAG "swarm"

static struct swarm_node nodes[CLAW_SWARM_MAX_NODES];
static int node_count = 0;
static claw_mutex_t swarm_lock;

int swarm_init(void)
{
    swarm_lock = claw_mutex_create("swarm");
    if (!swarm_lock) {
        CLAW_LOGE(TAG, "mutex create failed");
        return CLAW_ERROR;
    }

    memset(nodes, 0, sizeof(nodes));
    node_count = 0;
    CLAW_LOGI(TAG, "initialized, max_nodes=%d", CLAW_SWARM_MAX_NODES);
    return CLAW_OK;
}

int swarm_node_count(void)
{
    return node_count;
}

void swarm_list_nodes(void)
{
    int i;

    claw_mutex_lock(swarm_lock, CLAW_WAIT_FOREVER);
    CLAW_LOGI(TAG, "nodes online: %d/%d", node_count, CLAW_SWARM_MAX_NODES);
    for (i = 0; i < CLAW_SWARM_MAX_NODES; i++) {
        if (nodes[i].state != SWARM_NODE_OFFLINE) {
            CLAW_LOGI(TAG, "  node[%d] id=0x%08x state=%d",
                      i, (unsigned)nodes[i].id, nodes[i].state);
        }
    }
    claw_mutex_unlock(swarm_lock);
}
