#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <source_location>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <stdlib.h>
#endif

namespace {

constexpr long MmHard = 0x001;
constexpr long MmPrint = 0x100;
constexpr long MmConsole = 0x200;
constexpr int MmError = 2;
constexpr int MmWarning = 3;
constexpr int MmNosev = 0;
constexpr int MmOk = 0;
constexpr int MmNocon = 4;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "fmtmsg check failed at line %u\n", location.line());
        std::abort();
    }
}

void SetVerbosity(const char* value) {
#ifdef _WIN32
    _putenv_s("MSGVERB", value == nullptr ? "" : value);
#else
    if (value == nullptr) unsetenv("MSGVERB");
    else setenv("MSGVERB", value, 1);
#endif
}

std::filesystem::path g_capture;

void BeginCapture() {
    Require(std::freopen(g_capture.string().c_str(), "w", stderr) != nullptr);
}

std::string EndCapture() {
    std::fflush(stderr);
    std::ifstream input(g_capture);
    std::stringstream text;
    text << input.rdbuf();
    return text.str();
}

}

int main() {
    std::random_device random;
    g_capture = std::filesystem::temp_directory_path() / ("anyps5-fmtmsg-" + std::to_string(random()) + ".txt");

    SetVerbosity(nullptr);
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint | MmHard, "XSI:cat", MmError, "illegal option", "refer to cat in user's reference manual", "XSI:cat:001") == MmOk);
    Require(EndCapture() == "XSI:cat: ERROR: illegal option\nTO FIX: refer to cat in user's reference manual XSI:cat:001\n");

    SetVerbosity("severity:text:action");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmError, "illegal option", "refer to cat in user's reference manual", "XSI:cat:001") == MmOk);
    Require(EndCapture() == "ERROR: illegal option\nTO FIX: refer to cat in user's reference manual\n");

    SetVerbosity("not-a-keyword");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmError, "illegal option", nullptr, nullptr) == MmOk);
    Require(EndCapture() == "XSI:cat: ERROR: illegal option\n");

    SetVerbosity("label:tag");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmError, "illegal option", "refer", "XSI:cat:001") == MmOk);
    Require(EndCapture() == "XSI:cat\nXSI:cat:001\n");

    SetVerbosity(nullptr);
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, nullptr, MmError, "illegal option", nullptr, nullptr) == MmOk);
    Require(EndCapture() == "ERROR: illegal option\n");

    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmNosev, "illegal option", nullptr, nullptr) == MmOk);
    Require(EndCapture() == "XSI:cat: illegal option\n");

    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmWarning, "careful", nullptr, nullptr) == MmOk);
    Require(EndCapture() == "XSI:cat: WARNING: careful\n");

    BeginCapture();
    Require(fmtmsg_nid_postfix(MmConsole, "XSI:cat", MmError, "illegal option", nullptr, nullptr) == MmNocon);
    Require(EndCapture().empty());

    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint | MmConsole, "XSI:cat", MmError, "illegal option", nullptr, nullptr) == MmNocon);
    Require(EndCapture() == "XSI:cat: ERROR: illegal option\n");

    BeginCapture();
    Require(fmtmsg_nid_postfix(0, "XSI:cat", MmError, "illegal option", nullptr, nullptr) == MmOk);
    Require(EndCapture().empty());

    std::filesystem::remove(g_capture);
    return 0;
}
