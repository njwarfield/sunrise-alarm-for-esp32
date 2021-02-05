#include <ArduinoJson.h>

#include <alarmState.hpp>

AlarmState::AlarmState() {
}

AlarmState::AlarmState(String json) {
    const size_t capacity = JSON_ARRAY_SIZE(7) + JSON_OBJECT_SIZE(2) + 7*JSON_OBJECT_SIZE(3) + 60;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, json);
    enabled = doc["enabled"];
    for (JsonObject obj: doc["alarms"].as<JsonArray>()) {
        int day = obj["d"];
        int hour = obj["h"];
        int minute = obj["m"];
        if(day < 8 && hour < 24 && minute < 60) {
            SetAlarm(day, hour, minute);
        }
    }
}

void AlarmState::SetAlarm(int day, int hour, int minute) {
    it = map.find(day);
    if (it == map.end()) {
        Serial.printf("No day %d adding new record\n", day);
        map.insert(std::make_pair(day, std::make_tuple(hour, minute)));
    } else {
        Serial.printf("Editing existing day %d\n", day);
        it->second = std::make_tuple(hour, minute);
    }
};

tuple<int, int> AlarmState::GetAlarmByDay(int day) {
    it = map.find(day);
    return (it != map.end()) ? it->second : std::make_tuple(0, 0);
}

String AlarmState::serializeStateToJSON() {
    String alarmJSON;
    if (map.size() > 0) {
        const size_t capacity = JSON_ARRAY_SIZE(7) + JSON_OBJECT_SIZE(2) + 7*JSON_OBJECT_SIZE(3) + 30;
        DynamicJsonDocument doc(capacity);
        doc["enabled"] = enabled;
        JsonArray alarms = doc.createNestedArray("alarms");
        for (auto const& i: map) {
            JsonObject alarm = alarms.createNestedObject();
            tuple<int, int> alarmTime = GetAlarmByDay(i.first);
            alarm["d"] = i.first;
            alarm["h"] = get<0>(alarmTime);
            alarm["m"] = get<1>(alarmTime);
        }
        serializeJsonPretty(doc, alarmJSON);
    }
    return alarmJSON;
}