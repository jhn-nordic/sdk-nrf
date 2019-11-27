/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>
#include <shell/shell.h>
#include <settings/settings.h>

#define MODULE lte_module
#include "module_state_event.h"
#include "power_event.h"
#include "button_event.h"
#include "led_event.h"
#include "selector_event.h"
#include "config_event.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_LTE_MODULE_LOG_LEVEL);

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <kernel_structs.h>
#include <stdio.h>
#include <string.h>
#include <gps.h>
#include <sensor.h>
#include <console.h>
#include <misc/reboot.h>
#include <logging/log_ctrl.h>
#if defined(CONFIG_BSD_LIBRARY)
#include <net/bsdlib.h>
#include <bsd.h>
#include <lte_lc.h>
#include <modem_info.h>
#endif /* CONFIG_BSD_LIBRARY */
#include <net/cloud.h>
#include <net/socket.h>
#include <nrf_cloud.h>

#if defined(CONFIG_BOOTLOADER_MCUBOOT)
#include <dfu/mcuboot.h>
#endif

#include "cloud_codec.h"
#include "service_info.h"
#define THREAD_STACK_SIZE	8192
#define THREAD_PRIORITY		CONFIG_MAIN_THREAD_PRIORITY

#define CLOUD_CONNACK_WAIT_DURATION	K_SECONDS(CONFIG_CLOUD_WAIT_DURATION)

#if defined(CONFIG_BSD_LIBRARY) && \
!defined(CONFIG_LTE_LINK_CONTROL)
#error "Missing CONFIG_LTE_LINK_CONTROL"
#endif

#if defined(CONFIG_BSD_LIBRARY) && \
defined(CONFIG_LTE_AUTO_INIT_AND_CONNECT) && \
defined(CONFIG_NRF_CLOUD_PROVISION_CERTIFICATES)
#error "PROVISION_CERTIFICATES \
	requires CONFIG_LTE_AUTO_INIT_AND_CONNECT to be disabled!"
#endif

#define CLOUD_LED_ON_STR "{\"led\":\"on\"}"
#define CLOUD_LED_OFF_STR "{\"led\":\"off\"}"
#define CLOUD_LED_MSK UI_LED_1

struct rsrp_data {
	u16_t value;
	u16_t offset;
};
static void sensor_data_send(struct cloud_channel_data *data);
static struct cloud_backend *cloud_backend;

 /* Variables to keep track of nRF cloud user association. */
static bool recently_associated;
static bool association_with_pin;

/* Sensor data */
static struct cloud_channel_data button_cloud_data;
static struct cloud_channel_data device_cloud_data = {
	.type = CLOUD_CHANNEL_DEVICE_INFO,
	.tag = 0x1
};

static atomic_val_t send_data_enable;

/* Structures for work */
static struct k_work connect_work;
static struct k_delayed_work cloud_reboot_work;
static struct k_work device_status_work;

enum error_type {
	ERROR_CLOUD,
	ERROR_BSD_RECOVERABLE,
	ERROR_LTE_LC,
	ERROR_SYSTEM_FAULT
};

/* Forward declaration of functions */
static void app_connect(struct k_work *work);
static void work_init(void);
static void device_status_send(struct k_work *work);

/**@brief nRF Cloud error handler. */
void error_handler(enum error_type err_type, int err_code)
{
	if (err_type == ERROR_CLOUD) {
#if defined(CONFIG_BSD_LIBRARY)
		printk("Shutdown modem\n");
		bsdlib_shutdown();
#endif
	}

#if !defined(CONFIG_DEBUG) && defined(CONFIG_REBOOT)
	LOG_PANIC();
	sys_reboot(0);
#else
	switch (err_type) {
	case ERROR_CLOUD:
		/* Blinking all LEDs ON/OFF in pairs (1 and 4, 2 and 3)
		 * if there is an application error.
		 */
		printk("Error of type ERROR_CLOUD: %d\n", err_code);
	break;
	case ERROR_BSD_RECOVERABLE:
		/* Blinking all LEDs ON/OFF in pairs (1 and 3, 2 and 4)
		 * if there is a recoverable error.
		 */
		printk("Error of type ERROR_BSD_RECOVERABLE: %d\n", err_code);
	break;
	default:
		/* Blinking all LEDs ON/OFF in pairs (1 and 2, 3 and 4)
		 * undefined error.
		 */
		printk("Unknown error type: %d, code: %d\n",
			err_type, err_code);
	break;
	}

	while (true) {
		k_cpu_idle();
	}
#endif /* CONFIG_DEBUG */
}

