#ifndef CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_SYSTEMSERVICE_HPP
#define CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_SYSTEMSERVICE_HPP

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

constexpr int SYSTEM_SERVICE_OK = 0;
constexpr int SYSTEM_SERVICE_ERROR_PARAMETER = -2136932349;
constexpr int SYSTEM_SERVICE_ERROR_NO_EVENT = -2136932348;

constexpr int SYSTEM_SERVICE_RUNNING_APP_ID = 1;
constexpr int SYSTEM_SERVICE_INVALID_TITLE_APP_ID = -1;
constexpr std::size_t SYSTEM_SERVICE_TITLE_ID_LENGTH = 9;

constexpr int LNC_UTIL_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80940005u);
constexpr int SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80e30003u);

constexpr int SYSTEM_SERVICE_PARAM_ID_LANG = 1;
constexpr int SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT = 2;
constexpr int SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT = 3;
constexpr int SYSTEM_SERVICE_PARAM_ID_TIME_ZONE = 4;
constexpr int SYSTEM_SERVICE_PARAM_ID_SUMMERTIME = 5;
constexpr int SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME = 6;
constexpr int SYSTEM_SERVICE_PARAM_ID_GAME_PARENTAL_LEVEL = 7;
constexpr int SYSTEM_SERVICE_PARAM_ID_100 = 100;
constexpr int SYSTEM_SERVICE_PARAM_ID_400 = 400;
constexpr int SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN = 1000;

constexpr std::size_t SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH = 65;

constexpr int SYSTEM_SERVICE_PARAM_LANG_JAPANESE = 0;
constexpr int SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US = 1;
constexpr int SYSTEM_SERVICE_PARAM_LANG_ARABIC = 21;
constexpr int SYSTEM_SERVICE_PARAM_LANG_UKRAINIAN = 30;
constexpr int SYSTEM_SERVICE_PARAM_DATE_FORMAT_DDMMYYYY = 1;
constexpr int SYSTEM_SERVICE_PARAM_TIME_FORMAT_24HOUR = 1;

inline int SystemServiceConsoleLanguage(std::string_view caller) {
    const char* configured = std::getenv("ANYPS5_LANGUAGE");
    if (configured == nullptr || configured[0] == '\0') return SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US;
    const std::string_view text(configured);
    int language = -1;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), language);
    if (error != std::errc{} || end != text.data() + text.size() || language < SYSTEM_SERVICE_PARAM_LANG_JAPANESE || language > SYSTEM_SERVICE_PARAM_LANG_UKRAINIAN) {
        throw std::runtime_error(std::string(caller) + ": ANYPS5_LANGUAGE must be a console language number from " +
            std::to_string(SYSTEM_SERVICE_PARAM_LANG_JAPANESE) + " to " + std::to_string(SYSTEM_SERVICE_PARAM_LANG_UKRAINIAN) +
            ", not \"" + std::string(text) + "\"");
    }
    return language;
}

#endif
