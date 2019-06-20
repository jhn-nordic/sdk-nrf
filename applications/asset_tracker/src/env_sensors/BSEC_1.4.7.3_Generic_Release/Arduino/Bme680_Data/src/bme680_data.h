/**
 * Copyright (C) 2017 - 2018 Bosch Sensortec GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of the
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 * The information provided is believed to be accurate and reliable.
 * The copyright holder assumes no responsibility
 * for the consequences of use
 * of such information nor for any infringement of patents or
 * other rights of third parties which may result from its use.
 * No license is granted by implication or otherwise under any patent or
 * patent rights of the copyright holder.
 *
 * @file	bme680_data.h
 * @date	31 Jan 2018
 * @version	1.0
 *
 */

//#ifndef BME680_CLASS_H
//#define BME680_CLASS_H

/* Includes */
#include "Arduino.h"
#include "Wire.h"
#include "SPI.h"
#include "bme680\bme680.h"

/* Macro definitions */
#define BSEC_HUMIDITY_OVERSAMPLING       UINT8_C(1)            /*!< Humidity oversampling rate */
#define BSEC_TEMPERATURE_OVERSAMPLING    UINT8_C(2)            /*!< Temperature oversampling rate */
#define BSEC_PRESSURE_OVERSAMPLING       UINT8_C(5)            /*!< Pressure oversampling rate */
#define BSEC_HEATER_TEMP_LP              UINT16_C(320)         /*!< Low Power mode heater temperature */
#define BSEC_HEATING_DURATION_LP         UINT16_C(197)         /*!< Low Power mode heating duration */
#define BSEC_HEATER_TEMP_ULP             UINT16_C(400)         /*!< Ultra Low Power mode heater temperature */
#define BSEC_HEATING_DURATION_ULP        UINT16_C(1943)        /*!< Ultra Low Power mode heating duration */
#define BSEC_SAMPLE_RATE_ULP             (0.0033333f)          /*!< Sample rate in case of Ultra Low Power Mode */
#define BSEC_SAMPLE_RATE_LP              (0.33333f)            /*!< Sample rate in case of Low Power Mode */
#define BSEC_TIMESTAMP_SCALE_MS          (1e6f)                /*!< Timestamp scale from ms to ns */
#define BSEC_TIMESTAMP_SCALE_S           (1e9f)                /*!< Timestamp scale from s to ns */

/* BSEC class definition */
class Bme680
{
public:
	/* Public variables */
	int64_t nextCall;			// Stores the time when the algorithm has to be called next in ms
	int8_t bme680Status;		// Placeholder for the BME680 driver's error codes
	float rawTemperature, pressure, rawHumidity, gasResistance;
	
	/* Public APIs */
	/**
	 * @brief Constructor
	 */
	Bme680();

	/**
	 * @brief Function to initialize the BSEC library and the BME680 sensor
	 * @param devId		: Device identifier parameter for the read/write interface functions
	 * @param intf		: Physical communication interface
	 * @param read		: Pointer to the read function
	 * @param write		: Pointer to the write function
	 * @param idleTask	: Pointer to the idling task
	 */
	void begin(uint8_t devId, enum bme680_intf intf, bme680_com_fptr_t read, bme680_com_fptr_t write, bme680_delay_fptr_t idleTask);
	
	/**
	 * @brief Function to initialize the BSEC library and the BME680 sensor
	 * @param i2cAddr	: I2C address
	 * @param i2c		: Pointer to the TwoWire object
	 */
	void begin(uint8_t i2cAddr, TwoWire &i2c);
	
	/**
	 * @brief Function to initialize the BSEC library and the BME680 sensor
	 * @param chipSelect	: SPI chip select
	 * @param spi			: Pointer to the SPIClass object
	 */
	void begin(uint8_t chipSelect, SPIClass &spi);

	/**
     * @brief Callback from the user to trigger reading of data from the BME680, process and store outputs
	 * @param datalogFlag	: Flag to indicate basic data logging from sensor without BSEC
	 * @param sampleRate    : sample rate of the operation mode 
     * @return true if there is new data. false otherwise
     */
    bool run(float sampleRate);

	/**
	 * @brief Function to set the temperature offset
	 * @param tempOffset	: Temperature offset in degree Celsius
	 */
	void setTemperatureOffset(float tempOffset)
	{
		_tempOffset = tempOffset;
	}

private:
	/* Private variables */
	struct bme680_dev _bme680;
	struct bme680_field_data _data;
	float _tempOffset;
	// Global variables to help create a millisecond timestamp that doesn't overflow every 51 days.
	// If it overflows, it will have a negative value. Something that should never happen.
	uint32_t millisOverflowCounter;
	uint32_t lastTime;
	static TwoWire *i2c_obj;
	static SPIClass *spi_obj;
 
