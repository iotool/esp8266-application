// ESP8266 - Chatbox - Captive Portal
// (C) 2022 by github.com/iotool
// 
// Chat running on ESP8266 wifi chip.
// 
// dynamic access point
// dynamic subnet
// captive portal popup
// captive portal redirect
// max client 8 connections
// max client auto disconnect
// serial debug state
// reboot on out of memory
// reboot use rtc memory for chat
// OTA update for ArduinoDroid

// overwrite esp-sdk (wifi_set_country)
extern "C" {
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define AP_PHYMOD  WIFI_PHY_MODE_11B
#define AP_POWER   20    // 0..20 Lo/Hi
#define AP_CHANNEL 13    // 1..13
#define AP_MAXCON  8     // 1..8
#define AP_CHATMEM 2048  // buffer esp
#define ESP_OUTMEM 2048  // reboot
#define ESP_RTCADR 65    // offset
#define ESP_RTCMEM 443   // buffer rtc

const char* apName = "Chatbox-";
const char* apPass = "";
const char* apHost = "web";

DNSServer dnsServer;
ESP8266WebServer httpServer(80);

// init once at setup
String apSSID="";
String urlHome="";
String urlChatRefresh="4,url=";
String chatData="Admin: Chat gestartet";

// use rtc memory as backup
typedef struct { 
  uint32_t check;
  char chat[ESP_RTCMEM];
} rtcStore;
rtcStore rtcMem;

void setup() {
    
  // serial debug
  Serial.begin(19200);
  Serial.println(F("boot"));
  delay(50);
    
  // RTC restore after reset
  system_rtc_mem_read(
    ESP_RTCADR, &rtcMem, sizeof(rtcMem)
  );
  if (rtcMem.check == 0xDE49) {
    chatData=String(rtcMem.chat);
  }
    
  // wifi: don't write setup to flash
  WiFi.persistent(false); 
    
  // wifi: dynamic SSID by physical MAC
  uint8_t apMAL = WL_MAC_ADDR_LENGTH;
  uint8_t apMAC[apMAL];
  WiFi.softAPmacAddress(apMAC);
  apSSID += (apMAC[apMAL-2]<16?"0":"");
  apSSID += String(apMAC[apMAL-2],HEX);
  apSSID += (apMAC[apMAL-1]<16?"0":"");
  apSSID += String(apMAC[apMAL-1],HEX);
  apSSID.toUpperCase(); 
  apSSID = String(apName)+apSSID;
  apSSID += F(" (http://");
  apSSID += String(apHost);
  apSSID += F(".local)");

  // wifi: fast network scan (1 channel)
  wifi_country_t apCountry = {
    .cc = "EU",          // country
    .schan = AP_CHANNEL, // start chnl
    .nchan = 1,          // num of chnl
    .policy = WIFI_COUNTRY_POLICY_MANUAL 
  };
  wifi_set_country(&apCountry);

  // wifi: cap.portal popup public ip
  // IPAddress apIP(172,0,0,1);
  uint8_t apIP2=apMAC[apMAL-2]%224+32;
  uint8_t apIP3=apMAC[apMAL-1];
  IPAddress apIP(172,apIP2,apIP3,1);
  IPAddress apNetMsk(255,255,255,0);
  
  // wifi: start access point 
  WiFi.mode(WIFI_AP_STA);
  WiFi.setPhyMode(AP_PHYMOD);
  WiFi.setOutputPower(AP_POWER);
  WiFi.hostname(apHost);
  WiFi.softAPConfig(apIP,apIP,apNetMsk);
  WiFi.softAP(apSSID,apPass,AP_CHANNEL);
  if (String(apPass).length()==0) {
    WiFi.softAP(apSSID);
  }
    
  // wifi: increase ap clients
  struct softap_config apConfig;
  wifi_softap_get_config(&apConfig);
  apConfig.max_connection = AP_MAXCON;
  wifi_softap_set_config(&apConfig);

  // dns: spoofing for http
  dnsServer.start(53, "*", apIP);

  // dns: multicast DNS web.local
  if (MDNS.begin(apHost)) {
    MDNS.addService("http","tcp",80);
  }
  
  // http: webserver
  urlHome = F("http://");
  urlHome += WiFi.softAPIP().toString();
  urlChatRefresh += urlHome;
  urlHome += F("/home");
  urlChatRefresh += F("/chat");
  httpServer.onNotFound(onHttpToHome);
  httpServer.on("/home",onHttpHome);
  httpServer.on("/chat",onHttpChat);
  httpServer.on("/chatf",onHttpChatFrm);
  httpServer.on("/chata",onHttpChatAdd);
  httpServer.begin();

  // ota: update
  ArduinoOTA.setPort(8266);
  /* ArduinoOTA.setPassword(
    (const char *)"ChatB0x"
  ); */
  ArduinoOTA.onStart([]()
    {Serial.println("OTA Start");});
  ArduinoOTA.onEnd([]()
    {Serial.println("\nOTA End");});
  ArduinoOTA.begin();
    
  delay(100);
}

uint32_t uptime=0;
uint8_t requests=0;
boolean disconnectClients=false;
uint8_t disconnectTimer=0;

void loop() {
  uptime=millis();
  doDebug();
  doServer();
  doDisconnect();
  doReboot();
  yield();
}

void doTasks() {
  delay(1);
  yield();
}

void doDisconnect() {
  // disconnect all clients
  // if max clients connected
  if (uptime%107==0) {
    // max client timer 
    if (WiFi.softAPgetStationNum()
        == AP_MAXCON) {
      if (disconnectTimer==0) {
        // start timer 10s
        disconnectTimer=100;
      }
    } else {
      // stop timer
      disconnectTimer=0;
    }
    if (disconnectTimer>1) {
      // increase timer 13ms
      disconnectTimer--;
      delay(1);
    } else if (disconnectTimer==1) {
      // end timer, if max client > 3s
      disconnectTimer=0;
      disconnectClients=true;
    }
  }
  if (disconnectClients) {
    // disconnect all clients
    disconnectClients=false;
    WiFi.softAPdisconnect(false);
    WiFi.softAP(
      apSSID,apPass,AP_CHANNEL
    );
    if (String(apPass).length()==0) {
      WiFi.softAP(apSSID);
    }
  }
}

void doBackup() {
  rtcMem.check = 0xDE49;
  if (chatData.length()<ESP_RTCMEM){
    chatData.toCharArray(
      rtcMem.chat,chatData.length()+1
    );
  } else {
    chatData.toCharArray(
      rtcMem.chat,ESP_RTCMEM-1
    );
    rtcMem.chat[ESP_RTCMEM]=0;
  }
  system_rtc_mem_write(
    ESP_RTCADR,&rtcMem,sizeof(rtcMem)
  );     
}

void doReboot() {
  if (uptime%107==0) {
    if (system_get_free_heap_size()
        < ESP_OUTMEM) {
      doBackup();
      delay(300);
      WiFi.softAPdisconnect(true);
      delay(200);
      ESP.restart();
    }
  }
}

void doDebug(){
  if (uptime%1009==0) {
    // debug status
    Serial.print(uptime);
    Serial.print(F("ms,"));
    Serial.print(
      system_get_free_heap_size());
    Serial.print(F("B,"));
    Serial.print(requests);
    Serial.print(F("R,"));
    Serial.print(
      WiFi.softAPgetStationNum());    
    Serial.print(F("C,"));
    Serial.print(disconnectTimer);    
    Serial.print(F("D,"));
    Serial.print(rtcMem.check);    
    Serial.print(F("T"));
    Serial.println();
    requests=0;
    doTasks();
  }
}

void doServer() {
  if (uptime%9==0) {
    // 9: 111 Req/s ~  5.4 KB/Req
    // 11: 90 Req/s ~  6.6 KB/Req
    // 13: 76 Req/s ~  7.7 KB/Req
    // 17: 58 Req/s ~ 10.3 KB/Req
    dnsServer.processNextRequest();
    doTasks();
    httpServer.handleClient();
    doTasks();
    ArduinoOTA.handle();
    doTasks();
  }
}

String getChat() {
  return chatData;
}
    
void addChat(String usr, String msg) {
  chatData = usr+": "+msg+"<br><br>"+chatData;
  chatData.substring(0,AP_CHATMEM);
  doBackup();
}

void onHttpToHome() {
  httpServer.sendHeader(
    F("Location"), urlHome
  );
  httpServer.send(
    302, F("text/plain"), F("")
  );
  requests++;
  doTasks();
}

void onHttpHome() {
  httpServer.send(
    200, F("text/html"), F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>WiFi Chat</h1><hr><br>Dieser Hotspot bietet einen lokalen Chat. Die Nachrichten werden nicht dauerhaft gespeichert. Bitte alle rechtlichen Regeln einhalten und keine Beleidigungen!<br><br><a href=/chat>OK - akzeptiert</a></body></html>")
  );
  requests++;
  doTasks();
}

void onHttpChat() {
  httpServer.sendHeader(
    F("Refresh"), F("8")
  );
  /* --- bug garbage collection ---
  httpServer.setContentLength(
    CONTENT_LENGTH_UNKNOWN
  );
  httpServer.send(
    200,F("text/html"),F("")
  );
  httpServer.sendContent(
    F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body>chat <a href=/chatf>add</a><hr>"));
  httpServer.sendContent(
    F("</body></html>"));
  httpServer.sendContent(F(""));
  httpServer.client().stop();
  --- */
  /* dynamic html */
  String html="";
  html+=F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>WiFi Chat</h1><hr><a href=/chatf>neue Nachricht</a><hr><br>");
  html+=getChat();
  html+=F("</body></html>");
  httpServer.send(
    200, F("text/html"),html
  );
  html="";
  requests++;
  doTasks();
}

void onHttpChatFrm() {
  httpServer.send(
    200, F("text/html"), F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>WiFi Chat</h1><hr><a href=/chat>abbrechen</a><hr><br><form action=/chata method=POST>Absender:<br><input type=text name=usr><br><br>Nachricht:<br><textarea name=msg rows=3 cols=22></textarea><br><br><input type=submit value=senden></form></body></html>")
  );
  requests++;
  doTasks();
}

String maskHttpArg(String id) {
  String prm = httpServer.arg(id);
  prm.replace("<","&lt;");
  prm.replace(">","&gt;");
  prm.replace("\r","\n");
  prm.replace("\n"," ");
  prm.replace("  "," ");
  return prm;
}

void onHttpChatAdd() {
  httpServer.sendHeader(
    F("Refresh"), urlChatRefresh
  );
  String usr = maskHttpArg("usr");
  usr.substring(0,20);
  String msg = maskHttpArg("msg");
  msg.substring(0,64-usr.length());
  addChat(usr,msg);
  String html="";
  html+=F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>WiFi Chat</h1><hr><a href=/chat>weiter</a><hr><br>");
  html+=usr;
  html+=F(": ");
  html+=msg;
  html+=F("</body></html>");
  httpServer.send(
    200, F("text/html"),html
  );
  html=""; usr=""; msg="";
  requests++;
  doTasks();
}