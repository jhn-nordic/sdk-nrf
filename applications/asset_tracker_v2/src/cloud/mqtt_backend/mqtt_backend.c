#include <mqtt_backend.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <net/cloud.h>
#include <stdio.h>
#include <random/rand32.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(mqtt_backend, CONFIG_MQTT_BACKEND_LOG_LEVEL);

BUILD_ASSERT_MSG(sizeof(CONFIG_MQTT_BACKEND_BROKER_HOST_NAME) > 1,
		 "MQTT Backend broker hostname not set");

#if defined(CONFIG_MQTT_BACKEND_IPV6)
#define MQTT_BACKEND_FAMILY AF_INET6
#else
#define MQTT_BACKEND_FAMILY AF_INET
#endif


static char client_id_buf[CONFIG_MQTT_BACKEND_CLIENT_ID_MAX_LEN + 1];
static char rx_buffer[CONFIG_MQTT_BACKEND_MQTT_RX_TX_BUFFER_LEN];
static char tx_buffer[CONFIG_MQTT_BACKEND_MQTT_RX_TX_BUFFER_LEN];
static char payload_buf[CONFIG_MQTT_BACKEND_MQTT_PAYLOAD_BUFFER_LEN];

static struct mqtt_client client;
static struct sockaddr_storage broker;


static mqtt_backend_evt_handler_t module_evt_handler;
static atomic_t disconnect_requested;
static atomic_t connection_poll_active;

/* Flag that indicates if the client is disconnected from the
 * AWS IoT broker, or not.
 */
static atomic_t mqtt_backend_disconnected = ATOMIC_INIT(1);

static K_SEM_DEFINE(connection_poll_sem, 0, 1);

static void mqtt_backend_notify_event(const struct mqtt_backend_evt *evt)
{
	if ((module_evt_handler != NULL) && (evt != NULL)) {
		module_evt_handler(evt);
	}
}


static int publish_get_payload(struct mqtt_client *const c, size_t length)
{
	if (length > sizeof(payload_buf)) {
		LOG_ERR("Incoming MQTT message too large for payload buffer");
		return -EMSGSIZE;
	}

	return mqtt_readall_publish_payload(c, payload_buf, length);
}

static void mqtt_evt_handler(struct mqtt_client *const c,
			     const struct mqtt_evt *mqtt_evt)
{
	int err;

	struct mqtt_backend_evt mqtt_backend_evt = { 0 };


	switch (mqtt_evt->type) {
	case MQTT_EVT_CONNACK:
		LOG_DBG("MQTT client connected!");


		LOG_DBG("CONNACK, error: %d",
			mqtt_evt->param.connack.return_code);


		mqtt_backend_evt.type = MQTT_BACKEND_EVT_CONNECTED;
		mqtt_backend_notify_event(&mqtt_backend_evt);
		mqtt_backend_evt.type = MQTT_BACKEND_EVT_READY;
		mqtt_backend_notify_event(&mqtt_backend_evt);

		break;
	case MQTT_EVT_DISCONNECT:
		LOG_DBG("MQTT_EVT_DISCONNECT: result = %d", mqtt_evt->result);


		mqtt_backend_evt.type = MQTT_BACKEND_EVT_DISCONNECTED;
		mqtt_backend_notify_event(&mqtt_backend_evt);

		break;
	case MQTT_EVT_PUBLISH: {
		const struct mqtt_publish_param *p = &mqtt_evt->param.publish;

		LOG_DBG("MQTT_EVT_PUBLISH: id = %d len = %d ",
			p->message_id,
			p->message.payload.len);

		err = publish_get_payload(c, p->message.payload.len);
		if (err) {
			LOG_ERR("publish_get_payload, error: %d", err);
			break;
		}

		if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
			const struct mqtt_puback_param ack = {
				.message_id = p->message_id
			};

			mqtt_publish_qos1_ack(c, &ack);
		}


		mqtt_backend_evt.type = MQTT_BACKEND_EVT_DATA_RECEIVED;
		mqtt_backend_evt.ptr = payload_buf;
		mqtt_backend_evt.len = p->message.payload.len;
		mqtt_backend_notify_event(&mqtt_backend_evt);


	} break;
	case MQTT_EVT_PUBACK:
		LOG_DBG("MQTT_EVT_PUBACK: id = %d result = %d",
			mqtt_evt->param.puback.message_id,
			mqtt_evt->result);
		break;
	case MQTT_EVT_SUBACK:
		LOG_DBG("MQTT_EVT_SUBACK: id = %d result = %d",
			mqtt_evt->param.suback.message_id,
			mqtt_evt->result);
		break;
	default:
		break;
	}
}

