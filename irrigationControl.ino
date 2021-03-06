#include <SimpleTimer.h>
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <BlynkSimpleEsp8266.h>

// Required for Blynk's RTC
#include <TimeLib.h>
#include <WidgetRTC.h>

// Required for OTA
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const char* auth = "fromBlynkApp";
const char* ssid = "ssid";
const char* pw = "pw";

SimpleTimer timer;
WidgetRTC rtc;
WidgetTerminal terminal(V100);

bool firstStart;
int zone1 = 13;                     // WeMos D1 Mini Pro pin D7 (Sprinklers south)
int zone2 = 5;                      // WeMos D1 Mini Pro pin D1 (Sprinklers north)
int zone3 = 4;                      // WeMos D1 Mini Pro pin D2 (Bubblers front)
int zone4 = 14;                     // WeMos D1 Mini Pro pin D5 (Bubblers back)
int bypass = 12;                    // WeMos D1 Mini Pro pin D6
int runStart;                       // Millis that zone started
int runEnd;                         // Millis that zone ended
String lastZone;                    // Last zone that ran
String currentTimeDate;             // Formatted 12:15pm 2/15
int sprinklerRunLimit = 900000L;    // Millis that sprinkler can run before auto shutoff to protect from forgotton manual operation
int bubblerRunLimit = 3600000L;     // Millis that bubblers can run before auto shutoff
int zone12minFlag = 0;

bool checkinFlag;

unsigned long softStartZone1stage1 = 2147483647;
unsigned long softStartZone1stage2 = 2147483647;
unsigned long softStartZone1stage3 = 2147483647;
unsigned long softStopZone1stage1 = 2147483647;
unsigned long softStopZone1stage2 = 2147483647;
unsigned long softStopZone1stage3 = 2147483647;

unsigned long softStartZone2stage1 = 2147483647;
unsigned long softStartZone2stage2 = 2147483647;
unsigned long softStartZone2stage3 = 2147483647;
unsigned long softStopZone2stage1 = 2147483647;
unsigned long softStopZone2stage2 = 2147483647;
unsigned long softStopZone2stage3 = 2147483647;

void setup()
{
  Serial.begin(9600);
  WiFi.hostname("irrigationControl");
  Blynk.begin(auth, ssid, pw);

  //WiFi.softAPdisconnect(true); // Per https://github.com/esp8266/Arduino/issues/676 this turns off AP

  while (Blynk.connect() == false) {
    // Wait until connected
  }

  // Default all relays to off
  digitalWrite(zone1, LOW);
  digitalWrite(zone2, LOW);
  digitalWrite(zone3, LOW);
  digitalWrite(zone4, LOW);

  pinMode(zone1, OUTPUT);
  pinMode(zone2, OUTPUT);
  pinMode(zone3, OUTPUT);
  pinMode(zone4, OUTPUT);

  // START OTA ROUTINE
  ArduinoOTA.setHostname("irrigationControl");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: % u % % \r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[ % u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  // END OTA ROUTINE

  rtc.begin();

  timer.setInterval(2500L, timeMachine);
  timer.setInterval(5000L, overflowProtect);
  //timer.setInterval(1000L, zone12min);
  timer.setInterval(1000L, runTimer);
  timer.setInterval(1000L, softStartZone1);
  timer.setInterval(1000L, softStopZone1);
  timer.setInterval(1000L, softStartZone2);
  timer.setInterval(1000L, softStopZone2);

  Blynk.syncAll();
}

void loop()
{
  Blynk.run();
  timer.run();
  ArduinoOTA.handle();

  if (second() == 0 && !checkinFlag) {
    Blynk.setProperty(V0, "label", String("Set Manual Run                  ") + currentTimeDate);
    checkinFlag = true;
  }
  else if (second() == 2) {
    checkinFlag = false;
  }
}

// Sprinkler soft start and stop to reduce water hammer
void softStartZone1() {

  if (softStartZone1stage1 <= now() && softStartZone1stage2 >= now())  // Start bubbler to help reduce water hammer... between 0 and 3 seconds
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
  }
  if (softStartZone1stage2 <= now() && softStartZone1stage3 >= now())   // then overlap with start of spinklers... between 3 and 6 seconds
  {
    digitalWrite(zone1, HIGH);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
  }
  if (softStartZone1stage3 <= now())   // then turn off bubblers... at 6 seconds
  {
    digitalWrite(zone1, HIGH);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, LOW);
    digitalWrite(zone4, LOW);
    softStartZone1stage1 = 2147483647;
    softStartZone1stage2 = 2147483647;
    softStartZone1stage3 = 2147483647;
  }
}

