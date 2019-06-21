/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <kernel.h>
#include <sensor.h>
#include <device.h>
#include <spi.h>
#include <gpio.h>
#include <misc/byteorder.h>
#include <sensor/pmw3360.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(paw3212, CONFIG_PAW3212_LOG_LEVEL);


/* Timings defined by spec */
#define T_NCS_SCLK	1			/* 120 ns */
#define T_SRX		(20 - T_NCS_SCLK)	/* 20 us */
#define T_SCLK_NCS_WR	(35 - T_NCS_SCLK)	/* 35 us */
#define T_SWX		(180 - T_SCLK_NCS_WR)	/* 180 us */
#define T_SRAD		160			/* 160 us */
#define T_SRAD_MOTBR	35			/* 35 us */
#define T_BEXIT		1			/* 500 ns */

/* Timing defined on SROM download burst mode figure */
#define T_BRSEP		15			/* 15 us */


/* Sensor registers */
#define PAW3212_REG_PRODUCT_ID			0x00
#define PAW3212_REG_REVISION_ID			0x01
#define PAW3212_REG_MOTION			0x02
#define PAW3212_REG_DELTA_X_L			0x03

#define PAW3212_REG_DELTA_Y_L			0x04

#define PAW3212_REG_POWER_UP_RESET		0x06


/* Sensor identification values */
#define PAW3212_PRODUCT_ID			0x30




#define SPI_WRITE_BIT				BIT(7)


extern const size_t paw3212_firmware_length;
extern const u8_t paw3212_firmware_data[];


enum async_init_step {
	ASYNC_INIT_STEP_POWER_UP,
	ASYNC_INIT_STEP_CONFIGURE,

	ASYNC_INIT_STEP_COUNT
};

struct paw3212_data {
	struct device                *cs_gpio_dev;
	struct device                *irq_gpio_dev;
	struct device                *spi_dev;
	struct gpio_callback         irq_gpio_cb;
	struct k_spinlock            lock;
	s16_t                        x;
	s16_t                        y;
	sensor_trigger_handler_t     data_ready_handler;
	struct k_delayed_work        init_work;
	enum async_init_step         async_init_step;
	int                          err;
	bool                         ready;
	bool                         last_read_burst;
};

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB |
		     SPI_MODE_CPOL | SPI_MODE_CPHA,
	.frequency = CONFIG_DESKTOP_SPI_FREQ_HZ,
	.slave = 0,
};

static const s32_t async_init_delay[ASYNC_INIT_STEP_COUNT] = {
	[ASYNC_INIT_STEP_POWER_UP]         = 1,
	[ASYNC_INIT_STEP_CONFIGURE]        = 0,
};


static int paw3212_async_init_power_up(struct paw3212_data *dev_data);
static int paw3212_async_init_configure(struct paw3212_data *dev_data);


static int (* const async_init_fn[ASYNC_INIT_STEP_COUNT])(struct paw3212_data *dev_data) = {
	[ASYNC_INIT_STEP_POWER_UP] = paw3212_async_init_power_up,
	[ASYNC_INIT_STEP_CONFIGURE] = paw3212_async_init_configure,
};


static struct paw3212_data paw3212_data;
DEVICE_DECLARE(paw3212);


static int spi_cs_ctrl(struct paw3212_data *dev_data, bool enable)
{
	u32_t val = (enable) ? (0) : (1);
	int err;

	if (!enable) {
		k_busy_wait(T_NCS_SCLK);
	}

	err = gpio_pin_write(dev_data->cs_gpio_dev, CONFIG_PAW3212_CS_GPIO_PIN,
			     val);
	if (err) {
		LOG_ERR("SPI CS ctrl failed");
	}

	if (enable) {
		k_busy_wait(T_NCS_SCLK);
	}

	return err;
}

