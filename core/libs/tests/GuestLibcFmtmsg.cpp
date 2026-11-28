#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <source_location>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int64_t MmHard = 0x001;
constexpr std::int64_t MmPrint = 0x100;
constexpr std::int64_t MmConsole = 0x200;
constexpr int MmError = 2;
constexpr int MmWarning = 3;
constexpr int MmNosev = 0;
constexpr int MmOk = 0;
constexpr int MmNocon = 2;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "fmtmsg check failed at line %u\n", location.line());
        std::abort();
    }
}

void SetVerbosity(const char* value) {
    if (value == nullptr) Require(unsetenv_nid_postfix("MSGVERB") == 0);
    else Require(setenv_nid_postfix("MSGVERB", value, 1) == 0);
}

void SetHostVerbosity(const char* value) {
#ifdef _WIN32
    Require(_putenv_s("MSGVERB", value == nullptr ? "" : value) == 0);
#else
    if (value == nullptr) Require(unsetenv("MSGVERB") == 0);
    else Require(setenv("MSGVERB", value, 1) == 0);
#endif
}

std::filesystem::path g_capture;
int g_savedStderr = -1;

void BeginCapture() {
    std::fflush(stderr);
#ifdef _WIN32
    const int file = ::_open(g_capture.string().c_str(), _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY, _S_IREAD | _S_IWRITE);
    Require(file >= 0);
    g_savedStderr = ::_dup(2);
    Require(g_savedStderr >= 0 && ::_dup2(file, 2) == 0);
    ::_close(file);
#else
    const int file = ::open(g_capture.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    Require(file >= 0);
    g_savedStderr = ::dup(2);
    Require(g_savedStderr >= 0 && ::dup2(file, 2) == 2);
    ::close(file);
#endif
}

std::string EndCapture() {
    std::fflush(stderr);
#ifdef _WIN32
    Require(::_dup2(g_savedStderr, 2) == 0);
    ::_close(g_savedStderr);
#else
    Require(::dup2(g_savedStderr, 2) == 2);
    ::close(g_savedStderr);
#endif
    g_savedStderr = -1;
    std::ifstream input(g_capture, std::ios::binary);
    std::stringstream text;
    text << input.rdbuf();
    std::string result = text.str();
    std::string normalized;
    for (std::size_t index = 0; index < result.size(); ++index) {
        if (result[index] == '\r' && index + 1 < result.size() && result[index + 1] == '\n') continue;
        normalized += result[index];
    }
    return normalized;
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
    Require(EndCapture() == "XSI:cat XSI:cat:001\n");

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

    SetVerbosity("text:label");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmError, "illegal option", nullptr, nullptr) == MmOk);
    Require(EndCapture() == "illegal option: XSI:cat\n");

    SetVerbosity("label::text");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmError, "illegal option", "refer", nullptr) == MmOk);
    Require(EndCapture() == "XSI:cat: illegal option\n");

    SetVerbosity("label:");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmError, "illegal option", "refer", "XSI:cat:001") == MmOk);
    Require(EndCapture() == "XSI:cat\n");

    SetVerbosity("label:severity:text:action:tag:tag");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint | MmHard, "XSI:cat", MmError, "illegal option", "refer to cat in user's reference manual", "XSI:cat:001") == MmOk);
    Require(EndCapture() == "XSI:cat: ERROR: illegal option\nTO FIX: refer to cat in user's reference manual XSI:cat:001\n");

    SetVerbosity("::");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmError, "illegal option", "refer", "XSI:cat:001") == MmOk);
    Require(EndCapture().empty());

    SetVerbosity(nullptr);
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint, "XSI:cat", MmError, "illegal option", nullptr, "XSI:cat:001") == MmOk);
    Require(EndCapture() == "XSI:cat: ERROR: illegal option XSI:cat:001\n");

    BeginCapture();
    Require(fmtmsg_nid_postfix(MmConsole, nullptr, 0, nullptr, nullptr, nullptr) == MmOk);
    Require(EndCapture().empty());

    SetVerbosity(nullptr);
    SetHostVerbosity("label");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint | MmHard, "XSI:cat", MmError, "illegal option", "refer to cat in user's reference manual", "XSI:cat:001") == MmOk);
    Require(EndCapture() == "XSI:cat: ERROR: illegal option\nTO FIX: refer to cat in user's reference manual XSI:cat:001\n");

    SetVerbosity("");
    BeginCapture();
    Require(fmtmsg_nid_postfix(MmPrint | MmHard, "XSI:cat", MmError, "illegal option", "refer to cat in user's reference manual", "XSI:cat:001") == MmOk);
    Require(EndCapture() == "XSI:cat: ERROR: illegal option\nTO FIX: refer to cat in user's reference manual XSI:cat:001\n");

    SetVerbosity(nullptr);
    SetHostVerbosity(nullptr);

    std::filesystem::remove(g_capture);
    return 0;
}
