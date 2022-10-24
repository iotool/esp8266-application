// Demo ESP8266 Beacon
// 2022 by http://github.com/iotool
// 
// Broadcast beacon frame to every wifi node.
// The regular interval are 102.4ms between
// beacon packets.

extern "C" { 
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>

// beacon frame definition
uint8_t beaconPacket[109] = {
  /* 0-3 type/subtype beacon frame */ 
  0x80, 0x00, 0x00, 0x00,
  /* 4-9 destination: broadcast */ 
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  /* 10-15 soucre mac / priv NID... */ 
  0x4e, 0x49, 0x44, // 10-12 mesh
  0x00, 0x00, 0x00, // 13-15 node
  /* 16-21 access point mac */ 
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  // --- Fixed parameters --- 
  /* 22-23 fragment & seq (by SDK) */ 
  0x00, 0x00,
  /* 24-31 timestamp ### */ 
  0x83, 0x51, 0xf7, 0x8f, 
  0x0f, 0x00, 0x00, 0x00,
  /* 32-33 interval 102ms/1024ms */ 
  0xe8, 0x03, // 0x64,0x00 / 0xe8, 0x03
  /* 34-35 capabilities */ 
  0x31, 0x00,
  // --- tagged parameters ---
  // SSID parameters
  /* 36-37 ssid length 32 */
  0x00, 0x20,              
  /* 38-69 ssid buffer ### */ 
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  // --- supported rates ---
  /* 70 - 71 */ 
  0x01, 0x08,                         
  // Tag: Supported Rates, Tag len: 8
  /* 72 */ 0x82,  // 1(B)
  /* 73 */ 0x84,  // 2(B)
  /* 74 */ 0x8b,  // 5.5(B)
  /* 75 */ 0x96,  // 11(B)
  /* 76 */ 0x24,  // 18
  /* 77 */ 0x30,  // 24
  /* 78 */ 0x48,  // 36
  /* 79 */ 0x6c,  // 54
  // Current Channel
  /* 80 - 81 */ 0x03, 0x01,         
  // Channel set, length
  /* 82 */      0x01,             
  // Current Channel
  // RSN information
  /* 83-84 */ 0x30, 0x18,
  /* 85-86 */ 0x01, 0x00,
  /* 87-90 */ 0x00, 0x0f, 0xac, 0x02,
  /* 91-92 */ 0x02, 0x00,
  /* 93-100 */ 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04, 
  /*Fix: changed 0x02(TKIP) to 0x04(CCMP) is default. WPA2 with TKIP not supported by many devices*/
  /* 101 - 102 */ 0x01, 0x00,
  /* 103 - 106 */ 0x00, 0x0f, 0xac, 0x02,
  /* 107 - 108 */ 0x00, 0x00
};

// beacon frame definition
uint8_t beaconHiddenPacket[77] = {
  /* 0-3 type/subtype beacon frame */ 
  0x80, 0x00, 0x00, 0x00,
  /* 4-9 destination: broadcast */ 
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  /* 10-15 soucre mac / priv NID... */ 
  0x4e, 0x49, 0x44, // 10-12 mesh
  0x00, 0x00, 0x00, // 13-15 node
  /* 16-21 access point mac */ 
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  // --- Fixed parameters --- 
  /* 22-23 fragment & seq (by SDK) */ 
  0x00, 0x00,
  /* 24-31 timestamp ### */ 
  0x83, 0x51, 0xf7, 0x8f, 
  0x0f, 0x00, 0x00, 0x00,
  /* 32-33 interval 102ms/1024ms */ 
  0xe8, 0x03, // 0x64,0x00 / 0xe8, 0x03
  /* 34-35 capabilities */ 
  0x31, 0x00,
  // --- tagged parameters ---
  // SSID parameters
  /* 36-37 ssid length 32 */
  0x00, 0x00, // 0x00, 0x20,
  /* 38-69 ssid buffer ### */
  /*---
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  ---*/
  // --- supported rates ---
  /* 70 - 71 */ 
  0x01, 0x08,                         
  // Tag: Supported Rates, Tag len: 8
  /* 72 */ 0x82,  // 1(B)
  /* 73 */ 0x84,  // 2(B)
  /* 74 */ 0x8b,  // 5.5(B)
  /* 75 */ 0x96,  // 11(B)
  /* 76 */ 0x24,  // 18
  /* 77 */ 0x30,  // 24
  /* 78 */ 0x48,  // 36
  /* 79 */ 0x6c,  // 54
  // Current Channel
  /* 80 - 81 */ 0x03, 0x01,         
  // Channel set, length
  /* 82 */      0x01,             
  // Current Channel
  // RSN information
  /* 83-84 */ 0x30, 0x18,
  /* 85-86 */ 0x01, 0x00,
  /* 87-90 */ 0x00, 0x0f, 0xac, 0x02,
  /* 91-92 */ 0x02, 0x00,
  /* 93-100 */ 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04, 
  /*Fix: changed 0x02(TKIP) to 0x04(CCMP) is default. WPA2 with TKIP not supported by many devices*/
  /* 101 - 102 */ 0x01, 0x00,
  /* 103 - 106 */ 0x00, 0x0f, 0xac, 0x02,
  /* 107 - 108 */ 0x00, 0x00
};

void setup(){
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.setOutputPower(14);
  WiFi.softAP(
    ("ESP-"+String(ESP.getChipId()))
  );

  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
  
  Serial.begin(115200);
  Serial.println("boot");
  delay(10);
}

uint32_t timerBeacon=0;
uint32_t timerScan=0;

void loop() {
  // Scan
  if (millis()-timerScan>=125) {
    timerScan=millis();
    if (WiFi.scanComplete()>=0) {
      uint8_t f[16]={0};
      for(int i=0;
          i<WiFi.scanComplete();
          i++) {
        f[WiFi.BSSID(i)[1]]=1;
      }
      for(int i=1;i<16;i++){
        f[0]+=f[i];
      }
      if (f[0]==16) {
        digitalWrite(LED_BUILTIN,LOW);
        Serial.print("found:");
        Serial.println(
          WiFi.scanComplete()
        );
      }
      for(int i=0;
          i<WiFi.scanComplete();
          i++){
        Serial.print("ch:");
        Serial.print(
          WiFi.channel(i)
        );
        Serial.print(" mac:");
        Serial.print(
          WiFi.BSSIDstr(i)
        );
        Serial.print(" rssi:");
        Serial.print(WiFi.RSSI(i)); 
        if (WiFi.isHidden(i)) {
          Serial.print(" hidden");
        }
        Serial.println();
        digitalWrite(LED_BUILTIN,HIGH);
      }
      WiFi.scanDelete();
    }
    WiFi.scanNetworks(
      true, // async
      true  // hidden
    );
  }
  // Beacon
  if (millis()-timerBeacon>=102) {
    timerBeacon=millis();
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    WiFi.softAPmacAddress(macAddr);
    // beacon
    beaconPacket[10]=macAddr[0];
    beaconPacket[11]=macAddr[1];
    beaconPacket[12]=macAddr[2];
    beaconPacket[13]=macAddr[3];
    beaconPacket[14]=macAddr[4];
    beaconPacket[15]=macAddr[5]+1;
    beaconPacket[16]=macAddr[0];
    beaconPacket[17]=macAddr[1];
    beaconPacket[18]=macAddr[2];
    beaconPacket[19]=macAddr[3];
    beaconPacket[20]=macAddr[4];
    beaconPacket[21]=macAddr[5]+1;
    char ssid[32] = "ESP-Beacon";
    beaconPacket[38]=0;
    memcpy(
      &beaconPacket[38], ssid, 11
    );
    beaconPacket[82] = 1; // channel
    beaconPacket[34] = 0x31; // wpa2
    wifi_send_pkt_freedom(
      beaconPacket, 
      sizeof(beaconPacket), 
      true
    );
    delay(20);
    // hidden beacon
    beaconHiddenPacket[10]=macAddr[0];
    beaconHiddenPacket[11]=macAddr[1];
    beaconHiddenPacket[12]=macAddr[2];
    beaconHiddenPacket[13]=macAddr[3];
    beaconHiddenPacket[14]=macAddr[4];
    beaconHiddenPacket[15]=macAddr[5]+2;
    beaconHiddenPacket[15]=macAddr[0];
    beaconHiddenPacket[16]=macAddr[1];
    beaconHiddenPacket[17]=macAddr[2];
    beaconHiddenPacket[19]=macAddr[3];
    beaconHiddenPacket[20]=macAddr[4];
    beaconHiddenPacket[21]=macAddr[5]+2;        
    beaconHiddenPacket[62] = 1; // chnl
    // hidden beacon
    wifi_send_pkt_freedom(
      beaconHiddenPacket, 
      sizeof(beaconHiddenPacket), 
      true
    );
    delay(20);
  }
}
