// Liquid Level Sensor Sketch

// Show the raw resistance values measured from an eTape liquid level sensor.
// See details on the sensor at:
//   https://www.adafruit.com/products/1786

// Created by Tony DiCola
// Released under an MIT license: http://opensource.org/licenses/MIT

// Configuration values:
#define SERIES_VOLTAGE    5    // Value of the ref voltage in mV??    
#define SENSOR_PIN          0      // Analog pin which is connected to the sensor. 
#define REF_PIN    1 //

// The following are calibration values you can fill in to compute the volume of measured liquid.
// To find these values first start with no liquid present and record the resistance as the
// ZERO_VOLUME_RESISTANCE value.  Next fill the container with a known volume of liquid and record
// the sensor resistance (in ohms) as the CALIBRATION_RESISTANCE value, and the volume (which you've
// measured ahead of time) as CALIBRATION_VOLUME.
#define ZERO_LEVEL_VOLTS    0.458  // Resistance value (in ohms) when no liquid is present.
#define CALIBRATION_VOLTS  3.643    // Resistance value (in ohms) when liquid is at max line.
#define CALIBRATION_LEVEL        8.0    // Volume (in any units) when liquid is at max line.

long readVcc() {
  long result;
  
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1125300L / result; // Back-calculate AVcc in mV
  return result;
}


void setup(void) {
  Serial.begin(115200);
  Serial.println("REBOOT"); 

  
}
 
void loop(void) {  
  
  
  // Measure sensor voltage.
  float Vref = readVcc()/1000.0;
  Serial.print("Vref: ");
  Serial.print(Vref, 3);
  Serial.println(" volts");
  //
  float A1Ref =  readVoltage(REF_PIN, Vref);
  Serial.print("A1Ref: ");
  Serial.print(A1Ref, 3);
  Serial.println(" volts");
  
  float voltage = readVoltage(SENSOR_PIN, A1Ref);
  Serial.print("Voltage: "); 
  Serial.print(voltage, 3);
  Serial.println(" volts");
  
  // Map voltage to volume.
  float level = voltsToLevel(voltage, ZERO_LEVEL_VOLTS, CALIBRATION_VOLTS, CALIBRATION_LEVEL);
  Serial.print("Calculated level: ");
  Serial.println(level, 2);
  // Delay for a second.
  delay(1000);
}

float readVoltage(int pin, float Vref) {
  // Get ADC value.
  int sensorValue = analogRead(pin);
  // Convert ADC reading to voltage.
  float voltage = sensorValue * (Vref/1024.0);
  return voltage;
}

float voltsToLevel(float voltage, float zeroVoltage, float calVoltage, float calLevel) {
  float slope = (calLevel - 1.0) / (calVoltage - zeroVoltage);
  float intercept = 1 + (-slope * zeroVoltage);
  return (voltage * slope) + intercept;
  
}



float voltageToLevel(float voltage, float zeroVoltage, float calVoltage, float calLevel) {
  if (voltage < zeroVoltage || (zeroVoltage - calVoltage) == 0.0) {
    // Stop if the value is above the zero threshold, or no max resistance is set (would be divide by zero).
    return 0.0;
  }
  // Compute scale factor by mapping resistance to 0...1.0+ range relative to maxResistance value.
  float scale = (zeroVoltage - voltage) / (zeroVoltage - calVoltage);
  // Scale maxLevel based on computed scale factor.
  return calLevel * scale;
}

