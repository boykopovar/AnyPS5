#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr const char* Marker = "APS5_LOAD_EXEC_STANDARD_ERROR";
constexpr const char* AfterLoadExec = "--after-load-exec";

void Require(bool value) {
    if (!value) std::abort();
}

std::string Drain(int descriptor) {
    std::string text;
    char buffer[4096];
    for (ssize_t count; (count = ::read(descriptor, buffer, sizeof(buffer))) > 0;) text.append(buffer, static_cast<std::size_t>(count));
    ::close(descriptor);
    return text;
}

void Run(const char* mode, const char* const* arguments) {
    int host[2] = {-1, -1};
    int guest[2] = {-1, -1};
    Require(::pipe(host) == 0);
    Require(::pipe(guest) == 0);
    const pid_t child = ::fork();
    Require(child >= 0);
    if (child == 0) {
        ::close(host[0]);
        ::close(guest[0]);
        if (::dup2(host[1], 2) != 2) ::_exit(70);
        ::close(host[1]);
        if (dup2_nid_postfix(guest[1], 2) != 2) ::_exit(71);
        ::close(guest[1]);
        if (::setenv(Marker, mode, 1) != 0) ::_exit(72);
        sceSystemServiceLoadExec("/app0/eboot.bin", arguments);
        ::_exit(73);
    }
    ::close(host[1]);
    ::close(guest[1]);
    const std::string hostText = Drain(host[0]);
    const std::string guestText = Drain(guest[0]);
    int status = 0;
    Require(::waitpid(child, &status, 0) == child);
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    Require(hostText == std::string("started ") + mode + "\n");
    Require(guestText.empty());
}

}

int main(int argc, char** argv) {
    if (const char* mode = std::getenv(Marker); mode != nullptr) {
        if (std::strcmp(mode, "list") == 0) Require(argc == 1 && std::strcmp(argv[0], AfterLoadExec) == 0);
        else Require(std::strcmp(mode, "none") == 0 && argc == 1);
        std::fprintf(stderr, "started %s\n", mode);
        return 0;
    }
    const char* const arguments[] = {AfterLoadExec, nullptr};
    Run("list", arguments);
    Run("none", nullptr);
    std::puts("LoadExec standard error tests passed");
    return 0;
}
