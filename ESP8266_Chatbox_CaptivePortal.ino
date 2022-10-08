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
// OTA password protect by cli
// admin cli for status, ota, restart
// dynamic html without memory issue
// validate input size

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

/* --- configuration --- */

#define AP_PHYMOD  WIFI_PHY_MODE_11B
#define AP_POWER   20    // 0..20 Lo/Hi
#define AP_CHANNEL 13    // 1..13
#define AP_MAXCON  8     // 1..8
#define AP_CHATMEM 2048  // buffer esp
#define ESP_OUTMEM 2048  // reboot
#define ESP_RTCADR 65    // offset
#define ESP_RTCMEM 443   // buffer rtc
#define LED_OFF    0
#define LED_ON     1
#define CHAT_MLEN  56    // msg length
#define CHAT_MARY  1     // msg array 64

const char* apName = "Chatbox-";
const char* apPass = "";
const char* apHost = "web";

/* --- variables --- */

DNSServer dnsServer;
ESP8266WebServer httpServer(80);

// init once at setup
String apSSID="";
String urlHome="";
String urlChatRefresh="4,url=";

// use rtc memory as backup
typedef struct { 
  uint32_t check;
  char chat[ESP_RTCMEM];
} rtcStore;
rtcStore rtcMem;

// V1: use DRAM for buffer
String chatData="Admin~WEB: Welcome\n";

// V2: 64B/message (static ~ nicht DRAM)
typedef struct {
  uint16_t node;     // mac address node
  uint16_t id:10;    // sequence message
  uint8_t  mlen:6;   // length message
  uint8_t  slen:4;   // length sender
  uint8_t  rlen:4;   // length receiver
  uint32_t age:24;   // 250ms slots
  char     data[CHAT_MLEN]; // buffer
} chatMsgT;
static chatMsgT chatMsgB[CHAT_MARY]={0};

void setup() {

  // builtin led
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
    
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
    chatData=chatData.substring(
        0,chatData.lastIndexOf("\n"));
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
  httpServer.on("/cli",onHttpCli);
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
  // ArduinoOTA.begin();
  
  delay(100);
}

/* --- main --- */

// Flags
struct {
  uint8_t enableCliCommand:1;
  uint8_t enableOtaUpdate:1;
  uint8_t beginOtaServer:1;
  uint8_t disconnectClients:1;
  uint8_t builtinLedMode:2;
  uint8_t reserve:2;
} flag = {0,0,0,0,0,0};

// time
uint32_t uptime=0;
uint32_t uptimeLast=0;

void loop() {
  uptimeLast=uptime;
  uptime=millis();
  if (uptime!=uptimeLast) {
    // only one per millisecond
    doLed();
    doDebug();
    doServer();
    doDisconnect();
    doReboot();
  }
  yield();
}

void doServer() {
  if (uptime%11==0) {
    // 9: 111 Req/s ~  5.4 KB/Req
    // 11: 90 Req/s ~  6.6 KB/Req
    // 13: 76 Req/s ~  7.7 KB/Req
    // 17: 58 Req/s ~ 10.3 KB/Req
    dnsServer.processNextRequest();
    yield();
    httpServer.handleClient();
    yield();
    if (flag.enableOtaUpdate == 1) {
      if (flag.beginOtaServer == 0) {
        flag.beginOtaServer = 1;
        ArduinoOTA.begin();
      } else {
        ArduinoOTA.handle();
      }
      yield();
    }
  }
}

/* --- resilience --- */

uint8_t disconnectTimer=0;

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
    } else if (disconnectTimer==1) {
      // end timer, if max client > 3s
      disconnectTimer=0;
      flag.disconnectClients=1;
    }
  }
  if (flag.disconnectClients==1) {
    // disconnect all clients
    flag.disconnectClients=0;
    WiFi.softAPdisconnect(false);
    WiFi.softAP(
      apSSID,apPass,AP_CHANNEL
    );
    yield();
    if (String(apPass).length()==0) {
      WiFi.softAP(apSSID);
    }
    yield();
  }
}

void doReboot() {
  // reboot before out of memory freeze
  if (uptime%103==0) {
    if (system_get_free_heap_size()
        < ESP_OUTMEM) {
      doBackup();
      delay(300);
      WiFi.softAPdisconnect(true);
      yield();
      delay(200);
      ESP.restart();
    }
  }
}

/* --- debugging --- */

void doLed() {
  switch(flag.builtinLedMode) {
    case LED_OFF:
      digitalWrite(LED_BUILTIN,HIGH);
      break;
    case LED_ON:
      digitalWrite(LED_BUILTIN,LOW);
      break;
  }
  yield();
}

uint8_t requests=0;

void doDebug(){
  if (uptime%1009==0) {
    // debug status
    Serial.print(uptime);
    Serial.print(F("ms,"));
    Serial.print(
      ESP.getFreeHeap());
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
    yield();
  }
}

/* --- chat data --- */

String getChat() {
  return chatData;
}
    
void addChat(
  String ms, 
  String mr,
  String mb) {
  if (mr.length()>0) {
    ms+="@"+mr;
  }
  if (ms.length()>0) {
    ms=ms+": ";
  }
  if (ms.length()+mb.length()>0) {
    chatData=ms+mb+"<br><br>\n" \
            +chatData;
    if (chatData.length()>AP_CHATMEM) {
      chatData=chatData.substring(
        0,AP_CHATMEM);
      chatData=chatData.substring(
        0,chatData.lastIndexOf("\n"));
    }
    doBackup();
  }
  ms="";
  mr="";
  mb="";
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
  yield();
}

/* --- http handler --- */

