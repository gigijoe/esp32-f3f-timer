#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "CRSFforArduino.hpp"
#include "player.h"
#include "lcd204.h"
#include "f3f.h"

#define BUZZER 21

void buzzerSetup() {
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
}

static uint32_t buzzerTime = 0;
void buzzerStart()
{
  buzzerTime = millis();
  digitalWrite(BUZZER, HIGH);
}

void buzzerLoop()
{
  if(buzzerTime == 0)
    return;
  if(millis() - buzzerTime > 100) {
    digitalWrite(BUZZER, LOW);
    buzzerTime = 0;
  }
}

HardwareSerial teleBusA = HardwareSerial(1);
HardwareSerial teleBusB = HardwareSerial(2);

CRSFforArduino crsfA = CRSFforArduino(&teleBusA, 18, 19);
//CRSFforArduino crsfB = CRSFforArduino(&teleBusB, 16, 17);
CRSFforArduino crsfB = CRSFforArduino(&teleBusB, 12, 14);

uint32_t serNoA = 0;
uint32_t serNoB = 0;

// microSD Card Reader connections
#define SD_CS          5
#define SPI_MOSI      23 
#define SPI_MISO      13
#define SPI_SCK       27

/*
* CRSF
*/

void crsfSetup()
{
    Serial.println("CRSF setup");

    /* Initialise CRSF for Arduino */
    if (!crsfA.begin()) {
      Serial.println("CRSF A initialization failed!");
        return;
    }

    if (!crsfB.begin()) {
      Serial.println("CRSF B initialization failed!");
        return;
    }

    /* Show the user that the sketch is ready. */
    Serial.println("Ready");
    //delay(1000);
}

#define VIEW_RC_CHANNELS 0

#if VIEW_RC_CHANNELS > 0
const int channelCount = 8;
const char *channelNames[crsfProtocol::RC_CHANNEL_COUNT] = {
    "A", "E", "T", "R", "Aux1", "Aux2", "Aux3", "Aux4", "Aux5", "Aux6", "Aux7", "Aux8", "Aux9", "Aux10", "Aux11", "Aux12"};

static_assert(channelCount <= crsfProtocol::RC_CHANNEL_COUNT, "The number of RC channels must be less than or equal to the maximum number of RC channels supported by CRSF.");
#endif

void crsfLoop()
{
/* Call crsf.update() in the main loop to process CRSF packets. */
crsfA.update();
/* Call crsf.update() in the main loop to process CRSF packets. */
crsfB.update();

#if VIEW_RC_CHANNELS > 0  
    // Use timeNow to store the current time in milliseconds.
    uint32_t timeNow = millis();

/* You can process RC channel data here. */

    static unsigned long lastPrint = 0;
    if (timeNow - lastPrint >= 100)
    {
        lastPrint = timeNow;
        Serial.print("Base A Channels <");
        for (uint8_t i = 0; i < channelCount; i++)
        {
          Serial.print(channelNames[i]);
          Serial.print(": ");
          Serial.print(crsfA.rcToUs(crsfA.getChannel(i + 1)));
            if (i < channelCount - 1)
            {
              Serial.print(", ");
            }
        }
        Serial.println(">");

        lastPrint = timeNow;
        Serial.print("Base B Channels <");
        for (uint8_t i = 0; i < channelCount; i++)
        {
          Serial.print(channelNames[i]);
          Serial.print(": ");
          Serial.print(crsfB.rcToUs(crsfB.getChannel(i + 1)));
            if (i < channelCount - 1)
            {
              Serial.print(", ");
            }
        }
        Serial.println(">");        
    }
#else
    // Use timeNow to store the current time in milliseconds.
    uint32_t timeNow = millis();

    static unsigned long lastPrint = 0;
    if (timeNow - lastPrint >= 300)
    {
      if(crsfA.rcToUs(crsfA.getChannel(1)) == 2000) {
        Serial.printf("<A%04u>\r\n", serNoA % 10000);
        F3F_TiggleBaseA(serNoA++);
        yield();
        buzzerStart();
      }

      if(crsfB.rcToUs(crsfB.getChannel(1)) == 2000) {
        Serial.printf("<B%04u>\r\n", serNoB % 10000);
        F3F_TiggleBaseB(serNoB++);
        yield();
        buzzerStart();
      }
      lastPrint = timeNow;
    }
#endif
}

/*
* Multicast UDP
*/

#include "WiFi.h"
#include "AsyncUDP.h"

AsyncUDP udp;

