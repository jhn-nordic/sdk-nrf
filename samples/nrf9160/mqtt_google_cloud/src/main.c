/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <uart.h>
#include <string.h>
#include <zephyr/jwt.h>
#include <entropy.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <lte_lc.h>
#include <time.h>
#include <dk_buttons_and_leds.h>
#include "nrf_inbuilt_key.h"
#include <console.h>
#include "ntp.h"
#include <sensor.h>
#include "bsec_interface.h"

#define TEMP_FROM_EXT_HEAT_SOURCE	0.0f

static float iaq, temperature, pressure, humidity, gas_resistance, raw_temperature, raw_humidity;
static u8_t iaq_accuracy;

static bsec_library_return_t update_subscription(bsec_virtual_sensor_t *sensor_list, u8_t n_sensors, float sample_rate)
{
	bsec_sensor_configuration_t virtual_sensors[BSEC_NUMBER_OUTPUTS],
	                sensor_settings[BSEC_MAX_PHYSICAL_SENSOR];
	u8_t n_virtual_sensors = 0,
	                n_sensor_settings = BSEC_MAX_PHYSICAL_SENSOR;

	for (u8_t i = 0; i < n_sensors; i++) {
		virtual_sensors[n_virtual_sensors].sensor_id = sensor_list[i];
		virtual_sensors[n_virtual_sensors].sample_rate = sample_rate;
		n_virtual_sensors++;
	}

	return bsec_update_subscription(virtual_sensors, n_virtual_sensors,
	                sensor_settings, &n_sensor_settings);
}

static bsec_library_return_t run(struct device *dev, s32_t *snooze_dur)
{
	bsec_library_return_t bsec_ret = BSEC_OK;
	bsec_bme_settings_t bme680_settings;
	struct sensor_value temp, pres, hum, gas_res;

	s64_t uptime = k_uptime_get();

	s64_t call_time_ns = uptime * 1000000LL;

	bsec_ret = bsec_sensor_control(call_time_ns, &bme680_settings);
	if (bsec_ret < BSEC_OK) /* If there is an error. Ignore warnings */
		return bsec_ret;

	if (bme680_settings.process_data) {
		sensor_sample_fetch(dev);

		sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		sensor_channel_get(dev, SENSOR_CHAN_PRESS, &pres);
		sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &hum);
		sensor_channel_get(dev, SENSOR_CHAN_GAS_RES, &gas_res);
	}

	bsec_input_t inputs[BSEC_MAX_PHYSICAL_SENSOR];
	u8_t n_inputs = 0, n_outputs = 0;

	if (bme680_settings.process_data & BSEC_PROCESS_TEMPERATURE) {
		inputs[n_inputs].sensor_id = BSEC_INPUT_TEMPERATURE;
		inputs[n_inputs].signal = temp.val1 + temp.val2 / 1000000.0f;
		inputs[n_inputs].time_stamp = call_time_ns;
		n_inputs++;
		/* Temperature offset from the real temperature due to external heat sources */
		inputs[n_inputs].sensor_id = BSEC_INPUT_HEATSOURCE;
		inputs[n_inputs].signal = TEMP_FROM_EXT_HEAT_SOURCE;
		inputs[n_inputs].time_stamp = call_time_ns;
		n_inputs++;
	}
	if (bme680_settings.process_data & BSEC_PROCESS_HUMIDITY) {
		inputs[n_inputs].sensor_id = BSEC_INPUT_HUMIDITY;
		inputs[n_inputs].signal = hum.val1 + hum.val2 / 1000000.0f;
		inputs[n_inputs].time_stamp = call_time_ns;
		n_inputs++;
	}
	if (bme680_settings.process_data & BSEC_PROCESS_PRESSURE) {
		inputs[n_inputs].sensor_id = BSEC_INPUT_PRESSURE;
		inputs[n_inputs].signal = pres.val1 + pres.val2 / 1000000.0f;
		inputs[n_inputs].time_stamp = call_time_ns;
		n_inputs++;
	}
	if (bme680_settings.process_data & BSEC_PROCESS_GAS) {
		inputs[n_inputs].sensor_id = BSEC_INPUT_GASRESISTOR;
		inputs[n_inputs].signal = gas_res.val1 + gas_res.val2 / 1000000.0f;
		inputs[n_inputs].time_stamp = call_time_ns;
		n_inputs++;
	}

	if (n_inputs > 0) {
		n_outputs = BSEC_NUMBER_OUTPUTS;
		bsec_output_t outputs[BSEC_NUMBER_OUTPUTS];

		bsec_ret = bsec_do_steps(inputs, n_inputs, outputs, &n_outputs);
		if (bsec_ret != BSEC_OK)
			return bsec_ret;

		if (n_outputs > 0) {
			for (u8_t i = 0; i < n_outputs; i++) {
				switch (outputs[i].sensor_id) {
				case BSEC_OUTPUT_IAQ:
					iaq = outputs[i].signal;
					iaq_accuracy = outputs[i].accuracy;
					break;
				case BSEC_OUTPUT_RAW_TEMPERATURE:
					raw_temperature = outputs[i].signal;
					break;
				case BSEC_OUTPUT_RAW_PRESSURE:
					pressure = outputs[i].signal;
					break;
				case BSEC_OUTPUT_RAW_HUMIDITY:
					raw_humidity = outputs[i].signal;
					break;
				case BSEC_OUTPUT_RAW_GAS:
					gas_resistance = outputs[i].signal;
					break;
				case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
					temperature = outputs[i].signal;
					break;
				case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
					humidity = outputs[i].signal;
					break;
				default:
					break;
				}
			}
		}
	}

	*snooze_dur = (bme680_settings.next_call / 1000000LL) - k_uptime_get();

	return bsec_ret;
}
#define MQTT_INPUT_PERIOD K_SECONDS(1)
#define MQTT_LIVE_PERIOD K_SECONDS(55)

