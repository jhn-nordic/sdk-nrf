/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <uart.h>
#include <string.h>
#include <gpio.h>
/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t err)
{
	printk("bsdlib recoverable error: %u\n", err);
}

/**@brief Irrecoverable BSD library error. */
void bsd_irrecoverable_error_handler(uint32_t err)
{
	printk("bsdlib irrecoverable error: %u\n", err);

	__ASSERT_NO_MSG(false);
}

void main(void)
{
	/* 	struct device *port;
	port=device_get_binding(DT_GPIO_P0_DEV_NAME);
	
	gpio_pin_configure(port, 8, GPIO_DIR_OUT);


	 gpio_pin_write(port, 8, 0);*/
	printk("The AT host sample started\n");
}
