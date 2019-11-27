/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>
#include <shell/shell.h>
#include <settings/settings.h>

#define MODULE demo_module
#include "module_state_event.h"
#include "click_event.h"
#include "selector_event.h"
#include "config_event.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DEMO_MODULE_LOG_LEVEL);


enum state {
	STATE_DISABLED,
	STATE_IDLE,
	STATE_WDT_TEST,
};

struct state_switch {
	enum state state;
	enum click click;
	enum state next_state;
	void (*func)(void);
};


static void wdt_stop(void);

static const struct state_switch state_switch[] = {
	 /* State           Event         Next state         Callback */
	{STATE_IDLE,        CLICK_DOUBLE,  STATE_WDT_TEST, wdt_stop},
	{STATE_WDT_TEST,       CLICK_SHORT,  STATE_IDLE,  NULL},
};


static enum state state;

static void wdt_stop(void)
{
	LOG_INF("stopping feed");
	
}



static void handle_click(enum click click)
{
	if (click != CLICK_NONE) {
		for (size_t i = 0; i < ARRAY_SIZE(state_switch); i++) {
			if ((state_switch[i].state == state) &&
			    (state_switch[i].click == click)) {
				if (state_switch[i].func) {
					state_switch[i].func();
				}
				state = state_switch[i].next_state;

				return;
			}
		}
	}
}

static int init(void)
{
	state = STATE_IDLE;

	return 0;
}

static bool click_event_handler(const struct click_event *event)
{
	// if (event->key_id != CONFIG_DESKTOP_BLE_PEER_CONTROL_BUTTON) {
	// 	return false;
	// }

	if (likely(state != STATE_DISABLED)) {
		handle_click(event->click);
	}

	return false;
}

static bool event_handler(const struct event_header *eh)
{
	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			if (!init()) {
				module_set_state(MODULE_STATE_READY);
			} else {
				module_set_state(MODULE_STATE_ERROR);
			}
		}
		return false;
	}

	if (is_click_event(eh)) {
		return click_event_handler(cast_click_event(eh));
	}
	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}
EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, click_event);