void onHttpToHome() {
  // redirect to home
  httpServer.sendHeader(
    F("Location"), urlHome
  );
  httpServer.send(
    302, F("text/plain"), F("")
  );
  requests++;
  yield();
}

void onHttpHome() {
  // tiny static landing page
  httpServer.send(
    200, F("text/html"), F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>WiFi Chat</h1><hr><br>Dieser Hotspot bietet einen lokalen Chat. Die Nachrichten werden nicht dauerhaft gespeichert. Bitte alle rechtlichen Regeln einhalten und keine Beleidigungen!<br><br><a href=/chat>OK - akzeptiert</a></body></html>")
  );
  requests++;
  yield();
}

void onHttpChat() {
  // chat messages with autorefresh
  httpServer.sendHeader(
    F("Refresh"), F("8")
  );
  /* -- bugfix garbage collection -- */
  uint32_t timestart = millis();
  uint16_t timeout = 500;
  httpServer.sendHeader(
    F("Connection"), F("close")
  );
  httpServer.setContentLength(
    CONTENT_LENGTH_UNKNOWN
  );
  httpServer.send(
    200,F("text/html"),F("")
  );
  httpServer.sendContent(
    F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>WiFi Chat</h1><hr><a href=/chatf>neue Nachricht</a><hr><br>"));
  httpServer.sendContent(getChat());
  httpServer.sendContent(
    F("</body></html>"));
  yield();
  // close connection
  httpServer.sendContent(F(""));
  while (
    httpServer.client().available()) {
    httpServer.client().read();
    yield();
    if (millis()-timestart>timeout) {
      break;
    }
  }
  yield();
  httpServer.client().stop();
  /* -- dynamic html with string -- *
  String html="";
  html+=F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>WiFi Chat</h1><hr><a href=/chatf>neue Nachricht</a><hr><br>");
  html+=getChat();
  html+=F("</body></html>");
  httpServer.send(
    200, F("text/html"),html
  );
  html="";
  * -- */
  requests++;
  yield();
}

void onHttpChatFrm() {
  httpServer.send(
    200, F("text/html"), F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /><script>function onInp(){var df=document.forms.mf,cl=56-(df.ms.value.length+df.mr.value.length+df.mb.value.length); document.getElementById('mn').innerText=cl;}</script></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>WiFi Chat</h1><hr><a href=/chat>abbrechen</a><hr><br><form name=mf action=/chata method=POST>Absender:<br><input type=text name=ms maxlength=16 onChange=onInp() onkeyup=onInp()><br><br>Empf&auml;nger:<br><input type=text name=mr maxlength=16 onChange=onInp() onkeyup=onInp()><br><br>Nachricht:<br><textarea name=mb rows=3 cols=22 maxlength=56 onChange=onInp() onkeyup=onInp()></textarea><br><br><input type=submit value=senden> <span id=mn></span></form></body></html>")
  );
  requests++;
  yield();
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
  String ms = maskHttpArg("ms");
  ms.replace("~","-");
  ms = ms.substring(0,16);
  String mr = maskHttpArg("mr");
  mr = mr.substring(0,16);
  String mb = maskHttpArg("mb");
  mb=mb.substring(
    0,56-ms.length()-mr.length());
  addChat(ms,mr,mb);
  String html="";
  html+=F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>WiFi Chat</h1><hr><a href=/chat>weiter</a><hr><br>");
  html+=ms;
  if (mr.length()>0) {
    html+=F("@");
    html+=mr;
  }
  html+=F(": ");
  html+=mb;
  html+=F("</body></html>");
  httpServer.send(
    200, F("text/html"),html
  );
  html=""; ms=""; mr=""; mb="";
  requests++;
  yield();
}

void onHttpCli() {
  String text= F(
    "Version: 20221009-0116\n"
    "/cli?cmd=login-password\n"
    "/cli?cmd=logoff\n"
    "/cli?cmd=restart\n"
    "/cli?cmd=ota-on\n"
    "/cli?cmd=ota-off\n"
    "\n"
    "OTA-Update\n\n"
    "1. wifi connect\n"
    "2. disable firewall\n"
    "3. login-password\n"
    "4. ota-on (led on)\n"
    "5. arduinodroid upload wifi\n"
    "   server: web.local\n"
    "   port: 8266\n"
    "6. on-error use restart\n"
    "\n"
    "ESP-Status\n"
  );
  text+="\nUptime:"+String(uptime);
  text+="\nClients:"+String(
    WiFi.softAPgetStationNum());
  text+="\nRequests:"+String(requests);
  text+="\nMemory:"+String(
    system_get_free_heap_size());
  text+="\nRtcCheck:"+String(
    rtcMem.check);
  text+="\nCliLogin:"+String(
    flag.enableCliCommand);
  text+="\nCliOta:"+String(
    flag.enableOtaUpdate);
  httpServer.send(
      200,F("text/plain"),text
  );
  text="";
  String cmd=httpServer.arg("cmd");
  if (cmd == F("login-password")) {
    flag.enableCliCommand=1;
  }
  if (flag.enableCliCommand==1) {
    if (cmd == F("logoff")) {
      flag.enableCliCommand=0;
      flag.enableOtaUpdate = 0;
      flag.builtinLedMode=LED_OFF;
    }
    if (cmd == F("restart")) {
      ESP.restart();
    }
    if (cmd ==  F("ota-off")) {
      flag.enableOtaUpdate = 0;
      flag.builtinLedMode=LED_OFF;
    }
    if (cmd ==  F("ota-on")) {
      flag.enableOtaUpdate = 1;
      flag.builtinLedMode=LED_ON;
    }
  }
  cmd="";
  requests++;
  yield();
}