void k_sys_fatal_error_handler(unsigned int reason,
			       const z_arch_esf_t *esf)
{
	ARG_UNUSED(esf);

	LOG_PANIC();
	printk("Running main.c error handler");
	error_handler(ERROR_SYSTEM_FAULT, reason);
	CODE_UNREACHABLE;
}

void cloud_error_handler(int err)
{
	error_handler(ERROR_CLOUD, err);
}

/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t err)
{
	error_handler(ERROR_BSD_RECOVERABLE, (int)err);
}


/**@brief Send button presses to cloud */
static void button_send(bool pressed)
{
	static char data[] = "1";

	if (!atomic_get(&send_data_enable)) {
		return;
	}

	if (pressed) {
		data[0] = '1';
	} else {
		data[0] = '0';
	}

	button_cloud_data.data.buf = data;
	button_cloud_data.data.len = strlen(data);
	button_cloud_data.tag += 1;

	if (button_cloud_data.tag == 0) {
		button_cloud_data.tag = 0x1;
	}

	sensor_data_send(&button_cloud_data);
}


static void cloud_cmd_handler(struct cloud_command *cmd)
{
	/* Command handling goes here. */
	if (cmd->recipient == CLOUD_RCPT_MODEM_INFO) {
	} else if (cmd->recipient == CLOUD_RCPT_UI) {

		if (cmd->type == CLOUD_CMD_LED_RED) {
			const struct led_effect ledon=LED_EFFECT_LED_ON(LED_COLOR((u8_t)cmd->value));
			struct led_event *event1 = new_led_event();
			event1->led_id = 1;
			event1->led_effect = &ledon;
			EVENT_SUBMIT(event1);
		} else if (cmd->type == CLOUD_CMD_LED_GREEN) {
			const struct led_effect ledon=LED_EFFECT_LED_ON(LED_COLOR((u8_t)cmd->value));
			struct led_event *event1 = new_led_event();
			event1->led_id = 2;
			event1->led_effect = &ledon;
			EVENT_SUBMIT(event1);
		} else if (cmd->type == CLOUD_CMD_LED_BLUE) {
			const struct led_effect ledon=LED_EFFECT_LED_ON(LED_COLOR((u8_t)cmd->value));
			struct led_event *event1 = new_led_event();
			event1->led_id = 3;
			event1->led_effect = &ledon;
			EVENT_SUBMIT(event1);
		}
	}
}

/**@brief Poll device info and send data to the cloud. */
static void device_status_send(struct k_work *work)
{
	if (!atomic_get(&send_data_enable)) {
		return;
	}

	cJSON *root_obj = cJSON_CreateObject();

	if (root_obj == NULL) {
		printk("Unable to allocate JSON object\n");
		return;
	}

	size_t item_cnt = 0;

	const char *const ui[] = {
		CLOUD_CHANNEL_STR_BUTTON,
	};

	const char *const fota[] = {
#if defined(CONFIG_CLOUD_FOTA_APP)
		SERVICE_INFO_FOTA_STR_APP,
#endif
#if defined(CONFIG_CLOUD_FOTA_MODEM)
		SERVICE_INFO_FOTA_STR_MODEM
#endif
	};

	if (service_info_json_object_encode(ui, ARRAY_SIZE(ui),
					    fota, ARRAY_SIZE(fota),
					    SERVICE_INFO_FOTA_VER_CURRENT,
					    root_obj) == 0) {
		++item_cnt;
	}

	if (item_cnt == 0) {
		cJSON_Delete(root_obj);
		return;
	}

	device_cloud_data.data.buf = (char *)root_obj;
	device_cloud_data.data.len = item_cnt;
	device_cloud_data.tag += 1;

	if (device_cloud_data.tag == 0) {
		device_cloud_data.tag = 0x1;
	}

	/* Transmits the data to the cloud. Frees the JSON object. */
	sensor_data_send(&device_cloud_data);
}

