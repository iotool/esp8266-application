// ESP8266 Access-Point Fast-Scan One-Channel
// 
// Wemos D1 Mini ESP12E
// 120m / 393ft in open terrain
// 
// LED blink, if beacon frame receive
// Serial USB terminal App to debug RSSI
// 
// This sketch demonstrate, how you can use
// the wifi_set_country to reduce the scan
// to a single channel (2300ms to 110ms).
//
// 2022-10-05 V1
// 802.11b -20dBm for longer range 120m
// async network scan in background
// single channel for fast network scan 110ms
// dynamic SSID by physical MAC address

// overwrite esp-sdk (e.g. wifi_set_country)
extern "C" {
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>

// access point definition
#define AP_PHYMOD  WIFI_PHY_MODE_11B // 11Mbits
#define AP_POWER   20  // 0..20 Lo/Hi
#define AP_CHANNEL 13  // 1..13
const char* apName = "ESP-test-";
const char* apPass = "12345678";

void setup() {
  
  // don't write wifi setup to flash
  WiFi.persistent(false);

  // signal boot phase (low=on, high=off)
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);

  // dynamic SSID by physical MAC address
  uint8_t apMAL = WL_MAC_ADDR_LENGTH;
  uint8_t apMAC[apMAL];
  WiFi.softAPmacAddress(apMAC);
  String apSSID = "";
  apSSID += (apMAC[apMAL-2]<16?"0":"");
  apSSID += String(apMAC[apMAL-2],HEX);
  apSSID += (apMAC[apMAL-1]<16?"0":"");
  apSSID += String(apMAC[apMAL-1],HEX);
  apSSID.toUpperCase();
  apSSID = String(apName)+apSSID;

  // fast network scan (only one channel)
  wifi_country_t apCountry = {
    .cc = "EU",           // country
    .schan = AP_CHANNEL,  // start channel
    .nchan = 1,           // number of ch
    .policy = WIFI_COUNTRY_POLICY_MANUAL
  };
  wifi_set_country(&apCountry);

  // start wifi
  WiFi.mode(WIFI_AP_STA);
  WiFi.setPhyMode(AP_PHYMOD);
  WiFi.setOutputPower(AP_POWER);
  WiFi.softAP(apSSID,apPass,AP_CHANNEL);

  // debug setup via serial
  Serial.begin(19200);
  delay(100);
  Serial.println();
  Serial.println("SSID: "+apSSID);
  Serial.println("PASSWD: "+String(apPass));
  Serial.println("CHANNEL: "+String(AP_CHANNEL));
  Serial.println("POWER: "+String(AP_POWER));
  delay(200);

  // turn off led
  digitalWrite(LED_BUILTIN,HIGH);

}

// simple time slots
uint16_t timer=0;

void loop() {

  // timer
  uint16_t interval=100;
  delay(interval);
  timer+=interval;
  if (timer>=10000) {timer=0;}

  // scan network
  if (timer%1000==0) {

    // scan async in backgroud
    WiFi.scanNetworks(true);
    Serial.println("Scan...");

  } else {

    // fetch after 100ms (wifi beacon interval)
    int scanN=WiFi.scanComplete();
    if (scanN>0) {
      byte scanF=0;

      // debug SSID via serial
      for (int i=0; i<scanN; i++) {
        if(WiFi.channel(i)!=AP_CHANNEL){
        } else 
        if (!String(WiFi.SSID(i).c_str()).startsWith(apName)) {} else 
        if (true) {
          scanF++;
          Serial.printf("%d: %s, Ch:%d (%ddBm) %s\n", i+1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "");
        }
      }

      // clear scan result
      WiFi.scanDelete();

      // signal led
      if (scanF>0) {
        Serial.println();
        digitalWrite(LED_BUILTIN,LOW);
        delay(100);
        digitalWrite(LED_BUILTIN,HIGH);
        timer+=100;
      }

    } // scanN>0
  } // scan network

}
