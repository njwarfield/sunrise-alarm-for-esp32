#include <alarmState.hpp>

AlarmState::AlarmState(int hour, int minute) {
    alarm_hour = hour;
    alarm_minute = minute;
}

void AlarmState::SetAlarm(int hour, int minute) {
    alarm_hour = hour;
    alarm_minute = minute;
};
