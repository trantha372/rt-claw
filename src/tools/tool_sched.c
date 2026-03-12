/*
 * Copyright (c) 2025, rt-claw Development Team
 * SPDX-License-Identifier: MIT
 *
 * Scheduler tools — let AI add/remove timed tasks via Tool Use.
 */

#include "claw_os.h"
#include "claw_tools.h"
#include "scheduler.h"
#include "ai_engine.h"

#include <string.h>
#include <stdio.h>

#ifdef CLAW_PLATFORM_ESP_IDF

#include "cJSON.h"

#define TAG              "tool_sched"
#define SCHED_AI_MAX     4
#define SCHED_PROMPT_MAX 256
#define SCHED_REPLY_MAX  1024

typedef struct {
    char prompt[SCHED_PROMPT_MAX];
    char reply[SCHED_REPLY_MAX];
    int  in_use;
} sched_ai_ctx_t;

static sched_ai_ctx_t s_ctx[SCHED_AI_MAX];

static sched_ai_ctx_t *ctx_alloc(void)
{
    for (int i = 0; i < SCHED_AI_MAX; i++) {
        if (!s_ctx[i].in_use) {
            s_ctx[i].in_use = 1;
            return &s_ctx[i];
        }
    }
    return NULL;
}

static void ctx_free_by_prompt(const char *prompt)
{
    for (int i = 0; i < SCHED_AI_MAX; i++) {
        if (s_ctx[i].in_use &&
            strcmp(s_ctx[i].prompt, prompt) == 0) {
            s_ctx[i].in_use = 0;
            return;
        }
    }
}

static void sched_ai_callback(void *arg)
{
    sched_ai_ctx_t *ctx = (sched_ai_ctx_t *)arg;

    if (ai_chat_raw(ctx->prompt, ctx->reply,
                    SCHED_REPLY_MAX) == CLAW_OK) {
        printf("\n\033[0;33m<sched>\033[0m %s\n", ctx->reply);
        fflush(stdout);
    }
}

static int tool_schedule_task(const cJSON *params, cJSON *result)
{
    cJSON *name_j = cJSON_GetObjectItem(params, "name");
    cJSON *interval_j = cJSON_GetObjectItem(params, "interval_seconds");
    cJSON *count_j = cJSON_GetObjectItem(params, "count");
    cJSON *prompt_j = cJSON_GetObjectItem(params, "prompt");

    if (!name_j || !cJSON_IsString(name_j) ||
        !interval_j || !cJSON_IsNumber(interval_j) ||
        !prompt_j || !cJSON_IsString(prompt_j)) {
        cJSON_AddStringToObject(result, "error",
                                "missing name, interval_seconds, or prompt");
        return CLAW_ERROR;
    }

    const char *name = name_j->valuestring;
    int interval_s = interval_j->valueint;
    int count = -1;

    if (count_j && cJSON_IsNumber(count_j)) {
        count = count_j->valueint;
    }

    if (interval_s < 1) {
        cJSON_AddStringToObject(result, "error",
                                "interval_seconds must be >= 1");
        return CLAW_ERROR;
    }

    sched_ai_ctx_t *ctx = ctx_alloc();
    if (!ctx) {
        cJSON_AddStringToObject(result, "error",
                                "max scheduled AI tasks reached");
        return CLAW_ERROR;
    }

    snprintf(ctx->prompt, SCHED_PROMPT_MAX, "%s",
             prompt_j->valuestring);

    if (sched_add(name, (uint32_t)interval_s * 1000, (int32_t)count,
                  sched_ai_callback, ctx) != CLAW_OK) {
        ctx->in_use = 0;
        cJSON_AddStringToObject(result, "error",
                                "scheduler full or duplicate name");
        return CLAW_ERROR;
    }

    cJSON_AddStringToObject(result, "status", "ok");
    char msg[128];

    snprintf(msg, sizeof(msg),
             "task '%s' scheduled every %ds (count=%d)",
             name, interval_s, count);
    cJSON_AddStringToObject(result, "message", msg);
    return CLAW_OK;
}

static int tool_remove_task(const cJSON *params, cJSON *result)
{
    cJSON *name_j = cJSON_GetObjectItem(params, "name");

    if (!name_j || !cJSON_IsString(name_j)) {
        cJSON_AddStringToObject(result, "error", "missing name");
        return CLAW_ERROR;
    }

    const char *name = name_j->valuestring;

    if (sched_remove(name) != CLAW_OK) {
        cJSON_AddStringToObject(result, "error", "task not found");
        return CLAW_ERROR;
    }

    ctx_free_by_prompt(name);
    cJSON_AddStringToObject(result, "status", "ok");

    char msg[64];

    snprintf(msg, sizeof(msg), "task '%s' removed", name);
    cJSON_AddStringToObject(result, "message", msg);
    return CLAW_OK;
}

static const char schema_schedule[] =
    "{\"type\":\"object\","
    "\"properties\":{"
    "\"name\":{\"type\":\"string\","
    "\"description\":\"Unique task name\"},"
    "\"interval_seconds\":{\"type\":\"integer\","
    "\"description\":\"Interval in seconds between executions\"},"
    "\"count\":{\"type\":\"integer\","
    "\"description\":\"Number of executions (-1 for infinite, default -1)\"},"
    "\"prompt\":{\"type\":\"string\","
    "\"description\":\"AI prompt to execute on each tick\"}},"
    "\"required\":[\"name\",\"interval_seconds\",\"prompt\"]}";

static const char schema_remove[] =
    "{\"type\":\"object\","
    "\"properties\":{"
    "\"name\":{\"type\":\"string\","
    "\"description\":\"Name of the task to remove\"}},"
    "\"required\":[\"name\"]}";

void claw_tools_register_sched(void)
{
    memset(s_ctx, 0, sizeof(s_ctx));

    claw_tool_register("schedule_task",
        "Schedule a recurring AI task. The prompt will be executed "
        "periodically at the given interval. Use this when the user "
        "asks to do something repeatedly or on a timer.",
        schema_schedule, tool_schedule_task);

    claw_tool_register("remove_task",
        "Remove a previously scheduled recurring task by name.",
        schema_remove, tool_remove_task);
}

#else

void claw_tools_register_sched(void)
{
}

#endif
