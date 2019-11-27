/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <event_manager.h>

#define MODULE main
#include "module_state_event.h"
#include <hal/nrf_gpio.h>
#include <hal/nrf_power.h>
#include <hal/nrf_regulators.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE);
#include <drivers/gpio.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_power.h>
#include <hal/nrf_regulators.h>
#define LED_PORT	DT_ALIAS_LED0_GPIOS_CONTROLLER
#define LED		DT_ALIAS_LED0_GPIOS_PIN

/* 1000 msec = 1 sec */
#define SLEEP_TIME	1000

void main(void)
{
	if (event_manager_init()) {
		LOG_ERR("Event manager not initialized");
	} else {
		module_set_state(MODULE_STATE_READY);
	}
	
// 		void main(void)
// {
// 	u32_t cnt = 0;
// 	struct device *dev;

// 	dev = device_get_binding(LED_PORT);
// 	/* Set LED pin as output */
// 	gpio_pin_configure(dev, LED, GPIO_DIR_OUT);

// 	while (cnt < 5) {
// 		/* Set pin to HIGH/LOW every 1 second */
// 		gpio_pin_write(dev, LED, cnt % 2);
// 		cnt++;
// 		k_sleep(SLEEP_TIME);
// 	}

// 		nrf_gpio_cfg_input(26,
// 			NRF_GPIO_PIN_PULLUP);
// 		nrf_gpio_cfg_sense_set(26,
// 			NRF_GPIO_PIN_SENSE_LOW);

// 		/*
// 		 * The LTE modem also needs to be stopped by issuing a command
// 		 * through the modem API, before entering System OFF mode.
// 		 * Once the command is issued, one should wait for the modem
// 		 * to respond that it actually has stopped as there may be a
// 		 * delay until modem is disconnected from the network.
// 		 * Refer to https://infocenter.nordicsemi.com/topic/ps_nrf9160/
// 		 * pmu.html?cp=2_0_0_4_0_0_1#system_off_mode
// 		 */
// 		//lte_lc_power_off();	/* Gracefully shutdown the modem.*/
// 		//bsd_shutdown();		/* Gracefully shutdown the BSD library*/
// 		nrf_regulators_system_off(NRF_REGULATORS_NS);
	
}


