#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/libc.hpp"
#include <csignal>
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

extern "C" FileStream* __stderrp_nid_postfix;

static void Require(bool value) { if (!value) std::abort(); }

struct Outcome {
    std::string output;
    int status = 0;
};

static Outcome Run(const char* expression, const char* function) {
    int channel[2] = {-1, -1};
    Require(::pipe(channel) == 0);
    const pid_t child = ::fork();
    Require(child >= 0);
    if (child == 0) {
        ::close(channel[0]);
        if (::dup2(channel[1], 2) != 2) ::_exit(70);
        ::close(channel[1]);
#ifdef __linux__
        if (::prctl(PR_SET_DUMPABLE, 0) != 0) ::_exit(71);
#endif
        _ZSt14_Atomic_assertPKcS0__nid_postfix(expression, function);
    }
    ::close(channel[1]);
    Outcome outcome;
    char buffer[256];
    for (ssize_t count; (count = ::read(channel[0], buffer, sizeof(buffer))) > 0;) outcome.output.append(buffer, static_cast<std::size_t>(count));
    ::close(channel[0]);
    Require(::waitpid(child, &outcome.status, 0) == child);
    return outcome;
}

int main() {
    const auto named = Run("invalid memory order", "atomic_load");
    Require(WIFSIGNALED(named.status) && WTERMSIG(named.status) == SIGABRT);
    Require(named.output == "invalid memory order in function atomic_load\n");

    const auto anonymous = Run("invalid memory order", nullptr);
    Require(WIFSIGNALED(anonymous.status) && WTERMSIG(anonymous.status) == SIGABRT);
    Require(anonymous.output == "invalid memory order\n");

    const std::filesystem::path probe = "guest_stderr_probe_" + std::to_string(::getpid()) + ".txt";
    const pid_t redirected = ::fork();
    Require(redirected >= 0);
    if (redirected == 0) {
        if (freopen_nid_postfix(probe.c_str(), "w", __stderrp_nid_postfix) != __stderrp_nid_postfix) ::_exit(72);
        if (setvbuf_nid_postfix(__stderrp_nid_postfix, nullptr, 0, 0) != 0) ::_exit(73);
#ifdef __linux__
        if (::prctl(PR_SET_DUMPABLE, 0) != 0) ::_exit(71);
#endif
        _ZSt14_Atomic_assertPKcS0__nid_postfix("redirected assertion", "sample_func");
    }
    int redirectedStatus = 0;
    Require(::waitpid(redirected, &redirectedStatus, 0) == redirected);
    Require(WIFSIGNALED(redirectedStatus) && WTERMSIG(redirectedStatus) == SIGABRT);
    std::ifstream stream(probe, std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    stream.close();
    std::filesystem::remove(probe);
    Require(content == "redirected assertion in function sample_func\n");

    bool threw = false;
    try {
        _ZSt14_Atomic_assertPKcS0__nid_postfix(nullptr, "atomic_load");
    } catch (const std::runtime_error& error) {
        threw = true;
        Require(std::string(error.what()).find("null string") != std::string::npos);
    }
    Require(threw);

    return 0;
}
