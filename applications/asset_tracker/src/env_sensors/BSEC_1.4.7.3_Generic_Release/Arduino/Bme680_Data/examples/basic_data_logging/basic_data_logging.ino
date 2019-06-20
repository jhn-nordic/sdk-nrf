//preprocessor directive to not use bsec in case of basic data logging from the sensor 

#include "bme680_data.h"

// Helper functions declarations
void checkSensorStatus(void);
void errLeds(void);

// Create an object of the class Bsec
Bme680 envSensor;

String output;
float sampleRate; 

// Entry point for the example
void setup(void)
{
  Serial.begin(115200); 
  sampleRate = BSEC_SAMPLE_RATE_LP;
  envSensor.begin(BME680_I2C_ADDR_PRIMARY, Wire);
  checkSensorStatus();
    
  // Print the header
  output = "\nTimestamp [ms], raw temperature [°C], pressure [hPa], raw relative humidity [%], gas [Ohm]";
  Serial.println(output);
}

// Function that is looped forever
void loop(void)
{
  if (envSensor.run(sampleRate)) { // If new data is available
    output = String(millis());
    output += ", " + String(envSensor.rawTemperature);
    output += ", " + String(envSensor.pressure);
    output += ", " + String(envSensor.rawHumidity);
    output += ", " + String(envSensor.gasResistance);
    Serial.println(output);
  } else {
    checkSensorStatus();
  }
}

// Helper function definitions
void checkSensorStatus(void)
{
  if (envSensor.bme680Status != BME680_OK) {
    if (envSensor.bme680Status < BME680_OK) {
      output = "BME680 error code : " + String(envSensor.bme680Status);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BME680 warning code : " + String(envSensor.bme680Status);
      Serial.println(output);
    }
  }
}

void errLeds(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}


