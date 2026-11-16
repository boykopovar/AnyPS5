#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

namespace {

constexpr int guestEperm = 1;
constexpr int guestEnoent = 2;
constexpr int guestEnomem = 12;
constexpr int guestEfault = 14;
constexpr int guestEinval = 22;

constexpr int ctlHw = 6;
constexpr int hwNcpu = 3;
constexpr int hwByteorder = 4;
constexpr int hwPhysmem = 5;
constexpr int hwPagesize = 7;
constexpr int hwUsermem = 11;

constexpr int expectedPagesize = 16384;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

void AuditParameterValidation() {
    const int mibPagesize[] = {ctlHw, hwPagesize};
    int val = 0;
    std::size_t len = sizeof(val);

    constexpr std::uint32_t invalidLengths[] = {0, 1, 25, 26, 100, 1000};
    for (const std::uint32_t badLength : invalidLengths) {
        *__error_nid_postfix() = 0;
        RequireFailure(sysctl_nid_postfix(mibPagesize, badLength, &val, &len, nullptr, 0), guestEinval);
    }

    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(nullptr, 2, &val, &len, nullptr, 0), guestEfault);

    auto* unmappedMib = reinterpret_cast<const int*>(std::uintptr_t{0x10});
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(unmappedMib, 2, &val, &len, nullptr, 0), guestEfault);

    auto* unmappedHighMib = reinterpret_cast<const int*>(~std::uintptr_t{0xfff});
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(unmappedHighMib, 2, &val, &len, nullptr, 0), guestEfault);

    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix(nullptr, &val, &len, nullptr, 0), guestEfault);

    auto* unmappedName = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix(unmappedName, &val, &len, nullptr, 0), guestEfault);

    auto* unmappedHighName = reinterpret_cast<const char*>(~std::uintptr_t{0xfff});
    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix(unmappedHighName, &val, &len, nullptr, 0), guestEfault);

    auto* unmappedPtr = reinterpret_cast<void*>(std::uintptr_t{0x10});
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(mibPagesize, 2, unmappedPtr, &len, nullptr, 0), guestEfault);

    auto* unmappedLen = reinterpret_cast<std::size_t*>(std::uintptr_t{0x10});
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(mibPagesize, 2, &val, unmappedLen, nullptr, 0), guestEfault);

    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.pagesize", unmappedPtr, &len, nullptr, 0), guestEfault);

    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.pagesize", &val, unmappedLen, nullptr, 0), guestEfault);
}

void AuditHwPagesize() {
    const int mib[] = {ctlHw, hwPagesize};

    std::size_t queryLen = 0;
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &queryLen, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(queryLen == sizeof(int));

    queryLen = 0;
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctlbyname_nid_postfix("hw.pagesize", nullptr, &queryLen, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(queryLen == sizeof(int));

    int pagesize = 0;
    std::size_t len = sizeof(pagesize);
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctl_nid_postfix(mib, 2, &pagesize, &len, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(pagesize == expectedPagesize);
    Require(len == sizeof(int));

    int byName = 0;
    len = sizeof(byName);
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctlbyname_nid_postfix("hw.pagesize", &byName, &len, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(byName == expectedPagesize);
    Require(len == sizeof(int));

    char small[2] = {};
    len = sizeof(small);
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(mib, 2, small, &len, nullptr, 0), guestEnomem);
    Require(len == sizeof(small));

    len = sizeof(small);
    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.pagesize", small, &len, nullptr, 0), guestEnomem);
    Require(len == sizeof(small));

    int dummy = 4096;
    len = sizeof(int);
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(mib, 2, &pagesize, &len, &dummy, sizeof(dummy)), guestEperm);

    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.pagesize", &pagesize, &len, &dummy, sizeof(dummy)), guestEperm);
}

void AuditHwNcpu() {
    const int mib[] = {ctlHw, hwNcpu};

    std::size_t queryLen = 0;
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &queryLen, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(queryLen == sizeof(int));

    int ncpu = 0;
    std::size_t len = sizeof(ncpu);
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctl_nid_postfix(mib, 2, &ncpu, &len, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(ncpu >= 1);
    Require(len == sizeof(int));

    int byName = 0;
    len = sizeof(byName);
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctlbyname_nid_postfix("hw.ncpu", &byName, &len, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(byName == ncpu);
    Require(len == sizeof(int));

    char small[1] = {};
    len = sizeof(small);
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(mib, 2, small, &len, nullptr, 0), guestEnomem);
    Require(len == sizeof(small));

    len = sizeof(small);
    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.ncpu", small, &len, nullptr, 0), guestEnomem);
    Require(len == sizeof(small));

    int dummy = 8;
    len = sizeof(int);
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(mib, 2, &ncpu, &len, &dummy, sizeof(dummy)), guestEperm);

    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.ncpu", &ncpu, &len, &dummy, sizeof(dummy)), guestEperm);
}

void AuditHwPhysmem() {
    const int mib[] = {ctlHw, hwPhysmem};

    std::size_t queryLen = 0;
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctl_nid_postfix(mib, 2, nullptr, &queryLen, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(queryLen == sizeof(std::uint64_t));

    std::uint64_t physmem = 0;
    std::size_t len = sizeof(physmem);
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctl_nid_postfix(mib, 2, &physmem, &len, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(physmem > 0);
    Require((physmem % expectedPagesize) == 0);
    Require(len == sizeof(std::uint64_t));

    std::uint64_t byName = 0;
    len = sizeof(byName);
    *__error_nid_postfix() = 0x5a5a;
    Require(sysctlbyname_nid_postfix("hw.physmem", &byName, &len, nullptr, 0) == 0);
    Require(*__error_nid_postfix() == 0x5a5a);
    Require(byName == physmem);
    Require(len == sizeof(std::uint64_t));

    char small[4] = {};
    len = sizeof(small);
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(mib, 2, small, &len, nullptr, 0), guestEnomem);
    Require(len == sizeof(small));

    len = sizeof(small);
    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.physmem", small, &len, nullptr, 0), guestEnomem);
    Require(len == sizeof(small));

    std::uint64_t dummy = 1024 * 1024;
    len = sizeof(physmem);
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(mib, 2, &physmem, &len, &dummy, sizeof(dummy)), guestEperm);

    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.physmem", &physmem, &len, &dummy, sizeof(dummy)), guestEperm);
}

void AuditHwByteorderAndHwUsermem() {
    const int byteorderMib[] = {ctlHw, hwByteorder};
    int val = 0;
    std::size_t len = sizeof(val);

    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(byteorderMib, 2, &val, &len, nullptr, 0), guestEnoent);

    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.byteorder", &val, &len, nullptr, 0), guestEnoent);

    const int usermemMib[] = {ctlHw, hwUsermem};
    *__error_nid_postfix() = 0;
    RequireFailure(sysctl_nid_postfix(usermemMib, 2, &val, &len, nullptr, 0), guestEnoent);

    *__error_nid_postfix() = 0;
    RequireFailure(sysctlbyname_nid_postfix("hw.usermem", &val, &len, nullptr, 0), guestEnoent);
}

} // namespace

int main() {
    AuditParameterValidation();
    AuditHwPagesize();
    AuditHwNcpu();
    AuditHwPhysmem();
    AuditHwByteorderAndHwUsermem();
    return 0;
}
