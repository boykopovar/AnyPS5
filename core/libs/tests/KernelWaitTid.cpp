// Tests the APS5_TRACE_WAITS_TID probe of KernelTraceWait_nid_postfix: with the probe off the line
// keeps its old format and merges every thread into one entry per call site; with it on, two threads
// calling the same call site produce two entries keyed by their OS thread id, in a labelled window.
#include "prx/libkernel/Time/include/Time.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

void check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", reason);
        std::exit(1);
    }
}

// Same identifier domain as the probe: the host OS thread id.
unsigned long osThreadId() {
#ifdef _WIN32
    return static_cast<unsigned long>(::GetCurrentThreadId());
#else
    return static_cast<unsigned long>(::gettid());
#endif
}

struct Entry {
    std::string kind;
    std::string site;
    long long calls = 0;
    long long tid = -1;
};

std::vector<Entry> parseEntries(const std::string& line) {    std::vector<Entry> entries;
    std::size_t at = line.find(" | ");
    while (at != std::string::npos) {
        const std::size_t begin = at + 3;
        const std::size_t next = line.find(" | ", begin);
        const std::string part = line.substr(begin, next == std::string::npos ? std::string::npos : next - begin);
        Entry entry;
        char site[96] = {};
        char kind[32] = {};
        if (std::sscanf(part.c_str(), "%31s %95s", kind, site) == 2) {
            entry.kind = kind;
            entry.site = site;
            // The per-thread key prints "<site> tid <n> x<calls> ..."; the old key prints "<site> x<calls> ...".
            const std::size_t x = part.rfind(" x");
            if (x != std::string::npos) entry.calls = std::atoll(part.c_str() + x + 2);
            const std::size_t t = part.find(" tid ");
            if (t != std::string::npos) entry.tid = std::atoll(part.c_str() + t + 5);
            entries.push_back(entry);
        }
        at = next;
    }
    return entries;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "off";
    const bool perThread = mode == "on" || mode == "many-on";
    const bool many = mode == "many-off" || mode == "many-on";
    const bool focusMode = mode == "focus";
#ifdef _WIN32
    check(_putenv_s("APS5_TRACE_WAITS", "1") == 0, "APS5_TRACE_WAITS was not set");
    check((_putenv_s("APS5_TRACE_WAITS_TID", perThread ? "1" : "") == 0), "APS5_TRACE_WAITS_TID was not set");
#else
    check(setenv("APS5_TRACE_WAITS", "1", 1) == 0, "APS5_TRACE_WAITS was not set");
    check((perThread ? setenv("APS5_TRACE_WAITS_TID", "1", 1) : unsetenv("APS5_TRACE_WAITS_TID")) == 0, "APS5_TRACE_WAITS_TID was not set");
#endif
    const char* path = perThread ? "kernel-wait-tid-on.log" : "kernel-wait-tid-off.log";
    if (many) path = perThread ? "kernel-wait-tid-many-on.log" : "kernel-wait-tid-many-off.log";
    if (focusMode) path = "kernel-wait-tid-focus.log";
    std::remove(path);

    // Redirect stderr for the duration of the probe so its report can be parsed.
    std::fflush(stderr);
#ifdef _WIN32
    const int saved = _dup(2);
    check(saved >= 0, "stderr could not be duplicated");
    const int fd = _open(path, _O_CREAT | _O_WRONLY | _O_TRUNC | _O_TEXT, _S_IREAD | _S_IWRITE);
    check(fd >= 0, "log file could not be opened");
    check(_dup2(fd, 2) == 0, "stderr could not be redirected");
    _close(fd);
#else
    std::check(freopen(path, "w", stderr) != nullptr, "stderr could not be redirected");
#endif

    const void* site = reinterpret_cast<const void*>(0x1234);
    const void* otherSite = reinterpret_cast<const void*>(0x5678);
    unsigned long focused = 0;
    if (focusMode) {
        // Focus names a thread and the probe must then drop every other thread, keep the old unbounded
        // process report untouched when no focus is set, and only report from the focused thread.
        std::thread focusedThread([&] {
            focused = osThreadId();
            sceKernelTraceWaitsFocus_nid_postfix(focused);
            KernelTraceWait_nid_postfix("sema", site, 1000000, false);
            std::this_thread::sleep_for(std::chrono::milliseconds(3200));
            KernelTraceWait_nid_postfix("sema", site, 1000000, false);
        });
        std::thread otherThread([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            KernelTraceWait_nid_postfix("sema", otherSite, 5000000, false);
        });
        focusedThread.join();
        otherThread.join();
        sceKernelTraceWaitsFocus_nid_postfix(0);
    } else if (many) {
        // Twelve distinct call sites from one thread: the off probe prints the old top 8, the
        // per-thread probe prints up to 24 and must keep every one of them.
        for (int index = 0; index < 12; ++index) {
            KernelTraceWait_nid_postfix("sema", reinterpret_cast<const void*>(0x1000 + 16 * index), 1000000, false);
        }
    } else {
        std::thread first([site] { KernelTraceWait_nid_postfix("sema", site, 1000000, false); });
        std::thread second([site] { KernelTraceWait_nid_postfix("sema", site, 2000000, false); });
        first.join();
        second.join();
    }
    // In focus mode the report already fired from the focused thread and the focus was cleared, so a
    // further call from this thread would open a second, unfocused report line.
    if (!focusMode) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3200));
        KernelTraceWait_nid_postfix("sema", many ? reinterpret_cast<const void*>(0x1000) : site, 1000000, false);
    }
    std::fflush(stderr);

