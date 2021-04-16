#include "cloud/cloud_wrapper.h"
#include <zephyr.h>
#include "cloud/mqtt_backend/mqtt_backend.h"
#include <modem/at_cmd.h>

#define MODULE my_integration

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUD_INTEGRATION_LOG_LEVEL);

#if !defined(CONFIG_CLOUD_CLIENT_ID_USE_CUSTOM)
#define MY_CLOUD_CLIENT_ID_LEN 15
#else
#define MY_CLOUD_CLIENT_ID_LEN (sizeof(CONFIG_CLOUD_CLIENT_ID) - 1)
#endif

#define CFG_TOPIC "%s/cfg"
#define CFG_TOPIC_LEN (MY_CLOUD_CLIENT_ID_LEN + 4)
#define BATCH_TOPIC "%s/batch"
#define BATCH_TOPIC_LEN (MY_CLOUD_CLIENT_ID_LEN + 6)
#define MESSAGES_TOPIC "%s/messages"
#define MESSAGES_TOPIC_LEN (MY_CLOUD_CLIENT_ID_LEN + 9)



#define REQUEST_SHADOW_DOCUMENT_STRING ""

static char client_id_buf[MY_CLOUD_CLIENT_ID_LEN + 1];
static char batch_topic[BATCH_TOPIC_LEN + 1];
static char cfg_topic[CFG_TOPIC_LEN + 1];
static char messages_topic[MESSAGES_TOPIC_LEN + 1];

static struct mqtt_backend_config config;

static cloud_wrap_evt_handler_t wrapper_evt_handler;

static void cloud_wrapper_notify_event(const struct cloud_wrap_event *evt)
{
	if ((wrapper_evt_handler != NULL) && (evt != NULL)) {
		wrapper_evt_handler(evt);
	} else {
		LOG_ERR("Library event handler not registered, or empty event");
	}
}

static int populate_app_endpoint_topics(void)
{
	int err;

	err = snprintf(batch_topic, sizeof(batch_topic), BATCH_TOPIC,
		       client_id_buf);
	if (err != BATCH_TOPIC_LEN) {
		return -ENOMEM;
	}

	err = snprintf(messages_topic, sizeof(messages_topic), MESSAGES_TOPIC,
		       client_id_buf);
	if (err != MESSAGES_TOPIC_LEN) {
		return -ENOMEM;
	}

	err = snprintf(cfg_topic, sizeof(cfg_topic), CFG_TOPIC, client_id_buf);
	if (err != CFG_TOPIC_LEN) {
		return -ENOMEM;
	}

	return 0;
}


void mqtt_backed_event_handler(const struct mqtt_backend_evt *const evt)
{
	struct cloud_wrap_event cloud_wrap_evt = { 0 };
	bool notify = false;

	switch (evt->type) {
	case MQTT_BACKEND_EVT_CONNECTING:
		LOG_DBG("MQTT_BACKEND_EVT_CONNECTING");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_CONNECTING;
		notify = true;
		break;
	case MQTT_BACKEND_EVT_CONNECTED:
		LOG_DBG("MQTT_BACKEND_EVT_CONNECTED");
		const struct mqtt_backend_topic_data sub_topic = {
			.str = cfg_topic,
			.len = CFG_TOPIC_LEN,
		};
		mqtt_backend_subscribe(&sub_topic);
		break;
		
	case MQTT_BACKEND_EVT_READY:
		LOG_DBG("MQTT_BACKEND_EVT_READY");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_CONNECTED;
		notify = true;
		break;
	case MQTT_BACKEND_EVT_DISCONNECTED:
		LOG_DBG("MQTT_BACKEND_EVT_DISCONNECTED");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_DISCONNECTED;
		notify = true;
		break;
	case MQTT_BACKEND_EVT_DATA_RECEIVED:
		LOG_DBG("MQTT_BACKEND_EVT_DATA_RECEIVED");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_DATA_RECEIVED;
		cloud_wrap_evt.data.buf = evt->ptr;
		cloud_wrap_evt.data.len = evt->len;
		notify = true;
		break;
	
	default:
		LOG_ERR("Unknown MQTT backend event type: %d", evt->type);
		break;
	}

	if (notify) {
		cloud_wrapper_notify_event(&cloud_wrap_evt);
	}
}

