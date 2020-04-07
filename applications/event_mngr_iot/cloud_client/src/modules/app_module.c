/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define MODULE app_module
#include "module_state_event.h"
#include "power_event.h"

#include "cloudapi_event.h"
#include "button_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_APP_MODULE_LOG_LEVEL);

#include <zephyr.h>

bool ready_to_send=false;
static struct k_delayed_work cloud_update_work;

static void cloud_update_work_fn(struct k_work *work)
{
	LOG_INF("Publishing message: %s", CONFIG_CLOUD_MESSAGE);

	struct cloud_msg msg = {
		.qos = CLOUD_QOS_AT_MOST_ONCE,
		.endpoint.type = CLOUD_EP_TOPIC_MSG,
		.buf = CONFIG_CLOUD_MESSAGE,
		.len = sizeof(CONFIG_CLOUD_MESSAGE)
	};
	struct cloudapi_msg_event *event = new_cloudapi_msg_event();
	memcpy(&event->msg,&msg,sizeof(struct cloud_msg));
	EVENT_SUBMIT(event);
}

static void init(void)
{
	LOG_INF("Cloud client has started");
	k_delayed_work_init(&cloud_update_work, cloud_update_work_fn);
}

static void handle_button_event(const struct button_event *event)
{
	if(event->pressed) {
		if(ready_to_send) {
			k_delayed_work_submit(&cloud_update_work, K_NO_WAIT);	
		}
	}
}

static void handle_cloudapi_event(const struct cloudapi_event *event)
{
	if(event->event.type == CLOUD_EVT_READY)
	{
		ready_to_send = true;
	}
	else if(event->event.type == CLOUD_EVT_DISCONNECTED)
	{
		ready_to_send = false;
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (is_cloudapi_event(eh))
	{
		const struct cloudapi_event *event = cast_cloudapi_event(eh);
		handle_cloudapi_event(event);
		return false;	
	}
	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			init();
			return false;
		}

		return false;
	}
	if (is_button_event(eh)) {
		const struct button_event *event = cast_button_event(eh);
		handle_button_event(event);
		return false;
	}
	if (is_wake_up_event(eh)) {
		return false;
	}

	if (is_power_down_event(eh)) {
		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE(MODULE, wake_up_event);
EVENT_SUBSCRIBE(MODULE, power_down_event);
EVENT_SUBSCRIBE(MODULE, button_event);
EVENT_SUBSCRIBE(MODULE, cloudapi_event);