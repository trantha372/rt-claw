/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * GPIO tools — expose GPIO control as LLM-callable tools.
 */

#include "claw/tools/claw_tools.h"

#include <string.h>
#include <stdio.h>

#define TAG "tool_gpio"

#ifdef CLAW_PLATFORM_ESP_IDF

#include "driver/gpio.h"
#include "soc/soc_caps.h"

#define GPIO_PIN_MAX  SOC_GPIO_PIN_COUNT

/* --- GPIO safety policy --- */

#define GPIO_POLICY_INPUT       (1 << 0)
#define GPIO_POLICY_OUTPUT      (1 << 1)
#define GPIO_POLICY_INPUT_OUTPUT (GPIO_POLICY_INPUT | GPIO_POLICY_OUTPUT)

typedef struct {
    int      pin;
    uint32_t allowed;       /* bitmask of GPIO_POLICY_* */
    const char *label;      /* human-readable purpose */
} gpio_policy_entry_t;

/*
 * Static whitelist — only listed pins are accessible to the AI.
 * Pins not in this table are silently blocked with a friendly hint.
 * Platform-specific via #if defined(CONFIG_IDF_TARGET_*).
 */
#if defined(CONFIG_IDF_TARGET_ESP32C3)

static const gpio_policy_entry_t s_gpio_policy[] = {
    {  0, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    {  1, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    {  2, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    {  3, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    {  4, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC / LED" },
    {  5, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    {  6, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    {  7, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    {  8, GPIO_POLICY_INPUT,        "boot strapping (input only)" },
    {  9, GPIO_POLICY_INPUT,        "boot strapping (input only)" },
    { 10, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    /* 11-17: SPI flash — forbidden */
    /* 18-19: USB-JTAG — forbidden */
    { 20, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 21, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
};

#elif defined(CONFIG_IDF_TARGET_ESP32S3)

static const gpio_policy_entry_t s_gpio_policy[] = {
    {  1, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    {  2, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    {  3, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    {  4, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    {  5, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    {  6, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    {  7, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    {  8, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    {  9, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    { 10, GPIO_POLICY_INPUT_OUTPUT, "user GPIO / ADC" },
    { 11, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 12, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 13, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 14, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 15, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 16, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 17, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 18, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 21, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    /* 19-20: USB-JTAG — forbidden */
    /* 26-32: SPI flash/PSRAM — forbidden */
    { 38, GPIO_POLICY_INPUT_OUTPUT, "user GPIO" },
    { 46, GPIO_POLICY_INPUT,        "input only" },
};

#else

static const gpio_policy_entry_t s_gpio_policy[] = { {0, 0, NULL} };

#endif

#define GPIO_POLICY_COUNT \
    (sizeof(s_gpio_policy) / sizeof(s_gpio_policy[0]))

/*
 * Check if a pin+mode combination is allowed.
 * Returns the policy entry on success, NULL on denial.
 */
static const gpio_policy_entry_t *gpio_check_policy(int pin,
                                                     uint32_t mode_bits)
{
    for (int i = 0; i < (int)GPIO_POLICY_COUNT; i++) {
        if (s_gpio_policy[i].pin == pin) {
            if ((s_gpio_policy[i].allowed & mode_bits) == mode_bits) {
                return &s_gpio_policy[i];
            }
            return NULL;    /* pin found but mode not allowed */
        }
    }
    return NULL;            /* pin not in whitelist */
}

static int gpio_policy_deny(int pin, const char *op, cJSON *result)
{
    char msg[128];

    /* Look up label for a friendlier message */
    for (int i = 0; i < (int)GPIO_POLICY_COUNT; i++) {
        if (s_gpio_policy[i].pin == pin) {
            snprintf(msg, sizeof(msg),
                     "GPIO %d: %s denied — pin is %s",
                     pin, op, s_gpio_policy[i].label);
            cJSON_AddStringToObject(result, "error", msg);
            return CLAW_ERROR;
        }
    }

    snprintf(msg, sizeof(msg),
             "GPIO %d: access denied — pin reserved by hardware "
             "(flash, USB-JTAG, or PSRAM)", pin);
    cJSON_AddStringToObject(result, "error", msg);
    return CLAW_ERROR;
}

static int tool_gpio_set(const cJSON *params, cJSON *result)
{
    cJSON *pin_j = cJSON_GetObjectItem(params, "pin");
    cJSON *level_j = cJSON_GetObjectItem(params, "level");

    if (!pin_j || !cJSON_IsNumber(pin_j) ||
        !level_j || !cJSON_IsNumber(level_j)) {
        cJSON_AddStringToObject(result, "error", "missing pin or level");
        return CLAW_ERROR;
    }

    int pin = pin_j->valueint;
    int level = level_j->valueint;

    if (pin < 0 || pin >= GPIO_PIN_MAX) {
        cJSON_AddStringToObject(result, "error", "pin out of range");
        return CLAW_ERROR;
    }

    if (!gpio_check_policy(pin, GPIO_POLICY_OUTPUT)) {
        return gpio_policy_deny(pin, "output", result);
    }

    /* Ensure pin is configured as output */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    esp_err_t err = gpio_set_level(pin, level ? 1 : 0);

    if (err == ESP_OK) {
        char msg[64];
        snprintf(msg, sizeof(msg), "GPIO %d set to %s",
                 pin, level ? "HIGH" : "LOW");
        cJSON_AddStringToObject(result, "status", "ok");
        cJSON_AddStringToObject(result, "message", msg);
        CLAW_LOGI(TAG, "%s", msg);
    } else {
        cJSON_AddStringToObject(result, "error", "gpio_set_level failed");
    }
    return (err == ESP_OK) ? CLAW_OK : CLAW_ERROR;
}

static int tool_gpio_get(const cJSON *params, cJSON *result)
{
    cJSON *pin_j = cJSON_GetObjectItem(params, "pin");

    if (!pin_j || !cJSON_IsNumber(pin_j)) {
        cJSON_AddStringToObject(result, "error", "missing pin");
        return CLAW_ERROR;
    }

    int pin = pin_j->valueint;
    if (pin < 0 || pin >= GPIO_PIN_MAX) {
        cJSON_AddStringToObject(result, "error", "pin out of range");
        return CLAW_ERROR;
    }

    if (!gpio_check_policy(pin, GPIO_POLICY_INPUT)) {
        return gpio_policy_deny(pin, "input", result);
    }

    int level = gpio_get_level(pin);
    cJSON_AddStringToObject(result, "status", "ok");
    cJSON_AddNumberToObject(result, "pin", pin);
    cJSON_AddNumberToObject(result, "level", level);
    cJSON_AddStringToObject(result, "level_str", level ? "HIGH" : "LOW");
    return CLAW_OK;
}

static int tool_gpio_config(const cJSON *params, cJSON *result)
{
    cJSON *pin_j = cJSON_GetObjectItem(params, "pin");
    cJSON *mode_j = cJSON_GetObjectItem(params, "mode");

    if (!pin_j || !cJSON_IsNumber(pin_j) ||
        !mode_j || !cJSON_IsString(mode_j)) {
        cJSON_AddStringToObject(result, "error",
                                "missing pin or mode");
        return CLAW_ERROR;
    }

    int pin = pin_j->valueint;
    if (pin < 0 || pin >= GPIO_PIN_MAX) {
        cJSON_AddStringToObject(result, "error", "pin out of range");
        return CLAW_ERROR;
    }

    gpio_mode_t mode;
    uint32_t policy_bits;
    const char *mode_str = mode_j->valuestring;
    if (strcmp(mode_str, "output") == 0) {
        mode = GPIO_MODE_OUTPUT;
        policy_bits = GPIO_POLICY_OUTPUT;
    } else if (strcmp(mode_str, "input") == 0) {
        mode = GPIO_MODE_INPUT;
        policy_bits = GPIO_POLICY_INPUT;
    } else if (strcmp(mode_str, "input_output") == 0) {
        mode = GPIO_MODE_INPUT_OUTPUT;
        policy_bits = GPIO_POLICY_INPUT_OUTPUT;
    } else {
        cJSON_AddStringToObject(result, "error",
                                "mode must be input/output/input_output");
        return CLAW_ERROR;
    }

    if (!gpio_check_policy(pin, policy_bits)) {
        return gpio_policy_deny(pin, mode_str, result);
    }

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = mode,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);

    if (err == ESP_OK) {
        char msg[64];
        snprintf(msg, sizeof(msg), "GPIO %d configured as %s", pin, mode_str);
        cJSON_AddStringToObject(result, "status", "ok");
        cJSON_AddStringToObject(result, "message", msg);
        CLAW_LOGI(TAG, "%s", msg);
    } else {
        cJSON_AddStringToObject(result, "error", "gpio_config failed");
    }
    return (err == ESP_OK) ? CLAW_OK : CLAW_ERROR;
}

/* JSON schema strings (static, compile-time) */

static const char schema_gpio_set[] =
    "{\"type\":\"object\","
    "\"properties\":{"
    "\"pin\":{\"type\":\"integer\",\"description\":\"GPIO pin number (0-21)\"},"
    "\"level\":{\"type\":\"integer\",\"enum\":[0,1],"
    "\"description\":\"0=LOW, 1=HIGH\"}},"
    "\"required\":[\"pin\",\"level\"]}";

static const char schema_gpio_get[] =
    "{\"type\":\"object\","
    "\"properties\":{"
    "\"pin\":{\"type\":\"integer\",\"description\":\"GPIO pin number (0-21)\"}},"
    "\"required\":[\"pin\"]}";

static const char schema_gpio_config[] =
    "{\"type\":\"object\","
    "\"properties\":{"
    "\"pin\":{\"type\":\"integer\",\"description\":\"GPIO pin number (0-21)\"},"
    "\"mode\":{\"type\":\"string\",\"enum\":[\"input\",\"output\",\"input_output\"],"
    "\"description\":\"GPIO direction mode\"}},"
    "\"required\":[\"pin\",\"mode\"]}";

void claw_tools_register_gpio(void)
{
    claw_tool_register("gpio_set",
        "Set a GPIO pin output level (HIGH=1 or LOW=0). "
        "Automatically configures the pin as output.",
        schema_gpio_set, tool_gpio_set);

    claw_tool_register("gpio_get",
        "Read the current level of a GPIO pin. Returns 0 (LOW) or 1 (HIGH).",
        schema_gpio_get, tool_gpio_get);

    claw_tool_register("gpio_config",
        "Configure a GPIO pin direction mode (input, output, or input_output).",
        schema_gpio_config, tool_gpio_config);
}

#else /* non-ESP-IDF */

void claw_tools_register_gpio(void)
{
    /* GPIO tools not available on this platform */
}

#endif
