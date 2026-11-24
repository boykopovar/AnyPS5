#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr std::int64_t MmPrint = 0x100;
constexpr std::int64_t MmConsole = 0x200;
constexpr int MmHalt = 1;
constexpr int MmError = 2;
constexpr int MmWarning = 3;
constexpr int MmInfo = 4;
constexpr int MmOk = 0;
constexpr int MmNocon = 2;

constexpr char DefaultVerbosity[] = "label:severity:text:action:tag";

std::vector<std::string> Components(const char* verbosity) {
    std::vector<std::string> words;
    std::string word;
    for (const char* cursor = verbosity;; ++cursor) {
        if (*cursor == ':' || *cursor == '\0') {
            if (!word.empty()) words.push_back(word);
            word.clear();
            if (*cursor == '\0') break;
        } else {
            word += *cursor;
        }
    }
    return words;
}

bool KnownComponent(const std::string& word) {
    return word == "label" || word == "severity" || word == "text" || word == "action" || word == "tag";
}

std::vector<std::string> Verbosity() {
    const char* value = std::getenv("MSGVERB");
    if (value != nullptr && *value != '\0' && std::strlen(value) <= sizeof(DefaultVerbosity) - 1) {
        auto words = Components(value);
        bool valid = true;
        for (const auto& word : words) valid = valid && KnownComponent(word);
        if (valid) return words;
    }
    return Components(DefaultVerbosity);
}

const char* SeverityKeyword(int severity) {
    switch (severity) {
        case MmHalt: return "HALT";
        case MmError: return "ERROR";
        case MmWarning: return "WARNING";
        case MmInfo: return "INFO";
        default: return nullptr;
    }
}

void Join(std::string& output, const char* separator) {
    if (!output.empty()) output += separator;
}

std::string Compose(const std::vector<std::string>& components, const char* label, int severity, const char* text, const char* action, const char* tag) {
    std::string output;
    const char* keyword = SeverityKeyword(severity);
    for (const auto& component : components) {
        if (component == "label" && label != nullptr) {
            Join(output, ": ");
            output += label;
        } else if (component == "severity" && keyword != nullptr) {
            Join(output, ": ");
            output += keyword;
        } else if (component == "text" && text != nullptr) {
            Join(output, ": ");
            output += text;
        } else if (component == "action" && action != nullptr) {
            Join(output, "\n");
            output += "TO FIX: ";
            output += action;
        } else if (component == "tag" && tag != nullptr) {
            Join(output, " ");
            output += tag;
        }
    }
    Join(output, "\n");
    return output;
}

}

extern "C" int APS5_VABI fmtmsg_nid_postfix(std::int64_t classification, const char* label, int severity, const char* text, const char* action, const char* tag) {
    if (classification & MmPrint) {
        const std::string output = Compose(Verbosity(), label, severity, text, action, tag);
        if (!output.empty()) {
            std::fputs(output.c_str(), stderr);
            std::fflush(stderr);
        }
    }
    if (classification & MmConsole) {
        if (!Compose(Components(DefaultVerbosity), label, severity, text, action, tag).empty()) return MmNocon;
    }
    return MmOk;
}
