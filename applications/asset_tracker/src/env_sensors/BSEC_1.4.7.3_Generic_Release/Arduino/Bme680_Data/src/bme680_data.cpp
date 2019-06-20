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
 * @file	bme680.cpp
 * @date	16 Mar 2018
 * @version	1.0
 *
 */
 
#include "bme680_data.h"

TwoWire* Bme680::i2c_obj = NULL;
SPIClass* Bme680::spi_obj = NULL;

/**
 * @brief Constructor
 */
Bme680::Bme680()
{
    nextCall = 0;
	millisOverflowCounter = 0;
	lastTime = 0;
	bme680Status = BME680_OK;
	_tempOffset = 0;
	zeroOutputs();
}

/**
 * @brief Function to initialize the BME680 sensor
 */
void Bme680::begin(uint8_t devId, enum bme680_intf intf, bme680_com_fptr_t read, bme680_com_fptr_t write, bme680_delay_fptr_t idleTask)
{
	_bme680.dev_id = devId;
	_bme680.intf = intf;
	_bme680.read = read;
	_bme680.write = write;
	_bme680.delay_ms = idleTask;
	_bme680.amb_temp = 25;
	_bme680.power_mode = BME680_FORCED_MODE;

	beginAPI();
}

/**
 * @brief Function to initialize the BSEC library and the BME680 sensor
 */
void Bme680::begin(uint8_t i2cAddr, TwoWire &i2c)
{
	_bme680.dev_id = i2cAddr;
	_bme680.intf = BME680_I2C_INTF;
	_bme680.read = Bme680::i2cRead;
	_bme680.write = Bme680::i2cWrite;
	_bme680.delay_ms = Bme680::delay_ms;
	_bme680.amb_temp = 25;
	_bme680.power_mode = BME680_FORCED_MODE;

	Bme680::i2c_obj = &i2c;
	Bme680::i2c_obj->begin();

	beginAPI(); 
}

/**
 * @brief Function to initialize the BSEC library and the BME680 sensor
 */
void Bme680::begin(uint8_t chipSelect, SPIClass &spi)
{
	_bme680.dev_id = chipSelect;
	_bme680.intf = BME680_SPI_INTF;
	_bme680.read = Bme680::spiTransfer;
	_bme680.write = Bme680::spiTransfer;
	_bme680.delay_ms = Bme680::delay_ms;
	_bme680.amb_temp = 25;
	_bme680.power_mode = BME680_FORCED_MODE;

	pinMode(chipSelect, OUTPUT);
	digitalWrite(chipSelect, HIGH);
	Bme680::spi_obj = &spi;
	Bme680::spi_obj->begin();
  
	beginAPI();
}

/**
 * @brief API code for the begin function
 */
void Bme680::beginAPI(void)
{
	bme680Status = bme680_init(&_bme680);
	if (bme680Status != BME680_OK) {
		return;
	}	
}

/**
 * @brief Callback from the user to trigger reading of data from the BME680 and display to the user
 */
bool Bme680::run(float sampleRate)
{
	bool newData = false;
	
	int64_t callTimeMs = getTimeMs();
	
	if (callTimeMs >= (nextCall/1e6)) {
		
		bme680_settings_t bme680Settings;
		
		getBme680Settings(callTimeMs, sampleRate, &bme680Settings);
		
		nextCall = bme680Settings.next_call;
		
		bme680Status = setBme680Config(bme680Settings);
		if (bme680Status != BME680_OK) {
			return false;
		}

		bme680Status = bme680_set_sensor_mode(&_bme680);
		if (bme680Status != BME680_OK) {
			return false;
		}

		/* Wait for measurement to complete */
		uint16_t meas_dur = 0;

		bme680_get_profile_dur(&meas_dur, &_bme680);
		delay_ms(meas_dur);

		newData = readProcessData();
	}	
	
	return newData;
}

/**
 * @brief Read data from the BME680 
 */
bool Bme680::readProcessData(void)
{
	bme680Status = bme680_get_sensor_data(&_data, &_bme680);
	if (bme680Status != BME680_OK) {
		return false;
	}

	rawTemperature = _data.temperature;
	pressure = _data.pressure;
	rawHumidity = _data.humidity;
	gasResistance = _data.gas_resistance;
	return true;
}

/**
 * @brief Assign settings for bme680 sensor based on sample rate 
 */
