/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#define MODULE lte_lc_module
#include "module_state_event.h"
#include "power_event.h"

#include "lte_lc_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_LTE_LC_MODULE_LOG_LEVEL);

#include <zephyr.h>
#include <modem/lte_lc.h>
#include <net/cloud.h>
#include <net/socket.h>

static void lte_handler(struct lte_lc_evt *evt)
{
	struct lte_lc_event *event = new_lte_lc_event();
	memcpy(&event->event,evt,sizeof(struct lte_lc_event));
	EVENT_SUBMIT(event);
}

static void modem_configure(void)
{
#if defined(CONFIG_BSD_LIBRARY)
	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
		int err;

#if defined(CONFIG_POWER_SAVING_MODE_ENABLE)
		/* Requesting PSM before connecting allows the modem to inform
		 * the network about our wish for certain PSM configuration
		 * already in the connection procedure instead of in a separate
		 * request after the connection is in place, which may be
		 * rejected in some networks.
		 */
		err = lte_lc_psm_req(true);
		if (err) {
			LOG_ERR("Failed to set PSM parameters, error: %d",
			       err);
		}

		LOG_INF("PSM mode requested");
#endif
		err = lte_lc_init_and_connect_async(lte_handler);
		if (err) {
			LOG_ERR("Modem could not be configured, error: %d",
			       err);
			return;
		}

		/* Check LTE events of type LTE_LC_EVT_NW_REG_STATUS in
		 * lte_handler() to determine when the LTE link is up.
		 */

		LOG_INF("Connecting to LTE network. ");
		LOG_INF("This may take several minutes.");
	}
#endif
}
static bool event_handler(const struct event_header *eh)
{
	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			modem_configure();
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
EVENT_SUBSCRIBE_EARLY(MODULE, power_down_event); 