/* Buffers for MQTT client. */
static u8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static u8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static u8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];

/* The mqtt client struct */
static struct mqtt_client client;

/* MQTT Broker details. */
static struct sockaddr_storage broker;

/* Connected flag */
static bool connected;

/* File descriptor */
struct pollfd fds;

static u8_t token[512];

/* private key information */
extern unsigned char zepfull_private_der[];
extern unsigned int zepfull_private_der_len;

static s64_t time_base;

static struct mqtt_utf8 password = {
	.utf8 = token
};
static struct mqtt_utf8 username = {
	.utf8 = "none",
	.size = sizeof("none")
};

time_t my_k_time(time_t *ptr)
{
	s64_t stamp;
	time_t now;

	stamp = k_uptime_get();
	now = (time_t)(time_base + (stamp / 1000));

	if (ptr) {
		*ptr = now;
	}

	return now;
}

/**@brief Function to print strings without null-termination
 */
static void data_print(u8_t *prefix, u8_t *data, size_t len)
{
	char buf[len + 1];

	memcpy(buf, data, len);
	buf[len] = 0;
	printk("%s%s\n", prefix, buf);
}

/**@brief Function to publish data on the configured topic
 */
static int data_publish(struct mqtt_client *c, enum mqtt_qos qos,
	u8_t *data, size_t len)
{
	struct mqtt_publish_param param;

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = CONFIG_MQTT_PUB_TOPIC;
	param.message.topic.topic.size = strlen(CONFIG_MQTT_PUB_TOPIC);
	param.message.payload.data = data;
	param.message.payload.len = len;
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	data_print("Publish: ", data, len);
	printk("to topic: %s len: %u\n",
		CONFIG_MQTT_PUB_TOPIC,
		(unsigned int)strlen(CONFIG_MQTT_PUB_TOPIC));

	return mqtt_publish(c, &param);
}

/**@brief Function to subscribe to the configured topic
 */
