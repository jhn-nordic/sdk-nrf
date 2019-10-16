/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <device.h>
#include <uart.h>
#include <nrfx.h>
#include <string.h>
#include <hal/nrf_power.h>
#include <power/reboot.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>

#include <bluetooth/services/nus.h>

#define STACKSIZE               1024
#define PRIORITY                7

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	        (sizeof(DEVICE_NAME) - 1)

static K_SEM_DEFINE(ble_init_ok, 0, 1);

static struct bt_conn *current_conn;
static struct bt_conn *auth_conn;

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, NUS_UUID_SERVICE),
};

/* Overriding weak function to set iSerial runtime. */
u8_t *usb_update_sn_string_descriptor(void)
{
	static u8_t buf[] = "PCA20035_12PLACEHLDRS";

	snprintk(&buf[9], 13, "%04X%08X",
		(uint32_t)(NRF_FICR->DEVICEADDR[1] & 0x0000FFFF)|0x0000C000,
		(uint32_t)NRF_FICR->DEVICEADDR[0]);

	return (u8_t *)&buf;
}

#define POWER_THREAD_STACKSIZE		CONFIG_IDLE_STACK_SIZE
#define POWER_THREAD_PRIORITY		K_LOWEST_APPLICATION_THREAD_PRIO

/* Heap block space is always one of 2^(2n) for n from 3 to 7.
 * (see reference/kernel/memory/heap.html for more info)
 * Here, we target 64-byte blocks. Since we want to fit one struct uart_data
 * into each block, the block layout becomes:
 * 16 bytes: reserved by the Zephyr heap block descriptor (not in struct)
 *  4 bytes: reserved by the Zephyr FIFO (in struct)
 * 40 bytes: UART data buffer (in struct)
 *  4 bytes: length field (in struct, padded for alignment)
 */
#define UART_BUF_SIZE 20

static K_FIFO_DEFINE(usb_0_tx_fifo);
static K_FIFO_DEFINE(usb_1_tx_fifo);
static K_FIFO_DEFINE(uart_0_tx_fifo);
static K_FIFO_DEFINE(uart_1_tx_fifo);

struct uart_data {
	void *fifo_reserved;
	u8_t buffer[UART_BUF_SIZE];
	u16_t len;
};

static struct serial_dev {
	struct device *dev;
	void *peer;
	struct k_fifo *fifo;
	struct k_sem sem;
	struct uart_data *rx;
} devs[4];

/* Frees data for incoming transmission on device blocked by full heap. */
static int oom_free(struct serial_dev *sd)
{
	struct serial_dev *peer_sd = (struct serial_dev *)sd->peer;
	struct uart_data *buf;

	/* First, try to free from FIFO of peer device (blocked stream) */
	buf = k_fifo_get(peer_sd->fifo, K_NO_WAIT);
	if (buf) {
		k_free(buf);
		return 0;
	}

	/* Then, try FIFO of the receiving device (reverse of blocked stream) */
	buf = k_fifo_get(sd->fifo, K_NO_WAIT);
	if (buf) {
		k_free(buf);
		return 0;
	}

	/* Finally, try all of them */
	for (int i = 0; i < ARRAY_SIZE(devs); i++) {
		buf = k_fifo_get(sd->fifo, K_NO_WAIT);
		if (buf) {
			k_free(buf);
			return 0;
		}
	}

	return -1; /* Was not able to free any heap memory */
}