/**@brief Send sensor data to nRF Cloud. **/
static void sensor_data_send(struct cloud_channel_data *data)
{
	int err = 0;
	struct cloud_msg msg = {
			.qos = CLOUD_QOS_AT_MOST_ONCE,
			.endpoint.type = CLOUD_EP_TOPIC_MSG
		};

	if (data->type == CLOUD_CHANNEL_DEVICE_INFO) {
		msg.endpoint.type = CLOUD_EP_TOPIC_STATE;
	}

	if (!atomic_get(&send_data_enable)) {
		return;
	}

	if (data->type != CLOUD_CHANNEL_DEVICE_INFO) {
		err = cloud_encode_data(data, &msg);
	} else {
		err = cloud_encode_digital_twin_data(data, &msg);
	}

	if (err) {
		printk("Unable to encode cloud data: %d\n", err);
	}

	err = cloud_send(cloud_backend, &msg);

	cloud_release_data(&msg);

	if (err) {
		printk("sensor_data_send failed: %d\n", err);
		cloud_error_handler(err);
	}
}

/**@brief Reboot the device if CONNACK has not arrived. */
static void cloud_reboot_handler(struct k_work *work)
{
	error_handler(ERROR_CLOUD, -ETIMEDOUT);
}

/**@brief nRF Cloud specific callback for cloud association event. */
static void on_user_pairing_req(const struct cloud_event *evt)
{
	if (evt->data.pair_info.type == CLOUD_PAIR_PIN) {
		association_with_pin = true;
		//ui_led_set_pattern(UI_CLOUD_PAIRING);
		printk("Waiting for cloud association with PIN\n");
	}
}

/** @brief Handle procedures after successful association with nRF Cloud. */
void on_pairing_done(void)
{
	if (association_with_pin) {
		recently_associated = true;

		printk("Successful user association.\n");
		printk("The device will attempt to reconnect to ");
		printk("nRF Cloud. It may reset in the process.\n");
		printk("Manual reset may be required if connection ");
		printk("to nRF Cloud is not established within ");
		printk("20 - 30 seconds.\n");
	}

	if (!association_with_pin) {
		return;
	}

	int err;

	printk("Disconnecting from nRF cloud...\n");

	err = cloud_disconnect(cloud_backend);
	if (err == 0) {
		printk("Reconnecting to cloud...\n");
		err = cloud_connect(cloud_backend);
		if (err == 0) {
			return;
		}
		printk("Could not reconnect\n");
	} else {
		printk("Disconnection failed\n");
	}

	printk("Fallback to controlled reboot\n");
	printk("Shutting down LTE link...\n");

#if defined(CONFIG_BSD_LIBRARY)
	err = lte_lc_power_off();
	if (err) {
		printk("Could not shut down link\n");
	} else {
		printk("LTE link disconnected\n");
	}
#endif

#ifdef CONFIG_REBOOT
	printk("Rebooting...\n");
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_COLD);
#endif
	printk("**** Manual reboot required ***\n");
}

