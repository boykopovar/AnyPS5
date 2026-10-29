#include "prx/libSceSystemService/HostUrl.hpp"
#include "prx/libSceSystemService/HostUrlBackends.hpp"
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <utility>
#ifdef __APPLE__
#include <crt_externs.h>
#else
extern char** environ;
#endif

namespace {

char** HostEnvironment() {
#ifdef __APPLE__
    return *_NSGetEnviron();
#else
    return environ;
#endif
}

const char* DefaultProgram() {
#ifdef __APPLE__
    return "/usr/bin/open";
#else
    return "xdg-open";
#endif
}

}

HostBrowserLauncher::HostBrowserLauncher() : program(DefaultProgram()) {}

HostBrowserLauncher::HostBrowserLauncher(std::string program) : program(std::move(program)) {}

bool HostBrowserLauncher::Launch(const std::string& url) {
    if (ValidateHostUrl(url) != HostUrlVerdict::Ok || program.empty()) return false;
    std::string name = program;
    std::string address = url;
    char* arguments[] = {name.data(), address.data(), nullptr};
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) return false;
    bool prepared = posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0) == 0 &&
                    posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0) == 0 &&
                    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0) == 0;
    pid_t child = 0;
    const int error = prepared ? posix_spawnp(&child, name.c_str(), &actions, nullptr, arguments, HostEnvironment()) : -1;
    posix_spawn_file_actions_destroy(&actions);
    if (error != 0) return false;
    std::thread([child] {
        int status = 0;
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
        }
    }).detach();
    return true;
}