void softStopZone1() {

  if (softStopZone1stage1 <= now() && softStopZone1stage2 >= now())
  {
    digitalWrite(zone1, HIGH);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
  }
  if (softStopZone1stage2 <= now() && softStopZone1stage3 >= now())
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
  }
  if (softStopZone1stage3 <= now())
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, LOW);
    digitalWrite(zone4, LOW);
    softStopZone1stage1 = 2147483647;
    softStopZone1stage2 = 2147483647;
    softStopZone1stage3 = 2147483647;
  }
}

void softStartZone2() {

  if (softStartZone2stage1 <= now() && softStartZone2stage2 >= now())
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
  }
  if (softStartZone2stage2 <= now() && softStartZone2stage3 >= now())
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, HIGH);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
  }
  if (softStartZone2stage3 <= now())
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, HIGH);
    digitalWrite(zone3, LOW);
    digitalWrite(zone4, LOW);
    softStartZone2stage1 = 2147483647;
    softStartZone2stage2 = 2147483647;
    softStartZone2stage3 = 2147483647;
  }
}

void softStopZone2() {

  if (softStopZone2stage1 <= now() && softStopZone2stage2 >= now())  // Start bubbler to help reduce water hammer... between 0 and 2 seconds
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, HIGH);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
  }
  if (softStopZone2stage2 <= now() && softStopZone2stage3 >= now())   // then turn off spinklers... between 3 and 5 seconds
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
  }
  if (softStopZone2stage3 <= now())   // then turn off bubblers... at 6 seconds
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, LOW);
    digitalWrite(zone4, LOW);
    softStopZone2stage1 = 2147483647;
    softStopZone2stage2 = 2147483647;
    softStopZone2stage3 = 2147483647;
  }
}

BLYNK_WRITE(V2)
{
  int pinData = param.asInt();

  if (pinData == 0) // Triggers when button is released only
  {

  }
}

int id = 0;

void lastZoneClear() {
  lastZone == NULL;
}

