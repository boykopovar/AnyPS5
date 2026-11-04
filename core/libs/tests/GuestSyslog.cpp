#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }
int main() {
    auto* captured = TemporaryFile::Open();
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
    Require(setlogmask_nid_postfix(0) == 0xff);
    syslog_nid_postfix(6, "station %s at %d kbps", "radio", 128);

    const int afterFirst = *__error_nid_postfix();
    syslog_nid_postfix(16 | 3, "open: %m, %%m kept, %d%%\n", 50);
    syslog_nid_postfix(0x10000 | 4, "masked");
    syslog_nid_postfix(7, "");

    Require(setlogmask_nid_postfix(1 << 3) == 0xff);
    Require(setlogmask_nid_postfix(0) == (1 << 3));
    int count = -1;
    syslog_nid_postfix(7, "suppressed %m%n", &count);

    Require(count == -1 && *__error_nid_postfix() == 2);
    syslog_nid_postfix(16 | 3, "allowed: %m");

    Require(*__error_nid_postfix() == 2);
    syslog_nid_postfix(0x10000 | 7, "suppressed %n", &count);

    Require(count == -1 && *__error_nid_postfix() == 2);
    Require(setlogmask_nid_postfix(1 << 4) == (1 << 3));
    syslog_nid_postfix(0x10000 | 4, "warning without diagnostic");
    syslog_nid_postfix(0x10000 | 7, "both suppressed %n", &count);

    Require(count == -1 && *__error_nid_postfix() == 2);
    Require(setlogmask_nid_postfix(0x100) == (1 << 4));
    Require(setlogmask_nid_postfix(0) == 0x100);
    syslog_nid_postfix(0x10000 | 3, "no priorities enabled %n", &count);

    Require(count == -1 && *__error_nid_postfix() == 2);
    Require(setlogmask_nid_postfix(-1) == 0x100);
    Require(setlogmask_nid_postfix(0) == -1);
    syslog_nid_postfix(7, "negative mask");

    Require(setlogmask_nid_postfix(0x55) == -1);
    for (int priority = 0; priority < 8; ++priority)
        syslog_nid_postfix(16 | priority, "level %d", priority);

    std::thread([] { Require(setlogmask_nid_postfix(0xff) == 0x55); }).join();
    Require(setlogmask_nid_postfix(0) == 0xff);
    syslog_nid_postfix(7, "mask shared across threads");
    syslog_nid_postfix(7, "wide %ls %d %S", u"guest", 5, u"text");
    openlog_nid_postfix("worker", 1, 16 << 3);
    syslog_nid_postfix(6, "configured default facility");
    openlog_nid_postfix(nullptr, 1, 0);
    syslog_nid_postfix(6, "null identity keeps tag");
    openlog_nid_postfix("daemon", 0, 24 << 3);
    syslog_nid_postfix(16 | 3, "explicit facility");
    openlog_nid_postfix("closing", 1, 0);
    closelog_nid_postfix();
    syslog_nid_postfix(6, "identity cleared, options and facility retained");

    std::fflush(stderr);
#ifdef _WIN32
    Require(::_dup2(saved, ::_fileno(stderr)) == 0 && ::_close(saved) == 0);
#else
    Require(::dup2(saved, ::fileno(stderr)) == ::fileno(stderr) && ::close(saved) == 0);
#endif
    Require(afterFirst == 2 && *__error_nid_postfix() == 2);
    std::rewind(captured);
    char text[1024]{};
    const auto length = std::fread(text, 1, sizeof(text) - 1, captured);
    const char* expected =
        "[syslog:14] station radio at 128 kbps\n"
        "[syslog:19] open: No such file or directory, %m kept, 50%\n"
        "[syslog:35] syslog: unknown facility/priority: 10004\n"
        "[syslog:12] masked\n"
        "[syslog:15] \n"
        "[syslog:19] allowed: No such file or directory\n"
        "[syslog:35] syslog: unknown facility/priority: 10007\n"
        "[syslog:12] warning without diagnostic\n"
        "[syslog:15] negative mask\n"
        "[syslog:16] level 0\n"
        "[syslog:18] level 2\n"
        "[syslog:20] level 4\n"
        "[syslog:22] level 6\n"
        "[syslog:15] mask shared across threads\n"
        "[syslog:15] wide guest 5 text\n";
    const std::string prefix = expected;
    const std::string pid = std::to_string(
#ifdef _WIN32
        ::_getpid()
#else
        ::getpid()
#endif
    );
    const std::string configured = "[syslog:134] worker[" + pid + "]: configured default facility\n"
        "[syslog:134] worker[" + pid + "]: null identity keeps tag\n"
        "[syslog:19] daemon: explicit facility\n"
        "[syslog:198] [" + pid + "]identity cleared, options and facility retained\n";
    Require(length == prefix.size() + configured.size() && std::string(text, length) == prefix + configured);
    Require(std::fclose(captured) == 0);
}
