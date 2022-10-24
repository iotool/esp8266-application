// Demo ESP8266 ScanNetwork passive
// 2022 by http://github.com/iotool/
//
// active network scan send 32x probe
// requests every 70us and take 100ms per
// channel. passive scan don't transmit
// any probe request and should wait 103ms,
// because every access point send a beacon
// every 102.4ms

extern "C" { 
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>

void setup(){
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.setOutputPower(14);
  struct softap_config apConfig;
  wifi_softap_get_config(&apConfig);
  apConfig.max_connection = 8;
  apConfig.beacon_interval = 1000; //1024ms
  wifi_softap_set_config(&apConfig);
  WiFi.softAP(
    ("ESP-"+String(ESP.getChipId()))
   ,"12345678"
   ,1
   ,false
  );
  
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
  
  Serial.begin(115200);
  Serial.println("boot");
  delay(10);
}

uint32_t timerScan=0;

void loop() {
  if (millis()-timerScan>1000) {
    timerScan=millis();
    struct scan_config scanConfig;
    scanConfig.ssid = 0;
    scanConfig.bssid = 0; 
    scanConfig.channel = 1;
    scanConfig.show_hidden = false;
    scanConfig.scan_type =
      WIFI_SCAN_TYPE_PASSIVE;
    scanConfig.scan_time.passive = 103;
    wifi_station_scan(
      &scanConfig,
      reinterpret_cast<scan_done_cb_t>(
        &scan_done));
    yield();
  }
}

void ICACHE_FLASH_ATTR
scan_done(void *arg, STATUS status) {
  uint8 bssid[6];
  uint8 ssid[33];
  uint8 cnt=0;
  Serial.println("scan_done");
  if(status == OK){
    struct bss_info *bss_link = (
      struct bss_info *)arg;
    while (bss_link != NULL) {
      cnt++;
      // ap
      os_memset(ssid, 0, 33);
      os_memcpy(bssid, bss_link->bssid, 6);
      if (bss_link->ssid_len <= 32) {
        os_memcpy(ssid, bss_link->ssid,
          bss_link->ssid_len);
      } else {
        os_memcpy(ssid, bss_link->ssid, 32);
      }
      Serial.print(macToString(bssid));
      Serial.print(" rssi ");
      Serial.print(bss_link->rssi);
      Serial.print(" ch ");
      Serial.print(bss_link->channel);
      Serial.print(" hide ");
      Serial.print(bss_link->is_hidden);
      Serial.println();
      Serial.println((char*)ssid);
      // next
      bss_link = bss_link->next.stqe_next;
    }
  }
  Serial.print("scan_done.status ");
  Serial.println(status);
  Serial.print("scan_done.cnt ");
  Serial.println(cnt);
}

String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}