BLYNK_WRITE(V0) {
  switch (param.asInt())
  {
    case 1: {                       // OFF

        if (lastZone == "Zone 1") {
          softStopZone1stage1 = now();
          softStopZone1stage2 = now() + 4;
          softStopZone1stage3 = now() + 8;
          timer.setTimeout(3000L, lastZoneClear);
        }
        else if (lastZone == "Zone 2") {
          softStopZone2stage1 = now();
          softStopZone2stage2 = now() + 4;
          softStopZone2stage3 = now() + 8;
          timer.setTimeout(3000L, lastZoneClear);
        }
        else {
          digitalWrite(zone1, LOW);
          digitalWrite(zone2, LOW);
          digitalWrite(zone3, LOW);
          digitalWrite(zone4, LOW);
        }

        if (runStart > 0)
        {
          runEnd = millis();
          terminal.println(String(lastZone) + " ran for " + ((runEnd - runStart) / 60000) + "m " + (((runEnd - runStart) / 1000) - (((runEnd - runStart) / 60000) * 60)) + "s.");
          terminal.flush();
        }

        runStart = 0;
        Blynk.setProperty(V1, "color", "#808080");

        break;
      }
    case 2: {                       // Zone 1 (sprinklers south)

        softStartZone1stage1 = now();
        softStartZone1stage2 = now() + 3;
        softStartZone1stage3 = now() + 6;

        /*
          digitalWrite(zone1, HIGH);
          digitalWrite(zone2, LOW);
          digitalWrite(zone3, LOW);
          digitalWrite(zone4, LOW);
        */

        if (runStart > 0)
        {
          runEnd = millis();
          terminal.println(String(lastZone) + " ran for " + ((runEnd - runStart) / 60000) + "m " + (((runEnd - runStart) / 1000) - (((runEnd - runStart) / 60000) * 60)) + "s.");
          terminal.flush();
        }

        terminal.println(String(currentTimeDate) + " - Zone 1 has started.");
        terminal.flush();

        runStart = millis();
        lastZone = "Zone 1";
        Blynk.setProperty(V1, "color", "#04C0F8");

        break;
      }
    case 3: {                       // Zone 2 (sprinklers north)
        digitalWrite(zone1, LOW);
        digitalWrite(zone2, HIGH);
        digitalWrite(zone3, LOW);
        digitalWrite(zone4, LOW);

        if (runStart > 0)
        {
          runEnd = millis();
          terminal.println(String(lastZone) + " ran for " + ((runEnd - runStart) / 60000) + "m " + (((runEnd - runStart) / 1000) - (((runEnd - runStart) / 60000) * 60)) + "s.");
          terminal.flush();
        }

        terminal.println(String(currentTimeDate) + " - Zone 2 has started.");
        terminal.flush();

        runStart = millis();
        lastZone = "Zone 2";
        Blynk.setProperty(V1, "color", "#04C0F8");

        break;
      }
    case 4: {                       // Zone 3 (drip front)
        digitalWrite(zone1, LOW);
        digitalWrite(zone2, LOW);
        digitalWrite(zone3, HIGH);
        digitalWrite(zone4, LOW);

        if (runStart > 0)
        {
          runEnd = millis();
          terminal.println(String(lastZone) + " ran for " + ((runEnd - runStart) / 60000) + "m " + (((runEnd - runStart) / 1000) - (((runEnd - runStart) / 60000) * 60)) + "s.");
          terminal.flush();
        }

        terminal.println(String(currentTimeDate) + " - Zone 3 has started.");
        terminal.flush();

        runStart = millis();
        lastZone = "Zone 3";
        Blynk.setProperty(V1, "color", "#04C0F8");

        break;
      }
    case 5: {                       // Zone 4 (drip back)
        digitalWrite(zone1, LOW);
        digitalWrite(zone2, LOW);
        digitalWrite(zone3, LOW);
        digitalWrite(zone4, HIGH);

        if (runStart > 0)
        {
          runEnd = millis();
          terminal.println(String(lastZone) + " ran for " + ((runEnd - runStart) / 60000) + "m " + (((runEnd - runStart) / 1000) - (((runEnd - runStart) / 60000) * 60)) + "s.");
          terminal.flush();
        }

        terminal.println(String(currentTimeDate) + " - Zone 4 has started.");
        terminal.flush();

        runStart = millis();
        lastZone = "Zone 4";
        Blynk.setProperty(V1, "color", "#04C0F8");

        break;
      }

    /*
        case 6: {
            zone12minFlag = 1;

            if (runStart > 0)
            {
              runEnd = millis();
              terminal.println(String(lastZone) + " ran for " + ((runEnd - runStart) / 60000) + "m " + (((runEnd - runStart) / 1000) - (((runEnd - runStart) / 60000) * 60)) + "s.");
              terminal.flush();
            }

            terminal.println(String(currentTimeDate) + " - 2+2 mode has started.");
            terminal.flush();

            runStart = millis();
            lastZone = "2+2 mode";
            Blynk.setProperty(V1, "color", "#04C0F8");

            break;
          }
    */
    default: {
        break;
      }
  }
}

void runTimer() {
  if (runStart > 0 && (((millis() - runStart) / 1000) - (((millis() - runStart) / 60000)) * 60) < 10) {
    Blynk.virtualWrite(V1, String((millis() - runStart) / 60000) + ":0" + (((millis() - runStart) / 1000) - (((millis() - runStart) / 60000)) * 60));
  }
  else if (runStart > 0 && (((millis() - runStart) / 1000) - (((millis() - runStart) / 60000)) * 60) > 9) {
    Blynk.virtualWrite(V1, String((millis() - runStart) / 60000) + ":" + (((millis() - runStart) / 1000) - (((millis() - runStart) / 60000)) * 60));
  }
  else {
    Blynk.virtualWrite(V1, "OFF");
  }
}

