#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI seteuid_nid_postfix(std::uint32_t);
int APS5_VABI setegid_nid_postfix(std::uint32_t);
int APS5_VABI setuid_nid_postfix(std::uint32_t);
int APS5_VABI setreuid_nid_postfix(std::uint32_t, std::uint32_t);
int APS5_VABI setregid_nid_postfix(std::uint32_t, std::uint32_t);
int APS5_VABI setgroups_nid_postfix(int, const std::uint32_t*);
int APS5_VABI getgroups_nid_postfix(int, std::uint32_t*);
int APS5_VABI getuid_nid_postfix(void);
int APS5_VABI geteuid_nid_postfix(void);
int APS5_VABI getgid_nid_postfix(void);
int APS5_VABI getegid_nid_postfix(void);
int APS5_VABI issetugid_nid_postfix(void);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }
using SetId = int (APS5_VABI *)(std::uint32_t);
using SetReId = int (APS5_VABI *)(std::uint32_t, std::uint32_t);

static bool Throws(SetId function, std::uint32_t id) {
    try {
        function(id);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool ThrowsRe(SetReId function, std::uint32_t r, std::uint32_t e) {
    try {
        function(r, e);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr std::uint32_t Unchanged = 0xffffffffu;

static void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

int main() {
    *__error_nid_postfix() = 13;
    Require(seteuid_nid_postfix(0) == 0);
    Require(setegid_nid_postfix(0) == 0);
    Require(*__error_nid_postfix() == 13);
    Require(Throws(seteuid_nid_postfix, 1000));
    Require(Throws(setegid_nid_postfix, 1000));
    Require(Throws(seteuid_nid_postfix, Unchanged));
    Require(Throws(setegid_nid_postfix, Unchanged));

    Require(setuid_nid_postfix(0) == 0);
    Require(getuid_nid_postfix() == 0 && geteuid_nid_postfix() == 0);
    Require(getgid_nid_postfix() == 0 && getegid_nid_postfix() == 0 && issetugid_nid_postfix() == 0);
    Require(setreuid_nid_postfix(Unchanged, Unchanged) == 0);
    Require(setreuid_nid_postfix(0, Unchanged) == 0);
    Require(setreuid_nid_postfix(Unchanged, 0) == 0);
    Require(setreuid_nid_postfix(0, 0) == 0);
    Require(ThrowsRe(setreuid_nid_postfix, 1000, 0));
    Require(ThrowsRe(setreuid_nid_postfix, 0, 1000));

    Require(setregid_nid_postfix(Unchanged, Unchanged) == 0);
    Require(setregid_nid_postfix(0, Unchanged) == 0);
    Require(setregid_nid_postfix(Unchanged, 0) == 0);
    Require(setregid_nid_postfix(0, 0) == 0);
    Require(ThrowsRe(setregid_nid_postfix, 1000, 0));
    Require(ThrowsRe(setregid_nid_postfix, 0, 1000));

    std::array<std::uint32_t, 4> groups{};
    Require(getgroups_nid_postfix(0, nullptr) == 1);
    Require(getgroups_nid_postfix(static_cast<int>(groups.size()), groups.data()) == 1 && groups[0] == 0);
    Require(groups[0] == static_cast<std::uint32_t>(getegid_nid_postfix()));

    const std::array<std::uint32_t, 3> wanted{0, 5, 7};
    Require(setgroups_nid_postfix(static_cast<int>(wanted.size()), wanted.data()) == 0);
    Require(getgroups_nid_postfix(0, nullptr) == 3);
    RequireFailure(getgroups_nid_postfix(2, groups.data()), GuestEinval);
    RequireFailure(getgroups_nid_postfix(3, nullptr), GuestEfault);
    Require(getgroups_nid_postfix(static_cast<int>(groups.size()), groups.data()) == 3);
    Require(groups[0] == 0 && groups[1] == 5 && groups[2] == 7);

    Require(setgroups_nid_postfix(0, nullptr) == 0);
    Require(getgroups_nid_postfix(static_cast<int>(groups.size()), groups.data()) == 1 && groups[0] == 0);

    RequireFailure(setgroups_nid_postfix(-1, wanted.data()), GuestEinval);
    RequireFailure(setgroups_nid_postfix(1024, wanted.data()), GuestEinval);
    RequireFailure(setgroups_nid_postfix(1, nullptr), GuestEfault);
    Require(getgroups_nid_postfix(0, nullptr) == 1);
    return 0;
}
