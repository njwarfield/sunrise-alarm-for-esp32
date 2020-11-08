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
const long gmtOffset_sec = TZ_OFFSET;

//DST Offset value is one hour (in seconds)
const int daylightOffset_sec = DST_OFFSET;
struct tm timeinfo;

//Alarm Settings
AlarmId brightness_id;
AlarmId wakeup_id;
AlarmState alarmState;

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

void printDigits(int digits) {
    Serial.print(":");
    if (digits < 10)
        Serial.print('0');
    Serial.print(digits);
}

void digitalClockDisplay() {
    // digital clock display of the time
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.println();
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

String getAlarmFromSPIFFS() {
    String alarms = " ";
    if (SPIFFS.exists(STATE_FILE)) {
        File file = SPIFFS.open(STATE_FILE);
        if (file && file.size()) {
            while (file.available()) {
                alarms += char(file.read());
            }
            file.close();
        }
    }
    return alarms;
}

void handleRoot(HTTPRequest *req, HTTPResponse *resp) {
    resp->setStatusCode(200);
    resp->setHeader("Content-type", "application/json");
    resp->println(alarmState.serializeStateToJSON());
}

void handleAlarmSet(HTTPRequest *req, HTTPResponse *resp) {
    const size_t capacity = JSON_ARRAY_SIZE(7) + 7 * JSON_OBJECT_SIZE(3) + 30;
    DynamicJsonDocument doc(capacity);
    byte buffer[capacity];
    req->readBytes(buffer, capacity);

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

    JsonArray array = doc["alarms"].as<JsonArray>();
    for (JsonVariant value : array) {
        int d = value["d"];
        int h = value["h"];
        int m = value["m"];
        Serial.printf("%d %d %d\n", d, h, m);
        alarmState.SetAlarm(d, h, m);
    }
    String alarms = alarmState.serializeStateToJSON();

    //Save to SPIFFS
    int bytes = 0;
    File file = SPIFFS.open("/alarmState.json", "w");
    if (file) {
        bytes = file.print(alarms);
        file.close();
    }
    if (bytes > 0) {
        resp->setStatusCode(200);
        resp->setHeader("Content-type", "application/json");
        resp->println(alarms);
    } else {
        resp->setStatusCode(500);
        resp->setStatusText("Unable to save alarms.");
    }
}

boolean tryEnableAlarm() {
    Serial.println("Enable Alarm");
    int alarmOffset = isPM() ? 1 : 0;
    int nextAlarmDay = (weekday() + alarmOffset > 7) ? 1 : weekday() + alarmOffset;
    tuple<int, int> alarm = alarmState.GetAlarmByDay(nextAlarmDay);
    wakeup_id = Alarm.alarmRepeat(get<0>(alarm), get<1>(alarm), 0, BeginSunrise);
    if (wakeup_id != dtINVALID_ALARM_ID) {
        Serial.printf("Alarm set, day: %d - hour: %d - minute: %d\n", nextAlarmDay, get<0>(alarm), get<1>(alarm));
        alarmState.enabled = true;
        return true;
    }
    alarmState.enabled = false;
    return false;
}

void disableAlarm() {
    Serial.println("Disabling Alarm");
    Alarm.free(brightness_id);
    brightness_id = dtINVALID_ALARM_ID;
    Alarm.free(wakeup_id);
    wakeup_id = dtINVALID_ALARM_ID;

    CRGB color = CRGB::Black;
    fill_solid(leds, NUM_LEDS, color);
    brightness_update = false;
    FastLED.show();
    FastLED.delay(1);
    alarmState.enabled = false;
}

boolean trySaveAlarm() {
    String alarms = alarmState.serializeStateToJSON();
    int bytes = 0;
    File file = SPIFFS.open("/alarmState.json", "w");
    if (file) {
        bytes = file.print(alarms);
        if (bytes == 0) {
            Serial.println("Unable to save alarmState");
            return false;
        }
        file.close();
    }
    return true;
}

void handleAlarmEnable(HTTPRequest *req, HTTPResponse *resp) {
    if (!alarmState.enabled) {
        if (!tryEnableAlarm()) {
            resp->setStatusCode(500);
            resp->setHeader("Content-Type", "text/html");
            resp->setStatusText("Unable to set alarm");
            return;
        }
        if (!trySaveAlarm()) {
            resp->setStatusCode(500);
            resp->setHeader("Content-Type", "text/html");
            resp->setStatusText("Unable to save alarm");
            return;
        }
        resp->setStatusCode(200);
        resp->setHeader("Content-Type", "text/html");
        resp->setStatusText("Alarm On");
    }
}

void handleAlarmDisable(HTTPRequest *req, HTTPResponse *resp) {
    if (!alarmState.enabled) {
        resp->setStatusCode(404);
        resp->setHeader("Content-Type", "text/html");
        resp->setStatusText("Alarm already disabled");
        return;
    }
    disableAlarm();
    if(trySaveAlarm()) {
        resp->setStatusCode(200);
        resp->setHeader("Content-Type", "text/html");
        resp->setStatusText("Alarm Off");
        return;
    }
    alarmState.enabled = false;
    resp->setStatusCode(500);
    resp->setHeader("Content-Type", "text/html");
    resp->setStatusText("Unable to save alarm");
}

void serverTask(void *params) {
    ResourceNode *nodeRoot = new ResourceNode("/", "GET", &handleRoot);
    httpServer.registerNode(nodeRoot);
    ResourceNode *alarmOn = new ResourceNode("/on", "GET", &handleAlarmEnable);
    httpServer.registerNode(alarmOn);
    ResourceNode *alarmOff = new ResourceNode("/off", "GET", &handleAlarmDisable);
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
    if (!SPIFFS.begin(true)) {
        Serial.println("Error mounting SPIFFS");
    }
    while (!Serial)
        ;
    GetTimeViaWifi();
    String alarms = getAlarmFromSPIFFS();
    alarmState = AlarmState(alarms);
    if (alarmState.enabled) {
        tryEnableAlarm() ? Serial.println("Alarm initialized.") : Serial.println("Alarm intialization failed.");
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