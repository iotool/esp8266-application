// Demo ESP8266 Probe Response
// 
// Send probe response on probe request 
// via unicast to the requesting station.
// Turn off and on wifi at your smartphone
// to force probe request and display the
// second hotspot.

#include <ESP8266WiFi.h>

const char* ssid     = "ESP-AccessPoint";
const char* password = "12345678";

// probe response frame definition
uint8_t probeResponsePacket[102] = {
  /* 0-3 type/subtype probe responce */ 
  0x50, 0x00, 0x00, 0x00,
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
  0x01, 0x01,                         
  // Tag: Supported Rates, Tag len: 8
  /* 72 */ 0x02,  // 1 Mbit/s
  // Current Channel
  /* 73 - 74 */ 0x03, 0x01,         
  // Channel set, length
  /* 75 */      0x01,             
  // Current Channel
  // RSN information
  /* 76-77 */ 0x30, 0x18,
  /* 78-79 */ 0x01, 0x00,
  /* 80-83 */ 0x00, 0x0f, 0xac, 0x02,
  /* 84-85 */ 0x02, 0x00,
  /* 86-93 */ 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04, 
  /*Fix: changed 0x02(TKIP) to 0x04(CCMP) is default. WPA2 with TKIP not supported by many devices*/
  /* 94 - 95 */ 0x01, 0x00,
  /* 96 - 99 */ 0x00, 0x0f, 0xac, 0x02,
  /* 100 - 101 */ 0x00, 0x00
};

WiFiEventHandler probeRequestHandler;

static uint8_t probeMac[6];

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password,1,false);

  probeRequestHandler = WiFi.onSoftAPModeProbeRequestReceived(&onProbeRequest);
}

void onProbeRequest(const WiFiEventSoftAPModeProbeRequestReceived& evt) {
  for(uint8_t i=0;i<6;i++){
    probeMac[i] = evt.mac[i];
  }
  digitalWrite(LED_BUILTIN, LOW);
  Serial.print("Probe request from: ");
  Serial.print(macToString(evt.mac));
  Serial.print(" RSSI: ");
  Serial.println(evt.rssi);
}

uint32_t timerProbe=0;

void loop() {
  if (millis()-timerProbe>50) {
    timerProbe=millis();
    if(probeMac[0]!=0x00){
      // mac address
      uint8_t sourceMac[6];
      WiFi.macAddress(sourceMac);
      // WiFi.softAPmacAddress(sourceMac);
      for(uint8_t i=0;i<6;i++){
        // destination mac
        probeResponsePacket[4+i]=
          probeMac[i]; 
        // source mac
        probeResponsePacket[10+i]=
          sourceMac[i];
        // bssi mac
        probeResponsePacket[16+i]=
          sourceMac[i];
      }
      // ssid          123456789012345678
      char ssid[32] = "ESP-ProbeResponce";
      memcpy(
        &probeResponsePacket[38], ssid, 17
      );
      probeResponsePacket[38+18]=0x00;
      // channel
      probeResponsePacket[75] = 1;
      // wpa2
      probeResponsePacket[34] = 0x31;
      // send
      for(int i=1; i<4; i++){
        wifi_send_pkt_freedom(
          probeResponsePacket, 
          sizeof(probeResponsePacket), 
          true
        );
        delay(20);
      }
      probeMac[0]=0x00;
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
}

String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}
