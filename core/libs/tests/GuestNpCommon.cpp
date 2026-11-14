#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <initializer_list>

static constexpr int INVALID_ARGUMENT = static_cast<int>(0x80550003);
static constexpr int NP_UTIL_INVALID_ARGUMENT = static_cast<int>(0x80550601);
static constexpr int NP_UTIL_INVALID_CHARACTER = static_cast<int>(0x80550608);
static constexpr int INVALID_PLATFORM_TYPE = static_cast<int>(0x80550004);
static constexpr int NOT_MATCH = static_cast<int>(0x80550609);
static constexpr int COND_TIMEDOUT = static_cast<int>(0x8055800B);
static constexpr int MUTEX_BUSY = static_cast<int>(0x8055800F);
static constexpr int HEAP_CREATE = static_cast<int>(0x8055800A);

static bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

template <typename F>
static bool expectRuntimeError(F operation, const char* message) {
    try {
        operation();
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
    }
    return expect(false, message);
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
    ok &= expect(sceNpCmpOnlineId(&kestrel, nullptr) == INVALID_ARGUMENT, "a null second online ID was compared");
    ok &= expect(sceNpCmpOnlineId(nullptr, nullptr) == INVALID_ARGUMENT, "null online IDs were compared");
    ok &= expect(sceNpCmpOnlineId(&kestrel, &kestrelCopy) == 0, "equal online IDs did not match");
    ok &= expect(sceNpCmpOnlineId(&kestrel, &heron) == NOT_MATCH, "different online IDs matched");

    NpIdFull low = npIdWithTag("kestrel_77", 0x00347370);
    NpIdFull high = low;
    high.reserved[7] = 1;
    std::int32_t order = 5;
    ok &= expect(sceNpCmpNpIdInOrder(&low, &high, nullptr) == INVALID_ARGUMENT, "a null order was accepted");
    ok &= expect(sceNpCmpNpIdInOrder(nullptr, &high, &order) == INVALID_ARGUMENT, "a null first NpId was accepted");
    ok &= expect(sceNpCmpNpIdInOrder(&low, nullptr, &order) == INVALID_ARGUMENT, "a null second NpId was accepted");
    ok &= expect(sceNpCmpNpIdInOrder(nullptr, nullptr, &order) == INVALID_ARGUMENT, "null NpIds were accepted");
    ok &= expect(sceNpCmpNpIdInOrder(nullptr, nullptr, nullptr) == INVALID_ARGUMENT, "all null arguments were accepted");
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
    const NpIdFull companion = npIdWithTag("heron-4", 0x706D6F63);
    ok &= expect(sceNpGetPlatformType(&companion) == INVALID_PLATFORM_TYPE, "the comp tag has a public platform type");
    ok &= expect(sceNpIntGetPlatformType(&companion) == 4 && sceNpIntGetPlatformType(&ps4) == 3 && sceNpIntGetPlatformType(&none) == 0,
        "the internal platform type does not include comp");
    ok &= expect(sceNpIntGetPlatformType(&other) == INVALID_PLATFORM_TYPE && sceNpIntGetPlatformType(nullptr) == INVALID_ARGUMENT,
        "the internal platform type accepted an unknown tag or a null NpId");

    NpIdFull tagged = npIdWithTag("heron-4", 0x00357370);
    const std::uint32_t tags[] = {0, 0x00337370, 0x32707370, 0x00347370};
    for (std::uint32_t type = 0; type < 4; ++type) {
        ok &= expect(sceNpSetPlatformType(&tagged, type) == 0 && std::memcmp(tagged.opt + 4, &tags[type], 4) == 0 && sceNpGetPlatformType(&tagged) == static_cast<int>(type),
            "setting a platform type did not write its tag");
    }
    ok &= expect(sceNpSetPlatformType(&tagged, 4) == INVALID_PLATFORM_TYPE && sceNpSetPlatformType(nullptr, 0) == INVALID_ARGUMENT,
        "the public setter accepted comp or a null NpId");
    ok &= expect(sceNpIntSetPlatformType(&tagged, 4) == 0 && sceNpIntGetPlatformType(&tagged) == 4, "the internal setter did not write comp");
    ok &= expect(sceNpIntSetPlatformType(&tagged, 2) == 0 && sceNpGetPlatformType(&tagged) == 2, "the internal setter did not write psp2");
    ok &= expect(sceNpIntSetPlatformType(&tagged, 5) == INVALID_PLATFORM_TYPE && sceNpIntSetPlatformType(nullptr, 1) == INVALID_ARGUMENT,
        "the internal setter accepted type 5 or a null NpId");

    constexpr int UTIL_INVALID_ARGUMENT = static_cast<int>(0x80550601);
    constexpr int UTIL_INVALID_NP_ID = static_cast<int>(0x80550605);
    NpIdFull canonical = npIdWithTag("heron-4", 0);
    canonical.reserved[0] = 1;
    ok &= expect(sceNpUtilCanonicalizeNpIdForPs4(&canonical, 0) == 0 && sceNpGetPlatformType(&canonical) == 3, "an untagged NpId did not get the ps4 tag");
    ok &= expect(sceNpUtilCanonicalizeNpIdForPsp2(&canonical, 0) == 0 && sceNpGetPlatformType(&canonical) == 3, "an existing tag was replaced");
    ok &= expect(sceNpUtilCanonicalizeNpIdForPsp2(&canonical, 1) == 0 && sceNpGetPlatformType(&canonical) == 2, "replacing did not write the psp2 tag");
    NpIdFull foreign = npIdWithTag("heron-4", 0x00337370);
    foreign.reserved[0] = 0;
    ok &= expect(sceNpUtilCanonicalizeNpIdForPs4(&foreign, 0) == UTIL_INVALID_NP_ID && sceNpGetPlatformType(&foreign) == 1,
        "an NpId without reserved[0] set was canonicalized");
    ok &= expect(sceNpUtilCanonicalizeNpIdForPs4(&foreign, 1) == UTIL_INVALID_NP_ID && sceNpGetPlatformType(&foreign) == 0,
        "replacing did not clear the tag before the NpId was refused");
    ok &= expect(sceNpUtilCanonicalizeNpIdForPs4(nullptr, 0) == UTIL_INVALID_ARGUMENT && sceNpUtilCanonicalizeNpIdForPsp2(nullptr, 1) == UTIL_INVALID_ARGUMENT,
        "canonicalizing a null NpId succeeded");
    NpIdFull loose = npIdWithTag("heron-4", 0x34737000);
    errno = 71;
    ok &= expect(sceNpIntGetPlatformType(reinterpret_cast<const NpIdFull*>(0x10)) == INVALID_ARGUMENT, "an unmapped NpId in sceNpIntGetPlatformType");
    ok &= expect(errno == 71, "errno altered on unmapped NpId in sceNpIntGetPlatformType");
    ok &= expect(sceNpGetPlatformType(&loose) == 0 && sceNpIntGetPlatformType(&loose) == 0, "a tag with opt[4] zero is not platform 0");
    const NpIdFull nearCompanion = npIdWithTag("heron-4", 0x716D6F63);
    ok &= expect(sceNpIntGetPlatformType(&nearCompanion) == INVALID_PLATFORM_TYPE, "comq was taken for comp");
    loose.reserved[0] = 1;
    ok &= expect(sceNpUtilCanonicalizeNpIdForPs4(&loose, 0) == 0 && std::memcmp(loose.opt + 4, &tags[3], 4) == 0, "a tag with opt[4] zero was not replaced");
    const std::int32_t replaceValues[] = {2, 0x100, -1};
    for (const std::int32_t replace : replaceValues) {
        NpIdFull replaced = npIdWithTag("heron-4", 0x00337370);
        replaced.reserved[0] = 1;
        replaced.opt[0] = 0x5A;
        ok &= expect(sceNpUtilCanonicalizeNpIdForPsp2(&replaced, replace) == 0 && sceNpGetPlatformType(&replaced) == 2 && replaced.opt[0] == 0x5A,
            "a non-zero replace did not clear only the tag");
    }
    NpIdFull refused = npIdWithTag("heron-4", 0);
    refused.reserved[0] = 2;
    ok &= expect(sceNpUtilCanonicalizeNpIdForPs4(&refused, 0) == UTIL_INVALID_NP_ID && sceNpGetPlatformType(&refused) == 0, "reserved[0] 2 was accepted");
    NpIdFull kept = npIdWithTag("heron-4", 0x00337370);
    std::memset(kept.opt, 0xA5, 4);
    kept.reserved[0] = 7;
    const NpIdFull before = kept;
    ok &= expect(sceNpSetPlatformType(&kept, 4) == INVALID_PLATFORM_TYPE && std::memcmp(&kept, &before, sizeof(kept)) == 0, "a refused type changed the NpId");
    ok &= expect(sceNpSetPlatformType(&kept, 3) == 0 && std::memcmp(kept.opt, before.opt, 4) == 0 && std::memcmp(kept.reserved, before.reserved, 8) == 0
            && std::memcmp(&kept.handle, &before.handle, sizeof(kept.handle)) == 0,
        "setting a platform type changed bytes outside the tag");

    std::array<std::uint8_t, 12> rawTitleId{};
    std::array<std::uint8_t, 21> titleIdObject{};
    for (std::size_t i = 0; i < rawTitleId.size(); ++i) rawTitleId[i] = static_cast<std::uint8_t>(i + 1);
    std::memcpy(titleIdObject.data() + 8, rawTitleId.data(), rawTitleId.size());
    ok &= expect(_ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(rawTitleId.data(), titleIdObject.data()), "equal title IDs did not compare equal");
    titleIdObject[8 + 11] = static_cast<std::uint8_t>(rawTitleId.back() ^ 1U);
    ok &= expect(!_ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(rawTitleId.data(), titleIdObject.data()), "different title IDs compared equal");
    titleIdObject[8 + 11] = rawTitleId.back();
    titleIdObject[8 + 12] = 1;
    ok &= expect(_ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(rawTitleId.data(), titleIdObject.data()), "title ID comparison read beyond the observed 12-byte range");
    const auto nearMaximum = reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max() - 4);
    ok &= expectRuntimeError([&] { _ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(rawTitleId.data(), nearMaximum); }, "title ID offset overflow did not throw");
    ok &= expectRuntimeError([&] { _ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(nullptr, titleIdObject.data()); }, "null raw title ID did not throw");
    ok &= expectRuntimeError([&] { _ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(rawTitleId.data(), nullptr); }, "null title ID object did not throw");
    const auto unreadable = reinterpret_cast<const void*>(std::uintptr_t{1});
    ok &= expectRuntimeError([&] { _ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(unreadable, titleIdObject.data()); }, "unreadable raw title ID did not throw");
    ok &= expectRuntimeError([&] { _ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(rawTitleId.data(), unreadable); }, "unreadable title ID object did not throw");

    std::array<std::uint8_t, 136> titleSecretObject{};
    NpTitleSecret titleSecret{};
    for (std::size_t i = 0; i < sizeof(titleSecret.data); ++i) titleSecret.data[i] = static_cast<std::uint8_t>(i * 3);
    std::memcpy(titleSecretObject.data() + 8, titleSecret.data, sizeof(titleSecret.data));
    ok &= expect(!_ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(titleSecretObject.data(), &titleSecret), "equal title secrets compared unequal");
    titleSecretObject[7] = 0xFF;
    ok &= expect(!_ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(titleSecretObject.data(), &titleSecret), "title secret comparison included the object prefix");
    titleSecretObject[8 + 127] ^= 1;
    ok &= expect(_ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(titleSecretObject.data(), &titleSecret), "different title secrets compared equal");
    ok &= expectRuntimeError([&] { _ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(nullptr, &titleSecret); }, "null title secret object did not throw");
    ok &= expectRuntimeError([&] { _ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(titleSecretObject.data(), nullptr); }, "null raw title secret did not throw");
    ok &= expectRuntimeError([&] { _ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(nearMaximum, &titleSecret); }, "title secret offset overflow did not throw");
    ok &= expectRuntimeError([&] { _ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(titleSecretObject.data(), unreadable); }, "unreadable raw title secret did not throw");
    ok &= expectRuntimeError([&] { _ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(unreadable, &titleSecret); }, "unreadable title secret object did not throw");

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

