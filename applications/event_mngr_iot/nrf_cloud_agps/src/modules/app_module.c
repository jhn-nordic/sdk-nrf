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
#include "gps_event.h"

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
		struct gps_config gps_cfg = {
		.nav_mode = GPS_NAV_MODE_PERIODIC,
		.power_mode = GPS_POWER_MODE_DISABLED,
		.timeout = 120,
		.interval = 240,
		};
		struct gps_start_event *event = new_gps_start_event();
		memcpy(&event->gps_cfg,&gps_cfg,sizeof(struct gps_config));
		EVENT_SUBMIT(event);
		
		ready_to_send = true;
	}
	else if(event->event.type == CLOUD_EVT_DISCONNECTED)
	{
		ready_to_send = false;
	}
}

// switch (evt->type) {
// 	case GPS_EVT_SEARCH_STARTED:
// 		LOG_DBG("GPS_EVT_SEARCH_STARTED");
// 		break;
// 	case GPS_EVT_SEARCH_STOPPED:
// 		LOG_DBG("GPS_EVT_SEARCH_STOPPED");
// 		break;
// 	case GPS_EVT_SEARCH_TIMEOUT:
// 		LOG_DBG("GPS_EVT_SEARCH_TIMEOUT");
// 		break;
// 	case GPS_EVT_OPERATION_BLOCKED:
// 		LOG_DBG("GPS_EVT_OPERATION_BLOCKED");
// 		break;
// 	case GPS_EVT_OPERATION_UNBLOCKED:
// 		LOG_DBG("GPS_EVT_OPERATION_UNBLOCKED");
// 		break;
// 	case GPS_EVT_AGPS_DATA_NEEDED:
// 		LOG_DBG("GPS_EVT_AGPS_DATA_NEEDED");
// 		break;
// 	case GPS_EVT_PVT:
// 		print_satellite_stats(&evt->pvt);
// 		break;
// 	case GPS_EVT_PVT_FIX:
// 		fix_timestamp = k_uptime_get();

// 		LOG_INF("---------       FIX       ---------");
// 		LOG_INF("Time to fix: %d seconds",
// 			(u32_t)(fix_timestamp - start_search_timestamp) / 1000);
// 		print_pvt_data(&evt->pvt);
// 		LOG_INF("-----------------------------------");
// 		break;
// 	case GPS_EVT_NMEA_FIX:
// 		send_nmea(evt->nmea.buf);
// 		break;
// 	default:
// 		break;
// 	}
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