static void button_sensor_init(void)
{
	button_cloud_data.type = CLOUD_CHANNEL_BUTTON;
	button_cloud_data.tag = 0x1;
}
void cloud_event_handler(const struct cloud_backend *const backend,
			 const struct cloud_event *const evt,
			 void *user_data)
{
	ARG_UNUSED(user_data);

	switch (evt->type) {
	case CLOUD_EVT_CONNECTED:
		printk("CLOUD_EVT_CONNECTED\n");
		k_delayed_work_cancel(&cloud_reboot_work);

		break;
	case CLOUD_EVT_READY:
		printk("CLOUD_EVT_READY\n");
		button_sensor_init();
		atomic_set(&send_data_enable, 1);

#if defined(CONFIG_BOOTLOADER_MCUBOOT)
		/* Mark image as good to avoid rolling back after update */
		boot_write_img_confirmed();
#endif
		break;
	case CLOUD_EVT_DISCONNECTED:
		printk("CLOUD_EVT_DISCONNECTED\n");
		break;
	case CLOUD_EVT_ERROR:
		printk("CLOUD_EVT_ERROR\n");
		break;
	case CLOUD_EVT_DATA_SENT:
		printk("CLOUD_EVT_DATA_SENT\n");
		break;
	case CLOUD_EVT_DATA_RECEIVED:
		printk("CLOUD_EVT_DATA_RECEIVED\n");
		cloud_decode_command(evt->data.msg.buf);
		break;
	case CLOUD_EVT_PAIR_REQUEST:
		printk("CLOUD_EVT_PAIR_REQUEST\n");
		on_user_pairing_req(evt);
		break;
	case CLOUD_EVT_PAIR_DONE:
		printk("CLOUD_EVT_PAIR_DONE\n");
		on_pairing_done();
		break;
	case CLOUD_EVT_FOTA_DONE:
		printk("CLOUD_EVT_FOTA_DONE\n");
		sys_reboot(SYS_REBOOT_COLD);
		break;
	default:
		printk("Unknown cloud event type: %d\n", evt->type);
		break;
	}
}

/**@brief Connect to nRF Cloud, */
static void app_connect(struct k_work *work)
{
	int err;
	err = cloud_connect(cloud_backend);

	if (err) {
		printk("cloud_connect failed: %d\n", err);
		cloud_error_handler(err);
	}
}


/**@brief Initializes and submits delayed work. */
static void work_init(void)
{
	k_work_init(&connect_work, app_connect);

	k_delayed_work_init(&cloud_reboot_work, cloud_reboot_handler);
	k_work_init(&device_status_work, device_status_send);
#if CONFIG_MODEM_INFO
	k_work_init(&rsrp_work, modem_rsrp_data_send);
#endif /* CONFIG_MODEM_INFO */
}

/**@brief Configures modem to provide LTE link. Blocks until link is
 * successfully established.
 */
static void modem_configure(void)
{
#if defined(CONFIG_BSD_LIBRARY)
	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already turned on
		 * and connected.
		 */
	} else {
		int err;

		printk("Connecting to LTE network. ");
		printk("This may take several minutes.\n");
		//ui_led_set_pattern(UI_LTE_CONNECTING);

		err = lte_lc_init_and_connect();
		if (err) {
			printk("LTE link could not be established.\n");
			error_handler(ERROR_LTE_LC, err);
		}

		printk("Connected to LTE network\n");
		//ui_led_set_pattern(UI_LTE_CONNECTED);
	}
#endif
}



void handle_bsdlib_init_ret(void)
{
	#if defined(CONFIG_BSD_LIBRARY)
	int ret = bsdlib_get_init_ret();

	/* Handle return values relating to modem firmware update */
	switch (ret) {
	case MODEM_DFU_RESULT_OK:
		printk("MODEM UPDATE OK. Will run new firmware\n");
		sys_reboot(SYS_REBOOT_COLD);
		break;
	case MODEM_DFU_RESULT_UUID_ERROR:
	case MODEM_DFU_RESULT_AUTH_ERROR:
		printk("MODEM UPDATE ERROR %d. Will run old firmware\n", ret);
		sys_reboot(SYS_REBOOT_COLD);
		break;
	case MODEM_DFU_RESULT_HARDWARE_ERROR:
	case MODEM_DFU_RESULT_INTERNAL_ERROR:
		printk("MODEM UPDATE FATAL ERROR %d. Modem failiure\n", ret);
		sys_reboot(SYS_REBOOT_COLD);
		break;
	default:
		break;
	}
	#endif /* CONFIG_BSD_LIBRARY */
}

