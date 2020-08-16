#define AlarmState_H

class AlarmState {
    private:
        int alarm_hour;
        int alarm_minute;
    public:
        AlarmState(int hour, int minute);
        int AlarmHour() {return alarm_hour;}
        int AlarmMinute() {return alarm_minute;}
        void SetAlarm(int hour, int minute);
};