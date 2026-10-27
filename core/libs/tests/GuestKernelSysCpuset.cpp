#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

#include "prx/libc/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
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

constexpr int Esrch = 3;
constexpr int Efault = 14;
constexpr int Einval = 22;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u (error %d)\n", static_cast<unsigned>(location.line()), *__error_nid_postfix());
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

} // namespace

int main() {
    int setid = -1;
    Require(cpuset_nid_postfix(nullptr) == -1 && Error() == Efault);
    Require(cpuset_nid_postfix(reinterpret_cast<int*>(0x10)) == -1 && Error() == Efault);

    int initialSet = -1;
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, &initialSet) == 0);
    Require(initialSet == 1);

    int set1 = -1;
    Require(cpuset_nid_postfix(&set1) == 0);
    Require(set1 >= 2);

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
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, 999999, nullptr) == -1 && Error() == Esrch);
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
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, 999999, &setid) == -1 && Error() == Esrch);

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
    Require(cpuset_setid_nid_postfix(WhichProcess, 999999, 1) == -1 && Error() == Esrch);
    Require(cpuset_setid_nid_postfix(WhichProcess, -1, -1) == -1 && Error() == Esrch);
    Require(cpuset_setid_nid_postfix(WhichProcess, -1, 999999) == -1 && Error() == Esrch);

    Require(cpuset_setid_nid_postfix(WhichProcess, -1, 1) == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, &setid) == 0);
    Require(setid == 1);

    Require(cpuset_setid_nid_postfix(WhichProcess, pid, set1) == -1 && Error() == Esrch);

    Require(cpuset_setid_nid_postfix(WhichProcess, -1, 0) == 0);
    Require(cpuset_getid_nid_postfix(LevelCpuset, WhichProcess, -1, &setid) == 0);
    Require(setid == 0);

    return 0;
}
