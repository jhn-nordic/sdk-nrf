/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <init.h>
#include <modem/at_cmd.h>
#include <modem/lte_lc_trace.h>

#include <memfault/core/build_info.h>
#include <memfault/core/compiler.h>
#include <memfault/core/platform/device_info.h>
#include <memfault/http/http_client.h>
#include <memfault/ports/zephyr/http.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(memfault_ncs, CONFIG_MEMFAULT_NCS_LOG_LEVEL);

#define IMEI_LEN 15

#if defined(CONFIG_SOC_NRF9160)
#define MEMFAULT_URL	"https://goto.memfault.com/create-key/nrf91"
#else
#define MEMFAULT_URL	"https://goto.memfault.com/create-key/nrf"
#endif

/* API key check */
BUILD_ASSERT(sizeof(CONFIG_MEMFAULT_NCS_PROJECT_KEY) > 1,
	"Memfault API Key not configured. Please visit " MEMFAULT_URL);

/* Device ID check */
BUILD_ASSERT(
	(sizeof(CONFIG_MEMFAULT_NCS_DEVICE_ID) > 1) || CONFIG_MEMFAULT_NCS_DEVICE_SERIAL_USE_IMEI,
	"Device ID must be set or configured to use IMEI");

/* Firmware type check */
BUILD_ASSERT(sizeof(CONFIG_MEMFAULT_NCS_FW_TYPE) > 1, "Firmware type must be configured");

/* Firmware version checks */
#if defined(CONFIG_MEMFAULT_NCS_FW_VERSION_STATIC)
BUILD_ASSERT(sizeof(CONFIG_MEMFAULT_NCS_FW_VERSION_STATIC) > 1,
	     "Firmware version must be configured");
#endif

static char fw_version[sizeof(CONFIG_MEMFAULT_NCS_FW_VERSION_PREFIX) + 8] =
	CONFIG_MEMFAULT_NCS_FW_VERSION_PREFIX;

#if defined(CONFIG_MEMFAULT_NCS_DEVICE_SERIAL_USE_IMEI)
static char device_serial[MAX(sizeof(CONFIG_MEMFAULT_NCS_DEVICE_ID), IMEI_LEN + 1)] =
	CONFIG_MEMFAULT_NCS_DEVICE_ID;
#else
static char device_serial[] = CONFIG_MEMFAULT_NCS_DEVICE_ID;
#endif

extern void memfault_ncs_metrcics_init(void);

sMfltHttpClientConfig g_mflt_http_client_config = {
	.api_key = CONFIG_MEMFAULT_NCS_PROJECT_KEY,
};

void memfault_platform_get_device_info(sMemfaultDeviceInfo *info)
{
	static bool is_init;

	if (!is_init) {
		const size_t version_len = strlen(fw_version);
		const size_t build_id_chars = 6 + 1 /* '\0' */;
		const size_t build_id_num_chars =
		    MIN(build_id_chars, sizeof(fw_version) - version_len - 1);

		memfault_build_id_get_string(&fw_version[version_len], build_id_num_chars);
		is_init = true;
	}

	*info = (sMemfaultDeviceInfo) {
		.device_serial = device_serial,
		.software_type = CONFIG_MEMFAULT_NCS_FW_TYPE,
		.software_version = fw_version,
		.hardware_version = CONFIG_MEMFAULT_NCS_HW_VERSION,
	};
}

static int request_imei(const char *cmd, char *buf, size_t buf_len)
{
	enum at_cmd_state at_state;
	int err = at_cmd_write(cmd, buf, buf_len, &at_state);

	if (err) {
		LOG_ERR("at_cmd_write failed, error: %d, at_state: %d", err, at_state);
	}

	return err;
}

static int device_info_init(void)
{
	int err;
	char imei_buf[IMEI_LEN + 2 + 1]; /* Add 2 for \r\n and 1 for \0 */

	err = request_imei("AT+CGSN", imei_buf, sizeof(imei_buf));
	if (err) {
		strncat(device_serial, "Unknown",
			sizeof(device_serial) - strlen(device_serial) - 1);
		LOG_ERR("Failed to retrieve IMEI");
	} else {
		imei_buf[IMEI_LEN] = '\0';
		strncat(device_serial, imei_buf,
			sizeof(device_serial) - strlen(device_serial) - 1);
	}

	LOG_DBG("Device serial generated: %s", log_strdup(device_serial));

	return err;
}

static int init(const struct device *unused)
{
	int err = 0;

	ARG_UNUSED(unused);

	if (IS_ENABLED(CONFIG_MEMFAULT_NCS_PROVISION_CERTIFICATES)) {
		err = memfault_zephyr_port_install_root_certs();
		if (err) {
			LOG_ERR("Failed to provision certificates, error: %d", err);
			LOG_WRN("Certificates can not be provisioned while LTE is active");
			/* We don't consider this a critical failure, as the application
			 * can attempt to provision at a later stage.
			 */
		}
	}

	if (IS_ENABLED(CONFIG_MEMFAULT_NCS_DEVICE_SERIAL_USE_IMEI)) {
		err = device_info_init();
		if (err) {
			LOG_ERR("Device info initialization failed, error: %d", err);
		}
	}

	memfault_ncs_metrcics_init();

	return err;
}

SYS_INIT(init, APPLICATION, CONFIG_MEMFAULT_NCS_INIT_PRIORITY);