struct Wakeup {
    PthreadCond cond = nullptr;
    PthreadMutex mutex = nullptr;
    bool lightweight = false;
    int waiting = 0;
    int woken = 0;
    bool ready = false;
};

static void* APS5_VABI waitUntilReady(void* arg) {
    auto* wakeup = static_cast<Wakeup*>(arg);
    sceNpMutexLock(&wakeup->mutex);
    ++wakeup->waiting;
    int result = 0;
    while (!wakeup->ready && result == 0) result = wakeup->lightweight ? sceNpLwCondWait(&wakeup->cond, &wakeup->mutex) : sceNpCondWait(&wakeup->cond, &wakeup->mutex);
    if (result == 0) ++wakeup->woken;
    sceNpMutexUnlock(&wakeup->mutex);
    return nullptr;
}

enum class Wake { One, All, To };

static bool wakeWaiters(bool lightweight, int count, Wake wake) {
    Wakeup wakeup;
    wakeup.lightweight = lightweight;
    bool ok = expect(sceNpMutexInit(&wakeup.mutex, "np-wake", 0) == 0, "the wake mutex was not created");
    ok &= expect((lightweight ? sceNpLwCondInit(&wakeup.cond, "np-lw-wake") : sceNpCondInit(&wakeup.cond, "np-wake", 0)) == 0 && wakeup.cond, "the wake condition was not created");
    if (!ok) return false;
    Pthread threads[2] = {};
    for (int index = 0; index < count; ++index)
        if (!expect(scePthreadCreate(&threads[index], nullptr, waitUntilReady, &wakeup, "np-waiter") == 0, "a waiter was not created")) std::abort();
    for (;;) {
        sceNpMutexLock(&wakeup.mutex);
        const bool waiting = wakeup.waiting == count;
        if (waiting) break;
        sceNpMutexUnlock(&wakeup.mutex);
        scePthreadYield();
    }
    wakeup.ready = true;
    int signalled = 0;
    if (wake == Wake::All) signalled = lightweight ? sceNpLwCondSignalAll(&wakeup.cond) : sceNpCondSignalAll(&wakeup.cond);
    else if (wake == Wake::To) signalled = lightweight ? sceNpLwCondSignalTo(&wakeup.cond, threads[0]) : sceNpCondSignalTo(&wakeup.cond, threads[0]);
    else signalled = lightweight ? sceNpLwCondSignal(&wakeup.cond) : sceNpCondSignal(&wakeup.cond);
    sceNpMutexUnlock(&wakeup.mutex);
    if (!expect(signalled == 0, "signalling the waiters failed")) std::abort();
    for (int index = 0; index < count; ++index) ok &= expect(scePthreadJoin(threads[index], nullptr) == 0, "a waiter was not joined");
    ok &= expect(wakeup.woken == count, "not every waiter returned from its wait");
    ok &= expect((lightweight ? sceNpLwCondDestroy(&wakeup.cond) : sceNpCondDestroy(&wakeup.cond)) == 0, "the wake condition was not destroyed");
    ok &= expect(sceNpMutexDestroy(&wakeup.mutex) == 0, "the wake mutex was not destroyed");
    return ok;
}