int cloud_wrap_init(cloud_wrap_evt_handler_t event_handler)
{
	int err;

#if !defined(CONFIG_CLOUD_CLIENT_ID_USE_CUSTOM)
	char imei_buf[20];

	/* Retrieve device IMEI from modem. */
	err = at_cmd_write("AT+CGSN", imei_buf, sizeof(imei_buf), NULL);
	if (err) {
		LOG_ERR("Not able to retrieve device IMEI from modem");
		return err;
	}

	/* Set null character at the end of the device IMEI. */
	imei_buf[MY_CLOUD_CLIENT_ID_LEN] = 0;

	strncpy(client_id_buf, imei_buf, sizeof(client_id_buf) - 1);

#else
	snprintf(client_id_buf, sizeof(client_id_buf), "%s",
		 CONFIG_CLOUD_CLIENT_ID);
#endif

	/* Fetch IMEI from modem data and set IMEI as cloud connection ID **/
	config.client_id = client_id_buf;
	config.client_id_len = strlen(client_id_buf);

	err = mqtt_backend_init(&config, mqtt_backed_event_handler);
	if (err) {
		LOG_ERR("mqtt_backend_init, error: %d", err);
		return err;
	}

	/* Populate cloud specific endpoint topics */
	err = populate_app_endpoint_topics();
	if (err) {
		LOG_ERR("populate_app_endpoint_topics, error: %d", err);
		return err;
	}

	LOG_DBG("********************************************");
	LOG_DBG(" The Asset Tracker v2 has started");
	LOG_DBG(" Version:     %s",
		CONFIG_ASSET_TRACKER_V2_APP_VERSION);
	LOG_DBG(" Client ID:   %s", log_strdup(client_id_buf));
	LOG_DBG(" Cloud:       %s", "my_integration");
	LOG_DBG(" Endpoint:    %s",
		CONFIG_MQTT_BACKEND_BROKER_HOST_NAME);
	LOG_DBG("********************************************");

	wrapper_evt_handler = event_handler;

	return 0;
}

int cloud_wrap_connect(void)
{
	int err;

	err = mqtt_backend_connect(NULL);
	if (err) {
		LOG_ERR("mqtt_backend_connect, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_disconnect(void)
{
	int err;

	err = mqtt_backend_disconnect();
	if (err) {
		LOG_ERR("mqtt_backend_disconnect, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_state_get(void)
{
	//Not impemented

	return 0;
}

int cloud_wrap_state_send(char *buf, size_t len)
{
	int err;

	struct mqtt_backend_tx_data msg = {
		.str = buf,
		.len = len,
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.str = messages_topic,
		.topic.len = MESSAGES_TOPIC_LEN,
		.topic.type = MQTT_BACKEND_TOPIC_MSG
	};

	err = mqtt_backend_send(&msg);
	if (err) {
		LOG_ERR("mqtt_backend_send, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_data_send(char *buf, size_t len)
{
	int err;

	struct mqtt_backend_tx_data msg = {
		.str = buf,
		.len = len,
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.str = messages_topic,
		.topic.len = MESSAGES_TOPIC_LEN,
		.topic.type = MQTT_BACKEND_TOPIC_MSG
	};

	err = mqtt_backend_send(&msg);
	if (err) {
		LOG_ERR("mqtt_backend_send, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_batch_send(char *buf, size_t len)
{
	int err;

	struct mqtt_backend_tx_data msg = {
		.str = buf,
		.len = len,
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.str = batch_topic,
		.topic.len = BATCH_TOPIC_LEN,
		.topic.type = MQTT_BACKEND_TOPIC_MSG
	};

	err = mqtt_backend_send(&msg);
	if (err) {
		LOG_ERR("mqtt_backend_send, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_ui_send(char *buf, size_t len)
{
	int err;

	struct mqtt_backend_tx_data msg = {
		.str = buf,
		.len = len,
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.str = messages_topic,
		.topic.len = MESSAGES_TOPIC_LEN,
		.topic.type = MQTT_BACKEND_TOPIC_MSG
	};

	err = mqtt_backend_send(&msg);
	if (err) {
		LOG_ERR("mqtt_backend_send, error: %d", err);
		return err;
	}

	return 0;
}
