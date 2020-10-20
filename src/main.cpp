#include <ArduinoJson.h>
#include <FastLED.h>
#include <SPIFFS.h>
#include <TimeAlarms.h>
#include <TimeLib.h>
#include <WiFi.h>
#include <sys/time.h>

#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <HTTPSServer.hpp>
#include <ResourceNode.hpp>
#include <SSLCert.hpp>
#include <config.hpp>
#include <util.hpp>

#include "alarmState.hpp"

using namespace httpsserver;

#define LED_PIN 16
#define NUM_LEDS 20
#define CHIPSET WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

//WIFI Settings
const char ssid[] = SSID;
const char password[] = IOT_WIFI_Password;

//NTP Time Settings
const char ntpServer[] = NTP_URL;
const long gmtOffset_sec = -21600;

//DST Offset value is one hour (in seconds)
const int daylightOffset_sec = 3600;
struct tm timeinfo;

//Alarm Settings
AlarmId brightness_id;
AlarmId wakeup_id;
AlarmState alarmState;

int alarmHour = 0;
int alarmMinute = 0;

bool begin_sunrise = false;
bool turn_off = false;

//LED Settings
int brightness = 0;
int max_brightness = 80;
bool brightness_update = false;
int brightness_interval = 2;

int temperature_counter = 0;
int temperature_index = 0;
bool temperature_update = false;

CRGB warmth[] = {
    CRGB(255, 147, 44),   //2200K
    CRGB(255, 190, 126),  //3K
    CRGB(255, 190, 126)   //3300K
};

//Webserver settings
WiFiServer server(8181);
HTTPServer httpServer = HTTPServer(80);
SSLCert *cert;
String header;

void printDigits(int digits) {
    Serial.print(":");
    if (digits < 10)
        Serial.print('0');
    Serial.print(digits);
}

void digitalClockDisplay() {
    // digital clock display of the time
    Serial.print(alarmHour);
    printDigits(alarmMinute);
    printDigits(second());
    Serial.println();
}

void AlarmDisplay() {
    // digital clock display of the time
    Serial.print(alarmHour);
    printDigits(alarmMinute);
    Serial.println();
}

void handleRoot(HTTPRequest *req, HTTPResponse *resp) {
    resp->setHeader("Content-Type", "text/html");
    resp->println("<!DOCTYPE html");
    resp->println("<html>");
    resp->println("<head><title>Alarm Details</title></head>");
    resp->println("<body>");
    resp->println("<h1>");
    resp->printf("Current Alarm set for: %d:%d \n", alarmHour, alarmMinute);
    resp->println("</h1>");
    resp->println("</body>");
    resp->println("</html>");
}

void IncreaseBrightness() {
    Serial.println("Increasing Brightness and temperature");
    if (brightness < max_brightness) {
        brightness_update = true;
        brightness = brightness + brightness_interval;
    }

    if (temperature_index < 2) {
        if (temperature_counter % 5 == 0 && temperature_counter > 0) {
            temperature_index++;
            temperature_update = true;
        }
        temperature_counter++;
    }

    Serial.print("Brightness Value: ");
    Serial.println(brightness);
    Serial.print("Temperature Index: ");
    Serial.println(temperature_index);
}

void BeginSunrise() {
    brightness = 0;
    temperature_counter = 0;
    temperature_index = 0;

    FastLED.setBrightness(brightness);
    CRGB color = warmth[temperature_index];
    fill_solid(leds, NUM_LEDS, color);

    Serial.println("Sunrise active");
    brightness_id = Alarm.timerRepeat(30, IncreaseBrightness);
    begin_sunrise = true;
}

void warmUpLights() {
    if (brightness_update == true) {
        Serial.println("Illuminating");
        FastLED.setBrightness(brightness);
        brightness_update = false;
    }

    if (temperature_update == true) {
        Serial.println("Warming...");
        CRGB color = warmth[temperature_index];
        fill_solid(leds, NUM_LEDS, color);
        temperature_update = false;
    }

    FastLED.show();
    FastLED.delay(5);
}

void GetTimeViaWifi() {
    Serial.begin(115200);
    delay(1000);
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" CONNECTED");
    Serial.println(WiFi.localIP());
    server.begin();
    delay(1000);

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    getLocalTime(&timeinfo);

    time_t newTime = mktime(&timeinfo);
    setTime(newTime);
    adjustTime(gmtOffset_sec + daylightOffset_sec);
    digitalClockDisplay();
}

void handleAlarmSet(HTTPRequest *req, HTTPResponse *resp) {
    DynamicJsonDocument doc(128);
    byte buffer[128];
    req->readBytes(buffer, 128);

    // If the request is still not read completely, we cannot process it.
    if (!req->requestComplete()) {
        resp->setStatusCode(413);
        resp->setStatusText("Request entity too large");
        resp->println("413 Request entity too large");
        return;
    }

    DeserializationError error = deserializeJson(doc, buffer);
    if (error) {
        resp->setStatusCode(500);
        resp->setStatusText("Error parsing JSON");
        resp->println("Error parsing JSON");
        return;
    }

    JsonArray array = doc.as<JsonArray>();
    for (JsonVariant value : array) {
        int d = value["d"];
        int h = value["h"];
        int m = value["m"];
        Serial.printf("%d %d %d", d, h, m);
        //Update map
        alarmState.SetAlarm(d, h, m);
        //set alarm if new update is next alarm
        if (d == weekday()) {
            if (wakeup_id != 0) {
                Alarm.free(wakeup_id);
                wakeup_id = 0;
            }
            wakeup_id = Alarm.alarmRepeat(h, m, 0, BeginSunrise);
        }
    }

    resp->setStatusCode(200);
    resp->setStatusText("Alarm Set");
    resp->printf("Alarms set");
}

void handleAlarmOff(HTTPRequest *req, HTTPResponse *resp) {
    Alarm.free(brightness_id);
    brightness_id = 0;
    Alarm.free(wakeup_id);
    wakeup_id = 0;

    CRGB color = CRGB::Black;
    fill_solid(leds, NUM_LEDS, color);
    brightness_update = false;
    FastLED.show();
    FastLED.delay(1);

    resp->setStatusCode(200);
    resp->setStatusText("Alarm Off");
    resp->println("Alarm turned off.");

    //Check/set next alarm
}

void serverTask(void *params) {
    ResourceNode *nodeRoot = new ResourceNode("/", "GET", &handleRoot);
    httpServer.registerNode(nodeRoot);
    ResourceNode *alarmOff = new ResourceNode("/off", "GET", &handleAlarmOff);
    httpServer.registerNode(alarmOff);
    ResourceNode *alarmSet = new ResourceNode("/set-alarm", "POST", &handleAlarmSet);
    httpServer.registerNode(alarmSet);
    httpServer.start();

    if (httpServer.isRunning()) {
        while (true) {
            httpServer.loop();
            delay(1);
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    GetTimeViaWifi();
    //load alarm state from memory

    int day = weekday();
    //set today's alarm if it exists
    if (alarmState.TodayHasAlarm(day)) {
        alarmHour = alarmState.AlarmHour(day);
        alarmMinute = alarmState.AlarmMinute(day);
        wakeup_id = Alarm.alarmRepeat(alarmHour, alarmMinute, 0, BeginSunrise);
    }

    FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    xTaskCreatePinnedToCore(serverTask, "http", 6144, NULL, 1, NULL, CONFIG_ARDUINO_RUNNING_CORE);
}

void loop() {
    //Sunrise Alarm loop
    if (begin_sunrise) {
        warmUpLights();
    }
    Alarm.delay(1000);
}