const char *ssid[] = {"F3F-AP-N", "F3F-AP-AC", "AEi"};
const char *password[] = {"0925322362", "0925322362", "0226962665"};
static uint8_t apIndex = 0;

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("Connected to AP successfully!");
}
 
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  lcdPrintRow(0, "AP: %s (%d)", WiFi.SSID(), WiFi.RSSI());
  lcdPrintRow(1, "IP: %s", WiFi.localIP().toString());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  Serial.print(bssid[5], HEX);
  Serial.print(":");
  Serial.print(bssid[4], HEX);
  Serial.print(":");
  Serial.print(bssid[3], HEX);
  Serial.print(":");
  Serial.print(bssid[2], HEX);
  Serial.print(":");
  Serial.print(bssid[1], HEX);
  Serial.print(":");
  Serial.println(bssid[0], HEX);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType(0);
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();

  if (udp.listenMulticast(IPAddress(224, 0, 0, 3), 9003)) {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    udp.onPacket([](AsyncUDPPacket packet) {
      if (packet.isMulticast()) {  // <<<====   Only listen to Multicasting Messages
#if 0
        Serial.print("UDP Packet Type: Multicast");
        Serial.print(", From: ");
        Serial.print(packet.remoteIP());
        Serial.print(":");
        Serial.print(packet.remotePort());
        Serial.print(", To: ");
        Serial.print(packet.localIP());
        Serial.print(":");
        Serial.print(packet.localPort());
        Serial.print(", Length: ");
        Serial.print(packet.length());
        Serial.print(", Data: ");
        Serial.write(packet.data(), packet.length());
        Serial.println();
#endif
        uint8_t *p = packet.data();
        if(p[0] == '<' && p[6] == '>') {
          if(p[1] == 'A') {
            uint8_t buf[5];
            memcpy(buf, &p[2], 4);
            buf[4] = 0;
            uint32_t serNo = atoi((const char *)buf) % 10000;
            if(serNo != serNoA) {
              buzzerStart();
              F3F_TiggleBaseA(serNo);
              yield();
              serNoA = serNo;              
            }
          } else if(p[1] == 'B') {
            uint8_t buf[5];
            memcpy(buf, &p[2], 4);
            buf[4] = 0;
            uint32_t serNo = atoi((const char *)buf) % 10000;
            if(serNo != serNoB) {
              buzzerStart();
              F3F_TiggleBaseB(serNo);
              yield();
              serNoB = serNo;
            }
          }
        }
      }
    });    
  }
}
 
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.print("Disconnected from WiFi AP ");
  Serial.println(WiFi.SSID());
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);

  //lcdPrintRow(1, "%d", info.wifi_sta_disconnected.reason);

  apIndex++;
  if(apIndex >= sizeof(ssid) / sizeof(ssid[0]))
    apIndex = 0;
  
  Serial.print("Trying to connect AP ");
  Serial.println(ssid[apIndex]);
  //WiFi.begin(ssid[apIndex], password[apIndex]);
  WiFi.STA.connect(ssid[apIndex], password[apIndex]);

  lcdPrintRow(0, "AP: %s", ssid[apIndex]);
  lcdPrintRow(1, "Connecting ...");
}
 
void mcastSetup()
{
  Serial.begin(115200);
  WiFi.disconnect(true);
  delay(1000);
 
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  /* Remove WiFi event
  Serial.print("WiFi Event ID: ");
  Serial.println(eventID);
  WiFi.removeEvent(eventID);*/
  WiFi.STA.begin();
  //WiFi.begin(ssid[apIndex], password[apIndex]);

  if(!WiFi.STA.waitStatusBits(ESP_NETIF_STARTED_BIT, 10000)){
    Serial.println("Failed to start WiFi!");
  }
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Maximum WIFI_POWER_19_5dBm
  int pwr = WiFi.getTxPower();
  Serial.printf("TX Power: %.1fdBm\n", pwr * 0.25);

#if 0
  Serial.println("WiFi scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("Scan done");
  if (n == 0) {
      Serial.println("No networks found");
  } else {
      Serial.print(n);
      Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
          // Print SSID and RSSI for each network found
          Serial.print(i + 1);
          Serial.print(": ");
          Serial.print(WiFi.SSID(i));
          Serial.print(" (");
          Serial.print(WiFi.RSSI(i));
          Serial.print(")");
          Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
          
          String net = WiFi.SSID(i);            
          for (int x = 0; x < net.length(); ++x){
            Serial.print((int)net[x]);
            Serial.print(" ");
          }
          Serial.println("\n----------");
      }
  }
  Serial.println("");
#endif

  WiFi.STA.connect(ssid[apIndex], password[apIndex]);

  lcdPrintRow(0, "AP: %s", ssid[apIndex]);
  lcdPrintRow(1, "Connecting ...");
  
  Serial.println();
  Serial.println();
  Serial.println("Wait for WiFi... ");
}

