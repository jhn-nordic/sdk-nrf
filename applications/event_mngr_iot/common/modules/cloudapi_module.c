/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define MODULE cloud_client_module
#include "module_state_event.h"
#include "power_event.h"
#include "button_event.h"

#include "lte_lc_event.h"
#include "cloudapi_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUDAPI_MODULE_LOG_LEVEL);

#include <zephyr.h>
#include <modem/lte_lc.h>
#include <net/cloud.h>
#include <net/socket.h>

#define THREAD_STACK_SIZE	8192
#define THREAD_PRIORITY		CONFIG_MAIN_THREAD_PRIORITY

static struct cloud_backend *cloud_backend;

void cloud_event_handler(const struct cloud_backend *const backend,
			 const struct cloud_event *const evt,
			 void *user_data)
{
	ARG_UNUSED(user_data);
	ARG_UNUSED(backend);

	struct cloudapi_event *event = new_cloudapi_event();
	memcpy(&event->event,evt,sizeof(struct cloud_event));
	EVENT_SUBMIT(event);
}

static K_THREAD_STACK_DEFINE(thread_stack, THREAD_STACK_SIZE);
static struct k_thread thread;

void init(void)
{
	int err;

	cloud_backend = cloud_get_binding(CONFIG_CLOUD_BACKEND);
	__ASSERT(cloud_backend != NULL, "%s backend not found",
		 CONFIG_CLOUD_BACKEND);

	err = cloud_init(cloud_backend, cloud_event_handler);
	if (err) {
		LOG_ERR("Cloud backend could not be initialized, error: %d",
			err);
	}
}
void main_fn(void)
{
	int err;
	LOG_INF("Connecting to cloud ");
	err = cloud_connect(cloud_backend);
	if (err) {
		LOG_ERR("cloud_connect, error: %d", err);
	}

	struct pollfd fds[] = {
		{
			.fd = cloud_backend->config->socket,
			.events = POLLIN
		}
	};

	while (true) {
		err = poll(fds, ARRAY_SIZE(fds),
			   cloud_keepalive_time_left(cloud_backend));
		if (err < 0) {
			LOG_ERR("poll() returned an error: %d\n", err);
			continue;
		}

		if (err == 0) {
			cloud_ping(cloud_backend);
			continue;
		}

		if ((fds[0].revents & POLLIN) == POLLIN) {
			cloud_input(cloud_backend);
		}

		if ((fds[0].revents & POLLNVAL) == POLLNVAL) {
			LOG_ERR("Socket error: POLLNVAL");
			LOG_ERR("The cloud socket was unexpectedly closed.");
		}

		if ((fds[0].revents & POLLHUP) == POLLHUP) {
			LOG_ERR("Socket error: POLLHUP");
			LOG_ERR("Connection was closed by the cloud.");
		}

		if ((fds[0].revents & POLLERR) == POLLERR) {
			LOG_ERR("Socket error: POLLERR");
			LOG_ERR("Cloud connection was unexpectedly closed.");
		}
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			/* Start state machine thread */
			//__ASSERT_NO_MSG(state.state == STATE_DISABLED);
			init();

			return false;
		}

		return false;
	}
	if (is_lte_lc_event(eh)) {
		const struct lte_lc_event *event =
			cast_lte_lc_event(eh);
			if(event->event.type == LTE_LC_EVT_NW_REG_STATUS && ((event->event.nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME) || (event->event.nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING)))
			{
				k_thread_create(&thread, thread_stack,
					THREAD_STACK_SIZE,
					(k_thread_entry_t)main_fn,
					NULL, NULL, NULL,
					THREAD_PRIORITY, 0, K_NO_WAIT);
				k_thread_name_set(&thread, MODULE_NAME "_thread");

			}
		return false;
	}
	if (is_cloudapi_event(eh))
	{
		return false;	
	}
	if (is_cloudapi_msg_event(eh))
	{
		const struct cloudapi_msg_event *event =
			cast_cloudapi_msg_event(eh);
			cloud_send(cloud_backend, (struct cloud_msg *)&(event->msg));
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
EVENT_SUBSCRIBE(MODULE, lte_lc_event);
EVENT_SUBSCRIBE(MODULE, cloudapi_event);
EVENT_SUBSCRIBE(MODULE, cloudapi_msg_event);
EVENT_SUBSCRIBE_EARLY(MODULE, power_down_event); 