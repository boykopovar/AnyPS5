#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

extern "C" {

extern FileStream* __stderrp_nid_postfix;
extern FileStream* __stdoutp_nid_postfix;
}

static void Require(bool value) { if (!value) std::abort(); }

static void SkipCrashHandler() {
#ifdef __linux__
    Require(::prctl(PR_SET_DUMPABLE, 0) == 0);
#endif
}

enum class Redirect { Duplicate, Reopen };

struct Outcome {
    std::string host;
    int status = 0;
};

static void RedirectGuest(Redirect redirect, const std::filesystem::path& probe) {
    if (redirect == Redirect::Duplicate) {
        int guest[2] = {-1, -1};
        Require(pipe_nid_postfix(guest) == 0);
        Require(dup2_nid_postfix(guest[1], 2) == 2);
        Require(write_nid_postfix(2, "guest", 5) == 5);
        return;
    }
    Require(freopen_nid_postfix(probe.c_str(), "w", __stderrp_nid_postfix) == __stderrp_nid_postfix);
    Require(setvbuf_nid_postfix(__stderrp_nid_postfix, nullptr, 0, 0) == 0);
    Require(fwrite_nid_postfix("guest", 1, 5, __stderrp_nid_postfix) == 5);
}

static Outcome Run(Redirect redirect, const std::filesystem::path& probe, bool terminate) {
    int host[2] = {-1, -1};
    Require(::pipe(host) == 0);
    const pid_t child = ::fork();
    Require(child >= 0);
    if (child == 0) {
        ::close(host[0]);
        if (::dup2(host[1], 2) != 2) ::_exit(70);
        ::close(host[1]);
        RedirectGuest(redirect, probe);
        std::fputs("host\n", stderr);
        if (terminate) {
            SkipCrashHandler();
            throw std::runtime_error("report kept");
        }
        if (redirect == Redirect::Reopen && fflush_nid_postfix(__stderrp_nid_postfix) != 0) ::_exit(71);
        ::_exit(0);
    }
    ::close(host[1]);
    Outcome outcome;
    char buffer[4096];
    for (ssize_t count; (count = ::read(host[0], buffer, sizeof(buffer))) > 0;) outcome.host.append(buffer, static_cast<std::size_t>(count));
    ::close(host[0]);
    Require(::waitpid(child, &outcome.status, 0) == child);
    return outcome;
}

static std::string Contents(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

int main() {
    const std::filesystem::path probe = "host_stderr_probe_" + std::to_string(::getpid()) + ".txt";
    for (const auto redirect : {Redirect::Duplicate, Redirect::Reopen}) {
        const auto written = Run(redirect, probe, false);
        Require(WIFEXITED(written.status) && WEXITSTATUS(written.status) == 0);
        Require(written.host == "host\n");
        if (redirect == Redirect::Reopen) Require(Contents(probe) == "guest");

        const auto terminated = Run(redirect, probe, true);
        Require(WIFSIGNALED(terminated.status) && WTERMSIG(terminated.status) == SIGABRT);
        Require(terminated.host.rfind("host\n", 0) == 0);
        Require(terminated.host.find("  what():  report kept\n") != std::string::npos);
        if (redirect == Redirect::Reopen) Require(Contents(probe).find("report kept") == std::string::npos);
    }
    std::filesystem::remove(probe);

    int host[2] = {-1, -1};
    Require(::pipe(host) == 0);
    const pid_t child = ::fork();
    Require(child >= 0);
    if (child == 0) {
        ::close(host[0]);
        if (::dup2(host[1], 1) != 1) ::_exit(70);
        ::close(host[1]);
        int guest[2] = {-1, -1};
        if (pipe_nid_postfix(guest) != 0 || dup2_nid_postfix(guest[1], 1) != 1) ::_exit(72);
        std::fputs("host\n", stdout);
        std::fflush(stdout);
        ::_exit(0);
    }
    ::close(host[1]);
    std::string output;
    char buffer[64];
    for (ssize_t count; (count = ::read(host[0], buffer, sizeof(buffer))) > 0;) output.append(buffer, static_cast<std::size_t>(count));
    ::close(host[0]);
    int status = 0;
    Require(::waitpid(child, &status, 0) == child);
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    Require(output == "host\n");

    for (const int standard : {1, 2}) {
        int host[2] = {-1, -1};
        Require(::pipe(host) == 0);
        const pid_t child = ::fork();
        Require(child >= 0);
        if (child == 0) {
            ::close(host[0]);
            int guest[2] = {-1, -1};
            if (pipe_nid_postfix(guest) != 0 || dup2_nid_postfix(guest[1], standard) != standard) ::_exit(72);
            const char* expected = "guest-fprintf\n";
            if (standard == 1) {
                if (printf_nid_postfix("guest-printf\n") < 0 || puts_nid_postfix("guest-puts") < 0 || fflush_nid_postfix(__stdoutp_nid_postfix) != 0) ::_exit(73);
                expected = "guest-printf\nguest-puts\n";
            } else {
                if (fprintf_nid_postfix(__stderrp_nid_postfix, "guest-fprintf\n") < 0) ::_exit(73);
                *__error_nid_postfix() = 2;
                perror_nid_postfix("guest-perror");
                if (fflush_nid_postfix(__stderrp_nid_postfix) != 0) ::_exit(73);
                expected = "guest-fprintf\nguest-perror: No such file or directory\n";
            }
            std::string got;
            char buffer[64];
            const auto length = std::char_traits<char>::length(expected);
            for (ssize_t count; got.size() < length && (count = ::read(guest[0], buffer, sizeof(buffer))) > 0;) got.append(buffer, static_cast<std::size_t>(count));
            if (got != expected) ::_exit(75);
            if (::write(host[1], "ok", 2) != 2) ::_exit(76);
            ::_exit(0);
        }
        ::close(host[1]);
        std::string marker;
        char buffer[8];
        for (ssize_t count; (count = ::read(host[0], buffer, sizeof(buffer))) > 0;) marker.append(buffer, static_cast<std::size_t>(count));
        ::close(host[0]);
        int status = 0;
        Require(::waitpid(child, &status, 0) == child);
        Require(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        Require(marker == "ok");
    }
    return 0;
}