static int testWake() {
    bool ok = true;
    for (const bool lightweight : {false, true}) {
        ok &= expect(wakeWaiters(lightweight, 1, Wake::One), lightweight ? "sceNpLwCondSignal did not wake its waiter" : "sceNpCondSignal did not wake its waiter");
        ok &= expect(wakeWaiters(lightweight, 2, Wake::All), lightweight ? "sceNpLwCondSignalAll did not wake both waiters" : "sceNpCondSignalAll did not wake both waiters");
        ok &= expect(wakeWaiters(lightweight, 1, Wake::To), lightweight ? "sceNpLwCondSignalTo did not wake its thread" : "sceNpCondSignalTo did not wake its thread");
    }
    return ok ? 0 : 1;
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
    ok &= expect(sceNpGetSystemClockUsec(nullptr) == INVALID_ARGUMENT, "a null destination was not rejected");
    std::int64_t later = -1;
    ok &= expect(sceNpGetSystemClockUsec(&later) == 0 && later >= usec && usec >= 0, "the clock went backwards");

    ok &= expect(usec >= before.tv_sec * 1000000 + before.tv_nsec / 1000 && usec <= after.tv_sec * 1000000 + after.tv_nsec / 1000, "the clock is not the monotonic kernel clock in microseconds");
    char version[12];
    std::memset(version, 'x', sizeof(version));
    sceNpGetSdkVersion(version);

    ok &= expect(std::memcmp(version, "1.00\0\0\0\0", 8) == 0, "the version is not 1.00 padded with zeros");
    ok &= expect(version[8] == 'x', "the version was written past 8 bytes");

    std::memset(version, 'x', sizeof(version));
    sceNpGetNavSdkVersion(version);

    ok &= expect(std::memcmp(version, "00.82\0\0\0", 8) == 0, "the nav SDK version is not 00.82 padded with zeros");
    ok &= expect(version[8] == 'x', "the nav SDK version was written past 8 bytes");
    return ok ? 0 : 1;
}

