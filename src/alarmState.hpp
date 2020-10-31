#include <tuple>
#include <map>
#include <string>

#define AlarmState_H
using namespace std;

class AlarmState {
    private:
        std::map<int, std::tuple<int, int>> map;
        std::map<int, std::tuple<int, int>>::iterator it;
    public:
        AlarmState();
        AlarmState(String json);
        String serializeStateToJSON();
        boolean enabled;
        bool TodayHasAlarm(int day) {
            tuple<int, int> alarm = GetAlarmByDay(day);
            if(get<0>(alarm) > 0 || get<1>(alarm) > 0) return true;
            return false;
        }
        int AlarmHour(int day) { 
            tuple<int, int> alarm = GetAlarmByDay(day);
            return get<0>(alarm);
         }
        int AlarmMinute(int day) {
            tuple<int, int> alarm = GetAlarmByDay(day);
            return get<1>(alarm);
        }
        void SetAlarm(int day, int hour, int minute);
        tuple<int, int> GetAlarmByDay(int day);
};