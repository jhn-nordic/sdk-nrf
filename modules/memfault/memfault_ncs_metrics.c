/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <modem/lte_lc_trace.h>
#include <memfault/metrics/metrics.h>
#include <memfault/core/platform/overrides.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(memfault_ncs_metrics, CONFIG_MEMFAULT_NCS_LOG_LEVEL);

static struct k_work_delayable stack_check_work;

static void stack_check_cb(const struct k_thread *cthread, void *user_data)
{
	struct k_thread *thread = (struct k_thread *)cthread;
	char hexname[11];
	const char *name;
	size_t unused;
	int err;
	enum stack_type {
		STACK_NONE,
		STACK_AT_CMD,
		STACK_CONNECTION_POLL
	} stack_type;

	ARG_UNUSED(user_data);

	name = k_thread_name_get((k_tid_t)thread);
	if (!name || name[0] == '\0') {
		name = hexname;

		snprintk(hexname, sizeof(hexname), "%p", (void *)thread);
		LOG_DBG("No thread name registered for %s", name);
		return;
	}

	if (strncmp("at_cmd_socket_thread", name, sizeof("at_cmd_socket_thread")) == 0) {
		stack_type = STACK_AT_CMD;
	} else if (strncmp("connection_poll_thread", name,
			   sizeof("connection_poll_thread")) == 0) {
		stack_type = STACK_CONNECTION_POLL;
	} else {
		LOG_DBG("Not relevant stack: %s", name);
		return;
	}

	err = k_thread_stack_space_get(thread, &unused);
	if (err) {
		LOG_WRN(" %-20s: unable to get stack space (%d)", name, err);
		return;
	}

	if (stack_type == STACK_AT_CMD) {
		LOG_DBG("Unused at_cmd_socket_thread stack size: %d", unused);

		err = memfault_metrics_heartbeat_set_unsigned(
			MEMFAULT_METRICS_KEY(at_cmd_unused_stack), unused);
		if (err) {
			LOG_ERR("Failed to set at_cmd_unused_stack");
		}
	} else if (stack_type == STACK_CONNECTION_POLL) {
		LOG_DBG("Unused connection_poll_thread stack size: %d", unused);

		err = memfault_metrics_heartbeat_set_unsigned(
			MEMFAULT_METRICS_KEY(connection_poll_unused_stack), unused);
		if (err) {
			LOG_ERR("Failed to set connection_poll_unused_stack");
		}
	}
}

static void stack_check_work_fn(struct k_work *work)
{
	k_thread_foreach_unlocked(stack_check_cb, NULL);

	if (work) {
		k_work_reschedule(k_work_delayable_from_work(work),
			K_SECONDS(CONFIG_MEMFAULT_NCS_STACK_CHECK_INTERVAL));
	}
}

static void lte_trace_cb(enum lte_lc_trace_type type)
{
	int err;
	static bool connected;
	static bool connect_timer_started;

	LOG_DBG("LTE trace: %d", type);

	switch (type) {
	case LTE_LC_TRACE_FUNC_MODE_NORMAL:
	case LTE_LC_TRACE_FUNC_MODE_ACTIVATE_LTE:
		if (connect_timer_started) {
			break;
		}

		err = memfault_metrics_heartbeat_timer_start(
			MEMFAULT_METRICS_KEY(lte_time_to_connect));
		if (err) {
			LOG_WRN("LTE connection time tracking was not started, error: %d", err);
		} else {
			connect_timer_started = true;
		}

		break;
	case LTE_LC_TRACE_NW_REG_REGISTERED_HOME:
	case LTE_LC_TRACE_NW_REG_REGISTERED_ROAMING:
		connected = true;

		if (!connect_timer_started) {
			LOG_WRN("lte_time_to_connect was not started");
			break;
		}

		err = memfault_metrics_heartbeat_timer_stop(
			MEMFAULT_METRICS_KEY(lte_time_to_connect));
		if (err) {
			LOG_WRN("LTE connection time tracking was not stopped, error: %d", err);
		} else {
			LOG_DBG("lte_time_to_connect stopped");
			connect_timer_started = false;
		}

		break;
	case LTE_LC_TRACE_NW_REG_NOT_REGISTERED:
	case LTE_LC_TRACE_NW_REG_SEARCHING:
	case LTE_LC_TRACE_NW_REG_REGISTRATION_DENIED:
	case LTE_LC_TRACE_NW_REG_UNKNOWN:
	case LTE_LC_TRACE_NW_REG_UICC_FAIL:
		if (connected) {
			err = memfault_metrics_heartbeat_add(
				MEMFAULT_METRICS_KEY(lte_connection_loss_count), 1);
			if (err) {
				LOG_ERR("Failed to increment lte_connection_loss_count");
			}

			if (connect_timer_started) {
				break;
			}

			err = memfault_metrics_heartbeat_timer_start(
				MEMFAULT_METRICS_KEY(lte_time_to_connect));
			if (err) {
				LOG_WRN("LTE connection time tracking was not started, error: %d",
					err);
			} else {
				LOG_DBG("lte_time_to_connect started");
				connect_timer_started = true;
			}
		}

		connected = false;
		break;
	default:
		break;
	}
}

/* Overriding weak implementation in Memfault SDK.
 * The function is run on every heartbeat and can be used to get fresh updates
 * of metrics.
 */
void memfault_metrics_heartbeat_collect_data(void)
{
	stack_check_work_fn(NULL);
}

void memfault_ncs_metrcics_init(void)
{
	if (IS_ENABLED(CONFIG_MEMFAULT_NCS_STACK_METRICS)) {
		k_work_init_delayable(&stack_check_work, stack_check_work_fn);
		k_work_schedule(&stack_check_work, K_NO_WAIT);
	}

	if (IS_ENABLED(CONFIG_MEMFAULT_NCS_LTE_METRICS)) {
		lte_lc_trace_handler_set(lte_trace_cb);
	}
}
