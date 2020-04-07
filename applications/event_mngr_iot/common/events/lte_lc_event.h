/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _LTE_LC_EVENT_H_
#define _LTE_LC_EVENT_H_

/**
 * @brief lte_lc Event
 * @defgroup lte_lc_event lte_lc Event
 * @{
 */

#include "event_manager.h"
#include <modem/lte_lc.h>

#ifdef __cplusplus
extern "C" {
#endif

struct lte_lc_event {
	struct event_header header;

	struct lte_lc_evt event;
};

EVENT_TYPE_DECLARE(lte_lc_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _LTE_LC_EVENT_H_ */
