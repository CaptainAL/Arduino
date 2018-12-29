--/***************************************************
  Adafruit MQTT Library FONA Example

  Designed specifically to work with the Adafruit FONA
  ----> http://www.adafruit.com/products/1946
  ----> http://www.adafruit.com/products/1963
  ----> http://www.adafruit.com/products/2468
  ----> http://www.adafruit.com/products/2542

  These cellular modules use TTL Serial to communicate, 2 pins are
  required to interface.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <Adafruit_SleepyDog.h>
#include <SoftwareSerial.h>
#include "Adafruit_FONA.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_FONA.h"



/*************************** FONA Pins ***********************************/

// Default pins for  FONA
#define FONA_RX  2
#define FONA_TX  3
#define FONA_RST 4
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

/************************* WiFi Access Point *********************************/

  // Optionally configure a GPRS APN, username, and password.
  // You might need to do this to access your network's GPRS/data
  // network.  Contact your provider for the exact APN, username,
  // and password values.  Username and password are optional and
  // can be removed, but APN is required.
#define FONA_APN       "wholesale"
#define FONA_USERNAME  ""
#define FONA_PASSWORD  ""

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "CaptainAl"
#define AIO_KEY         "1f779d58ecb946c0ad742b660d876deb"

/************ Global State (you don't need to change this!) ******************/

// Setup the FONA MQTT class by passing in the FONA class and MQTT server and login details.
Adafruit_MQTT_FONA mqtt(&fona, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

// FONAconnect is a helper function that sets up the FONA and connects to
// the GPRS network. See the fonahelper.cpp tab above for the source!
boolean FONAconnect(const __FlashStringHelper *apn, const __FlashStringHelper *username, const __FlashStringHelper *password);

/****************************** Feeds ***************************************/


// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
// photocell = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/photocell");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");

Adafruit_MQTT_Publish Level_feed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Water Level");
Adafruit_MQTT_Publish Voltage_feed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Voltage");

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

/*************************** Sketch Code ************************************/

// How many transmission failures in a row we're willing to be ok with before reset
uint8_t txfailures = 0;
#define MAXTXFAILURES 3

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


void setup() {
  while (!Serial);

  // Watchdog is optional!
  //Watchdog.enable(8000);
  
  Serial.begin(115200);

  Serial.println(F("Adafruit FONA MQTT demo"));

  mqtt.subscribe(&onoffbutton);

  Watchdog.reset();
  delay(5000);  // wait a few seconds to stabilize connection
  Watchdog.reset();
  
  // Initialise the FONA module
  while (! FONAconnect(F(FONA_APN), F(FONA_USERNAME), F(FONA_PASSWORD))) {
    Serial.println("Retrying FONA");
  }

  Serial.println(F("Connected to Cellular!"));

  Watchdog.reset();
  delay(5000);  // wait a few seconds to stabilize connection
  Watchdog.reset();

}

void loop() {
  // Make sure to reset watchdog every loop iteration!
  Watchdog.reset();

  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  Watchdog.reset();

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
  float level = voltageToLevel(voltage, ZERO_LEVEL_VOLTS, CALIBRATION_VOLTS, CALIBRATION_LEVEL);
  Serial.print("Calculated level: ");
  Serial.println(level, 2);
  // Delay for a second.

  delay(1000);  // Pause for 5 seconds.

  
  // Now we can publish stuff!

  //RESISTANCE FEED
  Serial.print(F("\nSending Voltage val "));
  Serial.print(voltage);
  Serial.print("...");
  if (! Voltage_feed.publish(voltage)) {
    Serial.println(F("Failed"));
    txfailures++;
  } else {
    Serial.println(F("OK!"));
    txfailures = 0;
  }

  //WATER LEVEL feed
  Serial.print(F("\nSending Level val "));
  Serial.print(level);
  Serial.print("...");
  if (! Level_feed.publish(level)) {
    Serial.println(F("Failed"));
    txfailures++;
  } else {
    Serial.println(F("OK!"));
    txfailures = 0;
  }

  Watchdog.reset();  
  // this is our 'wait for incoming subscription packets' busy subloop
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &onoffbutton) {
      Serial.print(F("Got: "));
      Serial.println((char *)onoffbutton.lastread);
    }
  }

  // ping the server to keep the mqtt connection alive, only needed if we're not publishing
  //if(! mqtt.ping()) {
  //  Serial.println(F("MQTT Ping failed."));
  //}

}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}


float readVoltage(int pin, float Vref) {
  // Get ADC value.
  int sensorValue = analogRead(pin);
  // Convert ADC reading to voltage.
  float voltage = sensorValue * (Vref/1024.0);
  return voltage;
}

float voltageToLevel(float voltage, float zeroVoltage, float calVoltage, float calLevel) {
  float slope = (calLevel - 1.0) / (calVoltage - zeroVoltage);
  float intercept = 1 + (-slope * zeroVoltage);
  return (voltage * slope) + intercept;
  
}
