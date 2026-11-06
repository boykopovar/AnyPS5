// Tests the APS5_TRACE_EQUEUE_IDENTITY probe of libkernel/Equeue.
//
// Pure part (no env, no guest API): wait/producer correlation classes, bounded combination table,
// formatting, and the process-wide output budget.
// Process part (real equeues through the libkernel exports): with the variable absent nothing is
// printed; with it present the probe prints one bounded line per window with the equeue identity, the
// waiter tid, the delivered events and the observable producer, distinguishes a wake driven by a
// concurrent producer from a buffered event, and gives a recreated equeue a new identity.
#include "prx/libkernel/Equeue/EqueueIdentityTrace.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#endif

namespace {

using namespace EqueueIdentityTraceDetail;

void check(bool condition, const std::string& reason) {
    if (!condition) {
        std::printf("FAILED: %s\n", reason.c_str());
        std::fflush(stdout);
        std::exit(1);
    }
}

void setEnv(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

std::string countField(const std::string& line, const char* key) {
    const std::string needle = std::string(" ") + key + " ";
    const std::size_t at = line.find(needle);
    if (at == std::string::npos) return {};
    const std::size_t begin = at + needle.size();
    const std::size_t end = line.find(' ', begin);
    return line.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

void testCorrelationClasses() {
    State state;
    Attach(state, 11, 5, "unit", 0);
    check(!HasActivity(state), "a fresh state must have no activity");

    // Wait woken by an instrumented producer.
    const void* const site = reinterpret_cast<const void*>(&testCorrelationClasses);
    const int tok1 = WaitEnter(state, 1000, 2000, 700, site);
    Producer(state, -11, 7, reinterpret_cast<const void*>(0xdeadbeef), site, 2500);
    WaitExit(state, tok1, 5000, 9000, 1);
    check(state.producerWaits == 1 && state.noProducerWaits == 0, "a producer during the wait must classify as prod");
    check(state.events == 1, "delivered events must be counted");
    check(state.lastWaitClass == 'p', "the last wait class must be p");

    // Buffered event: delivered, but no producer advanced during the wait.
    const int tok2 = WaitEnter(state, 10000, 12000, 700, reinterpret_cast<const void*>(0x1111));
    WaitExit(state, tok2, 15000, 16000, 2);
    check(state.noProducerWaits == 1, "a buffered event must classify as noprod");
    check(state.lastWaitClass == 'u', "the last wait class must be u");

    // Empty and error returns.
    const int tok3 = WaitEnter(state, 20000, 21000, 701, reinterpret_cast<const void*>(0x1111));
    WaitExit(state, tok3, 22000, 23000, 0);
    check(state.emptyWaits == 1, "an empty return must be counted");
    const int tok4 = WaitEnter(state, 24000, 25000, 701, reinterpret_cast<const void*>(0x1111));
    WaitExit(state, tok4, 26000, 27000, -9);
    check(state.errorWaits == 1, "an error return must be counted");
    check(state.waits == state.producerWaits + state.noProducerWaits + state.emptyWaits + state.errorWaits + state.openWaitCount,
          "accounting identity: waits must equal prod + noprod + empty + err + open");

    // A wait that is entered and never closed keeps the identity open until the caller closes it:
    // the integrated path does that from a catch-all, which is the only non-return path.
    const int tok5 = WaitEnter(state, 30000, 31000, 702, reinterpret_cast<const void*>(&testCorrelationClasses));
    check(state.waits == 5 && state.waiting, "an open wait must be visible before it closes");
    WaitExit(state, tok5, 32000, 33000, -1);

    Snapshot snapshot;
    TakeSnapshot(state, snapshot, 28000);
    char line[8192];
    check(FormatLine(snapshot, 28000, line, sizeof line), "the line must be formatted");
    const std::string text(line);
    check(text.rfind("[eq-id ", 0) == 0, "the line must start with the window header");
    check(text.find("eq 5/11 ") != std::string::npos, "the line must carry handle and probe id");
    check(countField(text, "waits") == "5", "waits must be counted");
    check(countField(text, "prod") == "1", "prod must be counted");
    check(countField(text, "noprod") == "1", "noprod must be counted");
    check(countField(text, "empty") == "1", "empty must be counted");
    check(countField(text, "err") == "2", "err must be counted");
    check(text.find(" | site ") != std::string::npos, "the per-site table must be printed");
    check(text.find("sites-folded 0") != std::string::npos, "with few sites nothing is folded");
    check(countField(text, "events") == "3", "delivered events must be summed");
    check(state.waits == state.producerWaits + state.noProducerWaits + state.emptyWaits + state.errorWaits,
          "accounting identity must hold after the extra error wait");
    check(text.find("lastw tid 702") != std::string::npos, "the last waiter tid must be printed");
    check(text.find("lastp f-11 i7 ") != std::string::npos, "the last producer filter/ident must be printed");
    check(text.size() < sizeof line, "the line must stay bounded");
    check(text.find("deadbeef") == std::string::npos, "the udata value must never be printed, only its hash");
}

void testConcurrentWaits() {
    State state;
    Attach(state, 13, 7, "concurrent", 0);
    const void* const siteA = reinterpret_cast<const void*>(0x1000);
    const void* const siteB = reinterpret_cast<const void*>(0x2000);
    const int tokenA = WaitEnter(state, 10000, 2000, 701, siteA);
    const int tokenB = WaitEnter(state, 12000, 2000, 702, siteB);
    check(tokenA >= 0 && tokenB >= 0 && tokenA != tokenB, "two open waits need distinct tokens");
    check(state.openWaitCount == 2, "two waits are open");
    WaitExit(state, tokenA, 20000, 3000, 1);
    WaitExit(state, tokenB, 22000, 3000, 0);
    check(state.waits == 2, "both waits are counted");
    check(state.openWaitCount == 0, "both waits close");
    check(state.waits == state.producerWaits + state.noProducerWaits + state.emptyWaits + state.errorWaits + state.openWaitCount,
          "identity holds after concurrent waits");
    WaitSite& siteEntryA = WaitSiteFor(state, siteA, 701);
    WaitSite& siteEntryB = WaitSiteFor(state, siteB, 702);
    check(siteEntryA.waits == 1 && siteEntryB.waits == 1, "each site keeps its own count");
}

// A wait that is still open when the window is reset must close into the new window with the site
// and tid captured at entry: the class must not land in a window whose wait count already went to
// zero, and the site must not degrade to (0,0) (F5).
void testOpenWaitAcrossReset() {
    State state;
    Attach(state, 21, 9, "reset-open", 1000);
    const void* const site = reinterpret_cast<const void*>(0x3000);
    const int token = WaitEnter(state, 5000, 6000, 801, site);
    check(token >= 0, "the open wait needs a slot");
    ResetWindow(state, 7000);
    check(state.waits == 0, "a reset clears the window counters");
    check(state.openWaitCount == 1, "a reset keeps the open wait");
    check(state.waitSiteCount == 0, "a new window starts with an empty site table");
    Producer(state, -11, 3, reinterpret_cast<const void*>(0x9000), site, 7500);
    WaitExit(state, token, 8000, 9000, 1);
    check(state.waits == 1 && state.producerWaits == 1,
          "a wait that closes after the boundary is counted in the closing window");
    WaitSite& entry = WaitSiteFor(state, site, 801);
    check(entry.waits == 1 && entry.producerWaits == 1 && entry.tid == 801,
          "the late exit keeps its own site and tid");
    check(state.waits == state.producerWaits + state.noProducerWaits + state.emptyWaits + state.errorWaits +
                               state.openWaitCount,
          "the identity holds across a window boundary");
}

// More concurrent waits than per-equeue open slots: the extra wait must be classified as an error
// at entry instead of being counted and never closed (which would break the identity).
void testOpenSlotExhaustion() {
    State state;
    Attach(state, 22, 10, "open-slots", 0);
    const void* const site = reinterpret_cast<const void*>(0x3100);
    int tokens[State::MaxOpenWaits];
    for (std::size_t index = 0; index < State::MaxOpenWaits; ++index) {
        tokens[index] = WaitEnter(state, 1000 + index, 2000, 900, site);
        check(tokens[index] >= 0, "every wait inside the slot budget needs a token");
    }
    const int extra = WaitEnter(state, 2000, 2000, 901, site);
    check(extra < 0, "a wait beyond the slot budget has no token");
    check(state.waits == State::MaxOpenWaits + 1, "every entered wait is counted");
    check(state.errorWaits == 1, "the wait beyond the slot budget is classified as err");
    for (std::size_t index = 0; index < State::MaxOpenWaits; ++index) WaitExit(state, tokens[index], 9000, 9000, 0);
    check(state.openWaitCount == 0, "every tracked wait closes");
    check(state.waits == state.emptyWaits + state.errorWaits,
          "the identity holds after an untrackable wait");
}

// Delete while a wait is open: the slot stays as a tombstone so the late exit is classified, and
// it is released as soon as nothing is in flight, so the 32-entry registry cannot be exhausted.
void testRetiredTombstoneReleasesSlot() {
    State state;
    Attach(state, 23, 11, "retire", 0);
    const void* const site = reinterpret_cast<const void*>(0x3200);
    const int first = WaitEnter(state, 1000, 2000, 902, site);
    const int second = WaitEnter(state, 1100, 2000, 903, site);
    check(first >= 0 && second >= 0, "two waits are tracked");
    Retire(state);
    check(state.retired && state.id == 23, "the deleted equeue keeps a tombstone while waits are open");
    WaitExit(state, first, 3000, 4000, -1);
    check(state.id == 23 && state.errorWaits == 1 && state.openWaitCount == 1,
          "a late exit is classified while another wait is still open");
    check(!ReleaseIfUnused(state), "a tombstone with an open wait must not be released");
    WaitExit(state, second, 3100, 4000, -1);
    check(state.id == 23 && state.openWaitCount == 0 && state.errorWaits == 2,
          "the final classification survives until the registry reports it");
    check(ReleaseIfUnused(state), "the unused tombstone is released");
    check(state.id == 0 && state.handle == 0 && !state.retired, "the released slot has no identity");
    Attach(state, 24, 12, "reuse", 0);
    check(state.id == 24 && state.handle == 12, "the released slot is reusable with a new identity");
}

// A FormatLine failure must not consume the window: the caller resets only after a successful
// format, so a short buffer keeps the counters and the events for the next attempt (F4).
void testFormatFailureKeepsWindow() {
    State state;
    Attach(state, 25, 13, "format", 0);
    const void* const site = reinterpret_cast<const void*>(0x3300);
    const int token = WaitEnter(state, 1000, 2000, 904, site);
    Producer(state, -11, 5, reinterpret_cast<const void*>(0x9100), site, 2500);
    WaitExit(state, token, 3000, 4000, 2);
    Snapshot snapshot;
    TakeSnapshot(state, snapshot, 5000);
    char tiny[8] = {};
    check(!FormatLine(snapshot, 5000, tiny, sizeof tiny), "a buffer too short must fail to format");
    check(state.waits == 1 && state.producerWaits == 1 && state.events == 2 && state.waitSiteCount == 1,
          "a failed format must not consume the window");
    char line[8192];
    check(FormatLine(snapshot, 6000, line, sizeof line), "the same snapshot must format with a real buffer");
    ResetWindow(state, 6000);
    check(!HasActivity(state) && state.waitSiteCount == 0,
          "a successful format followed by a reset clears the window");
}

void testBoundedCombos() {    State state;
    Attach(state, 12, 6, "combos", 0);
    for (int key = 0; key < 6; ++key) {
        Producer(state, -11, static_cast<std::uintptr_t>(key + 1), reinterpret_cast<const void*>(0x1000 + key),
                 reinterpret_cast<const void*>(0x2000 + key), 1000);
    }
    check(state.comboCount == 4, "only four combination slots are kept");
    check(state.comboOverflow == 2, "the remaining combinations must be counted as overflow");
    Snapshot snapshot;
    TakeSnapshot(state, snapshot, 2000);
    char line[8192];
    check(FormatLine(snapshot, 2000, line, sizeof line), "the combo line must be formatted");
    const std::string text(line);
    check(text.find("combos-overflow 2") != std::string::npos, "the combo overflow must be explicit");
    check(countField(text, "producers") == "6", "every producer must be counted even when folded");

    // Reuse: a new window starts clean but keeps identity.
    const std::uint64_t id = state.id;
    ResetWindow(state, 3000);
    check(state.id == id && state.comboCount == 0 && state.producers == 0, "reset keeps identity and clears counters");
    check(!HasActivity(state), "a reset state has no activity");
}

void testCoordinatorBudget() {
    CoordinatorState coordinator;
    std::uint64_t admitted = 0;
    for (std::uint64_t index = 0; index < 12; ++index) {
        const Decision decision = Admit(coordinator, 1000 + index * 10, 0);
        if (decision.admit) ++admitted;
    }
    check(admitted == CoordinatorState::MaxLines, "only MaxLines lines per window are admitted");
    check(coordinator.suppressed == 3, "suppressed lines must be counted");
    std::uint64_t windowIndex = coordinator.windowIndex;
    check(!Admit(coordinator, 2000, windowIndex).admit, "the same equeue must not print twice per window");
    check(Admit(coordinator, CoordinatorState::WindowNanos + 2000, 0).admit, "a new window must admit again");
}

std::size_t openEqueue(const char* name) {
    KernelEqueue handle = 0;
    const int result = sceKernelCreateEqueue(&handle, name);
    check(result == 0 && handle != 0, std::string("equeue creation failed: ") + name);
    return handle;
}

// The bodies must not be identical: identical functions can be folded to one address by the linker,
// and then the probe could not tell the two call sites apart (that is exactly what a real mixed
// bucket would look like).
int waitFromAlpha(KernelEqueue eq, KernelEvent* events, int num, int* out) {
    static volatile int alphaMarker = 1;
    alphaMarker = alphaMarker + 1;
    return sceKernelWaitEqueue(eq, events, num, out, nullptr);
}

int waitFromBeta(KernelEqueue eq, KernelEvent* events, int num, int* out) {
    static volatile int betaMarker = 1;
    betaMarker = betaMarker * 2;
    return sceKernelWaitEqueue(eq, events, num, out, nullptr);
}

int pollFromGamma(KernelEqueue eq, KernelEvent* events, int num, int* out) {
    static volatile int gammaMarker = 1;
    gammaMarker = gammaMarker - 1;
    const KernelUseconds zero = 0;
    return sceKernelWaitEqueue(eq, events, num, out, &zero);
}

// Two distinct producer call sites. The probe must report the immediate caller of the trigger, so
// two triggers from different functions must not report the same site (a site inherited from an
// earlier add/trigger would be the F6 defect).
void triggerFromDelta(KernelEqueue eq) {
    static volatile int deltaMarker = 1;
    deltaMarker = deltaMarker + 7;
    sceKernelTriggerUserEvent(eq, 42, nullptr);
}

void triggerFromEpsilon(KernelEqueue eq) {
    static volatile int epsilonMarker = 1;
    epsilonMarker = epsilonMarker ^ 9;
    sceKernelTriggerUserEvent(eq, 42, nullptr);
}

unsigned long currentThreadId() {
#if defined(_WIN32)
    return static_cast<unsigned long>(::GetCurrentThreadId());
#else
    return static_cast<unsigned long>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
}

void testProbeOff() {
    const std::size_t handle = openEqueue("probe-off");
    check(sceKernelAddUserEvent((KernelEqueue)handle, 1) == 0, "user event add failed");
    check(sceKernelTriggerUserEvent((KernelEqueue)handle, 1, nullptr) == 0, "user event trigger failed");
    KernelEvent events[4]{};
    int out = 0;
    check(sceKernelWaitEqueue((KernelEqueue)handle, events, 4, &out, nullptr) == 0 && out == 1, "the wait must return the event");
    check(sceKernelDeleteEqueue((KernelEqueue)handle) == 0, "equeue delete failed");
    const std::size_t empty = openEqueue("probe-off-empty");
    const KernelUseconds zero = 0;
    int polled = -1;
    sceKernelWaitEqueue((KernelEqueue)empty, events, 4, &polled, &zero);
    check(polled == 0, "the poll of an idle queue must return no events");
    check(sceKernelDeleteEqueue((KernelEqueue)empty) == 0, "empty equeue delete failed");
}

std::size_t firstHandle = 0;
std::size_t secondHandle = 0;
std::size_t overflowHandle = 0;
std::size_t siteHandle = 0;
std::size_t producerHandle = 0;
unsigned long siteTidAlpha = 0;
unsigned long siteTidBeta = 0;

void testProbeOn() {
    const std::size_t first = openEqueue("probe-on");
    firstHandle = first;
    check(sceKernelAddUserEvent((KernelEqueue)first, 7) == 0, "user event add failed");

    KernelEvent events[4]{};
    int out = 0;
    unsigned long waiterTid = 0;
    // Known correlation: a concurrent producer triggers the event while the waiter blocks.
    std::thread producer([first] {
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        sceKernelTriggerUserEvent((KernelEqueue)first, 7, nullptr);
    });
    std::thread waiter([&] {
        waiterTid = currentThreadId();
        check(sceKernelWaitEqueue((KernelEqueue)first, events, 4, &out, nullptr) == 0, "the blocking wait must return");
    });
    waiter.join();
    producer.join();
    check(out == 1, "the wake must deliver one event");

    // Second window: buffered event collection (poll) then report.
    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    const KernelUseconds zero = 0;
    out = -1;
    sceKernelWaitEqueue((KernelEqueue)first, events, 4, &out, &zero);

    // Reuse: delete and recreate the same guest handle space; the probe id must differ.
    check(sceKernelDeleteEqueue((KernelEqueue)first) == 0, "equeue delete failed");
    const std::size_t second = openEqueue("probe-on-again");
    secondHandle = second;
    check(sceKernelAddUserEvent((KernelEqueue)second, 3) == 0, "second user event add failed");
    sceKernelTriggerUserEvent((KernelEqueue)second, 3, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    out = -1;
    sceKernelWaitEqueue((KernelEqueue)second, events, 4, &out, &zero);
    check(sceKernelDeleteEqueue((KernelEqueue)second) == 0, "second equeue delete failed");
    check(waiterTid != 0, "the waiter tid must be captured");

    // Bounded registry: 33 live equeues, only 32 slots. The 33rd is counted as a slot overflow, which
    // is reported in the next line of a queue that does have a slot.
    std::vector<std::size_t> many;
    for (int index = 0; index < 33; ++index) many.push_back(openEqueue("probe-on-many"));
    overflowHandle = many.front();
    check(sceKernelAddUserEvent((KernelEqueue)overflowHandle, 5) == 0, "overflow user event add failed");
    sceKernelTriggerUserEvent((KernelEqueue)overflowHandle, 5, nullptr);
    sceKernelWaitEqueue((KernelEqueue)overflowHandle, events, 4, &out, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    out = -1;
    sceKernelWaitEqueue((KernelEqueue)overflowHandle, events, 4, &out, &zero);
    for (const std::size_t handle : many) check(sceKernelDeleteEqueue((KernelEqueue)handle) == 0, "many equeue delete failed");

    // Delete while a wait is blocked: the probe must keep a tombstone so the late exit is
    // classified as err, and release the slot afterwards (the queues below are still tracked).
    const std::size_t doomed = openEqueue("probe-delete-wait");
    check(sceKernelAddUserEvent((KernelEqueue)doomed, 77) == 0, "doomed event add failed");
    int doomedResult = 0;
    std::thread blocked([&] {
        KernelEvent local[4]{};
        int localOut = 0;
        doomedResult = sceKernelWaitEqueue((KernelEqueue)doomed, local, 4, &localOut, nullptr);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(sceKernelDeleteEqueue((KernelEqueue)doomed) == 0, "doomed equeue delete failed");
    blocked.join();
    check(doomedResult == SCE_KERNEL_ERROR_EBADF, "a wait blocked on a deleted equeue must return EBADF");

    // Site attribution: two different call sites wait on the same equeue from two threads, so the
    // per-site table must keep them apart instead of folding both into one bucket.
    siteHandle = openEqueue("probe-sites");
    // Two different call sites, two different threads, one equeue. The event is armed before each
    // wait, so both waits are deterministic buffered returns (class "u") and no event can be stolen
    // by the other thread.
    check(sceKernelAddUserEvent((KernelEqueue)siteHandle, 9) == 0, "site event add failed");
    sceKernelTriggerUserEvent((KernelEqueue)siteHandle, 9, nullptr);
    std::thread alpha([&] {
        KernelEvent local[4]{};
        int localOut = 0;
        siteTidAlpha = currentThreadId();
        check(waitFromAlpha((KernelEqueue)siteHandle, local, 4, &localOut) == 0 && localOut == 1, "alpha wait must return");
    });
    alpha.join();
    check(sceKernelDeleteUserEvent((KernelEqueue)siteHandle, 9) == 0, "site event delete failed");
    check(sceKernelAddUserEvent((KernelEqueue)siteHandle, 10) == 0, "site event add failed");
    sceKernelTriggerUserEvent((KernelEqueue)siteHandle, 10, nullptr);
    std::thread beta([&] {
        KernelEvent local[4]{};
        int localOut = 0;
        siteTidBeta = currentThreadId();
        check(waitFromBeta((KernelEqueue)siteHandle, local, 4, &localOut) == 0 && localOut == 1, "beta wait must return");
    });
    beta.join();
    check(sceKernelDeleteUserEvent((KernelEqueue)siteHandle, 10) == 0, "site event delete failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    out = -1;
    pollFromGamma((KernelEqueue)siteHandle, events, 4, &out);
    check(sceKernelDeleteEqueue((KernelEqueue)siteHandle) == 0, "site equeue delete failed");
    check(siteTidAlpha != siteTidBeta, "the two site waits must come from different threads");

    // Producer attribution: the reported producer site must be the immediate caller of the trigger.
    // Two different call sites, each in its own window, must not collapse to one inherited site.
    producerHandle = openEqueue("probe-producer");
    check(sceKernelAddUserEvent((KernelEqueue)producerHandle, 42) == 0, "producer event add failed");
    triggerFromDelta((KernelEqueue)producerHandle);
    out = 0;
    check(sceKernelWaitEqueue((KernelEqueue)producerHandle, events, 4, &out, nullptr) == 0 && out == 1,
          "the delta wait must return");
    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    triggerFromEpsilon((KernelEqueue)producerHandle);
    out = 0;
    check(sceKernelWaitEqueue((KernelEqueue)producerHandle, events, 4, &out, nullptr) == 0 && out == 1,
          "the epsilon wait must return");
    check(sceKernelDeleteEqueue((KernelEqueue)producerHandle) == 0, "producer equeue delete failed");

    // Concurrent create/delete while waits and producers run: the registry must not use a freed
    // object and must not crash.
    std::thread churn([] {
        for (int index = 0; index < 40; ++index) {
            KernelEqueue temp = 0;
            if (sceKernelCreateEqueue(&temp, "churn") == 0) {
                sceKernelAddUserEvent(temp, 1);
                sceKernelTriggerUserEvent(temp, 1, nullptr);
                KernelEvent local[2]{};
                int localOut = 0;
                sceKernelWaitEqueue(temp, local, 2, &localOut, nullptr);
                sceKernelDeleteEqueue(temp);
            }
        }
    });
    std::thread worker([] {
        const std::size_t queue = openEqueue("churn-live");
        for (int index = 0; index < 40; ++index) {
            sceKernelAddUserEvent((KernelEqueue)queue, index + 1);
            sceKernelTriggerUserEvent((KernelEqueue)queue, index + 1, nullptr);
            KernelEvent local[2]{};
            int localOut = 0;
            sceKernelWaitEqueue((KernelEqueue)queue, local, 2, &localOut, nullptr);
        }
        check(sceKernelDeleteEqueue((KernelEqueue)queue) == 0, "churn queue delete failed");
    });
    churn.join();
    worker.join();
}

std::string readFile(const char* path) {
    std::ifstream input(path);
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<std::string> eqLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("[eq-id ", 0) == 0) lines.push_back(line);
    }
    return lines;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "off";
    testCorrelationClasses();
    testConcurrentWaits();
    testOpenWaitAcrossReset();
    testOpenSlotExhaustion();
    testRetiredTombstoneReleasesSlot();
    testFormatFailureKeepsWindow();
    testBoundedCombos();
    testCoordinatorBudget();

    const char* path = "equeue_identity_test.log";
#if defined(_WIN32)
    const int saved = _dup(2);
    check(saved >= 0, "stderr could not be duplicated");
    const int fd = _open(path, _O_CREAT | _O_WRONLY | _O_TRUNC | _O_TEXT, _S_IREAD | _S_IWRITE);
    check(fd >= 0, "log file could not be opened");
    check(_dup2(fd, 2) == 0, "stderr could not be redirected");
    _close(fd);
#else
    check(freopen(path, "w", stderr) != nullptr, "stderr could not be redirected");
#endif

    if (mode == "off") {
        testProbeOff();
    } else {
        setEnv("APS5_TRACE_EQUEUE_IDENTITY", "1");
        testProbeOn();
    }
    std::fflush(stderr);

#if defined(_WIN32)
    check(_dup2(saved, 2) == 0, "stderr could not be restored");
    _close(saved);
#endif
    std::fflush(stderr);

    const std::string text = readFile(path);
    std::remove(path);

    const std::vector<std::string> lines = eqLines(text);

    if (mode == "off") {
        check(lines.empty(), "the probe must print nothing when the variable is absent");
        std::printf("equeue identity probe off: no output\n");
        return 0;
    }

    check(!lines.empty(), "the probe must print at least one line when enabled");
    for (const auto& line : lines) std::printf("LINE: %s\n", line.c_str());

    const std::string firstTag = "eq " + std::to_string(firstHandle) + "/";
    const std::string secondTag = "eq " + std::to_string(secondHandle) + "/";
    const std::string* first = nullptr;
    const std::string* secondLine = nullptr;
    for (const auto& line : lines) {
        if (first == nullptr && line.find(firstTag) != std::string::npos) first = &line;
        if (secondLine == nullptr && line.find(secondTag) != std::string::npos) secondLine = &line;
    }
    check(first != nullptr, "the probe must report the equeue that was waited on");
    check(first->find("| waits ") != std::string::npos, "the line must carry wait counters");
    check(countField(*first, "waits") == "2", "two waits in the first window");
    check(countField(*first, "prod") == "1", "the concurrent producer must classify as prod");
    check(countField(*first, "events") == "2", "delivered events must be summed");
    check(first->find("lastw tid ") != std::string::npos, "the waiter tid must be printed");
    check(first->find("lastp f-11 i7 ") != std::string::npos, "the producer filter and ident must be printed");
    check(first->find("| lastp ") != std::string::npos && first->find(" site ") != std::string::npos,
          "the producer site must be printed");
    check(first->find("slots-overflow 0") != std::string::npos, "the slot overflow counter must be reported");

    // A window boundary can fall between the two waits (the report is global and 3 s wide), so the
    // two call sites are looked up across every window of this equeue: what matters is that the probe
    // keeps them as two distinct (site, tid) entries instead of one mixed bucket.
    const std::string siteTag = "eq " + std::to_string(siteHandle) + "/";
    std::vector<std::string> siteLines;
    for (const auto& line : lines) {
        if (line.find(siteTag) != std::string::npos && line.find(" | site ") != std::string::npos) {
            siteLines.push_back(line);
        }
    }
    check(!siteLines.empty(), "the equeue with two call sites must be reported");
    bool alphaSeen = false;
    bool betaSeen = false;
    bool foldedZero = false;
    for (const auto& line : siteLines) {
        if (line.find("tid " + std::to_string(siteTidAlpha)) != std::string::npos) alphaSeen = true;
        if (line.find("tid " + std::to_string(siteTidBeta)) != std::string::npos) betaSeen = true;
        if (line.find("sites-folded 0") != std::string::npos) foldedZero = true;
    }
    check(alphaSeen, "the alpha thread must appear in the per-site table");
    check(betaSeen, "the beta thread must appear in the per-site table");
    check(foldedZero, "two sites plus the poll must fit in the printed table");

    // Producer site: two triggers from two different functions, one per window. A stale thread-local
    // site (F6) would make the second report repeat the first caller.
    const std::string producerTag = "eq " + std::to_string(producerHandle) + "/";
    std::vector<std::string> producerSites;
    for (const auto& line : lines) {
        if (line.find(producerTag) == std::string::npos) continue;
        const std::size_t lastProducer = line.find(" | lastp ");
        if (lastProducer == std::string::npos) continue;
        const std::size_t at = line.find("site ", lastProducer);
        const std::size_t end = line.find(" proc ", at);
        if (at != std::string::npos && end != std::string::npos) {
            producerSites.push_back(line.substr(at + 5, end - (at + 5)));
        }
    }
    check(producerSites.size() >= 2, "both producer windows must be reported");
    check(producerSites.front() != producerSites.back(),
          "two producer call sites must not report the same inherited site");
    check(producerSites.front().rfind("kernel_equeue_identity_tests.exe+0x", 0) == 0,
          "the producer site must be the test caller, not the libkernel wrapper");

    const std::string overflowTag = "eq " + std::to_string(overflowHandle) + "/";
    const std::string* overflowLine = nullptr;
    for (const auto& line : lines) {
        if (overflowLine == nullptr && line.find(overflowTag) != std::string::npos) overflowLine = &line;
    }
    check(overflowLine != nullptr, "the queue that still has a slot must be reported");
    check(overflowLine->find("slots-overflow 1") != std::string::npos,
          "the 33rd live equeue must be counted as a slot overflow");

    if (secondLine != nullptr) {
        const std::string firstId = first->substr(first->find("eq ") + 3, first->find(" nh") - (first->find("eq ") + 3));
        const std::string secondId = secondLine->substr(secondLine->find("eq ") + 3,
                                                        secondLine->find(" nh") - (secondLine->find("eq ") + 3));
        check(firstId != secondId, "a recreated equeue must get a different identity");
    }
    std::printf("equeue identity probe on: %zu line(s)\n", lines.size());
    return 0;
}