static int reg_read(struct paw3212_data *dev_data, u8_t reg, u8_t *buf)
{
	int err;

	__ASSERT_NO_MSG((reg & SPI_WRITE_BIT) == 0);

	err = spi_cs_ctrl(dev_data, true);
	if (err) {
		return err;
	}

	/* Write register address. */
	const struct spi_buf tx_buf = {
		.buf = &reg,
		.len = 1
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	err = spi_write(dev_data->spi_dev, &spi_cfg, &tx);
	if (err) {
		LOG_ERR("Reg read failed on SPI write");
		return err;
	}

	k_busy_wait(T_SRAD);

	/* Read register value. */
	struct spi_buf rx_buf = {
		.buf = buf,
		.len = 1,
	};
	const struct spi_buf_set rx = {
		.buffers = &rx_buf,
		.count = 1,
	};

	err = spi_read(dev_data->spi_dev, &spi_cfg, &rx);
	if (err) {
		LOG_ERR("Reg read failed on SPI read");
		return err;
	}

	err = spi_cs_ctrl(dev_data, false);
	if (err) {
		return err;
	}

	k_busy_wait(T_SRX);

	dev_data->last_read_burst = false;

	return 0;
}

static int reg_write(struct paw3212_data *dev_data, u8_t reg, u8_t val)
{
	int err;

	__ASSERT_NO_MSG((reg & SPI_WRITE_BIT) == 0);

	err = spi_cs_ctrl(dev_data, true);
	if (err) {
		return err;
	}

	u8_t buf[] = {
		SPI_WRITE_BIT | reg,
		val
	};
	const struct spi_buf tx_buf = {
		.buf = buf,
		.len = ARRAY_SIZE(buf)
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};

	err = spi_write(dev_data->spi_dev, &spi_cfg, &tx);
	if (err) {
		LOG_ERR("Reg write failed on SPI write");
		return err;
	}

	k_busy_wait(T_SCLK_NCS_WR);

	err = spi_cs_ctrl(dev_data, false);
	if (err) {
		return err;
	}

	k_busy_wait(T_SWX);

	dev_data->last_read_burst = false;

	return 0;
}




static void irq_handler(struct device *gpiob, struct gpio_callback *cb,
			u32_t pins)
{
	sensor_trigger_handler_t handler;

	k_spinlock_key_t key = k_spin_lock(&paw3212_data.lock);
	handler = paw3212_data.data_ready_handler;
	k_spin_unlock(&paw3212_data.lock, key);

	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};
	handler(DEVICE_GET(paw3212), &trig);
}

static int paw3212_async_init_power_up(struct paw3212_data *dev_data)
{
	/* Reset sensor */

	return reg_write(dev_data, PAW3212_REG_POWER_UP_RESET, 0x80);
}

static int paw3212_async_init_configure(struct paw3212_data *dev_data)
{
	int err;

	u8_t product_id;
	err = reg_read(dev_data, PAW3212_REG_PRODUCT_ID, &product_id);
	if (err) {
		LOG_ERR("Cannot obtain product id");
		return err;
	}
	LOG_ERR("product id %d", product_id);
	if (product_id != PAW3212_PRODUCT_ID) {
		LOG_ERR("Invalid product id!");
		return -EIO;
	}

	return err;
}

static void paw3212_async_init(struct k_work *work)
{
	struct paw3212_data *dev_data = &paw3212_data;

	ARG_UNUSED(work);

	LOG_DBG("PAW3212 async init step %d", dev_data->async_init_step);

	dev_data->err = async_init_fn[dev_data->async_init_step](dev_data);
	if (dev_data->err) {
		LOG_ERR("PAW3212 initialization failed");
	} else {
		dev_data->async_init_step++;

		if (dev_data->async_init_step == ASYNC_INIT_STEP_COUNT) {
			dev_data->ready = true;
			LOG_INF("PAW3212 initialized");
		} else {
			k_delayed_work_submit(&dev_data->init_work,
					      async_init_delay[dev_data->async_init_step]);
		}
	}
}

static int paw3212_init_cs(struct paw3212_data *dev_data)
{
	int err;

	dev_data->cs_gpio_dev =
		device_get_binding(CONFIG_PAW3212_CS_GPIO_DEV_NAME);
	if (!dev_data->cs_gpio_dev) {
		LOG_ERR("Cannot get CS GPIO device");
		return -ENXIO;
	}

	err = gpio_pin_configure(dev_data->cs_gpio_dev,
				 CONFIG_PAW3212_CS_GPIO_PIN,
				 GPIO_DIR_OUT);
	if (!err) {
		err = spi_cs_ctrl(dev_data, false);
	} else {
		LOG_ERR("Cannot configure CS PIN");
	}

	return err;
}

static int paw3212_init_irq(struct paw3212_data *dev_data)
{
	int err;

	dev_data->irq_gpio_dev =
		device_get_binding(CONFIG_PAW3212_IRQ_GPIO_DEV_NAME);
	if (!dev_data->irq_gpio_dev) {
		LOG_ERR("Cannot get IRQ GPIO device");
		return -ENXIO;
	}

	err = gpio_pin_configure(dev_data->irq_gpio_dev,
				 CONFIG_PAW3212_IRQ_GPIO_PIN,
				 GPIO_DIR_IN | GPIO_INT | GPIO_PUD_PULL_UP |
				 GPIO_INT_LEVEL | GPIO_INT_ACTIVE_LOW);
	if (err) {
		LOG_ERR("Cannot configure IRQ GPIO");
		return err;
	}

	gpio_init_callback(&dev_data->irq_gpio_cb, irq_handler,
			   BIT(CONFIG_PAW3212_IRQ_GPIO_PIN));

	err = gpio_add_callback(dev_data->irq_gpio_dev, &dev_data->irq_gpio_cb);
	if (err) {
		LOG_ERR("Cannot add IRQ GPIO callback");
	}

	return err;
}