static int subscribe(void)
{
	struct mqtt_topic subscribe_topic = {
		.topic = {
			.utf8 = CONFIG_MQTT_SUB_TOPIC,
			.size = strlen(CONFIG_MQTT_SUB_TOPIC)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};

	const struct mqtt_subscription_list subscription_list = {
		.list = &subscribe_topic,
		.list_count = 1,
		.message_id = 1234
	};

	printk("Subscribing to: %s len %u\n", CONFIG_MQTT_SUB_TOPIC,
		(unsigned int)strlen(CONFIG_MQTT_SUB_TOPIC));

	return mqtt_subscribe(&client, &subscription_list);
}

/**@brief Function to read the published payload.
 */
static int publish_get_payload(struct mqtt_client *c, size_t length)
{
	u8_t *buf = payload_buf;
	u8_t *end = buf + length;

	if (length > sizeof(payload_buf)) {
		return -EMSGSIZE;
	}

	while (buf < end) {
		int ret = mqtt_read_publish_payload(c, buf, end - buf);

		if (ret < 0) {
			if (ret == -EAGAIN) {
				printk("mqtt_read_publish_payload: EAGAIN");
				poll(&fds, 1, K_FOREVER);
				continue;
			}

			return ret;
		}

		if (ret == 0) {
			return -EIO;
		}

		buf += ret;
	}

	return 0;
}

/**@brief MQTT client event handler
 */
void mqtt_evt_handler(struct mqtt_client *const c,
		      const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			printk("MQTT connect failed %d\n", evt->result);
			break;
		}

		connected = true;
		printk("[%s:%d] MQTT client connected!\n", __func__, __LINE__);
		subscribe();
		break;

	case MQTT_EVT_DISCONNECT:
		printk("[%s:%d] MQTT client disconnected %d\n", __func__,
		       __LINE__, evt->result);

		connected = false;
		break;

	case MQTT_EVT_PUBLISH: {
		const struct mqtt_publish_param *p = &evt->param.publish;

		printk("[%s:%d] MQTT PUBLISH result=%d len=%d\n", __func__,
		       __LINE__, evt->result, p->message.payload.len);
		err = publish_get_payload(c, p->message.payload.len);
		if (err >= 0) {
			data_print("Received: ", payload_buf,
				p->message.payload.len);
			if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
				const struct mqtt_puback_param ack = {
					.message_id = p->message_id
				};

				/* Send acknowledgment. */
				mqtt_publish_qos1_ack(&client, &ack);
			}
			
		} else {
			printk("mqtt_read_publish_payload: Failed! %d\n", err);
		}
	} break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			printk("MQTT PUBACK error %d\n", evt->result);
			break;
		}

		printk("[%s:%d] PUBACK packet id: %u\n", __func__, __LINE__,
				evt->param.puback.message_id);
		break;

	case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			printk("MQTT SUBACK error %d\n", evt->result);
			break;
		}

		printk("[%s:%d] SUBACK packet id: %u\n", __func__, __LINE__,
				evt->param.suback.message_id);
		break;

	default:
		printk("[%s:%d] default: %d\n", __func__, __LINE__,
				evt->type);
		break;
	}
}

/**@brief Resolves the configured hostname and
 * initializes the MQTT broker structure
 */
static void broker_init(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo *addr;
	struct addrinfo hints;

	hints.ai_flags = 0;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;

	err = getaddrinfo(CONFIG_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);
	if (err) {
		printk("ERROR: getaddrinfo failed %d\n", err);

		return;
	}

	addr = result;
	err = -ENOENT;

	/* Look for address of the broker. */
	while (addr != NULL) {
		/* IPv4 Address. */
		if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
			struct sockaddr_in *broker4 =
				((struct sockaddr_in *)&broker);

			broker4->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)
				->sin_addr.s_addr;
			broker4->sin_family = AF_INET;
			broker4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);
			printk("IPv4 Address found 0x%08x\n",
				broker4->sin_addr.s_addr);
			break;
		} else {
			printk("ai_addrlen = %u should be %u or %u\n",
				(unsigned int)addr->ai_addrlen,
				(unsigned int)sizeof(struct sockaddr_in),
				(unsigned int)sizeof(struct sockaddr_in6));
		}

		addr = addr->ai_next;
		break;
	}

	/* Free the address. */
	freeaddrinfo(result);
}

/**@brief Initialize the MQTT client structure
 */