static void uart_interrupt_handler(void *user_data)
{
	struct serial_dev *sd = user_data;
	struct device *dev = sd->dev;
	struct serial_dev *peer_sd = (struct serial_dev *)sd->peer;

	uart_irq_update(dev);

	while (uart_irq_rx_ready(dev)) {
		int data_length;

		while (!sd->rx) {
			sd->rx = k_malloc(sizeof(*sd->rx));
			if (sd->rx) {
				sd->rx->len = 0;
			} else {
				int err = oom_free(sd);

				if (err) {
					printk("Could not free memory. Rebooting.\n");
					sys_reboot(SYS_REBOOT_COLD);
				}
			}
		}

		data_length = uart_fifo_read(dev, &sd->rx->buffer[sd->rx->len],
					   UART_BUF_SIZE - sd->rx->len);
		sd->rx->len += data_length;

		if (sd->rx->len > 0) {
			if ((sd->rx->len == UART_BUF_SIZE) ||
			   (sd->rx->buffer[sd->rx->len - 1] == '\n') ||
			   (sd->rx->buffer[sd->rx->len - 1] == '\r') ||
			   (sd->rx->buffer[sd->rx->len - 1] == '\0')) {
				k_fifo_put(peer_sd->fifo, sd->rx);
				k_fifo_put(&fifo_uart_rx_data, sd->rx);
				k_sem_give(&peer_sd->sem);

				sd->rx = NULL;
			}
		}
	}

	if (uart_irq_tx_ready(dev)) {
		struct uart_data *buf = k_fifo_get(sd->fifo, K_NO_WAIT);
		u16_t written = 0;

		/* Nothing in the FIFO, nothing to send */
		if (!buf) {
			uart_irq_tx_disable(dev);
			return;
		}

		while (buf->len > written) {
			written += uart_fifo_fill(dev,
						  &buf->buffer[written],
						  buf->len - written);
		}

		while (!uart_irq_tx_complete(dev)) {
			/* Wait for the last byte to get
			 * shifted out of the module
			 */
		}

		if (k_fifo_is_empty(sd->fifo)) {
			uart_irq_tx_disable(dev);
		}

		k_free(buf);
	}
}

void power_thread(void)
{
	while (1) {
		if (!nrf_power_usbregstatus_vbusdet_get()) {
		
//	nrf_power_system_off();
		}
		k_sleep(100);
	}
}

static void connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
		return;
	}

	printk("Connected\n");
	current_conn = bt_conn_ref(conn);


}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);

	if (auth_conn) {
		bt_conn_unref(auth_conn);
		auth_conn = NULL;
	}

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
}

static struct bt_conn_auth_cb conn_auth_callbacks;
static void bt_receive_cb(struct bt_conn *conn, const u8_t *const data,
			  u16_t len)
{
	char addr[BT_ADDR_LE_STR_LEN] = {0};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

	printk("Received data from: %s\n", addr);

}

static struct bt_conn_cb conn_callbacks = {
	.connected    = connected,
	.disconnected = disconnected,

};
static struct bt_gatt_nus_cb nus_cb = {
	.received_cb = bt_receive_cb,
};

static void bt_ready(int err)
{
	if (err) {
		printk("BLE init failed with error code %d\n", err);
		return;
	}

	err = bt_gatt_nus_init(&nus_cb);
	if (err) {
		printk("Failed to initialize UART service (err: %d)\n", err);
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd,
			      ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
	}

	/* Give two semaphores to signal both the led_blink_thread, and
	 * and the ble_write_thread that ble initialized successfully
	 */
	k_sem_give(&ble_init_ok);
}

