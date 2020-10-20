#include <alarmState.hpp>

AlarmState::AlarmState() {
}

void AlarmState::SetAlarm(int day, int hour, int minute) {
    it = map.find(day);
    if(it != map.end()) {
        map.insert(std::make_pair(day, std::make_tuple(hour, minute)));
    }
    else {
        it->second = std::make_tuple(hour, minute);
    }
};


std::tuple<int, int> AlarmState::GetAlarmByDay(int day) {
    it = map.find(day);
    if(it != map.end()) {
        return it->second;
    }
    return std::make_tuple(0,0);
};