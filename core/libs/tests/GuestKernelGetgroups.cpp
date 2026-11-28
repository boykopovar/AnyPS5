#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

} // namespace

int main() {
    const int count = getgroups_nid_postfix(0, nullptr);
    Require(count >= 1);

    std::uint32_t groups[64]{};
    const int fetched = getgroups_nid_postfix(static_cast<int>(sizeof(groups) / sizeof(groups[0])), groups);
    Require(fetched == count);

    *__error_nid_postfix() = 0;
    Require(getgroups_nid_postfix(count + 1, nullptr) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(getgroups_nid_postfix(count + 1, reinterpret_cast<std::uint32_t*>(0x10)) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(getgroups_nid_postfix(-1, groups) == count);

    *__error_nid_postfix() = 0;
    Require(getgroups_nid_postfix(-1, nullptr) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(getgroups_nid_postfix(-1, reinterpret_cast<std::uint32_t*>(0x10)) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    *__error_nid_postfix() = 0;
    Require(getgroups_nid_postfix(INT_MIN, groups) == count);

    *__error_nid_postfix() = 0;
    Require(getgroups_nid_postfix(INT_MIN, nullptr) == -1);
    Require(*__error_nid_postfix() == GuestEfault);

    return 0;
}
