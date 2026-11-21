#include "prx/libSceSystemService/include/SystemService.hpp"
#include "SceTypes.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "check failed: %s\n", what);
    std::abort();
}

void SetVariable(const char* name, std::string_view value) {
    if (value == "-") Require(::unsetenv(name) == 0, name);
    else Require(::setenv(name, std::string(value).c_str(), 1) == 0, name);
}

int Query(int paramId) {
    int value = -1;
    Require(sceSystemServiceParamGetInt(paramId, &value) == SYSTEM_SERVICE_OK, "query");
    return value;
}

bool Rejected(const char* value, std::string& message) {
    SetVariable("ANYPS5_LANGUAGE", value);
    int language = -1;
    try {
        static_cast<void>(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_LANG, &language));
    } catch (const std::runtime_error& error) {
        message = error.what();
        return language == -1;
    }
    return false;
}

std::string ReadAll(std::FILE* file) {
    std::string text;
    std::rewind(file);
    for (int c = std::fgetc(file); c != EOF; c = std::fgetc(file)) text += static_cast<char>(c);
    return text;
}

int CountNotices(const std::string& text) {
    int count = 0;
    for (std::size_t at = text.find("ANYPS5_LANGUAGE=auto:"); at != std::string::npos; at = text.find("ANYPS5_LANGUAGE=auto:", at + 1)) ++count;
    return count;
}

}

int main(int argc, char** argv) {
    Require(argc == 8, "arguments: expected LC_ALL LC_MESSAGES LANG LANGUAGE spelling notices");
    const int expected = std::atoi(argv[1]);
    const int expectedNotices = std::atoi(argv[7]);
    std::FILE* captured = std::tmpfile();
    Require(captured != nullptr && ::dup2(::fileno(captured), 2) == 2, "capture stderr");
    SetVariable("LC_ALL", argv[2]);
    SetVariable("LC_MESSAGES", argv[3]);
    SetVariable("LANG", argv[4]);
    SetVariable("LANGUAGE", argv[5]);
    SetVariable("ANYPS5_LANGUAGE", argv[6]);
    Require(Query(SYSTEM_SERVICE_PARAM_ID_LANG) == expected, "parameter 1");
    Require(Query(SYSTEM_SERVICE_PARAM_ID_400) == expected, "parameter 400");
    Require(SystemServiceConsoleLanguage("test") == expected, "SystemServiceConsoleLanguage");
    SetVariable("LC_ALL", "ja_JP.UTF-8");
    SetVariable("LC_MESSAGES", "ja_JP.UTF-8");
    SetVariable("LANG", "ja_JP.UTF-8");
    SetVariable("LANGUAGE", "ja");
    Require(Query(SYSTEM_SERVICE_PARAM_ID_LANG) == expected, "the host locale is resolved once");
    Require(SystemServiceConsoleLanguage("test") == expected, "the host locale is resolved once for every caller");
    Require(CountNotices(ReadAll(captured)) == expectedNotices, "number of notices");
    SetVariable("ANYPS5_LANGUAGE", "17");
    Require(Query(SYSTEM_SERVICE_PARAM_ID_LANG) == 17, "an integer after auto");
    SetVariable("ANYPS5_LANGUAGE", "0");
    Require(Query(SYSTEM_SERVICE_PARAM_ID_400) == 0, "integer 0 after auto");
    SetVariable("ANYPS5_LANGUAGE", argv[6]);
    Require(Query(SYSTEM_SERVICE_PARAM_ID_LANG) == expected, "auto again gives the same answer");
    SetVariable("ANYPS5_LANGUAGE", "");
    Require(Query(SYSTEM_SERVICE_PARAM_ID_LANG) == SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US, "empty after auto");
    SetVariable("ANYPS5_LANGUAGE", "-");
    Require(Query(SYSTEM_SERVICE_PARAM_ID_LANG) == SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US, "unset after auto");
    for (const char* value : {"auto ", " auto", "autoo", "aut", "a", "au to", "auto\n", "automatic", "-auto", "+auto", "31", "-1", "21a", "0x15", "arabic"}) {
        std::string message;
        Require(Rejected(value, message), value);
        Require(message.find("ANYPS5_LANGUAGE") != std::string::npos, "the error names the variable");
        Require(message.find("0 to 30 or auto") != std::string::npos, "the error lists auto");
    }
    Require(CountNotices(ReadAll(captured)) == expectedNotices, "no notice after the first resolution");
}
