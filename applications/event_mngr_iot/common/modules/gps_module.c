/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define MODULE gps_module
#include "module_state_event.h"
#include "power_event.h"

#include "gps_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_GPS_MODULE_LOG_LEVEL);

#include <zephyr.h>
#include <drivers/gps.h>
#include <net/cloud.h>
#include <net/socket.h>
static struct device *gps_dev;

static void gps_handler(struct device *dev, struct gps_event *evt)
{
	ARG_UNUSED(dev);
	struct gps_module_event *event = new_gps_module_event();
	memcpy(&event->event,evt,sizeof(struct gps_event));
	EVENT_SUBMIT(event);
}

static void init(void)
{
	int err;
	gps_dev = device_get_binding("NRF9160_GPS");
	if (gps_dev == NULL) {
		LOG_ERR("Could not get binding to nRF9160 GPS");
		return;
	}

	err = gps_init(gps_dev, gps_handler);
	if (err) {
		LOG_ERR("Could not initialize GPS, error: %d", err);
		return;
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			init();
			return false;
		}

		return false;
	}

	if (is_gps_start_event(eh))
	{
		struct gps_start_event *event = cast_gps_start_event(eh);
		int err;
		err = gps_start(gps_dev, &event->gps_cfg);
		if (err) {
			LOG_ERR("Failed to start GPS, error: %d", err);
			return false;
		}

		LOG_INF("Periodic GPS search started with interval %d s, timeout %d s",
			event->gps_cfg.interval, event->gps_cfg.timeout);
		return false;
	}
	if (is_gps_stop_event(eh))
	{
		int err;
		err = gps_stop(gps_dev);
		if (err) {
			LOG_ERR("Failed to stop GPS, error: %d", err);
			return false;
		}
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
EVENT_SUBSCRIBE(MODULE, gps_start_event);
EVENT_SUBSCRIBE(MODULE, gps_stop_event);
EVENT_SUBSCRIBE_EARLY(MODULE, power_down_event); 