static int paw3212_init_spi(struct paw3212_data *dev_data)
{
	dev_data->spi_dev = device_get_binding(CONFIG_PAW3212_SPI_DEV_NAME);
	if (!dev_data->spi_dev) {
		LOG_ERR("Cannot get SPI device");
		return -ENXIO;
	}

	return 0;
}

static int paw3212_init(struct device *dev)
{
	struct paw3212_data *dev_data = &paw3212_data;
	int err;

	ARG_UNUSED(dev);

	err = paw3212_init_cs(dev_data);
	if (err) {
		return err;
	}

	err = paw3212_init_irq(dev_data);
	if (err) {
		return err;
	}

	err = paw3212_init_spi(dev_data);
	if (err) {
		return err;
	}

	k_delayed_work_init(&dev_data->init_work, paw3212_async_init);

	k_delayed_work_submit(&dev_data->init_work,
			      async_init_delay[dev_data->async_init_step]);

	return err;
}

static int paw3212_sample_fetch(struct device *dev, enum sensor_channel chan)
{
	struct paw3212_data *dev_data = &paw3212_data;


	ARG_UNUSED(dev);

	if (unlikely(!dev_data->ready)) {
		LOG_INF("Device is not initialized yet");
		return -EBUSY;
	}
	int err;

	u8_t motion;
	err = reg_read(dev_data, PAW3212_REG_MOTION, &motion);
	if (err) {
		LOG_ERR("Cannot read X delta");
		return err;
	}

	u8_t x_sensor;
	err = reg_read(dev_data, PAW3212_REG_DELTA_X_L, &x_sensor);
	if (err) {
		LOG_ERR("Cannot read X delta");
		return err;
	}

	u8_t y_sensor;
	err = reg_read(dev_data, PAW3212_REG_DELTA_Y_L, &y_sensor);
	if (err) {
		LOG_ERR("Cannot read Y delta");
		return err;
	}
		LOG_ERR("I am here %d %d", x_sensor, y_sensor);
	if (!err) {
		s32_t x = (s8_t)x_sensor;
		s32_t y = (s8_t)y_sensor;

	
		dev_data->x = -x;
		dev_data->y = y;
	
	}

	return err;
}

static int paw3212_channel_get(struct device *dev, enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct paw3212_data *dev_data = &paw3212_data;

	ARG_UNUSED(dev);

	if (unlikely(!dev_data->ready)) {
		LOG_INF("Device is not initialized yet");
		return -EBUSY;
	}

	switch (chan) {
	case SENSOR_CHAN_POS_DX:
		val->val1 = dev_data->x;
		val->val2 = 0;
		break;

	case SENSOR_CHAN_POS_DY:
		val->val1 = dev_data->y;
		val->val2 = 0;
		break;

	default:
		return -ENOTSUP;
	}

	return 0;
}

static int paw3212_trigger_set(struct device *dev,
			       const struct sensor_trigger *trig,
			       sensor_trigger_handler_t handler)
{
	struct paw3212_data *dev_data = &paw3212_data;
	int err;

	ARG_UNUSED(dev);

	if (unlikely(trig->type != SENSOR_TRIG_DATA_READY)) {
		return -ENOTSUP;
	}

	if (unlikely(trig->chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	if (unlikely(!dev_data->ready)) {
		LOG_INF("Device is not initialized yet");
		return -EBUSY;
	}

	k_spinlock_key_t key = k_spin_lock(&dev_data->lock);

	if (handler) {
		err = gpio_pin_enable_callback(dev_data->irq_gpio_dev,
					       CONFIG_PAW3212_IRQ_GPIO_PIN);
	} else {
		err = gpio_pin_disable_callback(dev_data->irq_gpio_dev,
						CONFIG_PAW3212_IRQ_GPIO_PIN);
	}

	if (!err) {
		dev_data->data_ready_handler = handler;
	}

	k_spin_unlock(&dev_data->lock, key);

	return err;
}

static int paw3212_attr_set(struct device *dev, enum sensor_channel chan,
			    enum sensor_attribute attr,
			    const struct sensor_value *val)
{
	struct paw3212_data *dev_data = &paw3212_data;
	int err;

	ARG_UNUSED(dev);

	if (unlikely(chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	if (unlikely(!dev_data->ready)) {
		LOG_INF("Device is not initialized yet");
		return -EBUSY;
	}

	switch ((u32_t)attr) {


	default:
		LOG_ERR("Unknown attribute");
		return -ENOTSUP;
	}

	return err;
}

static const struct sensor_driver_api paw3212_driver_api = {
	.sample_fetch = paw3212_sample_fetch,
	.channel_get  = paw3212_channel_get,
	.trigger_set  = paw3212_trigger_set,
	.attr_set     = paw3212_attr_set,
};

DEVICE_AND_API_INIT(paw3212, CONFIG_PAW3212_DEV_NAME, paw3212_init, NULL, NULL,
		    POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &paw3212_driver_api);
