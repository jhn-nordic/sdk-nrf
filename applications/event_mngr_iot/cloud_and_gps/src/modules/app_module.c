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
#include "click_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_APP_MODULE_LOG_LEVEL);

#include <zephyr.h>
#include <stdio.h>

bool ready_to_send=false;
bool gps_request_start=false;

static void cloud_msg_send(struct cloud_msg *msg)
{
	struct cloudapi_msg_event *event = new_cloudapi_msg_event();
	memcpy(&event->msg,msg,sizeof(struct cloud_msg));
	EVENT_SUBMIT(event);
}

static void print_pvt_data(const struct gps_pvt *pvt_data)
{
	char buf[300];
	size_t len;

	len = snprintf(buf, sizeof(buf),
		      "Longitude:  %f\n"
		      "Latitude:   %f\n"
		      "Altitude:   %f\n"
		      "Speed:      %f\n"
		      "Heading:    %f\n"
		      "Date:       %02u-%02u-%02u\n"
		      "Time (UTC): %02u:%02u:%02u\n",
		      pvt_data->longitude, pvt_data->latitude,
		      pvt_data->altitude, pvt_data->speed, pvt_data->heading,
		      pvt_data->datetime.day, pvt_data->datetime.month,
		      pvt_data->datetime.year, pvt_data->datetime.hour,
		      pvt_data->datetime.minute, pvt_data->datetime.seconds);
	if (len < 0) {
		LOG_ERR("Could not construct PVT print");
	} else {
		LOG_INF("%s", log_strdup(buf));
	}
}

static void send_nmea(const char *nmea)
{
	char buf[150];
	struct cloud_msg msg = {
		.qos = CLOUD_QOS_AT_MOST_ONCE,
		.endpoint.type = CLOUD_EP_TOPIC_MSG,
		.buf = buf,
	};

	msg.len = snprintf(buf, sizeof(buf),
		"{"
			"\"appId\":\"GPS\","
			"\"data\":\"%s\","
			"\"messageType\":\"DATA\""
		"}", nmea);
	if (msg.len < 0) {
		LOG_ERR("Failed to create GPS cloud message");
		return;
	}
	if(ready_to_send)
	{
		cloud_msg_send(&msg);
		LOG_INF("GPS position sent to cloud");
	}
	else
	{
		LOG_ERR("Cannot send GPS postion to cloud. Cloud is not connected");
	}
}

static void init(void)
{
	LOG_INF("Cloud client has started");
}

static void handle_click_event(const struct click_event *event)
{
	if(event->click == CLICK_SHORT) {
		if(ready_to_send) {
			LOG_INF("Publishing message: %s", CONFIG_CLOUD_MESSAGE);
			struct cloud_msg msg = {
				.qos = CLOUD_QOS_AT_MOST_ONCE,
				.endpoint.type = CLOUD_EP_TOPIC_MSG,
				.buf = CONFIG_CLOUD_MESSAGE,
				.len = sizeof(CONFIG_CLOUD_MESSAGE)
			};
			cloud_msg_send(&msg);
		}
	}
	else if(event->click == CLICK_LONG)
	{
		if(ready_to_send) {
			 if(gps_request_start) {
				struct gps_stop_event *event = new_gps_stop_event();
				EVENT_SUBMIT(event);
				gps_request_start=false;
			 }
			 else {
				
				struct gps_config gps_cfg = {
				.nav_mode = GPS_NAV_MODE_PERIODIC,
				.power_mode = GPS_POWER_MODE_DISABLED,
				.timeout = 230,
				.interval = 240,
				.delete_agps_data=true
				};
				struct gps_start_event *event = new_gps_start_event();
				memcpy(&event->gps_cfg,&gps_cfg,sizeof(struct gps_config));
				EVENT_SUBMIT(event);
				gps_request_start=true;	 
			 }
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

static void handle_gps_module_event(const struct gps_module_event *event)
{
	static u32_t fix_timestamp, start_search_timestamp;
	switch (event->event.type) {
		case GPS_EVT_SEARCH_STARTED:
			start_search_timestamp = k_uptime_get();		
			break;
		case GPS_EVT_SEARCH_STOPPED:
		case GPS_EVT_SEARCH_TIMEOUT:
		case GPS_EVT_OPERATION_BLOCKED:
		case GPS_EVT_OPERATION_UNBLOCKED:
		case GPS_EVT_AGPS_DATA_NEEDED:
		case GPS_EVT_PVT:
			break;
		case GPS_EVT_PVT_FIX:
			fix_timestamp = k_uptime_get();

			LOG_INF("---------       FIX       ---------");
			LOG_INF("Time to fix: %d seconds",
				(u32_t)(fix_timestamp - start_search_timestamp) / 1000);
			print_pvt_data(&(event->event.pvt));
			LOG_INF("-----------------------------------");
			break;
		case GPS_EVT_NMEA_FIX:
			send_nmea(event->event.nmea.buf);
			break;
		default:
			break;
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

	if (is_click_event(eh)) {
		const struct click_event *event = cast_click_event(eh);
		handle_click_event(event);
		return false;
	}

	if (is_gps_module_event(eh)) {
		const struct gps_module_event *event = cast_gps_module_event(eh);
		handle_gps_module_event(event);
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
EVENT_SUBSCRIBE(MODULE, click_event);
EVENT_SUBSCRIBE(MODULE, cloudapi_event);
EVENT_SUBSCRIBE(MODULE, gps_module_event);