/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Interactive shell for Linux native platform.
 *
 * Chat-first REPL: direct input goes to AI, /commands for system.
 * Uses fgets for input — no raw-mode line editing (keep it simple).
 */

#include "osal/claw_os.h"
#include "claw/services/ai/ai_engine.h"
#include "claw/shell/shell_commands.h"
#include "claw_board.h"

#ifdef CONFIG_RTCLAW_SKILL_ENABLE
#include "claw/services/ai/ai_skill.h"
#endif

#include <stdio.h>
#include <string.h>

#define TAG         "shell"
#define REPLY_SIZE  4096
#define INPUT_SIZE  256
#define MAX_ARGS    8
#define SKILL_REPLY_SIZE 2048

extern volatile int g_exit_flag;

static char *s_reply;

/* ---- Command table ---- */

static void cmd_help(int argc, char **argv);
static void cmd_quit(int argc, char **argv);

static const shell_cmd_t s_builtin_commands[] = {
    SHELL_CMD("/help", cmd_help, "Show this help"),
    SHELL_CMD("/quit", cmd_quit, "Exit rt-claw"),
};

static void cmd_quit(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    g_exit_flag = 1;
}

static void cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int board_cmd_count = 0;
    const shell_cmd_t *board_cmds = board_platform_commands(&board_cmd_count);

    shell_print_help(s_builtin_commands,
                     SHELL_CMD_COUNT(s_builtin_commands));
    if (board_cmds && board_cmd_count > 0) {
        shell_print_help(board_cmds, board_cmd_count);
    }
    shell_print_help(shell_common_commands,
                     shell_common_command_count());

#ifdef CONFIG_RTCLAW_SKILL_ENABLE
    if (ai_skill_count() > 0) {
        printf("\n  Dynamic skills (invoke as /name [args]):\n");
        for (int i = 0; i < ai_skill_count(); i++) {
            printf("    /%s\n", ai_skill_get_name(i));
        }
    }
#endif

    printf("\n  Anything else is sent directly to AI.\n");
}

/* ---- Command dispatch ---- */

static void dispatch_command(char *line)
{
    char *argv[MAX_ARGS];
    int argc = shell_tokenize(line, argv, MAX_ARGS);

    if (argc == 0) {
        return;
    }
    if (shell_dispatch(s_builtin_commands,
                       SHELL_CMD_COUNT(s_builtin_commands),
                       argc, argv)) {
        return;
    }

    int board_cmd_count = 0;
    const shell_cmd_t *board_cmds = board_platform_commands(&board_cmd_count);
    if (board_cmds && shell_dispatch(board_cmds, board_cmd_count,
                                     argc, argv)) {
        return;
    }

    if (shell_dispatch(shell_common_commands,
                       shell_common_command_count(),
                       argc, argv)) {
        return;
    }

#ifdef CONFIG_RTCLAW_SKILL_ENABLE
    {
        char *skill_reply = claw_malloc(SKILL_REPLY_SIZE);
        if (skill_reply && ai_skill_try_command(argv[0], argc, argv,
                                                skill_reply,
                                                SKILL_REPLY_SIZE)
                == CLAW_OK) {
            printf("\n" CLR_GREEN "<rt-claw> " CLR_RESET "%s\n",
                   skill_reply);
            claw_free(skill_reply);
            return;
        }
        claw_free(skill_reply);
    }
#endif

    printf("Unknown command: %s (type /help)\n", argv[0]);
}

/* ---- Thinking animation ---- */

static volatile int s_anim_active;
static volatile int s_anim_phase;

static void anim_thread_fn(void *arg)
{
    (void)arg;
    int dots = 0;
    const char *dot_str[] = { ".", "..", "..." };

    while (s_anim_active && !claw_thread_should_exit()) {
        if (s_anim_phase == 0) {
            printf("\r  " CLR_MAGENTA "thinking %s"
                   CLR_RESET "   ", dot_str[dots]);
            fflush(stdout);
            dots = (dots + 1) % 3;
        }
        claw_thread_delay_ms(500);
    }
}

static void chat_status_cb(int status, const char *detail)
{
    if (status == AI_STATUS_THINKING) {
        s_anim_phase = 0;
    } else if (status == AI_STATUS_TOOL_CALL) {
        s_anim_phase = 1;
        printf("\r  " CLR_YELLOW "► %s" CLR_RESET
               "                    \n",
               detail ? detail : "?");
        fflush(stdout);
    } else if (status == AI_STATUS_DONE) {
        s_anim_active = 0;
    }
}

static void do_chat(const char *msg)
{
    s_anim_active = 1;
    s_anim_phase = 0;
    ai_set_status_cb(chat_status_cb);

    claw_thread_t anim = claw_thread_create("anim",
        anim_thread_fn, NULL, 2048, 20);

    int ret = ai_chat(msg, s_reply, REPLY_SIZE);

    s_anim_active = 0;
    ai_set_status_cb(NULL);
    if (anim) {
        claw_thread_delete(anim);
    }
    printf("\r                              \r");

    if (ret == CLAW_OK) {
        printf(CLR_GREEN "<rt-claw> " CLR_RESET "%s\n", s_reply);
    } else {
        printf(CLR_RED "<error> " CLR_RESET "%s\n", s_reply);
    }
}

/* ---- Public API ---- */

void linux_shell_loop(void)
{
    char input[INPUT_SIZE];

    s_reply = claw_malloc(REPLY_SIZE);
    if (!s_reply) {
        CLAW_LOGE(TAG, "no memory for reply buffer");
        return;
    }

    printf("\n");
    printf(CLR_CYAN "  rt-claw chat" CLR_RESET
           "  (type /help for commands)\n");
    printf("  Direct input sends to AI, /command for system.\n");
    printf("\n");

    while (!g_exit_flag) {
        printf("\n" CLR_CYAN "<You> " CLR_RESET);
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        /* Strip trailing newline */
        size_t len = strlen(input);
        while (len > 0 && (input[len - 1] == '\n' ||
                           input[len - 1] == '\r')) {
            input[--len] = '\0';
        }

        if (len == 0) {
            continue;
        }

        if (input[0] == '/') {
            dispatch_command(input);
        } else {
            do_chat(input);
        }
    }

    claw_free(s_reply);
    printf("\nBye!\n");
}
