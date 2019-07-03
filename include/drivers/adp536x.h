/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef ADP536X_H_
#define ADP536X_H_

/**@file adp536x.h
 *
 * @brief Driver for ADP536X.
 * @defgroup adp536x Driver for ADP536X
 * @{
 */

#include <zephyr.h>

/* Definition of VBUS current limit values. */
#define ADP536X_VBUS_ILIM_50mA		0x00
#define ADP536X_VBUS_ILIM_100mA		0x01
#define ADP536X_VBUS_ILIM_150mA		0x02
#define ADP536X_VBUS_ILIM_200mA		0x03
#define ADP536X_VBUS_ILIM_250mA		0x04
#define ADP536X_VBUS_ILIM_300mA		0x05
#define ADP536X_VBUS_ILIM_400mA		0x06
#define ADP536X_VBUS_ILIM_500mA		0x07

/* Definition of charging current values. */
#define ADP536X_CHG_CURRENT_10mA        0x00
#define ADP536X_CHG_CURRENT_50mA        0x04
#define ADP536X_CHG_CURRENT_100mA	0x09
#define ADP536X_CHG_CURRENT_150mA	0x0E
#define ADP536X_CHG_CURRENT_200mA	0x13
#define ADP536X_CHG_CURRENT_250mA	0x18
#define ADP536X_CHG_CURRENT_300mA	0x1D
#define ADP536X_CHG_CURRENT_320mA	0x1F

/* Definition of overcharge protection threshold values. */
#define ADP536X_OC_CHG_THRESHOLD_25mA	0x00
#define ADP536X_OC_CHG_THRESHOLD_50mA	0x01
#define ADP536X_OC_CHG_THRESHOLD_100mA	0x02
#define ADP536X_OC_CHG_THRESHOLD_150mA	0x03
#define ADP536X_OC_CHG_THRESHOLD_200mA	0x04
#define ADP536X_OC_CHG_THRESHOLD_250mA	0x05
#define ADP536X_OC_CHG_THRESHOLD_300mA	0x06
#define ADP536X_OC_CHG_THRESHOLD_400mA	0x07

/* Definition of low state of charge thresholds, in percent. */
#define ADP536X_SOC_LOW_THRESHOLD_6	0x00
#define ADP536X_SOC_LOW_THRESHOLD_11	0x01
#define ADP536X_SOC_LOW_THRESHOLD_21	0x02
#define ADP536X_SOC_LOW_THRESHOLD_31	0x03

/* Definition of charger states */
#define ADP536X_CHG_STAT_OFF		0x00
#define ADP536X_CHG_STAT_TRICKLE	0x01
#define ADP536X_CHG_STAT_FAST_CC	0x02
#define ADP536X_CHG_STAT_FAST_CV	0x03
#define ADP536X_CHG_STAT_COMPLETE	0x04
#define ADP536X_CHG_STAT_LDO_MODE	0x05
#define ADP536X_CHG_STAT_TIMER_EXP	0x06
#define ADP536X_CHG_STAT_BAT_DET	0x07

/* Definition of battery states. GT = greater than (only for charging). */
#define ADP536X_BAT_STAT_NORMAL		0x00
#define ADP536X_BAT_STAT_NO_BAT		0x01
#define ADP536X_BAT_STAT_GT_0		0x02
#define ADP536X_BAT_STAT_GT_VTRK	0x03
#define ADP536X_BAT_STAT_GT_VWEAK	0x04
#define ADP536X_BAT_STAT_GT_VOVCHG	0x05

/**
 * @brief Initialize ADP536X.
 *
 * @param[in] dev_name Pointer to the device name.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_init(const char *dev_name);

/**
 * @brief Set the VBUS current limit.
 *
 * @param[in] value The upper current threshold in LSB.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_vbus_current_set(u8_t value);

/**
 * @brief Set the charger current.
 *
 * @param[in] value The charger current in LSB.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_charger_current_set(u8_t value);

/**
 * @brief Set the charger termination voltage.
 *
 * This function configures the maximum battery voltage where
 * the charger remains active.
 *
 * @param[in] value The charger termination voltage.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_charger_termination_voltage_set(u8_t value);

/**
 * @brief Enable charging.
 *
 * @param[in] enable The requested charger operation state.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_charging_enable(bool enable);

/**
 * @brief Read the STATUS1 register.
 *
 * @param[out] buf The read value of the STATUS1 register.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_charger_status_1_read(u8_t *buf);

/**
 * @brief Read the CHARGER_STATUS field.
 *
 * @param[out] buf The read value of the CHARGER_STATUS field.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */

int adp536x_charger_status_read(u8_t *buf);

/**
 * @brief Read the STATUS2 register.
 *
 * @param[out] buf The read value of the STATUS2 register.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_charger_status_2_read(u8_t *buf);

/**
 * @brief Read the FAULTS register.
 *
 * @param[out] buf The read value of the FAULTS register.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_faults_read(u8_t *buf);

/**
 * @brief Read the PGOOD_STATUS register.
 *
 * @param[out] buf The read value of the PGOOD_STATUS register.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_pgood_status_read(u8_t *buf);

/**
 * @brief Read the BAT_SOC register.
 *
 * @param[out] buf The read value of the BAT_SOC register.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_bat_soc_read(u8_t *buf);

/**
 * @brief Read the VBAT_READ battery voltage registers.
 *
 * @param[out] buf The last measuered battery voltage.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_vbat_read(u16_t *buf);

/**
 * @brief Enable charge hiccup protection mode.
 *
 * @param[in] enable The requested hiccup protection state.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_oc_chg_hiccup_set(bool enable);

/**
 * @brief Enable discharge hiccup protection mode.
 *
 * @param[in] enable The requested hiccup protection state.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_oc_dis_hiccup_set(bool enable);

/**
 * @brief Enable the buck/boost regulator.
 *
 * @param[in] enable The requested regulator operation state.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_buckbst_enable(bool enable);

/**
 * @brief Set the buck regulator to 1.8 V.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_buck_1v8_set(void);

/**
 * @brief Set the buck/boost regulator to 3.3 V.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_buckbst_3v3_set(void);

/**
 * @brief Place the ADP536x in shipment mode (shutdown).
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_en_shipmode(void);

/**
 * @brief Reset the device to its default values.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_factory_reset(void);

/**
 * @brief Set the charge over-current threshold.
 *
 * @param[in] value The over-current threshold.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_oc_chg_current_set(u8_t value);

/**
 * @brief Set the buck discharge resistor status.
 *
 * @param[in] enable Boolean value to enable or disable the discharge resistor.
 */
int adp536x_buck_discharge_set(bool enable);

/** @brief Set the battery capacity input.
 *
 * @param[in] value The battery capacity input.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_bat_cap_set(u8_t value);


/**
 * @brief Enable/disable the fuel gauge.
 *
 * @param[in] enable The requested fuel gauge operation mode.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_fuel_gauge_set(bool enable);

/**
 * @brief Enable the fuel gauge sleep mode.
 *
 * @param[in] enable The requested fuel gauge operation mode.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_fuel_gauge_enable_sleep_mode(bool enable);


/**
 * @brief Set the fuel gauge update rate.
 *
 * @param[in] rate Set the fuel gauge update rate.
 *                 0: 1 min
 *                 1: 4 min
 *                 2: 8 min
 *                 3: 16 min
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_fuel_gauge_update_rate_set(u8_t rate);

/**
 * @brief Set the low state of charge threshold.
 *
 * @param[in] rate Set the fuel gauge update rate.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int adp536x_soc_low_threshold_set(u8_t value);

#endif /* ADP536X_H_ */
