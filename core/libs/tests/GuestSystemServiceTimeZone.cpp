#include "prx/libSceSystemService/include/SystemService.hpp"
#include "SceTypes.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <cstdlib>
#include <ctime>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void SetTimeZone(const char* zone) {
#ifdef _WIN32
    _putenv_s("TZ", zone);
    _tzset();
#else
    setenv("TZ", zone, 1);
    tzset();
#endif
}

int ParamInt(int paramId) {
    int value = -1;
    Require(sceSystemServiceParamGetInt(paramId, &value) == SYSTEM_SERVICE_OK);
    return value;
}

bool HostSummerTime() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    Require(localtime_s(&local, &now) == 0);
#else
    Require(localtime_r(&now, &local) != nullptr);
#endif
    return local.tm_isdst > 0;
}

void RequireZone(const char* zone, int minutes, int summer) {
    SetTimeZone(zone);
    Require(ParamInt(SYSTEM_SERVICE_PARAM_ID_TIME_ZONE) == minutes);
    Require(ParamInt(SYSTEM_SERVICE_PARAM_ID_SUMMERTIME) == summer);
}

}

int main() {
    RequireZone("IST-5:30", 330, 0);
    RequireZone("BRT+3", -180, 0);
    RequireZone("UTC0", 0, 0);
    SetTimeZone("CET-1CEST");
    Require(ParamInt(SYSTEM_SERVICE_PARAM_ID_TIME_ZONE) == 60);
    Require(ParamInt(SYSTEM_SERVICE_PARAM_ID_SUMMERTIME) == (HostSummerTime() ? 1 : 0));
#ifndef _WIN32
    RequireZone("CET-1CEST,0/0,J365/25", 60, 1);
#endif
}
