#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

extern "C" {
void APS5_VABI syslog_nid_postfix(int, const char*, ...);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    auto* captured = std::tmpfile();
    Require(captured != nullptr);
    std::fflush(stderr);
#ifdef _WIN32
    const int saved = ::_dup(::_fileno(stderr));
    Require(saved >= 0 && ::_dup2(::_fileno(captured), ::_fileno(stderr)) == 0);
#else
    const int saved = ::dup(::fileno(stderr));
    Require(saved >= 0 && ::dup2(::fileno(captured), ::fileno(stderr)) == ::fileno(stderr));
#endif
    *__error_nid_postfix() = 2;
    syslog_nid_postfix(6, "station %s at %d kbps", "radio", 128);
    const int afterFirst = *__error_nid_postfix();
    syslog_nid_postfix(16 | 3, "open: %m, %%m kept, %d%%\n", 50);
    syslog_nid_postfix(0x10000 | 4, "masked");
    syslog_nid_postfix(7, "");
    std::fflush(stderr);
#ifdef _WIN32
    Require(::_dup2(saved, ::_fileno(stderr)) == 0 && ::_close(saved) == 0);
#else
    Require(::dup2(saved, ::fileno(stderr)) == ::fileno(stderr) && ::close(saved) == 0);
#endif
    Require(afterFirst == 2 && *__error_nid_postfix() == 2);
    std::rewind(captured);
    char text[512]{};
    const auto length = std::fread(text, 1, sizeof(text) - 1, captured);
    const char* expected =
        "[syslog:14] station radio at 128 kbps\n"
        "[syslog:19] open: No such file or directory, %m kept, 50%\n"
        "[syslog:35] syslog: unknown facility/priority: 10004\n"
        "[syslog:12] masked\n"
        "[syslog:15] \n";
    Require(length == std::strlen(expected) && std::strcmp(text, expected) == 0);
    Require(std::fclose(captured) == 0);
}
