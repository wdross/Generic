#include <Arduino.h>
#include <ESP8266WiFi.h>
#define DEBUG_FAUXMO
#define DEBUG_FAUXMO_VERBOSE_TCP
#define DEBUG_FAUXMO_VERBOSE_UDP
#include "fauxmoESP.h"
#include "C:\Installs\arduino-1.6.12\libraries\Credentials.h"

#define SERIAL_BAUDRATE                 115200
#define ACTIVITY_LED 2 // blue LED near the antenna end of the Feather
#define STATE_FOR_OFF false

fauxmoESP fauxmo;

// -----------------------------------------------------------------------------
// Wifi
// -----------------------------------------------------------------------------

void wifiSetup() {

    // Set WIFI module to STA mode
    WiFi.mode(WIFI_STA);

    // Connect
    Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Wait
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(100);
    }
    Serial.println();

    // Connected!
    Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void setup() {
    pinMode(ACTIVITY_LED,OUTPUT);
    digitalWrite(ACTIVITY_LED,false == STATE_FOR_OFF);

    // Init serial port and clean garbage
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println("FauxMo demo sketch");

    // Wifi
    wifiSetup(); // stalls until connected to WiFi

    // Fauxmo
    fauxmo.setPort(80);  
    fauxmo.enable(true);
    fauxmo.addDevice("relay");
    fauxmo.addDevice("pixels");

    fauxmo.onSetState([](unsigned char device_id, const char * device_name, 
                         bool state, unsigned char value) {
      Serial.print("Device name: ");
      Serial.print(device_name);
      Serial.print("Value: ");
      Serial.print(value);
      Serial.println(state?" true":" false");
      // Here we handle the command received
      digitalWrite(ACTIVITY_LED,state == STATE_FOR_OFF);
    });
}

void loop() {
  fauxmo.handle();
}
