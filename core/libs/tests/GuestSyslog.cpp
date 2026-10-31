#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" {
#ifndef SYSLOG_RELOAD_TEST
extern const char* __progname_nid_postfix;
#endif
}
static void Require(bool value) { if (!value) std::abort(); }
static void RedirectStderr(int descriptor) {
    std::fflush(stderr);
#ifdef _WIN32
    Require(::_dup2(descriptor, ::_fileno(stderr)) == 0);
#else
    Require(::dup2(descriptor, ::fileno(stderr)) == ::fileno(stderr));
#endif
}
#ifdef SYSLOG_RELOAD_TEST
using Module = HMODULE;
static Module LoadKernel(const char* path) { return LoadLibraryW(std::filesystem::path(path).c_str()); }
static const char** ProgramName(Module module) { return reinterpret_cast<const char**>(GetProcAddress(module, "__progname_nid_postfix")); }
static void UnloadKernel(Module module) { Require(FreeLibrary(module) != 0); }
#endif

int main(int argc, char** argv) {
#ifdef SYSLOG_RELOAD_TEST
    Require(argc == 2);
#else
    Require(argc == 1);
#endif
    auto* captured = std::tmpfile();
    Require(captured != nullptr);
    std::fflush(stderr);
#ifdef _WIN32
    const int saved = ::_dup(::_fileno(stderr));
    const int captureDescriptor = ::_fileno(captured);
#else
    const int saved = ::dup(::fileno(stderr));
    const int captureDescriptor = ::fileno(captured);
#endif
    Require(saved >= 0);
    RedirectStderr(captureDescriptor);
#ifdef SYSLOG_RELOAD_TEST
    const auto loadKernel = [&] {
        RedirectStderr(saved);
        auto module = LoadKernel(argv[1]);
        RedirectStderr(captureDescriptor);
        Require(module != nullptr);
        return module;
    };
    syslog_nid_postfix(6, "before kernel load");
    auto kernel = loadKernel();
    auto* programName = ProgramName(kernel);
#else
    auto* programName = &__progname_nid_postfix;
    const char* const originalName = *programName;
#endif
    Require(programName != nullptr && std::strcmp(*programName, "eboot.bin") == 0);
    syslog_nid_postfix(6, "after kernel load");
    closelog_nid_postfix();
    *programName = "syslog-test";
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
    const int beforeThread = *__error_nid_postfix();
    Require(beforeThread == 2);
    std::thread([] { Require(setlogmask_nid_postfix(0xff) == 0x55); }).join();
    *__error_nid_postfix() = beforeThread;
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
    openlog_nid_postfix(nullptr, 0, 1 << 3);
    closelog_nid_postfix();
    char firstName[] = "first";
    *programName = firstName;
    syslog_nid_postfix(6, "program name fallback");
    *programName = "second";
    syslog_nid_postfix(6, "cached program pointer");
    firstName[0] = 'F';
    syslog_nid_postfix(6, "program buffer mutation");
    openlog_nid_postfix(nullptr, 1, 0);
    syslog_nid_postfix(6, "null identity retains fallback");
    closelog_nid_postfix();
    syslog_nid_postfix(6, "close refreshes fallback");
    openlog_nid_postfix(nullptr, 0, 0);
    closelog_nid_postfix();
    *programName = "masked";
    Require(setlogmask_nid_postfix(1 << 3) == 0xff);
    syslog_nid_postfix(7, "masked fallback");
    *programName = "unmasked";
    Require(setlogmask_nid_postfix(0xff) == (1 << 3));
    syslog_nid_postfix(6, "masked message leaves tag unset");
    closelog_nid_postfix();
    *programName = nullptr;
    syslog_nid_postfix(6, "null program name");
    *programName = "";
    syslog_nid_postfix(6, "empty program name");
    *programName = "next";
    syslog_nid_postfix(6, "cached empty program name");
    closelog_nid_postfix();
    syslog_nid_postfix(6, "updated program name");
    char identity[] = "explicit";
    openlog_nid_postfix(identity, 0, 0);
    identity[0] = 'E';
    syslog_nid_postfix(6, "explicit identity pointer");
    openlog_nid_postfix("", 1, 0);
    syslog_nid_postfix(6, "empty explicit identity");
    closelog_nid_postfix();
    openlog_nid_postfix(nullptr, 0, 0);
    auto releasedName = std::make_unique<char[]>(9);
    std::strcpy(releasedName.get(), "released");
    *programName = releasedName.get();
    syslog_nid_postfix(6, "before cached storage release");
    *programName = "replacement";
    closelog_nid_postfix();
    releasedName.reset();
    syslog_nid_postfix(6, "after close and storage release");
    closelog_nid_postfix();
#ifdef SYSLOG_RELOAD_TEST
    auto transientName = std::make_unique<char[]>(10);
    std::strcpy(transientName.get(), "transient");
    *programName = transientName.get();
    syslog_nid_postfix(6, "before kernel unload");
    UnloadKernel(kernel);
    transientName.reset();
    syslog_nid_postfix(6, "after kernel unload");
    kernel = loadKernel();
    programName = ProgramName(kernel);
    Require(programName != nullptr && std::strcmp(*programName, "eboot.bin") == 0);
    syslog_nid_postfix(6, "after kernel reload");
    openlog_nid_postfix("persistent", 0, 0);
    UnloadKernel(kernel);
    syslog_nid_postfix(6, "explicit identity survives kernel unload");
    closelog_nid_postfix();
    syslog_nid_postfix(6, "close clears explicit identity after unload");
#else
    *programName = originalName;
#endif
    RedirectStderr(saved);
#ifdef _WIN32
    Require(::_close(saved) == 0);
#else
    Require(::close(saved) == 0);
#endif
    Require(afterFirst == 2 && *__error_nid_postfix() == 2);
    std::rewind(captured);
    char text[4096]{};
    const auto length = std::fread(text, 1, sizeof(text) - 1, captured);
    const char* expected =
#ifdef SYSLOG_RELOAD_TEST
        "[syslog:14] before kernel load\n"
#endif
        "[syslog:14] eboot.bin: after kernel load\n"
        "[syslog:14] syslog-test: station radio at 128 kbps\n"
        "[syslog:19] syslog-test: open: No such file or directory, %m kept, 50%\n"
        "[syslog:35] syslog-test: syslog: unknown facility/priority: 10004\n"
        "[syslog:12] syslog-test: masked\n"
        "[syslog:15] syslog-test: \n"
        "[syslog:19] syslog-test: allowed: No such file or directory\n"
        "[syslog:35] syslog-test: syslog: unknown facility/priority: 10007\n"
        "[syslog:12] syslog-test: warning without diagnostic\n"
        "[syslog:15] syslog-test: negative mask\n"
        "[syslog:16] syslog-test: level 0\n"
        "[syslog:18] syslog-test: level 2\n"
        "[syslog:20] syslog-test: level 4\n"
        "[syslog:22] syslog-test: level 6\n"
        "[syslog:15] syslog-test: mask shared across threads\n"
        "[syslog:15] syslog-test: wide guest 5 text\n";
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
        "[syslog:198] syslog-test[" + pid + "]: identity cleared, options and facility retained\n";
    const std::string fallback =
        "[syslog:14] first: program name fallback\n"
        "[syslog:14] first: cached program pointer\n"
        "[syslog:14] First: program buffer mutation\n"
        "[syslog:14] First[" + pid + "]: null identity retains fallback\n"
        "[syslog:14] second[" + pid + "]: close refreshes fallback\n"
        "[syslog:14] unmasked: masked message leaves tag unset\n"
        "[syslog:14] null program name\n"
        "[syslog:14] : empty program name\n"
        "[syslog:14] : cached empty program name\n"
        "[syslog:14] next: updated program name\n"
        "[syslog:14] Explicit: explicit identity pointer\n"
        "[syslog:14] [" + pid + "]: empty explicit identity\n"
        "[syslog:14] released: before cached storage release\n"
        "[syslog:14] replacement: after close and storage release\n";
#ifdef SYSLOG_RELOAD_TEST
    const std::string reload =
        "[syslog:14] transient: before kernel unload\n"
        "[syslog:14] after kernel unload\n"
        "[syslog:14] eboot.bin: after kernel reload\n"
        "[syslog:14] persistent: explicit identity survives kernel unload\n"
        "[syslog:14] close clears explicit identity after unload\n";
#else
    const std::string reload;
#endif
    if (std::string(text, length) != prefix + configured + fallback + reload) {
        std::fprintf(stderr, "Unexpected syslog output:\n%s", text);
        std::abort();
    }
    Require(std::fclose(captured) == 0);
}
