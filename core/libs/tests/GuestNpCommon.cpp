#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

static constexpr int INVALID_ARGUMENT = static_cast<int>(0x80550003);
static constexpr int INVALID_PLATFORM_TYPE = static_cast<int>(0x80550004);
static constexpr int NOT_MATCH = static_cast<int>(0x80550609);
static constexpr int COND_TIMEDOUT = static_cast<int>(0x8055800B);
static constexpr int MUTEX_BUSY = static_cast<int>(0x8055800F);

static bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

static NpOnlineId onlineId(const char* text) {
    NpOnlineId id{};
    std::strncpy(id.data, text, sizeof(id.data));
    return id;
}

static NpIdFull npIdWithTag(const char* handle, std::uint32_t tag) {
    NpIdFull id{};
    id.handle = onlineId(handle);
    std::memcpy(id.opt + 4, &tag, sizeof(tag));
    return id;
}

static int testIds() {
    const NpOnlineId kestrel = onlineId("kestrel_77");
    const NpOnlineId kestrelCopy = onlineId("kestrel_77");
    const NpOnlineId heron = onlineId("heron-4");
    bool ok = expect(sceNpCmpOnlineId(nullptr, &kestrel) == INVALID_ARGUMENT, "a null online ID was compared");
    ok &= expect(sceNpCmpOnlineId(&kestrel, &kestrelCopy) == 0, "equal online IDs did not match");
    ok &= expect(sceNpCmpOnlineId(&kestrel, &heron) == NOT_MATCH, "different online IDs matched");

    NpIdFull low = npIdWithTag("kestrel_77", 0x00347370);
    NpIdFull high = low;
    high.reserved[7] = 1;
    std::int32_t order = 5;
    ok &= expect(sceNpCmpNpIdInOrder(&low, &high, nullptr) == INVALID_ARGUMENT, "a null order was accepted");
    ok &= expect(sceNpCmpNpIdInOrder(&low, &high, &order) == 0 && order == -1, "reserved bytes did not order the NpIds");
    ok &= expect(sceNpCmpNpIdInOrder(&high, &low, &order) == 0 && order == 1, "the reverse order is not 1");
    ok &= expect(sceNpCmpNpIdInOrder(&low, &low, &order) == 0 && order == 0, "an NpId is not equal to itself");
    high = low;
    high.opt[0] = 9;
    high.reserved[0] = 0;
    low.reserved[0] = 200;
    ok &= expect(sceNpCmpNpIdInOrder(&low, &high, &order) == 0 && order == -1, "opt did not take precedence over reserved");
    const NpIdFull heronId = npIdWithTag("heron-4", 0);
    ok &= expect(sceNpCmpNpIdInOrder(&heronId, &low, &order) == 0 && order == -1, "the handle did not take precedence");

    const NpIdFull none = npIdWithTag("heron-4", 0);
    const NpIdFull ps3 = npIdWithTag("heron-4", 0x00337370);
    const NpIdFull vita = npIdWithTag("heron-4", 0x32707370);
    const NpIdFull ps4 = npIdWithTag("heron-4", 0x00347370);
    const NpIdFull other = npIdWithTag("heron-4", 0x00357370);
    ok &= expect(sceNpGetPlatformType(nullptr) == INVALID_ARGUMENT, "a null NpId has a platform");
    ok &= expect(sceNpGetPlatformType(&none) == 0, "an untagged NpId is not platform 0");
    ok &= expect(sceNpGetPlatformType(&ps3) == 1, "the ps3 tag is not platform 1");
    ok &= expect(sceNpGetPlatformType(&vita) == 2, "the psp2 tag is not platform 2");
    ok &= expect(sceNpGetPlatformType(&ps4) == 3, "the ps4 tag is not platform 3");
    ok &= expect(sceNpGetPlatformType(&other) == INVALID_PLATFORM_TYPE, "an unknown tag was accepted");

    NpOnlineId unterminated = onlineId("abcdefghijklmnop");
    unterminated.data[16] = 'q';
    ok &= expect(sceNpIntIsValidOnlineId(nullptr) == 0, "a null online ID is valid");
    ok &= expect(sceNpIntIsValidOnlineId(&kestrel) == 1, "kestrel_77 is not valid");
    ok &= expect(sceNpIntIsValidOnlineId(&heron) == 1, "heron-4 is not valid");
    const NpOnlineId sixteen = onlineId("abcdefghijklmnop");
    const NpOnlineId shortId = onlineId("ab");
    const NpOnlineId leadingDash = onlineId("-heron");
    const NpOnlineId dotted = onlineId("her.on");
    ok &= expect(sceNpIntIsValidOnlineId(&sixteen) == 1, "a 16 character ID is not valid");
    ok &= expect(sceNpIntIsValidOnlineId(&unterminated) == 0, "an unterminated ID is valid");
    ok &= expect(sceNpIntIsValidOnlineId(&shortId) == 0, "a 2 character ID is valid");
    ok &= expect(sceNpIntIsValidOnlineId(&leadingDash) == 0, "an ID starting with '-' is valid");
    ok &= expect(sceNpIntIsValidOnlineId(&dotted) == 0, "an ID with '.' is valid");
    return ok ? 0 : 1;
}

