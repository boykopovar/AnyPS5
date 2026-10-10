#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {
std::int64_t APS5_VABI sysconf_nid_postfix(int);
int APS5_VABI getpagesize_nid_postfix();
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sysctl_nid_postfix(const int*, std::uint32_t, void*, std::size_t*, const void*, std::size_t);
int APS5_VABI sysctlbyname_nid_postfix(const char*, void*, std::size_t*, const void*, std::size_t);
extern char** environ_nid_postfix;
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int hwNcpu[] = {6, 3};
constexpr int scPageSize = 47;
constexpr int scProcessorsConfigured = 57;
constexpr int scProcessorsOnline = 58;
constexpr int scSupportedName = 121;
constexpr int eperm = 1;
constexpr int enomem = 12;
constexpr int efault = 14;
constexpr int einval = 22;

int Processors() {
    return static_cast<int>(sysconf_nid_postfix(scProcessorsOnline));
}

void RequireFailure(int result, int expectedError, const std::string& message) {
    RequireEqual(result, -1, message + " result");
    RequireEqual(*__error_nid_postfix(), expectedError, message + " errno");
}

const Case sizeQuery{"Sysctl_HwNcpuWithoutBuffer_ReportsIntLength", [] {
    std::size_t length = 0;
    RequireEqual(sysctl_nid_postfix(hwNcpu, 2, nullptr, &length, nullptr, 0), 0, "result");
    RequireEqual(length, sizeof(int), "length");
}};

const Case sysctlValue{"Sysctl_HwNcpu_ReturnsProcessorCountAndKeepsErrno", [] {
    *__error_nid_postfix() = 13;
    int value = 0;
    std::size_t length = sizeof(value);
    RequireEqual(sysctl_nid_postfix(hwNcpu, 2, &value, &length, nullptr, 0), 0, "result");
    RequireEqual(value, Processors(), "value");
    RequireEqual(length, sizeof(int), "length");
    RequireEqual(*__error_nid_postfix(), 13, "errno");
}};

const Case byNameValue{"Sysctlbyname_HwNcpuWithLargeBuffer_ReturnsProcessorCountAndIntLength", [] {
    *__error_nid_postfix() = 13;
    int value = 0;
    std::size_t length = 16;
    RequireEqual(sysctlbyname_nid_postfix("hw.ncpu", &value, &length, nullptr, 0), 0, "result");
    RequireEqual(value, Processors(), "value");
    RequireEqual(length, sizeof(int), "length");
    RequireEqual(*__error_nid_postfix(), 13, "errno");
}};

const Case smallBuffer{"Sysctlbyname_BufferTooSmall_FailsWithEnomemWithoutOverrun", [] {
    unsigned char bytes[4] = {0xaa, 0xaa, 0xaa, 0xaa};
    std::size_t length = 2;
    RequireFailure(sysctlbyname_nid_postfix("hw.ncpu", bytes, &length, nullptr, 0), enomem, "2 byte buffer");
    RequireEqual(length, std::size_t{2}, "length");
    RequireEqual(bytes[2], 0xaa, "byte 2");
    RequireEqual(bytes[3], 0xaa, "byte 3");
}};

const Case nullLength{"Sysctl_BufferWithoutLength_FailsWithEnomem", [] {
    unsigned char bytes[4]{};
    RequireFailure(sysctl_nid_postfix(hwNcpu, 2, bytes, nullptr, nullptr, 0), enomem, "null length");
}};

const Case newValue{"Sysctl_NewValue_FailsWithEperm", [] {
    int value = 0;
    std::size_t length = sizeof(value);
    RequireFailure(sysctl_nid_postfix(hwNcpu, 2, &value, &length, &value, sizeof(value)), eperm, "new value");
}};

const Case badLength{"Sysctl_InvalidNameLength_FailsWithEinval", [] {
    int value = 0;
    std::size_t length = sizeof(value);
    for (const std::uint32_t nameLength : {1u, 25u}) {
        RequireFailure(sysctl_nid_postfix(hwNcpu, nameLength, &value, &length, nullptr, 0), einval,
                       "name length " + std::to_string(nameLength));
    }
}};

const Case nullName{"Sysctl_NullName_FailsWithEfault", [] {
    int value = 0;
    std::size_t length = sizeof(value);
    RequireFailure(sysctl_nid_postfix(nullptr, 2, &value, &length, nullptr, 0), efault, "sysctl");
    RequireFailure(sysctlbyname_nid_postfix(nullptr, &value, &length, nullptr, 0), efault, "sysctlbyname");
}};

const Case unknownName{"Sysctl_UnsupportedName_Throws", [] {
    int value = 0;
    std::size_t length = sizeof(value);
    RequireThrows<std::runtime_error>([&] { sysctlbyname_nid_postfix("kern.osreldate", &value, &length, nullptr, 0); },
                                      "kern.osreldate");
    const int unknown[] = {6, 5};
    RequireThrows<std::runtime_error>([&] { sysctl_nid_postfix(unknown, 2, &value, &length, nullptr, 0); }, "mib 6.5");
}};

const Case pageSize{"Sysconf_PageSize_Is16KiBAndMatchesGetpagesize", [] {
    RequireEqual(sysconf_nid_postfix(scPageSize), std::int64_t{0x4000}, "sysconf page size");
    RequireEqual(static_cast<std::int64_t>(getpagesize_nid_postfix()), sysconf_nid_postfix(scPageSize), "getpagesize");
}};

const Case supportedNames{"Sysconf_SupportedNames_ArePositiveAndKeepErrno", [] {
    *__error_nid_postfix() = 13;
    sysconf_nid_postfix(scPageSize);
    getpagesize_nid_postfix();
    for (const int name : {scProcessorsConfigured, scProcessorsOnline, scSupportedName}) {
        Require(sysconf_nid_postfix(name) > 0, "sysconf(" + std::to_string(name) + ") is positive");
    }
    RequireEqual(*__error_nid_postfix(), 13, "errno");
}};

const Case unknownSysconf{"Sysconf_UnknownName_ReturnsFullWidthMinusOneWithEinval", [] {
    *__error_nid_postfix() = 0;
    RequireEqual(sysconf_nid_postfix(-1), std::int64_t{-1}, "sysconf(-1)");
    RequireEqual(*__error_nid_postfix(), einval, "errno");
    RequireEqual(sysconf_nid_postfix(0x7fffffff), std::int64_t{-1}, "sysconf(0x7fffffff)");
}};

const Case environment{"Environ_GuestEnvironment_IsEmpty", [] {
    Require(environ_nid_postfix != nullptr, "environ is set");
    Require(environ_nid_postfix[0] == nullptr, "environ has no entries");
}};

} // namespace