    /* Private structure variable */
	/*!
	 * @brief Structure to configure BME680 sensor  
	 *
	 * This structure contains settings that must be used to configure the BME680 to perform a forced-mode measurement. 
	 * The oversampling settings for temperature, humidity, and pressure should be set to the provided settings provided  
	 * in bme680_settings_t::temperature_oversampling, bme680_settings_t::humidity_oversampling, and
	 * bme680_settings_t::pressure_oversampling, respectively. 
	 *
	 * In case of bme680_settings_t::run_gas = 1, the gas sensor must be enabled with the provided 
	 * bme680_settings_t::heater_temperature and bme680_settings_t::heating_duration settings.
	 */
	typedef struct
	{
		int64_t next_call;                  /*!< @brief Time stamp of the next call of the sensor_control*/
		uint32_t process_data;              /*!< @brief Bit field describing which data is to be passed to bsec_do_steps() @sa BSEC_PROCESS_* */
		uint16_t heater_temperature;        /*!< @brief Heating temperature [degrees Celsius] */
		uint16_t heating_duration;          /*!< @brief Heating duration [ms] */
		uint8_t run_gas;                    /*!< @brief Enable gas measurements [0/1] */
		uint8_t pressure_oversampling;      /*!< @brief Pressure oversampling settings [0-5] */
		uint8_t temperature_oversampling;   /*!< @brief Temperature oversampling settings [0-5] */
		uint8_t humidity_oversampling;      /*!< @brief Humidity oversampling settings [0-5] */
		uint8_t trigger_measurement;        /*!< @brief Trigger a forced measurement with these settings now [0/1] */
	} bme680_settings_t;

	/* Private APIs */
	
    /**
     * @brief Read data from the BME680 and process it
     * @return true if there is new data. false otherwise
     */
    bool readProcessData(void);

	/**
     * @brief Assign settings for bme680 sensor based on sample rate
	 * @param callTimeMs: Current time in ns
	 * @param sampleRate:sample rate of the operation mode
	 * @param bme680Settings: BME680 sensor's settings	 
     */
	void getBme680Settings(int64_t callTimeMs, float sampleRate, bme680_settings_t *sensor_settings);
	
	/**
	 * @brief Set the BME680 sensor's configuration
	 * @param bme680Settings: Settings to configure the BME680
	 * @return BME680 return code. BME680_OK for success, failure otherwise
	 */
	int8_t setBme680Config(bme680_settings_t bme680Settings);
 
   	/**
     * @brief API code for the begin function
     */
    void beginAPI(void);
	
	/**
	 * @brief Function to zero the outputs
	 */
	void zeroOutputs(void);
	
	/**
	 * @brief Function to calculate an int64_t timestamp in milliseconds
	 */
	int64_t getTimeMs(void);
	
	/**
	* @brief Task that delays for a ms period of time
	* @param period	: Period of time in ms
	*/
	static void delay_ms(uint32_t period);

	/**
	* @brief Callback function for reading registers over I2C
	* @param devId		: Library agnostic parameter to identify the device to communicate with
	* @param regAddr	: Register address
	* @param regData	: Pointer to the array containing the data to be read
	* @param length	: Length of the array of data
	* @return	Zero for success, non-zero otherwise
	*/
	static int8_t i2cRead(uint8_t devId, uint8_t regAddr, uint8_t *regData, uint16_t length);

	/**
	* @brief Callback function for writing registers over I2C
	* @param devId		: Library agnostic parameter to identify the device to communicate with
	* @param regAddr	: Register address
	* @param regData	: Pointer to the array containing the data to be written
	* @param length	: Length of the array of data
	* @return	Zero for success, non-zero otherwise
	*/
	static int8_t i2cWrite(uint8_t devId, uint8_t regAddr, uint8_t *regData, uint16_t length);

	/**
	* @brief Callback function for reading and writing registers over SPI
	* @param devId		: Library agnostic parameter to identify the device to communicate with
	* @param regAddr	: Register address
	* @param regData	: Pointer to the array containing the data to be read or written
	* @param length	: Length of the array of data
	* @return	Zero for success, non-zero otherwise
	*/
	static int8_t spiTransfer(uint8_t devId, uint8_t regAddr, uint8_t *regData, uint16_t length);
};

//#endif