static void client_init(struct mqtt_client *client)
{
	struct jwt_builder jb;
	int err;
	mqtt_client_init(client);

	broker_init();
	time_t now = my_k_time(NULL);

	err = jwt_init_builder(&jb, token, sizeof(token));
	if (err != 0) {
		printk("Error with JWT token");
		return;
	}

	//24 hour expiry is maximum allowed
	err = jwt_add_payload(&jb, now + 24* 60 * 60, now,
				      CONFIG_CLOUD_AUDIENCE);
	if (err != 0) {
		printk("Error with JWT token");
		return;
	}

	err = jwt_sign(&jb, zepfull_private_der,
			zepfull_private_der_len);

	if (err != 0) {
		printk("Error with JWT token");
		return;
	}
	
	/* MQTT client configuration */
	client->broker = &broker;
	client->evt_cb = mqtt_evt_handler;
	client->client_id.utf8 = (u8_t *)CONFIG_MQTT_CLIENT_ID;
	client->client_id.size = strlen(CONFIG_MQTT_CLIENT_ID);
	client->password = &password;
	password.size = jwt_payload_len(&jb);
	client->user_name = &username;
	client->protocol_version = MQTT_VERSION_3_1_1;

	/* MQTT buffers configuration */
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);

	/* MQTT transport configuration */
	client->transport.type = MQTT_TRANSPORT_SECURE;
}

/**@brief Initialize the file descriptor structure used by poll.
 */
static int fds_init(struct mqtt_client *c)
{
	if (c->transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds.fd = c->transport.tcp.sock;
	} else {
#if defined(CONFIG_MQTT_LIB_TLS)
		fds.fd = c->transport.tls.sock;
#else
		return -ENOTSUP;
#endif
	}

	fds.events = POLLIN;

	return 0;
}

/**@brief Configures modem to provide LTE link. Blocks until link is
 * successfully established.
 */
static void modem_configure(void)
{
#if defined(CONFIG_LTE_LINK_CONTROL)
	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already turned on
		 * and connected.
		 */
	} else {
		int err;

		printk("LTE Link Connecting ...\n");
		err = lte_lc_init_and_connect();
		__ASSERT(err == 0, "LTE link could not be established.");
		printk("LTE Link Connected!\n");
	}
#endif
}

// use GlobalSign Root CA - R2
#define GOOGLE_IOT_CORE_CA_PEM                                              \
"-----BEGIN CERTIFICATE----- \n"                                      \
"MIIDujCCAqKgAwIBAgILBAAAAAABD4Ym5g0wDQYJKoZIhvcNAQEFBQAwTDEgMB4G\n"  \
"A1UECxMXR2xvYmFsU2lnbiBSb290IENBIC0gUjIxEzARBgNVBAoTCkdsb2JhbFNp\n"  \
"Z24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMDYxMjE1MDgwMDAwWhcNMjExMjE1\n"  \
"MDgwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMjETMBEG\n"  \
"A1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjCCASIwDQYJKoZI\n"  \
"hvcNAQEBBQADggEPADCCAQoCggEBAKbPJA6+Lm8omUVCxKs+IVSbC9N/hHD6ErPL\n"  \
"v4dfxn+G07IwXNb9rfF73OX4YJYJkhD10FPe+3t+c4isUoh7SqbKSaZeqKeMWhG8\n"  \
"eoLrvozps6yWJQeXSpkqBy+0Hne/ig+1AnwblrjFuTosvNYSuetZfeLQBoZfXklq\n"  \
"tTleiDTsvHgMCJiEbKjNS7SgfQx5TfC4LcshytVsW33hoCmEofnTlEnLJGKRILzd\n"  \
"C9XZzPnqJworc5HGnRusyMvo4KD0L5CLTfuwNhv2GXqF4G3yYROIXJ/gkwpRl4pa\n"  \
"zq+r1feqCapgvdzZX99yqWATXgAByUr6P6TqBwMhAo6CygPCm48CAwEAAaOBnDCB\n"  \
"mTAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUm+IH\n"  \
"V2ccHsBqBt5ZtJot39wZhi4wNgYDVR0fBC8wLTAroCmgJ4YlaHR0cDovL2NybC5n\n"  \
"bG9iYWxzaWduLm5ldC9yb290LXIyLmNybDAfBgNVHSMEGDAWgBSb4gdXZxwewGoG\n"  \
"3lm0mi3f3BmGLjANBgkqhkiG9w0BAQUFAAOCAQEAmYFThxxol4aR7OBKuEQLq4Gs\n"  \
"J0/WwbgcQ3izDJr86iw8bmEbTUsp9Z8FHSbBuOmDAGJFtqkIk7mpM0sYmsL4h4hO\n"  \
"291xNBrBVNpGP+DTKqttVCL1OmLNIG+6KYnX3ZHu01yiPqFbQfXf5WRDLenVOavS\n"  \
"ot+3i9DAgBkcRcAtjOj4LaR0VknFBbVPFd5uRHg5h6h+u/N5GJG79G+dwfCMNYxd\n"  \
"AfvDbbnvRG15RjF+Cv6pgsH/76tuIMRQyV+dTZsXjAzlAcmgQWpzU/qlULRuJQ/7\n"  \
"TBj0/VLZjmmx6BEP3ojY+x1J96relc8geMJgEtslQIxq/H5COEBkEveegeGTLg==\n"  \
"-----END CERTIFICATE-----"

