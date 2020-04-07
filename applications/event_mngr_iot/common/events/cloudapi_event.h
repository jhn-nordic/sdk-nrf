/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _CLOUDAPI_EVENT_H_
#define _CLOUDAPI_EVENT_H_

/**
 * @brief cloud Event
 * @defgroup cloud_event cloud Event
 * @{
 */

#include "event_manager.h"
#include <net/cloud.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cloudapi_event {
	struct event_header header;

	struct cloud_event event;
};

EVENT_TYPE_DECLARE(cloudapi_event);

struct cloudapi_msg_event {
	struct event_header header;
	struct cloud_msg msg;
};

EVENT_TYPE_DECLARE(cloudapi_msg_event);
#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _CLOUDAPI_EVENT_H_ */
