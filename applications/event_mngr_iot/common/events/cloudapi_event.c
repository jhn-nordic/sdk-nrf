/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>

#include "cloudapi_event.h"

static const char * const event_name[] = {
	[CLOUD_EVT_CONNECTED] = "CLOUD_EVT_CONNECTED",
	[CLOUD_EVT_DISCONNECTED]    = "CLOUD_EVT_DISCONNECTED",
	[CLOUD_EVT_READY]   = "CLOUD_EVT_READY",
	[CLOUD_EVT_ERROR]    = "CLOUD_EVT_ERROR",
	[CLOUD_EVT_DATA_SENT]   = "CLOUD_EVT_DATA_SENT",
	[CLOUD_EVT_DATA_RECEIVED] = "CLOUD_EVT_DATA_RECEIVED",
	[CLOUD_EVT_PAIR_REQUEST]    = "CLOUD_EVT_PAIR_REQUEST",
	[CLOUD_EVT_PAIR_DONE]   = "CLOUD_EVT_PAIR_DONE",
	[CLOUD_EVT_FOTA_START]    = "CLOUD_EVT_FOTA_START",
	[CLOUD_EVT_FOTA_DONE]   = "CLOUD_EVT_FOTA_DONE",
	[CLOUD_EVT_FOTA_ERASE_PENDING]   = "CLOUD_EVT_FOTA_ERASE_PENDING",
	[CLOUD_EVT_FOTA_ERASE_DONE]    = "CLOUD_EVT_FOTA_ERASE_DONE",
};

static int log_cloudapi_event(const struct event_header *eh, char *buf,
				  size_t buf_len)
{
	const struct cloudapi_event *event = cast_cloudapi_event(eh);

	int err = 0;
	err = snprintf(
			buf,
			buf_len,
			": %s",
			event_name[event->event.type]);
	return err;
}

EVENT_TYPE_DEFINE(cloudapi_event,
		  IS_ENABLED(CONFIG_DESKTOP_INIT_LOG_CLOUDAPI_EVENT),
		  log_cloudapi_event,
		  NULL);

EVENT_TYPE_DEFINE(cloudapi_msg_event,
		  IS_ENABLED(CONFIG_DESKTOP_INIT_LOG_CLOUDAPI_EVENT),
		  NULL,
		  NULL);