const char googleIotCoreCaPem[] = GOOGLE_IOT_CORE_CA_PEM;



void set_certificate(void)
{
		/* Delete certificates */
		nrf_sec_tag_t sec_tag = 0;
		int err;
		for (nrf_key_mgnt_cred_type_t type = 0; type < 5; type++) {
			err = nrf_inbuilt_key_delete(sec_tag, type);
			printk("nrf_inbuilt_key_delete(%lu, %d) => result=%d \n",
				sec_tag, type, err);
		}

		//Provision CA Certificate. 
		err = nrf_inbuilt_key_write(sec_tag,
					NRF_KEY_MGMT_CRED_TYPE_CA_CHAIN,
					(char *)googleIotCoreCaPem,	
				 strlen(googleIotCoreCaPem));
		if (err) {
			printk("NRF_CLOUD_CA_CERTIFICATE err: %d", err);
			
		}
}

#if defined(CONFIG_DK_LIBRARY)
/**@brief Callback for button events from the DK buttons and LEDs library. */
static void button_handler(u32_t buttons, u32_t has_changed)
{
	if((buttons & has_changed) & 0x01)
	{
		
		char out_str[]="Button 1 was pressed\n";
		
		/* Echo back received data */
		data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
			out_str, sizeof(out_str));
	}
	if((buttons & has_changed) & 0x02)
	{
		
		char out_str[]="Button 2 was pressed\n";
		
		/* Echo back received data */
		data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
			out_str, sizeof(out_str));
	}
}
#endif /* defined(CONFIG_DK_LIBRARY) */
static void buttons_leds_init(void)
{
#if defined(CONFIG_DK_LIBRARY)
	int err;

	err = dk_buttons_init(button_handler);
	if (err) {
		printk("Could not initialize buttons, err code: %d\n", err);
	}

	err = dk_leds_init();
	if (err) {
		printk("Could not initialize leds, err code: %d\n", err);
	}

	err = dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);
	if (err) {
		printk("Could not set leds state, err code: %d\n", err);
	}
#endif
}

static struct k_delayed_work mqtt_live_work;

static void mqtt_live_handler(struct k_work *work) {
    if (1) {
        int err = mqtt_live(&client);
        if (err != 0) {
            printk("MQTT unable to live: %d %s\n", err, strerror(-err));
        }

        err = k_delayed_work_submit(&mqtt_live_work, MQTT_LIVE_PERIOD);
        if (err != 0) {
            printk("Unable to resubmit mqtt_live_work: %d %s\n", err, strerror(-err));
        }
    }
}

static struct k_delayed_work mqtt_input_work;

