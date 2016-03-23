/*****************************************************************************
The MIT License (MIT)

Copyright (c) 2015 by bbx10node@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 **************************************************************************/

/*
 * ESP8266 Arduino IDE
 *
 * WiFi clock for PCD8544 LCD display periodically synchronized with Network
 * Time Protocol servers.
 *
 * Synchronizes with time.nist.gov which randomly selects from a pool of
 * NTP (Network Time Protocol) servers.
 *
 * The PCD8544 LCD driver is a fork of the Adafruit driver with changes for the ESP8266.
 * Be sure to use the esp8266 branch!
 *
 * https://github.com/bbx10/Adafruit-PCD8544-Nokia-5110-LCD-library/tree/esp8266
 *
 * The Time library provides date and time with external date time sources. The library
 * requests UTC date and time from a Network Time Protocol (NTP) server every 5 minutes.
 * In between calls to the NTP server, the library uses the millis() function to update
 * the date and time. The NTP part of this program is based on the Time_NTP example.
 *
 * https://github.com/PaulStoffregen/Time
 *
 * The Adafruit_GFX library should be installed using the Arduino IDE Library manager.
 * No changes are needed for the ESP8266.
 *
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Time.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

static const char ssid[] = "********";      //  your network SSID (name)
static const char pass[] = "********";      // your network password

/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
static const char ntpServerName[] = "time.nist.gov";
static const char tzName[] = "  UTC";
static const int timeZone = 0;     // UTC
//static const int timeZone = -5;  // Eastern Standard Time (USA)
//static const int timeZone = -4;  // Eastern Daylight Time (USA)
//static const int timeZone = -8;  // Pacific Standard Time (USA)
//static const int timeZone = -7;  // Pacific Daylight Time (USA)

WiFiUDP Udp;
uint16_t localPort;  // local port to listen for UDP packets

// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 14 on Huzzah ESP8266
// MOSI is LCD DIN - this is pin 13 on an Huzzah ESP8266
// pin 12 - Data/Command select (D/C) on an Huzzah ESP8266
// pin 5 - LCD chip select (CS)
// pin 4 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(12, 5, 4);

void setup()
{
  Serial.begin(115200);

  // Initialize LCD
  display.begin();
  display.setContrast(50);
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.clearDisplay();
  display.print("Connecting...");
  display.display();

  Serial.println(F("ESP8266 NTP clock on PCD8544 LCD display"));
  Serial.print(F("Connecting to "));
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.print(F("IP number assigned by DHCP is "));
  Serial.println(WiFi.localIP());

  // Seed random with values unique to this device
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  uint32_t seed1 =
    (macAddr[5] << 24) | (macAddr[4] << 16) |
    (macAddr[3] << 8)  | macAddr[2];
  randomSeed(WiFi.localIP() + seed1 + micros());
  localPort = random(1024, 65535);

  Serial.println(F("Starting UDP"));
  Udp.begin(localPort);
  Serial.print(F("Local port: "));
  Serial.println(Udp.localPort());
  Serial.println(F("waiting for sync"));
  setSyncProvider(getNtpTime);
  setSyncInterval(5 * 60);

  // If the analogWrite is moved up too early in setup(), it triggers
  // a crash. Works OK here.
  // If crashing persists, turn the backlight on using digitalWrite
  // like this.
  // pinMode(15, OUTPUT);
  // digitalWrite(15, HIGH);
  // Backlight control on pin 15 at 25% (256/1024).
  analogWrite(15, 256);
}

void loop()
{
  static time_t prevDisplay = 0; // when the digital clock was displayed
  timeStatus_t ts = timeStatus();

  switch (ts) {
    case timeNeedsSync:
    case timeSet:
      if (now() != prevDisplay) { //update the display only if time has changed
        prevDisplay = now();
        digitalClockDisplay();
        if (ts == timeNeedsSync) {
          Serial.println(F("time needs sync"));
        }
      }
      break;
    case timeNotSet:
      Serial.println(F("Time not set"));
      now();
      break;
    default:
      break;
  }
}

const uint8_t SKINNY_COLON[] PROGMEM = {
  B00000000,
  B00000000,
  B01100000,
  B11110000,
  B11110000,
  B01100000,
  B00000000,
  B00000000,
  B01100000,
  B11110000,
  B11110000,
  B01100000,
  B00000000,
  B00000000,
};

void digitalClockDisplay() {
  tmElements_t tm;
  char *dayOfWeek;

  breakTime(now(), tm);
  dayOfWeek = dayShortStr(tm.Wday);
  // digital clock display of the time
  Serial.printf("%s %02d %02d %04d %02d:%02d:%02d\r\n",
                dayOfWeek, tm.Month, tm.Day, tm.Year + 1970,
                tm.Hour, tm.Minute, tm.Second);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);
  display.printf("%s %02d %02d %04d\n",
                 dayOfWeek, tm.Month, tm.Day, tm.Year + 1970);
  display.setTextSize(2);
  display.setCursor(0, 16);
  display.printf("%02d", tm.Hour);
  display.drawBitmap(24, 16, SKINNY_COLON, 4, 14, 1);
  display.setCursor(30, 16);
  display.printf("%02d", tm.Minute);
  display.drawBitmap(54, 16, SKINNY_COLON, 4, 14, 1);
  display.setCursor(60, 16);
  display.printf("%02d", tm.Second);

  display.setCursor(0, 32);
  display.print(tzName);
  display.display();
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress timeServerIP; // time.nist.gov NTP server address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.print(F("Transmit NTP Request "));
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while ((millis() - beginWait) < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println(F("Receive NTP Response"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + (timeZone * SECS_PER_HOUR);
    }
  }
  Serial.println(F("No NTP Response :-("));
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