/*
  // Runs zone 1 for X minutes, then zone 2 for X minutes, then shuts off.
  void zone12min() {
  if (zone12minFlag == 1)                                         // Start bubbler to help reduce water hammer...
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
    zone12minFlag = 2;
  }
  else if (zone12minFlag == 2 && millis() >= (runStart + 3000))   // then overlap with start of spinklers...
  {
    digitalWrite(zone1, HIGH);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
    zone12minFlag = 3;
  }
  else if (zone12minFlag == 3 && millis() >= (runStart + 6000))   // then turn off bubblers.
  {
    digitalWrite(zone1, HIGH);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, LOW);
    digitalWrite(zone4, LOW);
    zone12minFlag = 4;
  }
  else if (zone12minFlag == 4 && millis() >= (runStart + 243000)) // Switch zones (the nature of the valves opening/closeing should introduce some overlap)
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, HIGH);
    digitalWrite(zone3, LOW);
    digitalWrite(zone4, LOW);
    zone12minFlag = 5;
  }
    else if (zone12minFlag == 5 && millis() >= (runStart + 480000)) // Turn bubblers back on to absorb water hammer again while turning off spinklers.
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, HIGH);
    digitalWrite(zone4, LOW);
    zone12minFlag = 6;
  }
  else if (zone12minFlag == 6 && millis() >= (runStart + 483000))
  {
    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, LOW);
    digitalWrite(zone4, LOW);
    zone12minFlag = 0;
    Blynk.virtualWrite(V0, 1); // Do i need or want to add the pick code that I did for thc? Is this not working to clear the menu? Do I really need/want to syncAll here?!
    Blynk.syncAll();
  }
  }
*/

void timeMachine() {
  if (year() != 1970) {
    // Below gives me leading zeros minutes and AM/PM.
    if (minute() > 9 && hour() > 11) {
      currentTimeDate = String(hourFormat12()) + ":" + minute() + "pm " + month() + "/" + day();
    }
    else if (minute() < 10 && hour() > 11) {
      currentTimeDate = String(hourFormat12()) + ":0" + minute() + "pm " + month() + "/" + day();
    }
    else if (minute() > 9 && hour() < 12) {
      currentTimeDate = String(hourFormat12()) + ":" + minute() + "am " + month() + "/" + day();
    }
    else if (minute() < 10 && hour() < 12) {
      currentTimeDate = String(hourFormat12()) + ":0" + minute() + "am " + month() + "/" + day();
    }
  }

  if (!firstStart && year() != 1970) {
    terminal.println("");
    terminal.println("");
    terminal.println("");
    terminal.println(String("             ~~ ") + currentTimeDate + " ~~");
    terminal.println("  ~~ Irrigation controller has started ~~");
    terminal.println("");
    terminal.print("  IP: ");
    terminal.print(WiFi.localIP());
    terminal.print(" MAC: ");
    terminal.println(WiFi.macAddress());
    terminal.println("");
    terminal.println("");
    terminal.println("");
    terminal.flush();
    firstStart = true;

  }
}

void overflowProtect() {                  // MIGHT WANT TO CONSIDER REVISING THIS TO PROTECT OVERFLOW IN *ANY* RUN SITUATION (monitor relays)

  // Protects sprinklers from running over a certain amount of time
  if ( (runStart != 0 && millis() >= (runStart + sprinklerRunLimit)) && (lastZone == "Zone 1" || lastZone == "Zone 2") ) {
    Blynk.notify(String("Sprinklers auto-shutoff after running ") + (sprinklerRunLimit / 60000) + " minutes.");

    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, LOW);
    digitalWrite(zone4, LOW);

    runEnd = millis();
    terminal.println(String(lastZone) + " ran for " + ((runEnd - runStart) / 60000) + "m " + (((runEnd - runStart) / 1000) - (((runEnd - runStart) / 60000) * 60)) + "s (auto-shutoff).");
    terminal.flush();

    runStart = 0;
  }

  // Protects bubblers from running over a certain amount of time
  else if ( (runStart != 0 && millis() >= (runStart + bubblerRunLimit)) && (lastZone == "Zone 3" || lastZone == "Zone 4") ) {
    Blynk.notify(String("Bubblers auto-shutoff after running ") + (bubblerRunLimit / 60000) + " minutes.");

    digitalWrite(zone1, LOW);
    digitalWrite(zone2, LOW);
    digitalWrite(zone3, LOW);
    digitalWrite(zone4, LOW);

    runEnd = millis();
    terminal.println(String(lastZone) + " ran for " + ((runEnd - runStart) / 60000) + "m " + (((runEnd - runStart) / 1000) - (((runEnd - runStart) / 60000) * 60)) + "s (auto-shutoff).");
    terminal.flush();

    runStart = 0;
  }
}