static void mqtt_input_handler(struct k_work *work) {
    if (1) {
        int err = mqtt_input(&client);
        if (err != 0) {
            printk("Unable to get MQTT input: %d %s\n", err, strerror(-err));
            //return err;
        }

        err = k_delayed_work_submit(&mqtt_input_work, MQTT_INPUT_PERIOD);
        if (err != 0) {
            printk("Unable to resubmit mqtt_input_work: %d %s\n", err, strerror(-err));
        }
    }
}

struct env_sensor {
	enum sensor_channel channel;
	u8_t *dev_name;
	struct device *dev;
};

static struct env_sensor temp_sensor = {
	.channel = SENSOR_CHAN_AMBIENT_TEMP,
	.dev_name = CONFIG_TEMP_DEV_NAME
};

static struct env_sensor humid_sensor = {
	.channel = SENSOR_CHAN_HUMIDITY,
	.dev_name = CONFIG_TEMP_DEV_NAME
};

static struct env_sensor pressure_sensor = {
	.channel = SENSOR_CHAN_PRESS,
	.dev_name = CONFIG_TEMP_DEV_NAME
};

static struct env_sensor *env_sensors[] = {
	&temp_sensor,
	&humid_sensor,
	&pressure_sensor
};

/**@brief Initialize environment sensors. */
static void env_sensor_init(void)
{
	for (int i = 0; i < ARRAY_SIZE(env_sensors); i++) {
		env_sensors[i]->dev =
			device_get_binding(env_sensors[i]->dev_name);
		__ASSERT(env_sensors[i]->dev, "Could not get device %s\n",
			env_sensors[i]->dev_name);
	}
}

