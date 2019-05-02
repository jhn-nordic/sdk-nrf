/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef __CLOUD_BACKEND_H_
#define __CLOUD_BACKEND_H_

#include <zephyr.h>
#include <net/cloud.h>

void cloud_notify_event(struct cloud_backend *backend, struct cloud_event *evt)
{
	if (backend->config->handler) {
		backend->config->handler(backend, evt);
	}
}

#endif /* __CLOUD_BACKEND_H_ */
