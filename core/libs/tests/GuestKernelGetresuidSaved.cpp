#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdlib>

static void Require(bool value) {
    if (!value) std::abort();
}

constexpr int GuestEfault = 14;

static void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

int main() {
    std::uint32_t val = 99;

    Require(getresuid_nid_postfix(nullptr, nullptr, nullptr) == 0);

    RequireFailure(getresuid_nid_postfix(reinterpret_cast<std::uint32_t*>(1), nullptr, nullptr), GuestEfault);
    RequireFailure(getresuid_nid_postfix(nullptr, reinterpret_cast<std::uint32_t*>(1), nullptr), GuestEfault);
    RequireFailure(getresuid_nid_postfix(nullptr, nullptr, reinterpret_cast<std::uint32_t*>(1)), GuestEfault);

    Require(getresgid_nid_postfix(nullptr, nullptr, nullptr) == 0);

    RequireFailure(getresgid_nid_postfix(reinterpret_cast<std::uint32_t*>(1), nullptr, nullptr), GuestEfault);
    RequireFailure(getresgid_nid_postfix(nullptr, reinterpret_cast<std::uint32_t*>(1), nullptr), GuestEfault);
    RequireFailure(getresgid_nid_postfix(nullptr, nullptr, reinterpret_cast<std::uint32_t*>(1)), GuestEfault);

    Require(getresuid_nid_postfix(&val, nullptr, nullptr) == 0);
    Require(val == 0);

    Require(getresgid_nid_postfix(&val, nullptr, nullptr) == 0);
    Require(val == 0);

    return 0;
}
