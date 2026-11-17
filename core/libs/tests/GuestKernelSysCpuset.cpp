#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

namespace {

constexpr int LevelRoot = 1;
constexpr int LevelCpuset = 2;
constexpr int LevelWhich = 3;

constexpr int WhichThread = 1;
constexpr int WhichProcess = 2;
constexpr int WhichCpuset = 3;
constexpr int WhichIrq = 4;
constexpr int WhichJail = 5;
constexpr int WhichDomain = 6;

constexpr int Eperm = 1;
constexpr int Esrch = 3;
constexpr int Edeadlk = 11;
constexpr int Efault = 14;
constexpr int Einval = 22;

constexpr std::uint64_t RootMask = 0xffff;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u (error %d)\n", static_cast<unsigned>(location.line()), *__error_nid_postfix());
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

std::uint64_t Affinity(int level, int which, std::int64_t id) {
    std::uint64_t mask[2] = {~0ull, ~0ull};
    Require(cpuset_getaffinity_nid_postfix(level, which, id, sizeof(mask), mask) == 0 && mask[1] == 0);
    return mask[0];
}

bool SetAffinity(int level, int which, std::int64_t id, std::uint64_t value) {
    const std::uint64_t mask[2] = {value, 0};
    return cpuset_setaffinity_nid_postfix(level, which, id, sizeof(mask), mask) == 0;
}

template <typename Call> bool Throws(Call call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void NumberedSetAffinity() {
    const std::uint64_t available = sceKernelGetAvailableCpumask();
    const std::uint64_t lowest = available & (~available + 1);
    Require(Affinity(LevelCpuset, WhichProcess, -1) == available);
    for (const int level : {LevelRoot, LevelCpuset, LevelWhich}) {
        const std::uint64_t own = level == LevelRoot ? RootMask : available;
        Require(Affinity(level, WhichCpuset, 0) == RootMask);
        Require(Affinity(level, WhichCpuset, 1) == own);
        Require(Affinity(level, WhichCpuset, 4294967296LL + 1) == own);
        Require(Affinity(level, WhichCpuset, -4294967296LL) == RootMask);
        for (const std::int64_t missing : {std::int64_t{999999}, std::int64_t{4294967295LL}, std::int64_t{-2}}) {
            std::uint64_t untouched[1] = {0x5a};
            Require(cpuset_getaffinity_nid_postfix(level, WhichCpuset, missing, 8, untouched) == -1 && Error() == Esrch && untouched[0] == 0x5a);
            Require(cpuset_getaffinity_nid_postfix(level, WhichCpuset, missing, 8, nullptr) == -1 && Error() == Esrch);
            Require(!SetAffinity(level, WhichCpuset, missing, available) && Error() == Esrch);
        }
        Require(!SetAffinity(level, WhichCpuset, 0, available) && Error() == Eperm);
        Require(!SetAffinity(level, WhichCpuset, 0, 0) && Error() == Eperm);
    }
    Require(!SetAffinity(LevelRoot, WhichCpuset, 1, available) && Error() == Eperm);
    Require(!SetAffinity(LevelWhich, WhichCpuset, -1, 0) && Error() == Edeadlk);
    Require(Affinity(LevelCpuset, WhichProcess, -1) == available);
    Require(SetAffinity(LevelWhich, WhichCpuset, -1, lowest));
    Require(Affinity(LevelCpuset, WhichProcess, -1) == lowest);
    Require(Affinity(LevelWhich, WhichCpuset, 1) == lowest);
    Require(Affinity(LevelWhich, WhichThread, -1) == lowest);
    Require(SetAffinity(LevelWhich, WhichCpuset, 1, available));
    Require(Affinity(LevelWhich, WhichCpuset, -1) == available);
    Require(Affinity(LevelWhich, WhichThread, -1) == available);
    Require(SetAffinity(LevelCpuset, WhichCpuset, 4294967296LL + 1, lowest));
    Require(Affinity(LevelCpuset, WhichThread, -1) == lowest);
    Require(SetAffinity(LevelCpuset, WhichCpuset, -1, available));
    Require(Affinity(LevelCpuset, WhichProcess, -1) == available);
}

void VacatedSetAffinity(int current) {
    const std::uint64_t available = sceKernelGetAvailableCpumask();
    const std::uint64_t lowest = available & (~available + 1);
    Require(Affinity(LevelWhich, WhichCpuset, current) == available);
    Require(Affinity(LevelRoot, WhichCpuset, current) == RootMask);
    Require(SetAffinity(LevelWhich, WhichCpuset, current, lowest));
    Require(Affinity(LevelCpuset, WhichProcess, -1) == lowest);
    Require(SetAffinity(LevelWhich, WhichCpuset, current, available));
    Require(Affinity(LevelRoot, WhichCpuset, 1) == RootMask);
    Require(!SetAffinity(LevelRoot, WhichCpuset, 1, available) && Error() == Eperm);
    Require(Throws([] { Affinity(LevelCpuset, WhichCpuset, 1); }));
    Require(Throws([] { Affinity(LevelWhich, WhichCpuset, 1); }));
    Require(Throws([&] { SetAffinity(LevelCpuset, WhichCpuset, 1, available); }));
    Require(Throws([&] { SetAffinity(LevelWhich, WhichCpuset, 1, available); }));
}

} // namespace

int main() {
    const int foreignPid = getpid_nid_postfix() == 1 ? 2 : 1;
    int setid = -1;
    Require(cpuset_nid_postfix(nullptr) == -1 && Error() == Efault);
    Require(cpuset_nid_postfix(reinterpret_cast<int*>(0x10)) == -1 && Error() == Efault);

    int initialSet = -1;
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, &initialSet) == 0);
    Require(initialSet == 1);
    NumberedSetAffinity();

    int set1 = -1;
    Require(cpuset_nid_postfix(&set1) == 0);
    Require(set1 >= 2);
    VacatedSetAffinity(set1);

    int currentSet = -1;
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, &currentSet) == 0);
    Require(currentSet == set1);

    int set2 = -1;
    Require(cpuset_nid_postfix(&set2) == 0);
    Require(set2 > set1);

    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, &currentSet) == 0);
    Require(currentSet == set2);

    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, nullptr) == -1 && Error() == Efault);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, reinterpret_cast<int*>(0x10)) == -1 && Error() == Efault);
    Require(cpuset_getid_nid_postfix(LevelWhich, WhichProcess, -1, nullptr) == -1 && Error() == Einval);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, foreignPid, nullptr) == -1 && Error() == Esrch);
    Require(cpuset_getid_nid_postfix(LevelCpuset, 7, -1, nullptr) == -1 && Error() == Einval);

    Require(cpuset_getid_nid_postfix(0, WhichProcess, -1, &setid) == -1 && Error() == Einval);
    Require(cpuset_getid_nid_postfix(4, WhichProcess, -1, &setid) == -1 && Error() == Einval);
    Require(cpuset_getid_nid_postfix(LevelCpuset, 0, -1, &setid) == -1 && Error() == Einval);
    Require(cpuset_getid_nid_postfix(LevelCpuset, 7, -1, &setid) == -1 && Error() == Einval);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichIrq, -1, &setid) == -1 && Error() == Einval);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichDomain, -1, &setid) == -1 && Error() == Einval);

    Require(cpuset_getid_nid_postfix(LevelWhich, WhichProcess, -1, &setid) == -1 && Error() == Einval);
    Require(cpuset_getid_nid_postfix(LevelWhich, WhichThread, -1, &setid) == -1 && Error() == Einval);

    int pid = getpid_nid_postfix();
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, pid, &setid) == 0);
    Require(setid == set2);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, foreignPid, &setid) == -1 && Error() == Esrch);

    const int tid = scePthreadGetthreadid();
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichThread, -1, &setid) == 0);
    Require(setid == set2);
    Require(cpuset_getid_nid_postfix(LevelRoot, WhichThread, -1, &setid) == 0);
    Require(setid == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichThread, tid, &setid) == 0);
    Require(setid == set2);
    Require(cpuset_getid_nid_postfix(LevelRoot, WhichThread, tid, &setid) == 0);
    Require(setid == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichThread, 999999, &setid) == -1 && Error() == Esrch);
    Require(cpuset_getid_nid_postfix(LevelRoot, WhichThread, 999999, &setid) == -1 && Error() == Esrch);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichThread, -1, nullptr) == -1 && Error() == Efault);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichThread, -1, reinterpret_cast<int*>(0x10)) == -1 && Error() == Efault);
    Require(cpuset_getid_nid_postfix(LevelWhich, WhichThread, tid, &setid) == -1 && Error() == Einval);

    int outOfRangeSet = 0x1234;
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, 4294967296LL, &outOfRangeSet) == -1 && Error() == Esrch && outOfRangeSet == 0x1234);
    Require(cpuset_getid_nid_postfix(LevelRoot, WhichProcess, 4294967296LL, &outOfRangeSet) == -1 && Error() == Esrch && outOfRangeSet == 0x1234);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -4294967296LL, &outOfRangeSet) == -1 && Error() == Esrch && outOfRangeSet == 0x1234);

    Require(cpuset_getid_nid_postfix(LevelRoot, WhichProcess, -1, &setid) == 0);
    Require(setid == 0);
    Require(cpuset_getid_nid_postfix(LevelRoot, WhichProcess, pid, &setid) == 0);
    Require(setid == 0);

    Require(cpuset_setid_nid_postfix(WhichThread, -1, 1) == -1 && Error() == Einval);
    Require(cpuset_setid_nid_postfix(WhichCpuset, -1, 1) == -1 && Error() == Einval);
    Require(cpuset_setid_nid_postfix(WhichProcess, foreignPid, 1) == -1 && Error() == Esrch);
    Require(cpuset_setid_nid_postfix(WhichProcess, -1, -1) == -1 && Error() == Esrch);
    Require(cpuset_setid_nid_postfix(WhichProcess, -1, 999999) == -1 && Error() == Esrch);

    Require(cpuset_setid_nid_postfix(WhichProcess, -1, 1) == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, &setid) == 0);
    Require(setid == 1);

    Require(cpuset_setid_nid_postfix(WhichProcess, pid, set1) == -1 && Error() == Esrch);

    Require(cpuset_setid_nid_postfix(WhichProcess, -1, 0) == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, &setid) == 0);
    Require(setid == 0);

    Require(cpuset_getid_nid_postfix(LevelRoot, WhichCpuset, -1, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, -1, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelWhich, WhichCpuset, -1, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, 999999, &setid) == -1 && Error() == Esrch);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, 4294967296LL, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, -4294967296LL, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, 4294967296LL + 999999, &setid) == -1 && Error() == Esrch);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, 0, nullptr) == -1 && Error() == Efault);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, -1, nullptr) == -1 && Error() == Efault);
    Require(cpuset_getid_nid_postfix(LevelRoot, WhichCpuset, 0, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, 0, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelWhich, WhichCpuset, 0, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelRoot, WhichCpuset, 1, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, 1, &setid) == 0 && setid == 1);
    Require(cpuset_getid_nid_postfix(LevelWhich, WhichCpuset, 1, &setid) == 0 && setid == 1);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, set2, &setid) == -1 && Error() == Esrch);

    int set3 = -1;
    Require(cpuset_nid_postfix(&set3) == 0 && set3 >= 2);
    Require(cpuset_getid_nid_postfix(LevelRoot, WhichCpuset, set3, &setid) == 0 && setid == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, set3, &setid) == 0 && setid == set3);
    Require(cpuset_getid_nid_postfix(LevelWhich, WhichCpuset, set3, &setid) == 0 && setid == set3);
    Require(cpuset_setid_nid_postfix(WhichProcess, -1, 0) == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichCpuset, set3, &setid) == -1 && Error() == Esrch);

    return 0;
}