static int broker_init(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo *addr;
	struct addrinfo hints = {
		.ai_family = MQTT_BACKEND_FAMILY,
		.ai_socktype = SOCK_STREAM
	};

	err = getaddrinfo(CONFIG_MQTT_BACKEND_BROKER_HOST_NAME,
			  NULL, &hints, &result);
	if (err) {
		LOG_ERR("getaddrinfo, error %d", err);
		return err;
	}

	addr = result;

	while (addr != NULL) {
		if ((addr->ai_addrlen == sizeof(struct sockaddr_in)) &&
		    (MQTT_BACKEND_FAMILY == AF_INET)) {
			struct sockaddr_in *broker4 =
				((struct sockaddr_in *)&broker);
			char ipv4_addr[NET_IPV4_ADDR_LEN];

			broker4->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)
				->sin_addr.s_addr;
			broker4->sin_family = AF_INET;
			broker4->sin_port = htons(CONFIG_MQTT_BACKEND_BROKER_PORT);

			inet_ntop(AF_INET, &broker4->sin_addr.s_addr, ipv4_addr,
				  sizeof(ipv4_addr));
			LOG_DBG("IPv4 Address found %s", log_strdup(ipv4_addr));
			break;
		} else if ((addr->ai_addrlen == sizeof(struct sockaddr_in6)) &&
			   (MQTT_BACKEND_FAMILY == AF_INET6)) {
			struct sockaddr_in6 *broker6 =
				((struct sockaddr_in6 *)&broker);
			char ipv6_addr[NET_IPV6_ADDR_LEN];

			memcpy(broker6->sin6_addr.s6_addr,
			       ((struct sockaddr_in6 *)addr->ai_addr)
			       ->sin6_addr.s6_addr,
			       sizeof(struct in6_addr));
			broker6->sin6_family = AF_INET6;
			broker6->sin6_port = htons(CONFIG_MQTT_BACKEND_BROKER_PORT);

			inet_ntop(AF_INET6, &broker6->sin6_addr.s6_addr,
				  ipv6_addr, sizeof(ipv6_addr));
			LOG_DBG("IPv4 Address found %s", log_strdup(ipv6_addr));
			break;
		}

		LOG_DBG("ai_addrlen = %u should be %u or %u",
			(unsigned int)addr->ai_addrlen,
			(unsigned int)sizeof(struct sockaddr_in),
			(unsigned int)sizeof(struct sockaddr_in6));

		addr = addr->ai_next;
		break;
	}

	freeaddrinfo(result);

	return err;
}

static int client_broker_init(struct mqtt_client *const client)
{
	int err;

	mqtt_client_init(client);

	err = broker_init();
	if (err) {
		return err;
	}

	client->broker			= &broker;
	client->evt_cb			= mqtt_evt_handler;
	client->client_id.utf8		= (char *)client_id_buf;
	client->client_id.size		= strlen(client_id_buf);
	client->password		= NULL;
	client->user_name		= NULL;
	client->protocol_version	= MQTT_VERSION_3_1_1;
	client->rx_buf			= rx_buffer;
	client->rx_buf_size		= sizeof(rx_buffer);
	client->tx_buf			= tx_buffer;
	client->tx_buf_size		= sizeof(tx_buffer);

#if defined(CONFIG_MQTT_BACKEND_TLS_ENABLE)
	client->transport.type		= MQTT_TRANSPORT_SECURE;

	static sec_tag_t sec_tag_list[] = { CONFIG_MQTT_BACKEND_SEC_TAG };
	struct mqtt_sec_config *tls_cfg = &(client->transport).tls.config;

	tls_cfg->peer_verify		= 2;
	tls_cfg->cipher_count		= 0;
	tls_cfg->cipher_list		= NULL;
	tls_cfg->sec_tag_count		= ARRAY_SIZE(sec_tag_list);
	tls_cfg->sec_tag_list		= sec_tag_list;
	tls_cfg->hostname		= CONFIG_MQTT_BACKEND_BROKER_HOST_NAME;
#else
	client->transport.type		= MQTT_TRANSPORT_NON_SECURE;
#endif
	return err;
}

static int connection_poll_start(void)
{
	if (atomic_get(&connection_poll_active)) {
		LOG_DBG("Connection poll in progress");
		return -EINPROGRESS;
	}

	atomic_set(&disconnect_requested, 0);
	k_sem_give(&connection_poll_sem);

	return 0;
}

int mqtt_backend_ping(void)
{
	return mqtt_ping(&client);
}

int mqtt_backend_keepalive_time_left(void)
{
	return (int)mqtt_keepalive_time_left(&client);
}

int mqtt_backend_input(void)
{
	return mqtt_input(&client);
}

int mqtt_backend_send(const struct mqtt_backend_tx_data *const tx_data)
{
	struct mqtt_backend_tx_data tx_data_pub = {
		.str	    = tx_data->str,
		.len	    = tx_data->len,
		.qos	    = tx_data->qos,
		.topic.type = tx_data->topic.type,
		.topic.str  = tx_data->topic.str,
		.topic.len  = tx_data->topic.len
	};

	struct mqtt_publish_param param;

	param.message.topic.qos		= tx_data_pub.qos;
	param.message.topic.topic.utf8	= tx_data_pub.topic.str;
	param.message.topic.topic.size	= tx_data_pub.topic.len;
	param.message.payload.data	= tx_data_pub.str;
	param.message.payload.len	= tx_data_pub.len;
	param.message_id		= sys_rand32_get();
	param.dup_flag			= 0;
	param.retain_flag		= 0;

	LOG_DBG("Publishing to topic: %s",
		log_strdup(param.message.topic.topic.utf8));

	return mqtt_publish(&client, &param);
}