int testUtf8() {
    bool ok = expect(_sceNpMsgPackStrUtilIsValidUtf8String("hello"), "plain ASCII was not valid");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String(""), "an empty string was not valid");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String("\xC2\xA9"), "a two-byte sequence was not valid");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String("\xE2\x82\xAC"), "a three-byte sequence was not valid");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String("\xF0\x9F\x98\x80"), "a four-byte sequence was not valid");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String("a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80z"), "a mixed string was not valid");

    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8String("\xC0\x80"), "a C0 lead was accepted");
    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8String("\xC1\x80"), "a C1 lead was accepted");
    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8String("\x80"), "a bare continuation byte was accepted");
    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8String("\xBF"), "a BF continuation lead was accepted");
    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8String("\xF8\x80\x80\x80\x80"), "an F8 lead was accepted");
    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8String("\xFF"), "an FF byte was accepted");
    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8String("\xC2x"), "a non-continuation was accepted");
    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8String("\xE2\x82"), "a truncated sequence was accepted");
    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8String("\xF0\x9F\x98"), "a truncated four-byte sequence was accepted");

    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String("\xE0\x80\x80"), "the firmware scheme rejects an overlong three-byte encoding");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String("\xED\xA0\x80"), "the firmware scheme rejects a surrogate encoding");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String("\xF4\x90\x80\x80"), "the firmware scheme rejects a value past U+10FFFF");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String("\xF5\x80\x80\x80"), "the firmware scheme rejects an F5 lead");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8String("\xF7\xBF\xBF\xBF"), "the firmware scheme rejects an F7 lead");

    const char sized[] = {'\xE2', '\x82', '\xAC', 'q'};
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8StringWithSize(sized, 4), "a sized valid string was not valid");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8StringWithSize(sized, 3), "a sized sequence was not valid");
    ok &= expect(!_sceNpMsgPackStrUtilIsValidUtf8StringWithSize(sized, 2), "a truncated sized sequence was accepted");
    ok &= expect(_sceNpMsgPackStrUtilIsValidUtf8StringWithSize(sized, 0), "a zero size was not valid");

    ok &= expect(sceNpUtilNumChars(nullptr) == NP_UTIL_INVALID_ARGUMENT, "a null string was counted");
    ok &= expect(sceNpUtilNumChars("") == 0, "an empty string did not count zero");
    ok &= expect(sceNpUtilNumChars("hello") == 5, "ASCII did not count five");
    ok &= expect(sceNpUtilNumChars("a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80z") == 5, "a mixed string did not count sequences");
    ok &= expect(sceNpUtilNumChars("\x01") == NP_UTIL_INVALID_CHARACTER, "a control byte was counted");
    ok &= expect(sceNpUtilNumChars("\x7F") == NP_UTIL_INVALID_CHARACTER, "a DEL byte was counted");
    ok &= expect(sceNpUtilNumChars("\xC0\x80") == NP_UTIL_INVALID_CHARACTER, "a C0 lead was counted");
    ok &= expect(sceNpUtilNumChars("\x80") == NP_UTIL_INVALID_CHARACTER, "a bare continuation was counted");
    ok &= expect(sceNpUtilNumChars("\xF8\x80\x80\x80\x80") == NP_UTIL_INVALID_CHARACTER, "an F8 lead was counted");
    ok &= expect(sceNpUtilNumChars("\xC2x") == NP_UTIL_INVALID_CHARACTER, "a non-continuation was counted");
    ok &= expect(sceNpUtilNumChars("\xE0\x80\x80") == 1, "the firmware scheme did not count an overlong encoding");
    ok &= expect(sceNpUtilNumChars("\xED\xA0\x80") == 1, "the firmware scheme did not count a surrogate encoding");
    ok &= expect(sceNpUtilNumChars("\xF5\x80\x80\x80") == 1, "the firmware scheme did not count an F5 lead");
    return ok ? 0 : 1;
}

