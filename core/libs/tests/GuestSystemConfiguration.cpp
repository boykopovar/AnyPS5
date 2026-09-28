#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
extern "C" {
std::int64_t APS5_VABI sysconf_nid_postfix(int);
int APS5_VABI sysctlbyname_nid_postfix(const char*, void*, std::size_t*, const void*, std::size_t);
int APS5_VABI sysctl_nid_postfix(const int*, unsigned int, void*, std::size_t*, const void*, std::size_t);
int APS5_VABI getpagesize_nid_postfix();
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
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
    std::uint32_t cores = 0;
    std::size_t length = sizeof(cores);
    Require(sysctlbyname_nid_postfix("hw.ncpu", &cores, &length, nullptr, 0) == 0);
    Require(length == sizeof(cores) && cores > 0);
    length = 0;
    Require(sysctlbyname_nid_postfix("hw.pagesize", nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(std::uint32_t));
    std::uint32_t pageSize = 0;
    Require(sysctlbyname_nid_postfix("hw.pagesize", &pageSize, &length, nullptr, 0) == 0);
    Require(pageSize == 0x4000);
    std::uint64_t physical = 0;
    length = sizeof(physical);
    Require(sysctlbyname_nid_postfix("hw.physmem", &physical, &length, nullptr, 0) == 0);
    Require(length == sizeof(physical) && physical >= pageSize);
    length = 1;
    Require(sysctlbyname_nid_postfix("hw.ncpu", &cores, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12 && length == sizeof(cores));
    Require(sysctlbyname_nid_postfix("unknown.node", &cores, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 2);
    Require(sysctlbyname_nid_postfix(nullptr, &cores, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);
    Require(sysctlbyname_nid_postfix("hw.ncpu", &cores, nullptr, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);
    Require(sysctlbyname_nid_postfix("hw.ncpu", &cores, &length, &cores, sizeof(cores)) == -1);
    Require(*__error_nid_postfix() == 1);
    // Crispy Doom's SDL_GetSystemRAM fallback calls sysctl({CTL_HW, HW_REALMEM}, 2, ...).
    const int realMemoryMib[2] = {6, 12};
    std::uint64_t realMemory = 0;
    length = sizeof(realMemory);
    Require(sysctl_nid_postfix(realMemoryMib, 2, &realMemory, &length, nullptr, 0) == 0);
    Require(length == sizeof(realMemory) && realMemory >= pageSize);
    std::uint64_t namedRealMemory = 0;
    length = sizeof(namedRealMemory);
    Require(sysctlbyname_nid_postfix("hw.realmem", &namedRealMemory, &length, nullptr, 0) == 0);
    Require(realMemory == namedRealMemory);
    length = 0;
    Require(sysctl_nid_postfix(realMemoryMib, 2, nullptr, &length, nullptr, 0) == 0);
    Require(length == sizeof(realMemory));
    length = 4;
    Require(sysctl_nid_postfix(realMemoryMib, 2, &realMemory, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 12 && length == sizeof(realMemory));
    Require(sysctl_nid_postfix(realMemoryMib, 0, &realMemory, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 22);
    const int unknownMib[2] = {6, 99};
    Require(sysctl_nid_postfix(unknownMib, 2, &realMemory, &length, nullptr, 0) == -1);
    Require(*__error_nid_postfix() == 2);
}
