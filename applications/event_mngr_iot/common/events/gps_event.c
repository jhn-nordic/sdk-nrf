/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>

#include "gps_event.h"

static const char * const event_name[] = {
	[GPS_EVT_SEARCH_STARTED] = "GPS_EVT_SEARCH_STARTED",
	[GPS_EVT_SEARCH_STOPPED] = "GPS_EVT_SEARCH_STOPPED",
	[GPS_EVT_SEARCH_TIMEOUT] = "GPS_EVT_SEARCH_TIMEOUT",
	[GPS_EVT_PVT] = "GPS_EVT_PVT",
	[GPS_EVT_PVT_FIX] = "GPS_EVT_PVT_FIX",
	[GPS_EVT_NMEA] = "GPS_EVT_NMEA",
	[GPS_EVT_NMEA_FIX] = "GPS_EVT_NMEA_FIX",
	[GPS_EVT_OPERATION_BLOCKED] = "GPS_EVT_OPERATION_BLOCKED",
	[GPS_EVT_OPERATION_UNBLOCKED] = "GPS_EVT_OPERATION_UNBLOCKED",
	[GPS_EVT_AGPS_DATA_NEEDED] = "GPS_EVT_AGPS_DATA_NEEDED",
	[GPS_EVT_ERROR] = "GPS_EVT_ERROR"
};

static int log_gps_module_event(const struct event_header *eh, char *buf,
				  size_t buf_len)
{
	const struct gps_module_event *event = cast_gps_module_event(eh);

	int err = 0;

	switch(event->event.type) {
		case GPS_EVT_SEARCH_STARTED:
		case GPS_EVT_SEARCH_STOPPED:
		case GPS_EVT_SEARCH_TIMEOUT:
		case GPS_EVT_PVT:
		case GPS_EVT_PVT_FIX:
		case GPS_EVT_NMEA:
		case GPS_EVT_NMEA_FIX:
		case GPS_EVT_OPERATION_BLOCKED:
		case GPS_EVT_OPERATION_UNBLOCKED:
		case GPS_EVT_AGPS_DATA_NEEDED:
		case GPS_EVT_ERROR:

			err = snprintf(
				buf,
				buf_len,
				": %s ",
				event_name[event->event.type]
				);
			break;
		default:
			break;
	}

	return err;
}

EVENT_TYPE_DEFINE(gps_module_event,
		  IS_ENABLED(CONFIG_DESKTOP_INIT_LOG_GPS_EVENT),
		  log_gps_module_event,
		  NULL);


static int log_gps_start_event(const struct event_header *eh, char *buf,
				  size_t buf_len)
{
	const struct gps_start_event *event = cast_gps_start_event(eh);

	int err = snprintf(
			buf,
			buf_len,
			": nav_mode = %d \n\tpower_mode = %d\n\tinterval = %d\n\ttimeout = %d\n\t %s",
			event->gps_cfg.nav_mode,
			event->gps_cfg.power_mode,
			event->gps_cfg.interval,
			event->gps_cfg.timeout,
			event->gps_cfg.delete_agps_data ?
			"Delete AGPS data" : "Keep AGPS data");
	return err;
}
EVENT_TYPE_DEFINE(gps_start_event,
		  IS_ENABLED(CONFIG_DESKTOP_INIT_LOG_GPS_EVENT),
		  log_gps_start_event,
		  NULL);
