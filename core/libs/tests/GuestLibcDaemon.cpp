#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "daemon: %s\n", message);
    std::abort();
}

struct StandardIdentity {
    bool valid[3]{};
    std::uint64_t first[3]{};
    std::uint64_t second[3]{};
};

StandardIdentity CaptureStandardDescriptors() {
    StandardIdentity identity;
    for (int descriptor = 0; descriptor <= 2; ++descriptor) {
#ifdef _WIN32
        const intptr_t handle = ::_get_osfhandle(descriptor);
        identity.valid[descriptor] = handle != -1 && handle != -2;
        identity.first[descriptor] = static_cast<std::uint64_t>(handle);
#else
        struct stat status {};
        identity.valid[descriptor] = ::fstat(descriptor, &status) == 0;
        if (identity.valid[descriptor]) {
            identity.first[descriptor] = static_cast<std::uint64_t>(status.st_dev);
            identity.second[descriptor] = static_cast<std::uint64_t>(status.st_ino);
        }
#endif
    }
    return identity;
}

void RequireStandardDescriptorsUnchanged(const StandardIdentity& before, const char* message) {
    const StandardIdentity after = CaptureStandardDescriptors();
    for (int descriptor = 0; descriptor <= 2; ++descriptor) {
        Require(before.valid[descriptor] == after.valid[descriptor], message);
        Require(before.first[descriptor] == after.first[descriptor], message);
        Require(before.second[descriptor] == after.second[descriptor], message);
    }
}

void TestNoChdirNoClose() {
    const StandardIdentity descriptors = CaptureStandardDescriptors();
    char initialDir[1024]{};
    Require(getcwd_nid_postfix(initialDir, sizeof(initialDir)) != nullptr, "initial getcwd");
    Require(daemon_nid_postfix(1, 1) == 0, "daemon(1, 1)");
    char currentDir[1024]{};
    Require(getcwd_nid_postfix(currentDir, sizeof(currentDir)) != nullptr, "post daemon(1, 1) getcwd");
    Require(std::strcmp(initialDir, currentDir) == 0, "working directory unchanged when nochdir is set");
    RequireStandardDescriptorsUnchanged(descriptors, "daemon(1, 1) left the host standard descriptors in place");
}

void TestNoChdirClose() {
    const StandardIdentity descriptors = CaptureStandardDescriptors();
    char initialDir[1024]{};
    Require(getcwd_nid_postfix(initialDir, sizeof(initialDir)) != nullptr, "initial getcwd before daemon(1, 0)");
    Require(daemon_nid_postfix(1, 0) == 0, "daemon(1, 0)");
    char currentDir[1024]{};
    Require(getcwd_nid_postfix(currentDir, sizeof(currentDir)) != nullptr, "post daemon(1, 0) getcwd");
    Require(std::strcmp(initialDir, currentDir) == 0, "working directory unchanged when nochdir is set with close");
    RequireStandardDescriptorsUnchanged(descriptors, "daemon(1, 0) left the host standard descriptors in place");
}

void TestChdirNoClose() {
    const StandardIdentity descriptors = CaptureStandardDescriptors();
    Require(daemon_nid_postfix(0, 1) == 0, "daemon(0, 1)");
    char currentDir[1024]{};
    Require(getcwd_nid_postfix(currentDir, sizeof(currentDir)) != nullptr, "post daemon(0, 1) getcwd");
    Require(std::strcmp(currentDir, "/") == 0, "working directory is root when nochdir is 0");
    RequireStandardDescriptorsUnchanged(descriptors, "daemon(0, 1) left the host standard descriptors in place");
}

void TestChdirClose() {
    const StandardIdentity descriptors = CaptureStandardDescriptors();
    Require(daemon_nid_postfix(0, 0) == 0, "daemon(0, 0)");
    char currentDir[1024]{};
    Require(getcwd_nid_postfix(currentDir, sizeof(currentDir)) != nullptr, "post daemon(0, 0) getcwd");
    Require(std::strcmp(currentDir, "/") == 0, "working directory is root when nochdir is 0 with close");
    RequireStandardDescriptorsUnchanged(descriptors, "daemon(0, 0) left the host standard descriptors in place");
}

} // namespace

int main() {
    TestNoChdirNoClose();
    TestNoChdirClose();
    TestChdirNoClose();
    TestChdirClose();
    return 0;
}