void main(void)
{
	int ret;
	struct serial_dev *usb_0_sd = &devs[0];
	struct serial_dev *usb_1_sd = &devs[1];
	struct serial_dev *uart_0_sd = &devs[2];
	struct serial_dev *uart_1_sd = &devs[3];
	struct device *usb_0_dev, *usb_1_dev, *uart_0_dev, *uart_1_dev;

	usb_0_dev = device_get_binding("CDC_ACM_0");
	if (!usb_0_dev) {
		printk("CDC ACM device not found\n");
		return;
	}

	usb_1_dev = device_get_binding("CDC_ACM_1");
	if (!usb_1_dev) {
		printk("CDC ACM device not found\n");
		return;
	}

	uart_0_dev = device_get_binding("UART_0");
	if (!uart_0_dev) {
		printk("UART 0 init failed\n");
	}

	uart_1_dev = device_get_binding("UART_1");
	if (!uart_1_dev) {
		printk("UART 1 init failed\n");
	}

	usb_0_sd->dev = usb_0_dev;
	usb_0_sd->fifo = &usb_0_tx_fifo;
	usb_0_sd->peer = uart_0_sd;

	usb_1_sd->dev = usb_1_dev;
	usb_1_sd->fifo = &usb_1_tx_fifo;
	usb_1_sd->peer = uart_1_sd;

	uart_0_sd->dev = uart_0_dev;
	uart_0_sd->fifo = &uart_0_tx_fifo;
	uart_0_sd->peer = usb_0_sd;

	uart_1_sd->dev = uart_1_dev;
	uart_1_sd->fifo = &uart_1_tx_fifo;
	uart_1_sd->peer = usb_1_sd;

	k_sem_init(&usb_0_sd->sem, 0, 1);
	k_sem_init(&usb_1_sd->sem, 0, 1);
	k_sem_init(&uart_0_sd->sem, 0, 1);
	k_sem_init(&uart_1_sd->sem, 0, 1);

	uart_irq_callback_user_data_set(usb_0_dev, uart_interrupt_handler,
		usb_0_sd);
	uart_irq_callback_user_data_set(usb_1_dev, uart_interrupt_handler,
		usb_1_sd);
	uart_irq_callback_user_data_set(uart_0_dev, uart_interrupt_handler,
		uart_0_sd);
	uart_irq_callback_user_data_set(uart_1_dev, uart_interrupt_handler,
		uart_1_sd);

	uart_irq_rx_enable(usb_0_dev);
	uart_irq_rx_enable(usb_1_dev);
	uart_irq_rx_enable(uart_0_dev);
	uart_irq_rx_enable(uart_1_dev);

	printk("USB <--> UART bridge is now initialized\n");

	struct k_poll_event events[4] = {
		K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
						K_POLL_MODE_NOTIFY_ONLY,
						&usb_0_sd->sem, 0),
		K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
						K_POLL_MODE_NOTIFY_ONLY,
						&usb_1_sd->sem, 0),
		K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
						K_POLL_MODE_NOTIFY_ONLY,
						&uart_0_sd->sem, 0),
		K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
						K_POLL_MODE_NOTIFY_ONLY,
						&uart_1_sd->sem, 0),
	};

	ret = bt_enable(bt_ready);

	if (!ret) {

		bt_conn_cb_register(&conn_callbacks);

		if (IS_ENABLED(CONFIG_BT_GATT_NUS_SECURITY_ENABLED)) {
			bt_conn_auth_cb_register(&conn_auth_callbacks);
		}

		ret = k_sem_take(&ble_init_ok, K_MSEC(100));

		if (!ret) {
			printk("Bluetooth initialized\n");
		} else {
			printk("BLE initialization \
				did not complete in time\n");
		}
	}

	while (1) {
		ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);
		if (ret != 0) {
			continue;
		}

		if (events[0].state == K_POLL_TYPE_SEM_AVAILABLE) {
			events[0].state = K_POLL_STATE_NOT_READY;
			k_sem_take(&usb_0_sd->sem, K_NO_WAIT);
			uart_irq_tx_enable(usb_0_dev);
		} else if (events[1].state == K_POLL_TYPE_SEM_AVAILABLE) {
			events[1].state = K_POLL_STATE_NOT_READY;
			k_sem_take(&usb_1_sd->sem, K_NO_WAIT);
			uart_irq_tx_enable(usb_1_dev);
		} else if (events[2].state == K_POLL_TYPE_SEM_AVAILABLE) {
			events[2].state = K_POLL_STATE_NOT_READY;
			k_sem_take(&uart_0_sd->sem, K_NO_WAIT);
			uart_irq_tx_enable(uart_0_dev);
		} else if (events[3].state == K_POLL_TYPE_SEM_AVAILABLE) {
			events[3].state = K_POLL_STATE_NOT_READY;
			k_sem_take(&uart_1_sd->sem, K_NO_WAIT);
			uart_irq_tx_enable(uart_1_dev);
		}
	}
}

K_THREAD_DEFINE(power_thread_id, POWER_THREAD_STACKSIZE, power_thread,
		NULL, NULL, NULL, POWER_THREAD_PRIORITY, 0, K_NO_WAIT);


void ble_write_thread(void)
{
	/* Don't go any further until BLE is initailized */
	k_sem_take(&ble_init_ok, K_FOREVER);

	for (;;) {
		/* Wait indefinitely for data to be sent over bluetooth */
		struct uart_data *buf = k_fifo_get(&fifo_uart_rx_data,
						     K_FOREVER);

		if (bt_gatt_nus_send(NULL, buf->buffer, buf->len)) {
			printk("Failed to send data over BLE connection\n");
		}
		k_free(buf);
	}
}
K_THREAD_DEFINE(ble_write_thread_id, STACKSIZE, ble_write_thread, NULL, NULL,
		NULL, PRIORITY, 0, K_NO_WAIT);