static void* APS5_VABI returnTicket(void* arg) {
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(arg) + 1);
}

static int testSync() {
    PthreadMutex plain = nullptr;
    PthreadMutex recursive = nullptr;
    bool ok = expect(sceNpMutexInit(&plain, "np-plain", 0) == 0 && plain, "a plain mutex was not created");
    ok &= expect(sceNpLwMutexInit(&recursive, "np-recursive", 1) == 0 && recursive, "a recursive mutex was not created");
    if (!ok) return 1;
    ok &= expect(sceNpMutexLock(&plain) == 0, "the plain mutex did not lock");
    ok &= expect(sceNpMutexTryLock(&plain) == MUTEX_BUSY, "a held plain mutex did not report BUSY");
    ok &= expect(sceNpMutexUnlock(&plain) == 0, "the plain mutex did not unlock");
    ok &= expect(sceNpLwMutexTryLock(&recursive) == 0 && sceNpLwMutexTryLock(&recursive) == 0, "flag 1 did not make the mutex recursive");
    ok &= expect(sceNpLwMutexUnlock(&recursive) == 0 && sceNpLwMutexUnlock(&recursive) == 0, "the recursive mutex did not unlock twice");
    ok &= expect(sceNpLwMutexLock(&recursive) == 0, "the recursive mutex did not lock via sceNpLwMutexLock");
    ok &= expect(sceNpLwMutexUnlock(&recursive) == 0, "the recursive mutex did not unlock");
    ok &= expect(sceNpLwMutexDestroy(&recursive) == 0, "the recursive mutex was not destroyed");

    PthreadCond cond = nullptr;
    ok &= expect(sceNpCondInit(&cond, "np-cond", 0) == 0 && cond, "a condition was not created");
    ok &= expect(sceNpMutexLock(&plain) == 0, "the plain mutex did not lock for the wait");
    ok &= expect(sceNpCondTimedwait(&cond, &plain, 2000) == COND_TIMEDOUT, "a timed wait did not report TIMEDOUT");
    ok &= expect(sceNpMutexUnlock(&plain) == 0, "the mutex was not held after the wait");
    ok &= expect(sceNpCondSignal(&cond) == 0, "signalling without waiters failed");
    ok &= expect(sceNpCondDestroy(&cond) == 0, "the condition was not destroyed");
    ok &= expect(sceNpMutexDestroy(&plain) == 0, "the plain mutex was not destroyed");

    Pthread thread = nullptr;
    void* retval = nullptr;
    ok &= expect(scePthreadCreate(&thread, nullptr, returnTicket, reinterpret_cast<void*>(0x4140), "np-join") == 0, "the thread was not created");
    ok &= expect(sceNpJoinThread(thread, &retval) == 0 && retval == reinterpret_cast<void*>(0x4141), "the join did not return the thread result");
    return ok ? 0 : 1;
}

static int testClock() {
    KernelTimespec before{};
    KernelTimespec after{};
    std::int64_t usec = -1;
    sceKernelClockGettime(4, &before);

    bool ok = expect(sceNpGetSystemClockUsec(&usec) == 0, "the clock was not read");
    sceKernelClockGettime(4, &after);

    ok &= expect(usec >= before.tv_sec * 1000000 + before.tv_nsec / 1000 && usec <= after.tv_sec * 1000000 + after.tv_nsec / 1000, "the clock is not the monotonic kernel clock in microseconds");
    char version[12];
    std::memset(version, 'x', sizeof(version));
    sceNpGetSdkVersion(version);

    ok &= expect(std::strcmp(version, "1.00") == 0, "the version is not 1.00");
    ok &= expect(version[8] == 'x', "the version was written past 8 bytes");
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    int status = 2;
    if (std::strcmp(argv[1], "ids") == 0) status = testIds();
    else if (std::strcmp(argv[1], "sync") == 0) status = testSync();
    else if (std::strcmp(argv[1], "clock") == 0) status = testClock();
    if (status == 0) std::printf("NpCommon %s tests passed\n", argv[1]);
    return status;
}
