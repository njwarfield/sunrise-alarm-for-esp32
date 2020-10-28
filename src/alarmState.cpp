#include <ArduinoJson.h>
#include <alarmState.hpp>

AlarmState::AlarmState() {
}

AlarmState::AlarmState(String json) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, json);
    for (size_t i = 0; i < ; i++)
    {
        JsonObject alarmTime = doc[i];
        int day = alarmTime["d"];
        int hour = alarmTime["h"];
        int minute = alarmTime["m"];
        SetAlarm(day, hour, minute);
    }
}

void AlarmState::SetAlarm(int day, int hour, int minute) {
    it = map.find(day);
    if (it != map.end()) {
        map.insert(std::make_pair(day, std::make_tuple(hour, minute)));
    } else {
        it->second = std::make_tuple(hour, minute);
    }
};

std::tuple<int, int> AlarmState::GetAlarmByDay(int day) {
    it = map.find(day);
    if (it != map.end()) {
        return it->second;
    }
    return std::make_tuple(0, 0);
};

String AlarmState::serializeStateToJSON() {
    String alarmJSON;
    if (map.size() > 0) {
        DynamicJsonDocument doc(256);
        for (size_t i = 0; i < map.size()-1; i++) {
            JsonObject alarm = doc.createNestedObject();
            tuple<int, int> alarmTime = GetAlarmByDay(i);
            alarm["d"] = i;
            alarm["h"] = get<0>(alarmTime);
            alarm["m"] = get<1>(alarmTime);
            serializeJson(doc, alarmJSON);
        }
    }
    return alarmJSON;
}