/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <event_manager.h>

#define MODULE main
#include "module_state_event.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE);

#include <hal/nrf_uarte.h>

void main(void)
{
 	//Workaround for MCUBOOT not cleaning up serial port-
	if(!IS_ENABLED(CONFIG_SERIAL) && IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT)) {
		nrf_uarte_disable(NRF_UARTE0);
	}
	
	if (event_manager_init()) {
		LOG_ERR("Event manager not initialized");
	} else {
		module_set_state(MODULE_STATE_READY);
	}
}


