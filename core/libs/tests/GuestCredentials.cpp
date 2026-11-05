#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

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

using SetResId = int (APS5_VABI *)(std::uint32_t, std::uint32_t, std::uint32_t);

static bool ThrowsRes(SetResId function, std::uint32_t r, std::uint32_t e, std::uint32_t s) {
    try {
        function(r, e, s);
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
    Require(setgid_nid_postfix(0) == 0);
    Require(Throws(setgid_nid_postfix, 1000));
    Require(Throws(setgid_nid_postfix, Unchanged));
    Require(getuid_nid_postfix() == 0 && geteuid_nid_postfix() == 0);
    Require(getgid_nid_postfix() == 0 && getegid_nid_postfix() == 0 && issetugid_nid_postfix() == 0);

    std::uint32_t ruid = 99, euid = 99, suid = 99;
    *__error_nid_postfix() = 13;
    Require(getresuid_nid_postfix(&ruid, &euid, &suid) == 0);
    Require(ruid == 0 && euid == 0 && suid == 0);
    Require(*__error_nid_postfix() == 13);
    ruid = 99;
    euid = 99;
    suid = 99;
    Require(getresuid_nid_postfix(&ruid, nullptr, nullptr) == 0 && ruid == 0);
    Require(getresuid_nid_postfix(nullptr, &euid, nullptr) == 0 && euid == 0);
    Require(getresuid_nid_postfix(nullptr, nullptr, &suid) == 0 && suid == 0);
    Require(getresuid_nid_postfix(nullptr, nullptr, nullptr) == 0);

    std::uint32_t rgid = 99, egid = 99, sgid = 99;
    *__error_nid_postfix() = 13;
    Require(getresgid_nid_postfix(&rgid, &egid, &sgid) == 0);
    Require(rgid == 0 && egid == 0 && sgid == 0);
    Require(*__error_nid_postfix() == 13);
    rgid = 99;
    egid = 99;
    sgid = 99;
    Require(getresgid_nid_postfix(&rgid, nullptr, nullptr) == 0 && rgid == 0);
    Require(getresgid_nid_postfix(nullptr, &egid, nullptr) == 0 && egid == 0);
    Require(getresgid_nid_postfix(nullptr, nullptr, &sgid) == 0 && sgid == 0);
    Require(getresgid_nid_postfix(nullptr, nullptr, nullptr) == 0);
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

    Require(setresuid_nid_postfix(Unchanged, Unchanged, Unchanged) == 0);
    Require(setresuid_nid_postfix(0, Unchanged, Unchanged) == 0);
    Require(setresuid_nid_postfix(Unchanged, 0, Unchanged) == 0);
    Require(setresuid_nid_postfix(Unchanged, Unchanged, 0) == 0);
    Require(setresuid_nid_postfix(0, 0, 0) == 0);
    Require(ThrowsRes(setresuid_nid_postfix, 1000, 0, 0));
    Require(ThrowsRes(setresuid_nid_postfix, 0, 1000, 0));
    Require(ThrowsRes(setresuid_nid_postfix, 0, 0, 1000));
    Require(ThrowsRes(setresuid_nid_postfix, 1000, Unchanged, Unchanged));
    Require(ThrowsRes(setresuid_nid_postfix, Unchanged, 1000, Unchanged));
    Require(ThrowsRes(setresuid_nid_postfix, Unchanged, Unchanged, 1000));

    Require(setresgid_nid_postfix(Unchanged, Unchanged, Unchanged) == 0);
    Require(setresgid_nid_postfix(0, Unchanged, Unchanged) == 0);
    Require(setresgid_nid_postfix(Unchanged, 0, Unchanged) == 0);
    Require(setresgid_nid_postfix(Unchanged, Unchanged, 0) == 0);
    Require(setresgid_nid_postfix(0, 0, 0) == 0);
    Require(ThrowsRes(setresgid_nid_postfix, 1000, 0, 0));
    Require(ThrowsRes(setresgid_nid_postfix, 0, 1000, 0));
    Require(ThrowsRes(setresgid_nid_postfix, 0, 0, 1000));
    Require(ThrowsRes(setresgid_nid_postfix, 1000, Unchanged, Unchanged));
    Require(ThrowsRes(setresgid_nid_postfix, Unchanged, 1000, Unchanged));
    Require(ThrowsRes(setresgid_nid_postfix, Unchanged, Unchanged, 1000));

    std::array<std::uint32_t, 4> groups{};
    Require(getgroups_nid_postfix(0, nullptr) == 1);
    Require(getgroups_nid_postfix(static_cast<int>(groups.size()), groups.data()) == 1 && groups[0] == 0);
    Require(groups[0] == static_cast<std::uint32_t>(getegid_nid_postfix()));
    *__error_nid_postfix() = 13;
    RequireFailure(setgroups_nid_postfix(1, reinterpret_cast<const std::uint32_t*>(1)), GuestEfault);
    *__error_nid_postfix() = 13;
    RequireFailure(getgroups_nid_postfix(1, reinterpret_cast<std::uint32_t*>(1)), GuestEfault);
    Require(getgroups_nid_postfix(0, nullptr) == 1);

    *__error_nid_postfix() = 13;
    RequireFailure(getresuid_nid_postfix(reinterpret_cast<std::uint32_t*>(1), nullptr, nullptr), GuestEfault);
    *__error_nid_postfix() = 13;
    RequireFailure(getresgid_nid_postfix(nullptr, reinterpret_cast<std::uint32_t*>(1), nullptr), GuestEfault);

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
    const std::vector<std::uint32_t> maximal(1024, 0);
    const std::vector<std::uint32_t> oversized(1025, 0);
    *__error_nid_postfix() = 13;
    RequireFailure(setgroups_nid_postfix(static_cast<int>(oversized.size()), oversized.data()), GuestEinval);
    Require(setgroups_nid_postfix(static_cast<int>(maximal.size()), maximal.data()) == 0);
    Require(getgroups_nid_postfix(0, nullptr) == 1024);
    Require(setgroups_nid_postfix(0, nullptr) == 0);
    groups.fill(9);
    Require(getgroups_nid_postfix(-1, groups.data()) == 1 && groups[0] == 0 && groups[1] == 9);
    *__error_nid_postfix() = 13;
    RequireFailure(getgroups_nid_postfix(-1, nullptr), GuestEfault);
    *__error_nid_postfix() = 13;
    RequireFailure(getgroups_nid_postfix(-1, reinterpret_cast<std::uint32_t*>(0x10)), GuestEfault);
    *__error_nid_postfix() = 13;
    RequireFailure(setgroups_nid_postfix(1, reinterpret_cast<const std::uint32_t*>(0x10)), GuestEfault);
    RequireFailure(setgroups_nid_postfix(1, nullptr), GuestEfault);
    Require(getgroups_nid_postfix(0, nullptr) == 1);
    return 0;
}
