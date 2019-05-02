/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef ZEPHYR_INCLUDE_CLOUD_H_
#define ZEPHYR_INCLUDE_CLOUD_H_

/* Cloud API */

#include <zephyr.h>

/**@brief Cloud backend states. */
enum cloud_state {
	CLOUD_STATE_DISCONNECTED,
	CLOUD_STATE_DISCONNECTING,
	CLOUD_STATE_CONNECTED,
	CLOUD_STATE_CONNECTING,
	CLOUD_STATE_BUSY,
	CLOUD_STATE_ERROR,
	CLOUD_STATE_COUNT
};

/**@brief Cloud events that can be notified asynchronously by the backend. */
enum cloud_event_type {
	CLOUD_EVT_CONNECTED,
	CLOUD_EVT_DISCONNECTED,
	CLOUD_EVT_READY,
	CLOUD_EVT_ERROR,
	CLOUD_EVT_DATA_SENT,
	CLOUD_EVT_DATA_RECEIVED,
	CLOUD_EVT_COUNT
};

/**@brief Quality of Service for message sent by a cloud backend. */
enum cloud_qos {
	CLOUD_QOS_AT_MOST_ONCE,
	CLOUD_QOS_AT_LEAST_ONCE,
	CLOUD_QOS_EXACTLY_ONCE,
	CLOUD_QOS_COUNT
};

/**@brief Quality of Service for message sent by a cloud backend. */
enum cloud_resource {
	CLOUD_EP_TOPIC,
	CLOUD_EP_URI,
	CLOUD_EP_COUNT
};

/* Forward declaration of cloud backend type. */
struct cloud_backend;

/**@brief Cloud message type. */
struct cloud_msg {
	char *payload;
	size_t len;
	enum cloud_qos qos;
	struct {
		enum cloud_resource type;
		char *str;
		size_t len;
	} resource;
};

/**@brief Cloud event type. */
struct cloud_event {
	enum cloud_event_type type;
	union {
		struct cloud_msg msg;
		int err;
	} data;
};

/**
 * @brief Cloud event hander function type.
 *
 * @param[in] backend Pointer to cloud backend.
 * @param[in] evt Pointer to cloud event.
 */
typedef void (*cloud_evt_handler_t)(const struct cloud_backend *const backend,
				    const struct cloud_event *const evt);

/**
 * @brief Cloud backend API.
 *
 * All cloud backends must expose this API.
 */
struct cloud_api {
	int (*init)(struct cloud_backend *const backend,
		    cloud_evt_handler_t handler);
        int (*uninit)(const struct cloud_backend *const backend);
	int (*connect)(const struct cloud_backend *const backend);
	int (*disconnect)(const struct cloud_backend *const backend);
	int (*send)(const struct cloud_backend *const backend,
			 const struct cloud_msg *const msg);
	int (*ping)(const struct cloud_backend *const backend);
	int (*input)(const struct cloud_backend *const backend);
};

/**@brief Structure for cloud backend configuration. */
struct cloud_backend_config {
	char *name;
	cloud_evt_handler_t handler;
	int socket;
};

/**@brief Structure for cloud backend. */
struct cloud_backend {
	struct cloud_api *api;
	struct cloud_backend_config *config;
};

/**@brief API to initialize a cloud backend. Performs all necessary
 * 	  configuration of the backend to be able to connect its online
 * 	  counterpart.
 *
 * param[in] cloud Pointer to cloud backend structure.
 * param[in] ctx Pointer to backend specific data.
 */
static inline int cloud_init(
	struct cloud_backend *const backend,
	cloud_evt_handler_t handler)
{
	if (backend == NULL) {
		return -EINVAL;
	}

	if (handler == NULL) {
		return -EINVAL;
	}

	return backend->api->init(backend, handler);
}

/**@brief API to uninitialize a cloud backend. Gracefully disconnects
 *        remote endpoint and releases memory.
 *
 * param[in] cloud Pointer to cloud backend structure.
 */
static inline int cloud_uninit(const struct cloud_backend *const backend)
{
        __ASSERT_NO_MSG(backend != NULL);

        return backend->api->uninit(backend);
}
/**@brief Request connection to cloud backend.
 *
 * @details The backend is required to expose the socket in use when this
 * 	    function returns. The socket should then be available through
 * 	    backend->config->socket and the application may start listening
 * 	    for events on it.
 *
 * param[in] cloud Pointer to a cloud backend structure.
 *
 * @return 0 or a negative error code indicating reason of failure.
 */
static inline int cloud_connect(const struct cloud_backend *const backend)
{
	__ASSERT_NO_MSG(backend != NULL);

	return backend->api->connect(backend);
}

/**@brief API to disconnect from cloud backend.
 *
 * param[in] cloud Pointer to a cloud backend structure.
 *
 * @return 0 or a negative error code indicating reason of failure.
 */
static inline int cloud_disconnect(const struct cloud_backend *const backend)
{
	__ASSERT_NO_MSG(backend != NULL);

	return backend->api->disconnect(backend);
}

/**@brief API to send data to cloud.
 *
 * param[in] cloud Pointer to a cloud backend structure.
 * param[in] msg Pointer to cloud message structure.
 *
 * @return 0 or a negative error code indicating reason of failure.
 */
static inline int cloud_send(const struct cloud_backend *const backend,
		    struct cloud_msg *msg)
{
	__ASSERT_NO_MSG(backend != NULL);
	__ASSERT_NO_MSG(msg != NULL);

	return backend->api->send(backend, msg);
}

/**
 * @brief Optional API to ping the cloud's remote endpoint periodically.
 *
 * @param[in] backend Pointer to cloud backend.
 *
 * @return 0 or a negative error code indicating reason of failure.
 */
static inline int cloud_ping(const struct cloud_backend *const backend)
{
	__ASSERT_NO_MSG(backend != NULL);

	/* Ping will only be sent if the backend requires it. */
	if(backend->api->ping != NULL) {
		return backend->api->ping(backend);
	}

	return 0;
}

/**
 * @brief Process incoming data to backend.
 *
 * @note This is a non-blocking call.
 *
 * @param[in] backend Pointer to cloud backend.
 *
 * @return 0 or a negative error code indicating reason of failure.
 */
static inline int cloud_input(const struct cloud_backend *const backend)
{
	__ASSERT_NO_MSG(backend != NULL);

	return backend->api->input(backend);
}

struct cloud_backend *cloud_get_binding(const char *name);

#define CLOUD_BACKEND_DEFINE(_name, _api)			       \
									       \
	static struct cloud_backend_config UTIL_CAT(_name, _config) = {		\
		.name = STRINGIFY(_name)				       \
	};								       \
									       \
	static const struct cloud_backend _name				       \
	__attribute__ ((section(".cloud_backends"))) __attribute__((used)) =   \
	{								       \
		.api = &_api,						       \
		.config = &UTIL_CAT(_name, _config)				       \
	};


#endif /* ZEPHYR_INCLUDE_CLOUD_H_ */
