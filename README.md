# ESP8266 Application

- wifi_softap_set_config.max_connection <= 253
- softAP assign up to 253 ip addresses
- softAP handle max 8 clients and igrone others

## Access Point - Fast Scan - One Channel

- 802.11b -20dBm for longer range
- async network scan in background
- fast network scan on single channel
- dynamic SSID by physical MAC address

## Chatbox - Captive Portal

- browser based chat system
- access point by mac addr Chatbox-****
- subnet by mac addr 172.*.*.1
- captive portal with Android popup
- captive portal redirect to home
- max client 8 connections
- max client auto disconnect after 3s
- serial debug state / free heap
- reboot on out of memory before freez
- reboot use rtc memory for chatdata
- OTA update for ArduinoDroid
- CLI for admin login, ota, restart

## Demo ESP8266

- WiFi Beacon
- WiFi Probe Request
- WiFi Probe Response
- WiFi passive ScanNetwork
