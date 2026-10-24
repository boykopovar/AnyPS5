#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <stdexcept>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>
#include <unistd.h>
#endif
extern "C" int APS5_VABI daemon_nid_postfix(int, int);
static void Require(bool value) { if (!value) std::abort(); }
#ifdef _WIN32
static bool ReportsNotImplemented(int noChdir, int noClose) {
    try {
        daemon_nid_postfix(noChdir, noClose);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == "daemon: forking into the background not implemented";
    }
    return false;
}
int main() {
    Require(ReportsNotImplemented(1, 0));
    Require(ReportsNotImplemented(0, 0));
    Require(ReportsNotImplemented(1, 1));
}
#else
static std::string CurrentDirectory() {
    std::array<char, 4096> path{};
    Require(getcwd(path.data(), path.size()) != nullptr);
    return path.data();
}

static bool SameFile(const struct stat& first, const struct stat& second) {
    return first.st_dev == second.st_dev && first.st_ino == second.st_ino;
}

static void RunCase(int noChdir, int noClose, const std::string& originalDirectory, const std::array<struct stat, 3>& originalDescriptors) {
    std::array<char, 32> markerTemplate{};
    std::strcpy(markerTemplate.data(), "/tmp/anyps5-daemon-XXXXXX");
    const int marker = mkstemp(markerTemplate.data());
    Require(marker >= 0);
    close(marker);
    Require(unlink(markerTemplate.data()) == 0);
    const pid_t helper = fork();
    Require(helper >= 0);
    if (helper == 0) {
        if (daemon_nid_postfix(noChdir, noClose) != 0) _exit(10);
        const int output = open(markerTemplate.data(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (output < 0) _exit(11);
        struct stat nullFile{};
        if (stat("/dev/null", &nullFile) != 0) _exit(12);
        struct stat descriptors[3]{};
        for (int descriptor = 0; descriptor < 3; ++descriptor) {
            if (fstat(descriptor, &descriptors[descriptor]) != 0) _exit(13);
        }
        const std::string directory = CurrentDirectory();
        const char values[4] = {
            directory == (noChdir ? originalDirectory : "/") ? '1' : '0',
            getsid(0) == getpid() ? '1' : '0',
            SameFile(descriptors[0], nullFile) && SameFile(descriptors[1], nullFile) && SameFile(descriptors[2], nullFile) ? '1' : '0',
            SameFile(descriptors[0], originalDescriptors[0]) && SameFile(descriptors[1], originalDescriptors[1]) && SameFile(descriptors[2], originalDescriptors[2]) ? '1' : '0',
        };
        if (write(output, values, sizeof(values)) != static_cast<ssize_t>(sizeof(values))) _exit(14);
        close(output);
        _exit(0);
    }
    int status = 0;
    Require(waitpid(helper, &status, 0) == helper && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    std::array<char, 4> values{};
    bool received = false;
    for (int attempt = 0; attempt < 300; ++attempt) {
        const int input = open(markerTemplate.data(), O_RDONLY);
        if (input >= 0) {
            const ssize_t count = read(input, values.data(), values.size());
            close(input);
            if (count == static_cast<ssize_t>(values.size())) {
                received = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    unlink(markerTemplate.data());
    Require(received);
    Require(values[0] == '1');
    Require(values[1] == '1');
    if (noClose) Require(values[3] == '1');
    else Require(values[2] == '1');
}

int main() {
    const std::string originalDirectory = CurrentDirectory();
    std::array<struct stat, 3> originalDescriptors{};
    for (int descriptor = 0; descriptor < 3; ++descriptor) Require(fstat(descriptor, &originalDescriptors[descriptor]) == 0);
    for (int noChdir = 0; noChdir <= 1; ++noChdir) {
        for (int noClose = 0; noClose <= 1; ++noClose) RunCase(noChdir, noClose, originalDirectory, originalDescriptors);
    }
}
#endif
