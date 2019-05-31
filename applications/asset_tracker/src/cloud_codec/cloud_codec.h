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
enum cloud_channel {
	/** The GPS sensor on the device. */
	CLOUD_CHANNEL_GPS,
	/** The FLIP movement sensor on the device. */
	CLOUD_CHANNEL_FLIP,
	/** The IMPACT movement sensor on the device. */
	CLOUD_CHANNEL_IMPACT,
	/** The Button press sensor on the device. */
	CLOUD_CHANNEL_BUTTON,
	/** The PIN unit on the device. */
	CLOUD_CHANNEL_PIN,
	/** The RGB LED on the device. */
	CLOUD_CHANNEL_RGB_LED,
	/** The BUZZER on the device. */
	CLOUD_CHANNEL_BUZZER,
	/** The TEMP sensor on the device. */
	CLOUD_CHANNEL_TEMP,
	/** The Humidity sensor on the device. */
	CLOUD_CHANNEL_HUMID,
	/** The Air Pressure sensor on the device. */
	CLOUD_CHANNEL_AIR_PRESS,
	/** The Air Quality sensor on the device. */
	CLOUD_CHANNEL_AIR_QUAL,
	/** The RSPR data obtained from the modem. */
	CLOUD_CHANNEL_LTE_LINK_RSRP,
	/** The descriptive DEVICE data indicating its status. */
	CLOUD_CHANNEL_DEVICE_INFO,
};

struct cloud_data {
	char *buf;
	size_t len;
};

/**@brief Sensor data parameters. */
struct cloud_channel_data {
	/** The sensor that is the source of the data. */
	enum cloud_channel type;
	/** Sensor data to be transmitted. */
	struct cloud_data data;
	/** Unique tag to identify the sent data.
	 *  Useful for matching the acknowledgment.
	 */
	u32_t tag;
};

enum cloud_cmd_group {
	CLOUD_CMD_GROUP_SET,
	CLOUD_CMD_GROUP_GET,
};

enum cloud_cmd_recipient {
	CLOUD_RCPT_ENVIRONMENT,
	CLOUD_RCPT_MOTION,
	CLOUD_RCPT_UI,
	CLOUD_RCPT_MODEM_INFO,
};

enum cloud_cmd_type {
	CLOUD_CMD_ENABLE,
	CLOUD_CMD_DISABLE,
	CLOUD_CMD_THRESHOLD_HIGH,
	CLOUD_CMD_THRESHOLD_LOW,
	CLOUD_CMD_READ,
	CLOUD_CMD_READ_NEW,
	CLOUD_CMD_PWM,
	CLOUD_CMD_LED_RED,
	CLOUD_CMD_LED_GREEN,
	CLOUD_CMD_LED_BLUE,
	CLOUD_CMD_LED_PULSE_LENGTH,
	CLOUD_CMD_LED_PAUSE_LENGTH,
	CLOUD_CMD_PLAY_MELODY,
	CLOUD_CMD_PLAY_NOTE,
};

struct cloud_command {
	enum cloud_cmd_group group;
	enum cloud_cmd_recipient recipient;
	enum cloud_channel channel;
	enum cloud_cmd_type type;
	double value;
	bool state;
};

typedef void (*cloud_cmd_cb_t)(struct cloud_command *cmd);

int cloud_encode_data(const struct cloud_channel_data *channel,
			struct cloud_data *output);

int cloud_decode_command(char *input);

int cloud_decode_init(cloud_cmd_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* CLOUD_CODEC_H__ */
