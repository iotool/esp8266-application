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
// reboot use rtc memory for backup
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

#define AP_PHYMOD   WIFI_PHY_MODE_11B
#define AP_POWER    20   // 0..20 Lo/Hi
#define AP_CHANNEL  13   // 1..13
#define AP_MAXCON   8    // 1..8 conns
#define AP_MAXTOUT  15   // 1..25 sec
#define ESP_OUTMEM  4096 // autoreboot
#define ESP_RTCADR  65   // offset
#define LED_OFF     0    // led on
#define LED_ON      1    // led off
#define CHAT_MLEN   56   // msg length
#define CHAT_MARY   64   // msg array
#define CHAT_MRTC   6    // rtc array
#define CTYPE_HTML  1    // content html
#define CTYPE_TEXT  2    // content text

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

// 64B/message (static ~ nicht DRAM)
typedef struct {
  uint8_t  node[2]; // mac address node
  uint16_t id:10;   // message id
  uint8_t  mlen:6;  // length message
  uint8_t  slen:4;  // length sender
  uint8_t  rlen:4;  // length receiver
  uint32_t age:24;  // second age
  char     data[CHAT_MLEN]; // buffer
} chatMsgT;
static chatMsgT chatMsg[CHAT_MARY]={0};

// use rtc memory as backup
typedef struct { 
  uint32_t check;
  chatMsgT chatMsg[CHAT_MRTC];
} rtcMemT;
/* rtcMemT rtcMem; */

void setup() {

  // builtin led
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,HIGH);
    
  // serial debug
  Serial.begin(19200);
  Serial.println(F("boot"));
  delay(50);
    
  // RTC restore after reset
  rtcMemT rtcMem;
  system_rtc_mem_read(
    ESP_RTCADR, &rtcMem, sizeof(rtcMem)
  );
  if (rtcMem.check == 0xDE49) {
    for (int i=0;i<CHAT_MRTC;i++){
      chatMsg[i]=rtcMem.chatMsg[i];
    }
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
  httpServer.on("/chatr",onHttpChatRaw);
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
    updateChatAge();
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
        disconnectTimer=10*AP_MAXTOUT;
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
    struct softap_config apConfig;
    wifi_softap_get_config(&apConfig);
    apConfig.max_connection = AP_MAXCON;
    wifi_softap_set_config(&apConfig);
    yield();
  }
}

