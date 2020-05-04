/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _GPS_EVENT_H_
#define _GPS_EVENT_H_

/**
 * @brief gps Event
 * @defgroup gps_event gps Event
 * @{
 */

#include "event_manager.h"
#include <drivers/gps.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gps_module_event {
	struct event_header header;
	struct gps_event event;
};

EVENT_TYPE_DECLARE(gps_module_event);

struct gps_start_event {
	struct event_header header;
	struct gps_config gps_cfg;
};

EVENT_TYPE_DECLARE(gps_start_event);

struct gps_stop_event {
	struct event_header header;

};

EVENT_TYPE_DECLARE(gps_stop_event)

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _GPS_EVENT_H_ */
