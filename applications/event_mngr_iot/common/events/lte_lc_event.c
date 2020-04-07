/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>

#include "lte_lc_event.h"

static const char * const event_name[] = {
	[LTE_LC_EVT_NW_REG_STATUS] = "LTE_LC_EVT_NW_REG_STATUS",
	[LTE_LC_EVT_PSM_UPDATE]    = "LTE_LC_EVT_PSM_UPDATE",
	[LTE_LC_EVT_EDRX_UPDATE]   = "LTE_LC_EVT_EDRX_UPDATE",
	[LTE_LC_EVT_RRC_UPDATE]    = "LTE_LC_EVT_RRC_UPDATE",
	[LTE_LC_EVT_CELL_UPDATE]   = "LTE_LC_EVT_CELL_UPDATE",
};

static int log_lte_lc_event(const struct event_header *eh, char *buf,
				  size_t buf_len)
{
	const struct lte_lc_event *event = cast_lte_lc_event(eh);

	int err = 0;

	switch(event->event.type) {
		case LTE_LC_EVT_NW_REG_STATUS:
			err = snprintf(
				buf,
				buf_len,
				": %s: %s",
				event_name[event->event.type],
				event->event.nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
				"Connected - home network" : "Connected - roaming");
			break;

		case LTE_LC_EVT_PSM_UPDATE:
			err =snprintf(
				buf,
				buf_len,
				":\n%s: \n\tTAU: %d\n\tActive time: %d",
				event_name[event->event.type],
				event->event.psm_cfg.active_time,
				event->event.psm_cfg.tau);
			break;
		case LTE_LC_EVT_EDRX_UPDATE:
			err =snprintf(
				buf,
				buf_len,
				":\n%s: \n\teDRX: %f\n\tPTW: %f",
				event_name[event->event.type],
				event->event.edrx_cfg.edrx,
				event->event.edrx_cfg.ptw);
			break;
		case LTE_LC_EVT_RRC_UPDATE:
			err =snprintf(
				buf,
				buf_len,
				": %s: %s",
				event_name[event->event.type],
				event->event.rrc_mode == LTE_LC_RRC_MODE_CONNECTED ?
				"Connected" : "Idle");
			break;
		case LTE_LC_EVT_CELL_UPDATE:
			err =snprintf(
				buf,
				buf_len,
				":\n%s: \n\tCell ID: %d\n\tTracking area: %d",
				event_name[event->event.type],
				event->event.cell.id, event->event.cell.tac);
		default:
			break;
	}

	return err;
}

EVENT_TYPE_DEFINE(lte_lc_event,
		  IS_ENABLED(CONFIG_DESKTOP_INIT_LOG_LTE_LC_EVENT),
		  log_lte_lc_event,
		  NULL);