void Bme680::getBme680Settings(int64_t callTimeMs, float sampleRate, bme680_settings_t *bme680Settings)
{
	bme680Settings->next_call = int64_t((callTimeMs * BSEC_TIMESTAMP_SCALE_MS) + (BSEC_TIMESTAMP_SCALE_S / sampleRate));
	bme680Settings->run_gas = BME680_RUN_GAS_ENABLE;
	bme680Settings->humidity_oversampling = BSEC_HUMIDITY_OVERSAMPLING;
	bme680Settings->temperature_oversampling = BSEC_TEMPERATURE_OVERSAMPLING;
	bme680Settings->pressure_oversampling = BSEC_PRESSURE_OVERSAMPLING;	
	if (sampleRate == BSEC_SAMPLE_RATE_LP){
        bme680Settings->heater_temperature = BSEC_HEATER_TEMP_LP;
	    bme680Settings->heating_duration = BSEC_HEATING_DURATION_LP;
	}
	else if(sampleRate == BSEC_SAMPLE_RATE_ULP){
		bme680Settings->heater_temperature = BSEC_HEATER_TEMP_ULP;
	    bme680Settings->heating_duration = BSEC_HEATING_DURATION_ULP;
	}	
}

/**
 * @brief Set the BME680 sensor's configuration
 */
int8_t Bme680::setBme680Config(bme680_settings_t bme680Settings)
{
	_bme680.gas_sett.run_gas = bme680Settings.run_gas;
	_bme680.tph_sett.os_hum = bme680Settings.humidity_oversampling;
	_bme680.tph_sett.os_temp = bme680Settings.temperature_oversampling;
	_bme680.tph_sett.os_pres = bme680Settings.pressure_oversampling;
	_bme680.gas_sett.heatr_temp = bme680Settings.heater_temperature;
	_bme680.gas_sett.heatr_dur = bme680Settings.heating_duration;
	uint16_t desired_settings = BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL
	        | BME680_GAS_SENSOR_SEL;
	return bme680_set_sensor_settings(desired_settings, &_bme680);
}

/**
 * @brief Function to zero the outputs
 */
void Bme680::zeroOutputs(void)
{	
	pressure = 0.0f;	
	gasResistance = 0.0f;
	rawTemperature = 0.0f;
	rawHumidity = 0.0f;	
}

/**
 * @brief Function to calculate an int64_t timestamp in milliseconds
 */
int64_t Bme680::getTimeMs(void)
{
	int64_t timeMs = millis();

	if (lastTime > timeMs) { // An overflow occured
		lastTime = timeMs;
		millisOverflowCounter++;
	}

	return timeMs + (millisOverflowCounter * 0xFFFFFFFF);
}

/**
 @brief Task that delays for a ms period of time
 */
void Bme680::delay_ms(uint32_t period)
{
	// Wait for a period amount of ms
	// The system may simply idle, sleep or even perform background tasks
	delay(period);
}

/**
 @brief Callback function for reading registers over I2C
 */
int8_t Bme680::i2cRead(uint8_t devId, uint8_t regAddr, uint8_t *regData, uint16_t length)
{
	uint16_t i;
	int8_t rslt = 0;
	if(Bme680::i2c_obj) {
		Bme680::i2c_obj->beginTransmission(devId);
		Bme680::i2c_obj->write(regAddr);
		rslt = Bme680::i2c_obj->endTransmission();
		Bme680::i2c_obj->requestFrom((int) devId, (int) length);
		for (i = 0; (i < length) && Bme680::i2c_obj->available(); i++) {
			regData[i] = Bme680::i2c_obj->read();
		}
	} else {
		rslt = -1;
	}
	return rslt;
}

/**
 * @brief Callback function for writing registers over I2C
 */
int8_t Bme680::i2cWrite(uint8_t devId, uint8_t regAddr, uint8_t *regData, uint16_t length)
{
	uint16_t i;
	int8_t rslt = 0;
	if(Bme680::i2c_obj) {
		Bme680::i2c_obj->beginTransmission(devId);
		Bme680::i2c_obj->write(regAddr);
		for (i = 0; i < length; i++) {
			Bme680::i2c_obj->write(regData[i]);
		}
		rslt = Bme680::i2c_obj->endTransmission();
	} else {
		rslt = -1;
	}

	return rslt;
}

/**
 * @brief Callback function for reading and writing registers over SPI
 */
int8_t Bme680::spiTransfer(uint8_t devId, uint8_t regAddr, uint8_t *regData, uint16_t length)
{
	int8_t rslt = 0;
	if(Bme680::spi_obj) {
		Bme680::spi_obj->beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0)); // Can be upto 10MHz
	
		digitalWrite(devId, LOW);

		Bme680::spi_obj->transfer(regAddr); // Write the register address, ignore the return
		for (uint16_t i = 0; i < length; i++)
			regData[i] = Bme680::spi_obj->transfer(regData[i]);

		digitalWrite(devId, HIGH);
		Bme680::spi_obj->endTransaction();
	} else {
		rslt = -1;
	}

	return rslt;;
}