struct RecordingAllocator {
    int allocations;
    int reallocations;
    int releases;
    std::size_t lastSize;
    void* lastPointer;
    void* lastContext;
    bool fail;
};

static void* APS5_VABI recordAllocate(std::size_t size, void* context) {
    auto* record = static_cast<RecordingAllocator*>(context);
    ++record->allocations;
    record->lastSize = size;
    record->lastContext = context;
    return record->fail ? nullptr : std::malloc(size);
}

static void* APS5_VABI recordReallocate(void* pointer, std::size_t size, void* context) {
    auto* record = static_cast<RecordingAllocator*>(context);
    ++record->reallocations;
    record->lastSize = size;
    record->lastPointer = pointer;
    record->lastContext = context;
    return std::realloc(pointer, size);
}

static void APS5_VABI recordRelease(void* pointer, void* context) {
    auto* record = static_cast<RecordingAllocator*>(context);
    ++record->releases;
    record->lastPointer = pointer;
    record->lastContext = context;
    std::free(pointer);
}

static int testAlloc() {
    NpHeap emptyHeap {};
    bool ok = expect(_sceNpHeapMalloc(&emptyHeap, 8) == nullptr, "an unset heap allocated memory");
    ok &= expect(_sceNpHeapRealloc(&emptyHeap, nullptr, 8) == nullptr, "an unset heap reallocated memory");
    ok &= expect(_sceNpHeapStrdup(&emptyHeap, "text") == nullptr, "an unset heap duplicated a string");
    ok &= expect(_sceNpHeapStrndup(&emptyHeap, "text", 4) == nullptr, "an unset heap truncated a string");
    int untracked = 0;
    _sceNpHeapFree(&emptyHeap, &untracked);
    _sceNpHeapFree(&emptyHeap, nullptr);

    alignas(16) std::array<char, 65536> arena {};
    NpHeap heap {};
    ok &= expect(sceNpHeapInit(&heap, arena.data(), arena.size(), "test") == 0, "the test heap did not initialise");
    void* block = _sceNpHeapMalloc(&heap, 128);
    ok &= expect(block != nullptr, "the test heap did not allocate");
    ok &= expect(static_cast<char*>(block) >= arena.data() && static_cast<char*>(block) < arena.data() + arena.size(),
        "the test heap allocated outside its arena");
    void* grown = _sceNpHeapRealloc(&heap, block, 256);
    ok &= expect(grown != nullptr, "the test heap did not reallocate");
    char* copy = _sceNpHeapStrdup(&heap, "heap text");
    ok &= expect(copy != nullptr && std::strcmp(copy, "heap text") == 0, "the heap did not duplicate a string");
    char* truncated = _sceNpHeapStrndup(&heap, "abcdef", 3);
    ok &= expect(truncated != nullptr && std::strcmp(truncated, "abc") == 0, "the heap did not truncate a string");
    _sceNpHeapFree(&heap, grown);
    _sceNpHeapFree(&heap, copy);
    _sceNpHeapFree(&heap, truncated);
    sceNpHeapDestroy(&heap);
    ok &= expect(heap.mspace == nullptr, "the destroyed heap kept an mspace");

    NpHeap badHeap {};
    ok &= expect(sceNpHeapInit(&badHeap, nullptr, 4096, "bad") == HEAP_CREATE, "a heap initialised without an arena");

    RecordingAllocator record {};
    NpAllocator allocator {recordAllocate, recordReallocate, recordRelease, &record};
    void* allocation = _sceNpAllocatorMalloc(&allocator, 40);
    ok &= expect(allocation != nullptr && record.allocations == 1 && record.lastSize == 40 && record.lastContext == &record,
        "the allocator did not forward a malloc");
    void* resized = _sceNpAllocatorRealloc(&allocator, allocation, 96);
    ok &= expect(resized != nullptr && record.reallocations == 1 && record.lastSize == 96
        && record.lastPointer == allocation && record.lastContext == &record, "the allocator did not forward a realloc");
    char* duplicated = _sceNpAllocatorStrdup(&allocator, "allocator text");
    ok &= expect(duplicated != nullptr && std::strcmp(duplicated, "allocator text") == 0 && record.allocations == 2
        && record.lastSize == 15, "the allocator did not forward a strdup");
    char* shortened = _sceNpAllocatorStrndup(&allocator, "abcdef", 4);
    ok &= expect(shortened != nullptr && std::strcmp(shortened, "abcd") == 0 && record.lastSize == 5,
        "the allocator did not forward a strndup");
    record.fail = true;
    ok &= expect(_sceNpAllocatorStrndup(&allocator, "abcdef", 4) == nullptr, "the allocator copied a failed strndup");
    record.fail = false;
    _sceNpAllocatorFree(&allocator, resized);
    ok &= expect(record.releases == 1 && record.lastPointer == resized && record.lastContext == &record,
        "the allocator did not forward a free");
    _sceNpAllocatorFree(&allocator, duplicated);
    _sceNpAllocatorFree(&allocator, shortened);

    ok &= expect(_sceNpMalloc(16) == nullptr, "the global heap allocated before sceNpArchInit");
    ok &= expect(_sceNpRealloc(nullptr, 16) == nullptr, "the global heap reallocated before sceNpArchInit");
    _sceNpFree(&untracked);
    return ok ? 0 : 1;
}

