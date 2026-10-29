#include <cstdint>
#include <mutex>
#include <unordered_set>

#include "prx/libc/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "SceTypes.hpp"

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* funcName);
extern "C" bool GuestCopyOut_nid_no_patch(void* destination, const void* source, std::size_t bytes);

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

constexpr int GuestEsrch = 3;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;

constexpr int RootSetId = 0;
constexpr int DefaultSetId = 1;

std::mutex setMutex;
std::unordered_set<int> validSets{RootSetId, DefaultSetId};
int nextSetId = 2;
int currentProcessSetId = DefaultSetId;

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

} // namespace

extern "C" {

int APS5_VABI cpuset_nid_postfix(int* setid) {
    int id = 0;
    {
        std::lock_guard lock(setMutex);
        id = nextSetId++;
    }
    if (!GuestCopyOut_nid_no_patch(setid, &id, sizeof(id))) {
        return Fail(GuestEfault);
    }
    {
        std::lock_guard lock(setMutex);
        validSets.insert(id);
        if (currentProcessSetId >= 2 && currentProcessSetId != id) {
            validSets.erase(currentProcessSetId);
        }
        currentProcessSetId = id;
    }
    return 0;
}

int APS5_VABI cpuset_setid_nid_postfix(int which, std::int64_t id, int setid) {
    if (which != WhichProcess) {
        return Fail(GuestEinval);
    }
    if (id != -1 && id != getpid_nid_postfix()) {
        return Fail(GuestEsrch);
    }
    std::lock_guard lock(setMutex);
    if (setid == -1 || validSets.find(setid) == validSets.end()) {
        return Fail(GuestEsrch);
    }
    if (currentProcessSetId >= 2 && currentProcessSetId != setid) {
        validSets.erase(currentProcessSetId);
    }
    currentProcessSetId = setid;
    return 0;
}

int APS5_VABI cpuset_getid_nid_postfix(int level, int which, std::int64_t id, int* setid) {
    if (level == LevelWhich && which != WhichCpuset) {
        return Fail(GuestEinval);
    }
    if (level < LevelRoot || level > LevelWhich) {
        return Fail(GuestEinval);
    }
    if (which < WhichThread || which > WhichDomain || which == WhichIrq || which == WhichDomain) {
        return Fail(GuestEinval);
    }
    if (which != WhichProcess) {
        NotImplemented_nid_no_patch("cpuset_getid other than process");
    }
    if (id != -1 && id != getpid_nid_postfix()) {
        return Fail(GuestEsrch);
    }
    int resultId = 0;
    if (level == LevelRoot) {
        resultId = RootSetId;
    } else {
        std::lock_guard lock(setMutex);
        resultId = currentProcessSetId;
    }
    if (!GuestCopyOut_nid_no_patch(setid, &resultId, sizeof(resultId))) {
        return Fail(GuestEfault);
    }
    return 0;
}

}