static void get_and_send_env_data(void) {
	int num_sensors = ARRAY_SIZE(env_sensors);
	struct sensor_value data[num_sensors];
	char buf[6];
	char temp_str[30];
	char hum_str[30];
	char pres_str[30];
	char send_str[200];

	int err;
	u8_t len;
	for (int i = 0; i < num_sensors; i++) {
		err = sensor_sample_fetch_chan(env_sensors[i]->dev,
			env_sensors[i]->channel);
		if (err) {
			printk("Failed to fetch data from %s, error: %d\n",
				env_sensors[i]->dev_name, err);
			return;
		}

		err = sensor_channel_get(env_sensors[i]->dev,
			env_sensors[i]->channel, &data[i]);
		if (err) {
			printk("Failed to fetch data from %s, error: %d\n",
				env_sensors[i]->dev_name, err);
			return;
		}
		
		len = snprintf(buf, sizeof(buf), "%.1f",
			sensor_value_to_double(&data[i]));
		//printk("Data for sensor %d : %s \n", i, buf);
		
		if(i==0)
		{
			sprintf(temp_str, "\"temp\" : %s", buf);
		}
		if(i==1)
		{
			sprintf(hum_str, "\"hum\" : %s", buf);
		}
		if(i==2)
		{
			sprintf(pres_str, "\"pres\" : %s", buf);
		}
		/*env_cloud_data[i].data.ptr = buf;
		env_cloud_data[i].data.len = len;
		env_cloud_data[i].tag += 1;

		if (env_cloud_data[i].tag == 0) {
			env_cloud_data[i].tag = 0x1;
		}

		sensor_data_send(&env_cloud_data[i]);*/
	}
	time_t current_time=get_unix_time();
		char time_s[40];
		sprintf(time_s, "\"timestamp\" : \"%lld\"", current_time);
	sprintf(send_str, "{ %s , %s , %s , %s }", temp_str, hum_str, pres_str, time_s);
	err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
				send_str, strlen(send_str));
			if(err<0)
			{
				printk("error when publishing\r\n");
			}
	//printk("%s\n", send_str);
}
void main(void)
{
		char send_str[200];
	int err;
	//IF YOU NEED TO WRITE CERTIFICATE
	//set_certificate();
	
	printk("The MQTT simple sample started\n");
	console_getline_init();
	//time_base = (1553030949         +60) ;
	
	buttons_leds_init();
	modem_configure();
	//env_sensor_init();
	struct device *dev = device_get_binding("BME680");
	s32_t snooze_dur;

	printf("dev %p name %s\n", dev, dev->config->name);

	bsec_library_return_t bsec_ret;
	bsec_version_t bsec_ver;
	bsec_virtual_sensor_t subscription[4] = {
	                BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
	                BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
	                BSEC_OUTPUT_IAQ,
	                BSEC_OUTPUT_RAW_PRESSURE
	};

	bsec_ret = bsec_init();
	if (bsec_ret) {
		printf("BSEC Error: %d\n", bsec_ret);
		return;
	}

	bsec_ret = bsec_get_version(&bsec_ver);
	if (bsec_ret) {
		printf("BSEC Error: %d\n", bsec_ret);
		return;
	}

	printf("BSEC v%d.%d.%d.%d initialized.\n", bsec_ver.major, bsec_ver.minor, bsec_ver.major_bugfix,
	                bsec_ver.minor_bugfix);

	bsec_ret = update_subscription(subscription, 4, BSEC_SAMPLE_RATE_LP);
	if (bsec_ret) {
		printf("BSEC Error: %d\n", bsec_ret);
		return;
	}

	time_base=get_unix_time();
	printk("NTP time: %s", ctime((time_t *)&time_base));
	client_init(&client);

	err = mqtt_connect(&client);
	if (err != 0) {
		printk("ERROR: mqtt_connect %d\n", err);
		return;
	}

	err = fds_init(&client);
	if (err != 0) {
		printk("ERROR: fds_init %d\n", err);
		return;
	}

	k_delayed_work_init(&mqtt_live_work, mqtt_live_handler);
	k_delayed_work_submit(&mqtt_live_work, K_SECONDS(0));

	k_delayed_work_init(&mqtt_input_work, mqtt_input_handler);
	k_delayed_work_submit(&mqtt_input_work, K_SECONDS(0));
	
	dk_set_leds(4);
		k_sleep(K_MSEC(200));
		dk_set_leds(0);
int cnt=0;
	while (1) {
		bsec_ret = run(dev, &snooze_dur);
		if (bsec_ret) {
			printf("BSEC Error: %d\n", bsec_ret);
			return;
		}

		//printf("t: %d, T: %.2f; P: %.2f; H: %.2f; IAQ: %.2f; Acc: %d\n", k_uptime_get_32(), temperature,
		//                pressure, humidity, iaq, iaq_accuracy);

		cnt++;
		if(cnt>=20)
		{
			time_t current_time=get_unix_time();
			char time_s[40];
			sprintf(time_s, "\"timestamp\" : \"%lld\"", current_time);
			sprintf(send_str, "{ \"temp\" : %.2f , \"pres\" : %.2f , \"hum\" : %.2f ,  \"iaq\" : %.2f , \"iaq_acc\" : %d ,  %s }", temperature, pressure, humidity, iaq, iaq_accuracy,time_s);
			err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
				send_str, strlen(send_str));
			if(err<0)
			{
				printk("error when publishing\r\n");
			}
			dk_set_leds(2);
			k_sleep(K_MSEC(200));
			dk_set_leds(0);
			cnt=0;
		}
		
		k_sleep(snooze_dur);
		
	}
	while(0)
	{
		k_sleep(K_SECONDS(60));
		get_and_send_env_data();
		dk_set_leds(2);
		k_sleep(K_MSEC(200));
		dk_set_leds(0);


	
	}
	/*while (1) {
		//send input from console to cloud and add timestamp.
		char *s =console_getline();
		time_t current_time=get_unix_time();
		char time_s[40];
		sprintf(time_s, " , \"timestamp\" : \"%lld\" }", current_time);
		char * new_str ;
		if((new_str = malloc(strlen(s)+strlen(time_s)+1+2)) != NULL){
			new_str[0] = '{';   
			new_str[1] = ' ';
			new_str[2] = '\0'; // ensures end of string
			strcat(new_str,s);
			strcat(new_str,time_s);
		} else {
			
		}
		int err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
				new_str, strlen(new_str));
			if(err<0)
			{
				printk("error when publishing\r\n");
			}
	}*/
}
