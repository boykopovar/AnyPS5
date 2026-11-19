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
    if (running->fetch_add(1) >= HOST_BROWSER_MAX_OPENERS) {
        --*running;
        return false;
    }
    std::string name = program;
    std::string address = url;
    char* arguments[] = {name.data(), address.data(), nullptr};
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        --*running;
        return false;
    }
    posix_spawnattr_t attributes;
    if (posix_spawnattr_init(&attributes) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        --*running;
        return false;
    }
    bool prepared = posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0) == 0 &&
                    posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", O_WRONLY, 0) == 0 &&
                    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0) == 0 &&
#ifdef __APPLE__
                    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT) == 0;
#else
                    posix_spawn_file_actions_addclosefrom_np(&actions, 3) == 0;
#endif
    pid_t child = 0;
    const int error = prepared ? posix_spawnp(&child, name.c_str(), &actions, &attributes, arguments, HostEnvironment()) : -1;
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    if (error != 0) {
        --*running;
        return false;
    }
    std::thread([child, count = running] {
        int status = 0;
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
        }
        --*count;
    }).detach();
    return true;
}
