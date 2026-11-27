#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <source_location>

namespace {

constexpr int GuestEsrch = 3;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int GuestEagain = 35;
constexpr int GuestEnametoolong = 63;
constexpr std::uint32_t MutexType = 0x201;
constexpr std::uint32_t RwlockType = 0x203;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "namedobj check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

int Error() {
    return *__error_nid_postfix();
}

}

int main() {
    int object = 0;
    std::array<char, 33> name{};
    name.fill('n');
    name[32] = '\0';

    Require(__sys_namedobj_create_nid_postfix(nullptr, &object, MutexType) == -1 && Error() == GuestEinval);
    Require(__sys_namedobj_create_nid_postfix(reinterpret_cast<const char*>(0x10), &object, MutexType) == -1 && Error() == GuestEfault);
    Require(__sys_namedobj_create_nid_postfix(name.data(), &object, MutexType) == -1 && Error() == GuestEnametoolong);
    Require(__sys_namedobj_delete_nid_postfix(1, MutexType) == -1 && Error() == GuestEsrch);

    name[31] = '\0';
    const int first = __sys_namedobj_create_nid_postfix(name.data(), &object, MutexType);
    const int second = __sys_namedobj_create_nid_postfix("", nullptr, 0x10000 | RwlockType);
    Require(first == 1 && second == 2);

    Require(__sys_namedobj_delete_nid_postfix(0, MutexType) == -1 && Error() == GuestEsrch);
    Require(__sys_namedobj_delete_nid_postfix(3, MutexType) == -1 && Error() == GuestEsrch);
    Require(__sys_namedobj_delete_nid_postfix(0x10000 | static_cast<std::uint32_t>(first), MutexType) == -1 && Error() == GuestEsrch);
    Require(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(second), RwlockType) == 0);
    Require(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(second), RwlockType) == -1 && Error() == GuestEsrch);
    const int typed = __sys_namedobj_create_nid_postfix("typed", nullptr, MutexType);
    Require(typed == 2);
    Require(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(typed), RwlockType) == -1 && Error() == GuestEsrch);
    Require(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(typed), 0x10000 | RwlockType) == -1 && Error() == GuestEsrch);
    Require(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(typed), MutexType | 0x1000) == 0);
    Require(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(typed), MutexType) == -1 && Error() == GuestEsrch);

    std::set<int> live{first};
    while (live.size() < 0xFFFF) {
        const int id = __sys_namedobj_create_nid_postfix("object", &object, MutexType);
        Require(id >= 1 && id <= 0xFFFF);
        Require(live.insert(id).second);
    }
    Require(__sys_namedobj_create_nid_postfix("object", &object, MutexType) == -1 && Error() == GuestEagain);
    Require(__sys_namedobj_delete_nid_postfix(0x1234, MutexType) == 0);
    Require(__sys_namedobj_create_nid_postfix("object", &object, RwlockType) == 0x1234);
    Require(__sys_namedobj_create_nid_postfix("object", &object, MutexType) == -1 && Error() == GuestEagain);
    Require(__sys_namedobj_delete_nid_postfix(0x1234, RwlockType) == 0);
    live.erase(0x1234);
    for (const int id : live) Require(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(id), 0x20000 | MutexType) == 0);
    for (const int id : live) Require(__sys_namedobj_delete_nid_postfix(static_cast<std::uint32_t>(id), MutexType) == -1 && Error() == GuestEsrch);
}
