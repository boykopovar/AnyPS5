#include "SceTypes.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

extern "C" int APS5_VABI sceSystemServiceParamGetInt(int paramId, int* value);

namespace {

void Require(bool value) { if (!value) std::abort(); }

void SetLanguage(const char* value) {
#ifdef _WIN32
    Require(_putenv_s("ANYPS5_LANGUAGE", value != nullptr ? value : "") == 0);
#else
    Require((value != nullptr ? ::setenv("ANYPS5_LANGUAGE", value, 1) : ::unsetenv("ANYPS5_LANGUAGE")) == 0);
#endif
}

int Language() {
    int value = -1;
    Require(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_LANG, &value) == SYSTEM_SERVICE_OK);
    int sameValue = -1;
    Require(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_400, &sameValue) == SYSTEM_SERVICE_OK);
    Require(sameValue == value);
    return value;
}

bool ParameterRejected(int paramId) {
    int language = -1;
    try {
        static_cast<void>(sceSystemServiceParamGetInt(paramId, &language));
    } catch (const std::runtime_error& error) {
        return language == -1 && std::string(error.what()).find("ANYPS5_LANGUAGE") != std::string::npos;
    }
    return false;
}

bool Answers(int paramId) {
    return (paramId >= 1 && paramId <= 5) || (paramId >= SYSTEM_SERVICE_PARAM_ID_100 && paramId <= 111) || (paramId >= 200 && paramId <= 213) ||
        paramId == 300 || paramId == SYSTEM_SERVICE_PARAM_ID_400;
}

bool Rejected(const char* value) {
    SetLanguage(value);
    return ParameterRejected(SYSTEM_SERVICE_PARAM_ID_LANG) && ParameterRejected(SYSTEM_SERVICE_PARAM_ID_400);
}

}

int main() {
    for (int paramId = -1; paramId <= 2048; ++paramId) {
        Require(sceSystemServiceParamGetInt(paramId, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
        if (Answers(paramId)) continue;
        int value = -12345;
        Require(sceSystemServiceParamGetInt(paramId, &value) == SYSTEM_SERVICE_ERROR_PARAMETER);
        Require(value == -12345);
    }
    for (int paramId : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        int value = -12345;
        Require(sceSystemServiceParamGetInt(paramId, &value) == SYSTEM_SERVICE_ERROR_PARAMETER);
        Require(value == -12345);
    }
    SetLanguage(nullptr);
    Require(Language() == SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US);
    SetLanguage("");
    Require(Language() == SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US);
    SetLanguage("21");
    Require(Language() == SYSTEM_SERVICE_PARAM_LANG_ARABIC);
    SetLanguage("0");
    Require(Language() == SYSTEM_SERVICE_PARAM_LANG_JAPANESE);
    SetLanguage("30");
    Require(Language() == SYSTEM_SERVICE_PARAM_LANG_UKRAINIAN);
    for (const char* value : {"31", "-1", "+21", " 21", "21 ", "21a", "0x15", "arabic", "4294967317"}) {
        Require(Rejected(value));
    }

    SetLanguage("arabic");
    Require(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_LANG, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_400, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    for (int paramId : {0, SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, SYSTEM_SERVICE_PARAM_ID_GAME_PARENTAL_LEVEL, 8, 20, 401, 999,
                        SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN, 1001, 1008}) {
        int unknown = -1;
        Require(sceSystemServiceParamGetInt(paramId, &unknown) == SYSTEM_SERVICE_ERROR_PARAMETER);
        Require(unknown == -1);
        Require(sceSystemServiceParamGetInt(paramId, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    }
    constexpr std::pair<int, int> measured[] = {
        {100, 0}, {101, 1}, {102, 2}, {103, 100}, {104, 0x20202}, {105, 0}, {106, 0}, {107, 100}, {108, 0}, {109, 100}, {110, 0x20202},
        {111, 0}, {200, 0}, {201, 0}, {202, 0}, {203, 0}, {204, 0}, {205, 0}, {206, 0}, {207, 0}, {208, 0}, {209, 1}, {210, 71},
        {211, 0}, {212, 0}, {213, 0}, {300, 0},
    };
    for (const auto& [paramId, expected] : measured) {
        Require(Answers(paramId));
        int value = -1;
        Require(sceSystemServiceParamGetInt(paramId, &value) == SYSTEM_SERVICE_OK);
        Require(value == expected);
        Require(sceSystemServiceParamGetInt(paramId, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    }
}
