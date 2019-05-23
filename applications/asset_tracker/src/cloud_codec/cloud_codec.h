/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
/**@file
 *
 * @brief   Cloud codec module.
 *
 * Module that encodes and decodes data destined for a cloud solution.
 */

#ifndef CLOUD_CODEC_H__
#define CLOUD_CODEC_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Cloud sensor types. */
enum cloud_sensor {
	/** The GPS sensor on the device. */
	CLOUD_SENSOR_GPS,
	/** The FLIP movement sensor on the device. */
	CLOUD_SENSOR_FLIP,
	/** The Button press sensor on the device. */
	CLOUD_SENSOR_BUTTON,
	/** The TEMP sensor on the device. */
	CLOUD_SENSOR_TEMP,
	/** The Humidity sensor on the device. */
	CLOUD_SENSOR_HUMID,
	/** The Air Pressure sensor on the device. */
	CLOUD_SENSOR_AIR_PRESS,
	/** The Air Quality sensor on the device. */
	CLOUD_SENSOR_AIR_QUAL,
	/** The RSPR data obtained from the modem. */
	CLOUD_LTE_LINK_RSRP,
	/** The descriptive DEVICE data indicating its status. */
	CLOUD_DEVICE_INFO,
};

struct cloud_data {
	char *buf;
	size_t len;
};

/**@brief Sensor data parameters. */
struct cloud_sensor_data {
	/** The sensor that is the source of the data. */
	enum cloud_sensor type;
	/** Sensor data to be transmitted. */
	struct cloud_data data;
	/** Unique tag to identify the sent data.
	 *  Useful for matching the acknowledgment.
	 */
	u32_t tag;
};

int cloud_encode_sensor_data(const struct cloud_sensor_data *sensor,
				 struct cloud_data *output);

#ifdef __cplusplus
}
#endif

#endif /* CLOUD_CODEC_H__ */
