#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr long MmPrint = 0x100;
constexpr long MmConsole = 0x200;
constexpr int MmNosev = 0;
constexpr int MmHalt = 1;
constexpr int MmError = 2;
constexpr int MmWarning = 3;
constexpr int MmInfo = 4;
constexpr int MmOk = 0;
constexpr int MmNomsg = 1;
constexpr int MmNocon = 4;

constexpr unsigned SelectLabel = 0x01;
constexpr unsigned SelectSeverity = 0x02;
constexpr unsigned SelectText = 0x04;
constexpr unsigned SelectAction = 0x08;
constexpr unsigned SelectTag = 0x10;
constexpr unsigned SelectAll = SelectLabel | SelectSeverity | SelectText | SelectAction | SelectTag;

unsigned Selection() {
    const char* verbosity = std::getenv("MSGVERB");
    if (verbosity == nullptr || *verbosity == '\0') return SelectAll;
    unsigned selected = 0;
    std::string word;
    for (const char* cursor = verbosity;; ++cursor) {
        if (*cursor == ':' || *cursor == '\0') {
            if (word == "label") selected |= SelectLabel;
            else if (word == "severity") selected |= SelectSeverity;
            else if (word == "text") selected |= SelectText;
            else if (word == "action") selected |= SelectAction;
            else if (word == "tag") selected |= SelectTag;
            else return SelectAll;
            word.clear();
            if (*cursor == '\0') break;
        } else {
            word += *cursor;
        }
    }
    return selected;
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

}

extern "C" int APS5_VABI fmtmsg_nid_postfix(long classification, const char* label, int severity, const char* text, const char* action, const char* tag) {
    const unsigned selected = Selection();
    const bool printRequested = (classification & MmPrint) != 0;
    const bool consoleRequested = (classification & MmConsole) != 0;
    if (printRequested) {
        std::string first;
        const auto append = [&first](const char* piece) {
            if (!first.empty()) first += ": ";
            first += piece;
        };
        if ((selected & SelectLabel) && label != nullptr) append(label);
        const char* keyword = (selected & SelectSeverity) && severity != MmNosev ? SeverityKeyword(severity) : nullptr;
        if (keyword != nullptr) append(keyword);
        if ((selected & SelectText) && text != nullptr) append(text);
        std::string second;
        if ((selected & SelectAction) && action != nullptr) second = std::string("TO FIX: ") + action;
        if ((selected & SelectTag) && tag != nullptr) {
            if (!second.empty()) second += ' ';
            second += tag;
        }
        std::string output;
        if (!first.empty()) output += first + "\n";
        if (!second.empty()) output += second + "\n";
        if (!output.empty() && std::fputs(output.c_str(), stderr) < 0) return MmNomsg;
        std::fflush(stderr);
    }
    if (consoleRequested) return MmNocon;
    return MmOk;
}