void doReboot() {
  // reboot before out of memory freeze
  if (uptime%103==0) {
    if (system_get_free_heap_size()
        < ESP_OUTMEM) {
      doBackup();
      delay(100);
      WiFi.softAPdisconnect(true);
      delay(200);
      yield();
      ESP.restart();
      delay(200);
      yield();
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
    // rtc backup
    rtcMemT rtcMem;
    system_rtc_mem_read(
      ESP_RTCADR,&rtcMem,sizeof(rtcMem)
    );
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

String getChat(byte ctype,byte send) {
  String data="",temp,ms="",mr="",mb="";
  uint8_t count=0;
  uint8_t apMAL = WL_MAC_ADDR_LENGTH;
  uint8_t apMAC[apMAL];
  // node mac
  WiFi.softAPmacAddress(apMAC);
  if (ctype==CTYPE_TEXT){
    data += "\nN"+String(
        apMAC[apMAL-2]*256+
        apMAC[apMAL-1]);
  }
  // count messages
  for (uint8_t i=0;i<CHAT_MARY;i++) {
    if (chatMsg[i].data[0]!=0) {
      count++;
    }
  }
  if (ctype==CTYPE_TEXT){
    data += "\nC"+String(count);
  }
  // messages
  for (uint8_t i=0;i<CHAT_MARY;i++) {
    if (chatMsg[i].data[0]!=0) {
      temp = String(chatMsg[i].data);
      if (chatMsg[i].slen>0) {
        ms = temp.\
          substring(0,chatMsg[i].slen);
      }
      if (chatMsg[i].rlen>0) {
        mr = temp.\
          substring(chatMsg[i].slen,
                chatMsg[i].slen+
                chatMsg[i].rlen);
      }
      if (chatMsg[i].mlen>0) {
        mb = temp.\
          substring(chatMsg[i].slen+
                chatMsg[i].rlen,
                chatMsg[i].slen+
                chatMsg[i].rlen+
                chatMsg[i].mlen);
      }
      if (ctype==CTYPE_HTML){
        data += ms;
        if (chatMsg[i].rlen>0) {
          data += "@";
          data += mr;
        }
        if (chatMsg[i].slen>0
         || chatMsg[i].rlen>0) {
          data += ": ";
        }
        data += mb;
        data += "<br><br>";
      }
      if (ctype==CTYPE_TEXT){
        data += "\nN"+String(
          chatMsg[i].node[0]*256+
          chatMsg[i].node[1]);
        data += "\nI"+String(
          chatMsg[i].id);
        data += "\nA"+String(
          chatMsg[i].age);
        data += "\nS"+ms;
        data += "\nR"+mr;
        data += "\nM"+mb;
      }
      ms=""; mr=""; mb="";
      if (send==1 && data.length()>0) {
        httpServer.sendContent(data);
        data="";
      }
    }
  }
  if (send==1 && data.length()>0) {
    httpServer.sendContent(data);
    data="";
  }
  return data;
}
    
void addChat(
  String ms, 
  String mr,
  String mb) {
  // shift item
  if (chatMsg[0].data[0]!=0) {
    for (uint8_t i=CHAT_MARY-1;i>0;i--){
      chatMsg[i] = chatMsg[i-1];
    }
    chatMsg[0] = {0};
  }
  // add item
  chatMsg[0] = {0};
  // node
  uint8_t apMAL = WL_MAC_ADDR_LENGTH;
  uint8_t apMAC[apMAL];
  WiFi.softAPmacAddress(apMAC);
  chatMsg[0].node[0]=apMAC[apMAL-2];
  chatMsg[0].node[1]=apMAC[apMAL-1];
  // newest messages of this node
  chatMsg[0].age = 0;
  chatMsg[0].age--;
  for (int i=0;i<CHAT_MARY;i++) {
    if (chatMsg[0].node==chatMsg[i].node
     && chatMsg[0].age>chatMsg[i].age){
     chatMsg[0].age = chatMsg[i].age;    
    }
  }
  // next id for this node
  chatMsg[0].id = 0;
  for (int i=0;i<CHAT_MARY;i++) {
    if (chatMsg[0].node==chatMsg[i].node
     && chatMsg[0].age==chatMsg[i].age
     && chatMsg[0].id<chatMsg[i].id){
     chatMsg[0].id = chatMsg[i].id;    
    }
  }
  chatMsg[0].id++;
  chatMsg[0].age = 0;
  chatMsg[0].slen = ms.length();
  chatMsg[0].rlen = mr.length();
  chatMsg[0].mlen = mb.length();
  String data=ms+mr+mb;
  data=data.substring(0,CHAT_MLEN);
  data.toCharArray(
    chatMsg[0].data,data.length()+1
  );
  data="";
  ms="";
  mr="";
  mb="";
  doBackup();
}

uint32_t updateLast = 0;

void updateChatAge() {
  uint32_t periode=millis()-updateLast;
  if (periode>=1000) {
    periode -= 1000; // adjust 1000ms
    updateLast = millis()+periode;
    for (int i=0;i<CHAT_MARY;i++) {
      if (chatMsg[i].data[0]!=0
       && chatMsg[i].age<16777214) {
         chatMsg[i].age++;
      }
    }
  }
  yield();
}

void doBackup() {
  rtcMemT rtcMem = {0};
  rtcMem.check = 0xDE49;
  for (int i=0;i<CHAT_MRTC;i++){
    rtcMem.chatMsg[i]=chatMsg[i];
  }
  system_rtc_mem_write(
    ESP_RTCADR,&rtcMem,sizeof(rtcMem)
  );
  yield();
}

/* --- http page --- */

uint32_t httpTimeStart;

void doHttpStreamBegin(byte ctype) {
  httpTimeStart = millis();
  httpServer.sendHeader(
    F("Connection"), F("close")
  );
  httpServer.setContentLength(
    CONTENT_LENGTH_UNKNOWN
  );
  switch(ctype){
    case CTYPE_HTML:
      httpServer.send(
        200,F("text/html"),F("")
      );
      break;
    case CTYPE_TEXT:
      httpServer.send(
        200,F("text/plain"),F("")
      );
      break;
  }
  yield();
}

void doHttpStreamEnd() {
  httpServer.sendContent(F(""));
  while (
    httpServer.client().available()) {
    httpServer.client().read();
    yield();
    if (millis()-httpTimeStart>500) {
      break;
    }
  }
  yield();
  httpServer.client().stop();
  requests++;
  yield();
}

void doHtmlPageHeader() {
  httpServer.sendContent(
    F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' />"));
}

void doHtmlPageBody() {
  httpServer.sendContent(
    F("</head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF>"));
}

void doHtmlPageFooter() {
  httpServer.sendContent(
    F("</body></html>"));
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
    200, F("text/html"), F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>Chatbox</h1><hr><br>Welcome to the Chatbox hotspot. You can communicate anonymously with your neighbors through this access point. Please behave decently!<br><br>Willkommen beim WiFi Chat. Du kannst &uuml;ber den Hotspot anonym mit deinen Nachbarn kommunizieren. Bitte verhalte dich zivilisiert!<br><br><a href=/chat>OK - accept (akzeptiert)</a></body></html>")
  );
  requests++;
  yield();
}

void onHttpChat() {
  // chat messages with autorefresh
  httpServer.sendHeader(
    F("Refresh"), F("8")
  );
  doHttpStreamBegin(CTYPE_HTML);
  doHtmlPageHeader();
  doHtmlPageBody();
  httpServer.sendContent(
    F("<h1>Chatbox</h1><hr><a href=/chatf>create (erstellen)</a><hr><br>"));
  getChat(CTYPE_HTML,1);
  doHtmlPageFooter();
  doHttpStreamEnd();
}

void onHttpChatFrm() {
  httpServer.send(
    200, F("text/html"), F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /><script>function onInp(){var df=document.forms.mf,cl=56-(df.ms.value.length+df.mr.value.length+df.mb.value.length); document.getElementById('mn').innerText=cl;}</script></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>Chatbox</h1><hr><a href=/chat>cancle (abbrechen)</a><hr><br><form name=mf action=/chata method=POST>Sender (Absender):<br><input type=text name=ms maxlength=16 onChange=onInp() onkeyup=onInp()><br><br>Receiver (Empf&auml;nger):<br><input type=text name=mr maxlength=16 onChange=onInp() onkeyup=onInp()><br><br>Message (Nachricht):<br><textarea name=mb rows=3 cols=22 maxlength=56 onChange=onInp() onkeyup=onInp()></textarea><br><br><input type=submit value=senden> <span id=mn></span></form></body></html>")
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
  if (ms.length()>0) {
    ms.replace("~","-");
    ms = ms.substring(0,16);
  }
  String mr = maskHttpArg("mr");
  if (mr.length()>0) {
    mr = mr.substring(0,16);
  }
  String mb = maskHttpArg("mb");
  mb=mb.substring(
    0,56-ms.length()-mr.length());
  if ((ms.length()+
       mr.length()+
       mb.length())>0) {
    addChat(ms,mr,mb);
  }
  String html="";
  html+=F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1' /></head><body bgcolor=#003366 text=#FFFFCC link=#66FFFF vlink=#66FFFF alink=#FFFFFF><h1>Chatbox</h1><hr><a href=/chat>next (weiter)</a><hr><br>");
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

void onHttpChatRaw() {
  doHttpStreamBegin(CTYPE_TEXT);
  httpServer.sendContent(
    F("V1\nT"));
  httpServer.sendContent(
    String(millis()));
  getChat(CTYPE_TEXT,1);
  doHttpStreamEnd();
}

void onHttpCli() {
  rtcMemT rtcMem;
  system_rtc_mem_read(
    ESP_RTCADR, &rtcMem, sizeof(rtcMem)
  );
  String text= F(
    "Version: 20221010-1513\n"
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