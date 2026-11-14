#pragma once
// Board pins and network defaults, shared by the 883M (src/) and ILA200 (src/ila200/) firmware.
#if defined(BOARD_CYTRON_IRIV_IOC)
// Cytron IRIV IO Controller (RP2350A): pins from its datasheet. Isolated RS485 on UART1 with
// automatic direction control, so there is no DE/RE pin. Network is wired Ethernet (W5500).
#define STATUS_LED_PIN 29  // USR LED
#define BUZZER_PIN 11      // Internal passive buzzer
#define USER_BUTTON_PIN 28 // USER button, active low
#define RXD_PIN        25  // UART1 RX (Serial2) to the isolated RS485 driver
#define TXD_PIN        24  // UART1 TX (Serial2)
#define RS485_CTRL_PIN -1  // Direction is switched by the RS485 driver itself
#ifndef ETHERNET_HOSTNAME
#define ETHERNET_HOSTNAME "ila-gateway-883m"
#endif
#define RELAY_ALERT_PIN 12 // DO0, isolated output (ILA200 alert relay)
#define RELAY_ALARM_PIN 13 // DO1, isolated output (ILA200 alarm relay)
#define ETHERNET_FALLBACK_IP "192.168.4.1" // used when no DHCP server answers
#else
#if defined(BOARD_CYTRON_MAKER_ESP32)
// Cytron Maker ESP32: no RGB LED; the onboard GPIO2 LED shows status instead.
#define STATUS_LED_PIN 2
#define BUZZER_PIN 26     // Onboard piezo (has a mute switch)
#define USER_BUTTON_PIN 4 // Onboard user button
#else
#define RGB_PIN 48
#define NUM_PIXELS 1
#define BUZZER_PIN 5
#define USER_BUTTON_PIN 0
#endif
#define RXD_PIN        18 // Connect to RO (Receiver Output)
#define TXD_PIN        17 // Connect to DI (Driver Input)
#define RS485_CTRL_PIN 14 // Connect to DE and RE
#endif
#define WIFI_AP_SSID "ILA_Sensor_AP"
#define WIFI_AP_PASS "12345678"
