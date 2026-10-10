#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
extern "C" {
std::int64_t APS5_VABI sysconf_nid_postfix(int);
int APS5_VABI getpagesize_nid_postfix();
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sysctl_nid_postfix(const int*, std::uint32_t, void*, std::size_t*, const void*, std::size_t);
int APS5_VABI sysctlbyname_nid_postfix(const char*, void*, std::size_t*, const void*, std::size_t);
extern char** environ_nid_postfix;
}
static void Require(bool value) { if (!value) std::abort(); }
static void CheckProcessorCountSysctl() {
    const int mib[] = {6, 3};
    const int processors = static_cast<int>(sysconf_nid_postfix(58));
    *__error_nid_postfix() = 13;
    std::size_t length = 0;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &length, nullptr, 0) == 0 && length == sizeof(int));
    int value = 0;
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, nullptr, 0) == 0 && value == processors && length == sizeof(int));
    value = 0;
    length = 16;
    Require(sysctlbyname_nid_postfix("hw.ncpu", &value, &length, nullptr, 0) == 0 && value == processors && length == sizeof(int));
    Require(*__error_nid_postfix() == 13);
    unsigned char bytes[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    length = 2;
    Require(sysctlbyname_nid_postfix("hw.ncpu", bytes, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 2 && bytes[2] == 0xaa && bytes[3] == 0xaa);
    Require(sysctl_nid_postfix(mib, 2, bytes, nullptr, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    length = sizeof(value);
    Require(sysctl_nid_postfix(mib, 2, &value, &length, &value, sizeof(value)) == -1 && *__error_nid_postfix() == 1);
    Require(sysctl_nid_postfix(mib, 1, &value, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);
    Require(sysctl_nid_postfix(mib, 25, &value, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 22);
    Require(sysctl_nid_postfix(nullptr, 2, &value, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    Require(sysctlbyname_nid_postfix(nullptr, &value, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 14);
    bool threw = false;
    try {
        sysctlbyname_nid_postfix("kern.osreldate", &value, &length, nullptr, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    const int unknown[] = {6, 5};
    try {
        sysctl_nid_postfix(unknown, 2, &value, &length, nullptr, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}
static void CheckSdkVersionSysctl() {
    std::size_t length = 0;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", nullptr, &length, nullptr, 0) == 0 && length == sizeof(std::uint32_t));
    std::uint32_t version = 0;
    length = sizeof(version);
    Require(sysctlbyname_nid_postfix("kern.sdk_version", &version, &length, nullptr, 0) == 0 && version == 0x01000000 && length == sizeof(version));
    unsigned char bytes[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    length = 2;
    Require(sysctlbyname_nid_postfix("kern.sdk_version", bytes, &length, nullptr, 0) == -1 && *__error_nid_postfix() == 12);
    Require(length == 2 && bytes[2] == 0xaa && bytes[3] == 0xaa);
    length = sizeof(version);
    Require(sysctlbyname_nid_postfix("kern.sdk_version", &version, &length, &version, sizeof(version)) == -1 && *__error_nid_postfix() == 1);
}
int main() {
    CheckProcessorCountSysctl();
    CheckSdkVersionSysctl();
    *__error_nid_postfix() = 13;
    Require(sysconf_nid_postfix(47) == 0x4000);
    Require(getpagesize_nid_postfix() == sysconf_nid_postfix(47));
    Require(sysconf_nid_postfix(57) > 0);
    Require(sysconf_nid_postfix(58) > 0);
    Require(sysconf_nid_postfix(121) > 0);
    Require(*__error_nid_postfix() == 13);
    Require(sysconf_nid_postfix(-1) == -1); // verifies full-width signed return
    Require(*__error_nid_postfix() == 22);
    Require(sysconf_nid_postfix(0x7fffffff) == -1);
    Require(environ_nid_postfix && !environ_nid_postfix[0]);
}
