// Demo ESP8266 Probe Request
// 2022 by http://github.com/iotool
//
// Send probe request wifi packet.
// The minimum interval between probe request
// should >= 15ms, because the esp8266 is 
// blocking the rf channel. It's better to
// use standard scannetwork, that send 32x
// probe request every 70us.

extern "C" { 
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>

// probe request frame definition
uint8_t probeRequestPacket[36] = {
  /* 0-3 type/subtype beacon frame */ 
  0x40, 0x00, 0x00, 0x00,
  /* 4-9 destination: broadcast */ 
  0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF,
  /* 10-15 soucre mac / priv NID... */ 
  0x4e, 0x49, 0x44, // 10-12 mesh
  0x00, 0x00, 0x00, // 13-15 node
  /* 16-21 access point mac */ 
  0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF,
  // --- Fixed parameters --- 
  /* 22-23 fragment & seq (by SDK) */ 
  0x00, 0x00,
  // --- Fixed parameters --- 
  // 24-25 Tag00: SSID length 0
  0x00,	//wlan.tag.number => 0 			
  0x00,	//wlan.tag.length
  // 26-28 Tag01: Supp. Rates Mbit/sec
  0x01,	//wlan.tag.number => 1 				
  0x01,	//wlan.tag.length => 1
  0x02,	//1 Mbit/sec
  // 29-31 Tag03: Channel
  0x03,	//wlan.tag.number => 3 				
  0x01,	//wlan.tag.length => 1
  0x0d,	//channel.=> 13
  // 32-35 Frame Check Sequence
  0x00, 0x00, 0x00, 0x00
};

void setup(){
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.setOutputPower(14);
  WiFi.softAP(
    ("ESP-"+String(ESP.getChipId())+"-"+String(ESP.getChipId()%16))
   ,"12345678"
   ,13
  );

  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
  
  Serial.begin(115200);
  Serial.println("boot raw probe");
  delay(10);
}

uint32_t timerProbe=0;

void loop() {
  if (millis()-timerProbe>=1000) {
    timerProbe=millis();
    uint8_t mac[6];
    WiFi.softAPmacAddress(mac);
    Serial.print("send probe ");
    Serial.println(timerProbe);
    probeRequestPacket[10]=mac[0];                
    probeRequestPacket[11]=mac[1];
    probeRequestPacket[12]=mac[2];
    probeRequestPacket[13]=mac[3];
    probeRequestPacket[14]=mac[4];
    probeRequestPacket[15]=mac[5];
    Serial.println(
      macToString(mac)
    );
    wifi_send_pkt_freedom(
      probeRequestPacket, 
      sizeof(probeRequestPacket), 
      true
    );
    yield();
  }
}

String macToString(
      const unsigned char* mac) {
  // convert mac to string
  char buf[20];
  snprintf(buf, sizeof(buf),
    "%02x:%02x:%02x:%02x:%02x:%02x",
    mac[0], mac[1], mac[2],
    mac[3], mac[4], mac[5]);
  return String(buf);   
}