#ifdef _WIN32
    check(_dup2(saved, 2) == 0, "stderr could not be restored");
    _close(saved);
#endif
    std::fflush(stderr);

    std::ifstream input(path);
    std::stringstream buffer;
    buffer << input.rdbuf();
    std::remove(path);
    const std::string text = buffer.str();
    check(!text.empty(), "the waits report was not written");
    std::string line;
    std::vector<Entry> entries;
    std::string header;
    std::istringstream lines(text);
    while (std::getline(lines, line)) {
        if (line.rfind("[waits", 0) == 0 || line.rfind("[waits", 0) == std::string::npos) {
            const auto parsed = parseEntries(line);
            if (!parsed.empty() && header.empty()) header = line.substr(0, line.find('|'));
            entries.insert(entries.end(), parsed.begin(), parsed.end());
        }
    }
    check(!entries.empty(), "no waits entries were parsed");

    std::vector<Entry> sema;
    for (const auto& entry : entries) {
        if (entry.kind == "sema") sema.push_back(entry);
    }
    if (focusMode) {
        const std::string expected = "[waits focus tid " + std::to_string(focused);
        check(header.find(expected) == 0, "the focus report must name the focused thread in its header");
        check(!sema.empty(), "the focused thread's waits must be reported");
        long long total = 0;
        bool onlyFocused = true;
        for (const auto& entry : sema) {
            if (entry.tid != static_cast<long long>(focused)) onlyFocused = false;
            total += entry.calls;
        }
        check(onlyFocused, "every reported entry must belong to the focused thread");
        check(total == 2, "the other thread's single call must be dropped");
    } else if (many) {
        // Off: the merge keeps 12 call-site keys but the report is bounded to the old top 8.
        // On: up to 24 keys are printed, so all 12 survive.
        check(sema.size() == (perThread ? 12u : 8u), perThread
            ? "the per-thread probe must print every call site up to 24"
            : "the off probe must keep the old top 8 bound");
        check(header.find(perThread ? "[waits " : "[waits]") == 0, "unexpected header");
    } else if (!perThread) {
        check(header.find("[waits]") == 0, "the off probe must keep the old header");
        check(sema.size() == 1, "the off probe must merge both threads into one entry per call site");
        check(sema[0].calls == 3, "the merged entry must count the three calls");
    } else {
        check(header.find("[waits ") == 0 && header.find("..") != std::string::npos, "the on probe must label the window");
        check(sema.size() >= 2, "the on probe must keep one entry per thread for the same call site");
        check(sema[0].site == sema[1].site, "both entries must share the call site");
        long long total = 0;
        bool distinct = false;
        for (const auto& entry : sema) {
            total += entry.calls;
            for (const auto& other : sema) distinct = distinct || entry.tid != other.tid;
        }
        check(distinct, "the per-thread entries must carry different thread ids");
        check(total == 3, "the per-thread entries must count every call");
    }
    std::printf("kernel wait tid probe %s passed\n", mode.c_str());
    return 0;
}