static K_THREAD_STACK_DEFINE(thread_stack, THREAD_STACK_SIZE);
static struct k_thread thread;

void main_fn(void)
{
	int ret;

	printk("Asset tracker started\n");

	handle_bsdlib_init_ret();

	cloud_backend = cloud_get_binding("NRF_CLOUD");
	__ASSERT(cloud_backend != NULL, "nRF Cloud backend not found");

	ret = cloud_init(cloud_backend, cloud_event_handler);
	if (ret) {
		printk("Cloud backend could not be initialized, error: %d\n",
			ret);
		cloud_error_handler(ret);
	}

	ret = cloud_decode_init(cloud_cmd_handler);
	if (ret) {
		printk("Cloud command decoder could not be initialized, error: %d\n", ret);
		cloud_error_handler(ret);
	}

	work_init();
	modem_configure();
connect:
	ret = cloud_connect(cloud_backend);
	if (ret) {
		printk("cloud_connect failed: %d\n", ret);
		cloud_error_handler(ret);
	} else {
		k_delayed_work_submit(&cloud_reboot_work,
				      CLOUD_CONNACK_WAIT_DURATION);
	}

	struct pollfd fds[] = {
		{
			.fd = cloud_backend->config->socket,
			.events = POLLIN
		}
	};

	while (true) {
		/* The timeout is set to (keepalive / 3), so that the worst case
		 * time between two messages from device to broker is
		 * ((4 / 3) * keepalive + connection overhead), which is within
		 * MQTT specification of (1.5 * keepalive) before the broker
		 * must close the connection.
		 */
		ret = poll(fds, ARRAY_SIZE(fds),
			K_SECONDS(CONFIG_MQTT_KEEPALIVE / 3));

		if (ret < 0) {
			printk("poll() returned an error: %d\n", ret);
			error_handler(ERROR_CLOUD, ret);
			continue;
		}

		if (ret == 0) {
			cloud_ping(cloud_backend);
			continue;
		}

		if ((fds[0].revents & POLLIN) == POLLIN) {
			cloud_input(cloud_backend);
		}

		if ((fds[0].revents & POLLNVAL) == POLLNVAL) {
			printk("Socket error: POLLNVAL\n");
			printk("The cloud socket was unexpectedly closed.\n");
			error_handler(ERROR_CLOUD, -EIO);
			return;
		}

		if ((fds[0].revents & POLLHUP) == POLLHUP) {
			printk("Socket error: POLLHUP\n");
			printk("Connection was closed by the cloud.\n");
			error_handler(ERROR_CLOUD, -EIO);
			return;
		}

		if ((fds[0].revents & POLLERR) == POLLERR) {
			printk("Socket error: POLLERR\n");
			printk("Cloud connection was unexpectedly closed.\n");
			error_handler(ERROR_CLOUD, -EIO);
			return;
		}
	}

	cloud_disconnect(cloud_backend);
	goto connect;
}

static bool event_handler(const struct event_header *eh)
{
	if (is_button_event(eh)) {
		
		const struct button_event *event =
			cast_button_event(eh);
			button_send(event->pressed);

		return false;
	}
	if (is_module_state_event(eh)) {
		const struct module_state_event *event =
			cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			/* Start state machine thread */
			//__ASSERT_NO_MSG(state.state == STATE_DISABLED);
			k_thread_create(&thread, thread_stack,
					THREAD_STACK_SIZE,
					(k_thread_entry_t)main_fn,
					NULL, NULL, NULL,
					THREAD_PRIORITY, 0, K_NO_WAIT);
			k_thread_name_set(&thread, MODULE_NAME "_thread");

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
EVENT_SUBSCRIBE(MODULE, button_event);
EVENT_SUBSCRIBE_EARLY(MODULE, power_down_event);