int mqtt_backend_disconnect(void)
{
	return mqtt_disconnect(&client);
}

int mqtt_backend_connect(struct mqtt_backend_config *const config)
{
	int err;

	err = connection_poll_start();

	return err;
}

int mqtt_backend_init(const struct mqtt_backend_config *const config,
		 mqtt_backend_evt_handler_t event_handler)
{
	int err;
	err = snprintf(client_id_buf, sizeof(client_id_buf),
		       "nrf-%s", config->client_id);
	if (err <= 0) {
		return -EINVAL;
	}
	if (err >= sizeof(client_id_buf)) {
		return -ENOMEM;
	}
	
	

	module_evt_handler = event_handler;
	
	return 0;
}

int mqtt_backend_subscribe(const struct mqtt_backend_topic_data *const topic)
{
	struct mqtt_topic subscribe_topic = {
		.topic = {
			.utf8 = topic->str,
			.size = topic->len
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};

	const struct mqtt_subscription_list subscription_list = {
		.list = &subscribe_topic,
		.list_count = 1,
		.message_id = 1234
	};

	LOG_INF("Subscribing to: %s len %u", topic->str,
		topic->len);

	return mqtt_subscribe(&client, &subscription_list);

}

void mqtt_backend_poll(void)
{
	int err;
	struct pollfd fds[1];
	struct mqtt_backend_evt mqtt_backend_evt = {
		.type = MQTT_BACKEND_EVT_DISCONNECTED,
	};

start:
	k_sem_take(&connection_poll_sem, K_FOREVER);
	atomic_set(&connection_poll_active, 1);

	mqtt_backend_evt.type = MQTT_BACKEND_EVT_CONNECTING;
	mqtt_backend_notify_event(&mqtt_backend_evt);

	err = client_broker_init(&client);
	if (err) {
		LOG_ERR("client_broker_init, error: %d", err);
	}

	err = mqtt_connect(&client);
	if (err) {
		LOG_ERR("mqtt_connect, error: %d", err);
	}



	if (err != 0) {

		mqtt_backend_evt.type = MQTT_BACKEND_EVT_CONNECTING;
		mqtt_backend_notify_event(&mqtt_backend_evt);
		goto reset;
	} else {
		LOG_DBG("MQTT broker connection request sent.");
	}
	#if defined(CONFIG_MQTT_BACKEND_TLS_ENABLE)
	fds[0].fd = client.transport.tls.sock;
	#else
	fds[0].fd = client.transport.tcp.sock;
	#endif

	fds[0].events = POLLIN;

	mqtt_backend_evt.type = MQTT_BACKEND_EVT_DISCONNECTED;
	atomic_set(&mqtt_backend_disconnected, 0);

	while (true) {
		err = poll(fds, ARRAY_SIZE(fds), mqtt_backend_keepalive_time_left());

		/* If poll returns 0 the timeout has expired. */
		if (err == 0) {
			mqtt_backend_ping();
			continue;
		}

		if ((fds[0].revents & POLLIN) == POLLIN) {
			mqtt_backend_input();

			if (atomic_get(&mqtt_backend_disconnected) == 1) {
				LOG_DBG("The cloud socket is already closed.");
				break;
			}

			continue;
		}

		if (err < 0) {
			LOG_ERR("poll() returned an error: %d", err);
			break;
		}

		if ((fds[0].revents & POLLNVAL) == POLLNVAL) {
			LOG_DBG("Socket error: POLLNVAL");
			LOG_DBG("The cloud socket was unexpectedly closed.");
			break;
		}

		if ((fds[0].revents & POLLHUP) == POLLHUP) {
			LOG_DBG("Socket error: POLLHUP");
			LOG_DBG("Connection was closed by the cloud.");
			break;
		}

		if ((fds[0].revents & POLLERR) == POLLERR) {
			LOG_DBG("Socket error: POLLERR");
			LOG_DBG("Cloud connection was unexpectedly closed.");
			break;
		}
	}

	/* Upon a socket error, disconnect the client and notify the
	 * application. If the client has already been disconnected this has
	 * occurred via a MQTT DISCONNECT event and the application has
	 * already been notified.
	 */
	if (atomic_get(&mqtt_backend_disconnected) == 0) {
		mqtt_backend_notify_event(&mqtt_backend_evt);
		mqtt_backend_disconnect();
	}

reset:
	atomic_set(&connection_poll_active, 0);
	k_sem_take(&connection_poll_sem, K_NO_WAIT);
	goto start;
}

#ifdef CONFIG_BOARD_QEMU_X86
#define POLL_THREAD_STACK_SIZE 4096
#else
#define POLL_THREAD_STACK_SIZE 3072
#endif
K_THREAD_DEFINE(connection_poll_thread, POLL_THREAD_STACK_SIZE,
		mqtt_backend_poll, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