HeadLineType s_headLine = showCpuUsage;

void mcastLoop(){
  static uint32_t lastTime = 0;
  if(WiFi.status() == WL_CONNECTED) {
    if(millis() - lastTime >= 1000) {
      if(s_headLine == showCpuUsage)
        lcdPrintRow(0, "AP: %s (%d)", WiFi.SSID(), WiFi.RSSI());
      lastTime = millis();
    }
  }
}

/*
* F3F
*/

/*
hw_timer_t *  timer1 = NULL;
portMUX_TYPE  timerMux1 = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer1() {
  portENTER_CRITICAL_ISR(&timerMux1);
  portEXIT_CRITICAL_ISR(&timerMux1);
}
*/

#define BTN_START 32
#define BTN_A 36
#define BTN_B 39
#define BTN_STOP 33

#define BASE_A 34
#define BASE_B 35

void setup() {
  // Set microSD Card CS as OUTPUT and set HIGH
  pinMode(SD_CS, OUTPUT);      
  digitalWrite(SD_CS, HIGH); 

  pinMode(BTN_START, INPUT_PULLUP);      //input pull-up resistor is enabled
  pinMode(BTN_A, INPUT);
  pinMode(BTN_B, INPUT);
  pinMode(BTN_STOP, INPUT_PULLUP);      //input pull-up resistor is enabled

  pinMode(BASE_A, INPUT);      //input pull-up resistor is enabled
  pinMode(BASE_B, INPUT);

/* 
  timer1 = timerBegin(1000);               // 1k Hz
	timerAttachInterrupt(timer1, &onTimer1);   // attach handler, trigger on edge 
	timerAlarm(timer1, 1000L, true, 0);            // 1000 * 1us = 1ms, single shot
*/
  Serial.begin(115200);

  lcd2004Setup();

  // Initialize SPI bus for microSD Card
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);  
  // Start microSD Card
  if(!SD.begin(SD_CS, SPI, 40000000))
  {
    Serial.println("Error accessing microSD card!");
    lcdPrintRow(1, "  !!! SD Error !!!  ");
    while(true); 
  }

  crsfSetup();
  buzzerSetup();
  mcastSetup();

  F3F_Init([](HeadLineType type) {
    s_headLine = type;
    switch(type) {
      case showCpuUsage:
        if(WiFi.status() == WL_CONNECTED) {
          lcdPrintRow(0, "AP: %s (%d)", WiFi.SSID(), WiFi.RSSI());
          lcdPrintRow(1, "IP: %s", WiFi.localIP().toString());        
        }
        break;
      case showLastRecord:
        lcdPrintRow(0, "Last record %s", F3F_LastRecord());
        break;
      case showF3fMode:
        switch(F3F_Mode()) {
          case f3fCompetition:  lcdPrintRow(0, "F3F Competition");
            break;
          case f3fTraining: lcdPrintRow(0, "F3F Training");
            break;
        }
        break;
      case showWindData:
        lcdPrintRow(0, "Wind speed & range  ");
        break;
      case showVolume: 
        lcdPrintRow(0, "Volume : %d dbm", Mp3Player_GetVolume());
        break;
      default:
        break;
    }
  });

  xTaskCreatePinnedToCore(
    F3F_Task,      // Function that should be called
    "F3F_Task",    // Name of the task (for debugging)
    8192,               // Stack size (bytes)
    NULL,               // Parameter to pass
    (configMAX_PRIORITIES -1),                  // Task priority
    NULL,               // Task handle
    1          // Core you want to run the task on (0 or 1)
  );
}

void loop() {
  if(digitalRead(BTN_START) == LOW) {
    F3F_KeyStart();
    //buzzerStart();
  }

  if(digitalRead(BTN_A) == LOW) {
    F3F_KeyA();
    //buzzerStart();
  }

  if(digitalRead(BTN_B) == LOW) {
    F3F_KeyB();
    //buzzerStart();
  }

  if(digitalRead(BTN_STOP) == LOW) {
    F3F_KeyStop();
    //buzzerStart();
  }

  if(digitalRead(BASE_A) == LOW) {
    buzzerStart();
    F3F_KeyBaseA();
    yield();
  }

  if(digitalRead(BASE_B) == LOW) {
    buzzerStart();
    F3F_KeyBaseB();
    yield();
  }

  lcd2004Loop();
  crsfLoop();
  mcastLoop();
  buzzerLoop();
}
/*
void audio_info(const char *info){
  Serial.print("info        "); Serial.println(info);
}
*/