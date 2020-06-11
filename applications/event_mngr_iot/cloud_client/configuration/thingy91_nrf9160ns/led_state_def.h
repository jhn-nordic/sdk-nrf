/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "led_state.h"
#include "led_effect.h"

/* This configuration file is included only once from led_state module and holds
 * information about LED effect associated with each state.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} led_state_def_include_once;


/* Map function to LED ID */
static const u8_t led_map[LED_ID_COUNT] = {
	[LED_ID_SYSTEM_STATE] = 0,
};

static const struct led_effect led_system_state_effect[LED_SYSTEM_STATE_COUNT] = {
	[LED_SYSTEM_STATE_IDLE]     = LED_EFFECT_LED_BREATH(500, LED_COLOR(150, 150, 150)),
	[LED_SYSTEM_STATE_CONNECTED] = LED_EFFECT_LED_BREATHE_WITH_PAUSE(500,5000, LED_COLOR(0, 0, 255)),
	[LED_SYSTEM_STATE_ERROR]    = LED_EFFECT_LED_BLINK(200, LED_COLOR(255, 0, 0)),
};