static int testJws() {
    std::uint32_t encodeSize = 0;
    bool ok = expect(sceNpJwsHs256GetEncodeSize(0, nullptr) == NP_UTIL_INVALID_ARGUMENT, "a null encode size was accepted");
    ok &= expect(sceNpJwsHs256GetEncodeSize(0, &encodeSize) == 0 && encodeSize == 0x5E, "an empty payload sized wrongly");
    ok &= expect(sceNpJwsHs256GetEncodeSize(1, &encodeSize) == 0 && encodeSize == 0x62, "a one-byte payload sized wrongly");
    ok &= expect(sceNpJwsHs256GetEncodeSize(3, &encodeSize) == 0 && encodeSize == 0x62, "a three-byte payload sized wrongly");
    ok &= expect(sceNpJwsHs256GetEncodeSize(4, &encodeSize) == 0 && encodeSize == 0x66, "a four-byte payload sized wrongly");
    return ok ? 0 : 1;
}

static int testXml() {
    bool ok = expect(sceNpXmlParseInit(nullptr, nullptr, nullptr, nullptr) == NP_UTIL_INVALID_ARGUMENT,
        "a null context was initialised");
    std::array<std::uint8_t, 0x260> context;
    context.fill(0xAA);
    int arg2 = 0, arg3 = 0, arg4 = 0;
    ok &= expect(sceNpXmlParseInit(context.data(), &arg2, &arg3, &arg4) == 0, "the parser context did not initialise");
    ok &= expect(*reinterpret_cast<void**>(context.data()) == &arg2, "the second argument was not stored at offset 0");
    bool cleared = true;
    for (std::size_t i = 8; i < 0x250; ++i) cleared &= context[i] == 0;
    ok &= expect(cleared, "the parser state was not cleared");
    ok &= expect(*reinterpret_cast<void**>(context.data() + 0x250) == &arg3, "the third argument was not stored at offset 0x250");
    ok &= expect(*reinterpret_cast<void**>(context.data() + 0x258) == &arg4, "the fourth argument was not stored at offset 0x258");
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    int status = 2;
    if (std::strcmp(argv[1], "ids") == 0) status = testIds();
    else if (std::strcmp(argv[1], "sync") == 0) status = testSync();
    else if (std::strcmp(argv[1], "clock") == 0) status = testClock();
    else if (std::strcmp(argv[1], "wake") == 0) status = testWake();
    else if (std::strcmp(argv[1], "utf8") == 0) status = testUtf8();
    else if (std::strcmp(argv[1], "alloc") == 0) status = testAlloc();
    else if (std::strcmp(argv[1], "jws") == 0) status = testJws();
    else if (std::strcmp(argv[1], "xml") == 0) status = testXml();
    if (status == 0) std::printf("NpCommon %s tests passed\n", argv[1]);
    return status;
}
