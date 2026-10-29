#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <stdexcept>
#include <thread>
#include <utility>

static constexpr int SCE_OK = 0;
static constexpr int PS5_LOGICAL_CPUS = 16;

static void Require(bool value) { if (!value) std::abort(); }

struct RestoreProcessMask {
    KernelCpumask mask;
    bool* restored;

    ~RestoreProcessMask() {
        const std::uint64_t requested[1] = {mask};
        *restored = cpuset_setaffinity_nid_postfix(3, 2, -1, 8, requested) == 0;
    }
};

static constexpr std::size_t GUEST_CPUSET_BYTES = 32;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_ERANGE = 34;

static KernelCpumask ReadCpuset(Pthread thread) {
    std::uint8_t cpuset[GUEST_CPUSET_BYTES + 8];
    std::memset(cpuset, 0xAB, sizeof(cpuset));
    Require(pthread_getaffinity_np_nid_postfix(thread, GUEST_CPUSET_BYTES, cpuset) == 0);
    for (std::size_t i = sizeof(KernelCpumask); i < GUEST_CPUSET_BYTES; ++i) Require(cpuset[i] == 0);
    for (std::size_t i = GUEST_CPUSET_BYTES; i < sizeof(cpuset); ++i) Require(cpuset[i] == 0xAB);
    KernelCpumask mask = 0;
    std::memcpy(&mask, cpuset, sizeof(mask));
    return mask;
}

static void* APS5_VABI Worker(void* arg) {
    static_cast<std::future<void>*>(arg)->get();
    return nullptr;
}

struct RemoteThread {
    std::atomic<int> tid{0};
    std::future<void>* release = nullptr;
    KernelCpumask observed = 0;
    KernelCpumask inherited = 0;
    int cpu = -1;
};

static void* APS5_VABI RecordOwnAffinity(void* arg) {
    Require(scePthreadGetaffinity(scePthreadSelf(), static_cast<KernelCpumask*>(arg)) == SCE_OK);
    return nullptr;
}

static void* APS5_VABI RecordThreadId(void* arg) {
    auto& data = *static_cast<RemoteThread*>(arg);
    data.tid.store(scePthreadGetthreadid(), std::memory_order_release);
    data.release->get();
    Require(scePthreadGetaffinity(scePthreadSelf(), &data.observed) == SCE_OK);
    data.cpu = sceKernelGetCurrentCpu();
    Pthread child = nullptr;
    Require(scePthreadCreate(&child, nullptr, RecordOwnAffinity, &data.inherited, nullptr) == SCE_OK);
    Require(scePthreadJoin(child, nullptr) == SCE_OK);
    return nullptr;
}

static void* APS5_VABI ReportCpuset(void* arg) {
    auto* result = static_cast<std::uint64_t*>(arg);
    std::uint64_t set[4] = {~0ull, ~0ull, ~0ull, ~0ull};
    result[0] = cpuset_getaffinity_nid_postfix(3, 1, -1, 8, set);
    result[1] = set[0];
    result[2] = set[1];
    result[3] = cpuset_getaffinity_nid_postfix(3, 1, -1, sizeof(set), set);
    result[4] = set[0] | (set[1] != 0 || set[2] != 0 || set[3] != 0 ? 1ull << 63 : 0);
    result[5] = cpuset_getaffinity_nid_postfix(3, 1, -1, 4, set) == -1 && *__error_nid_postfix() == 34;
    result[6] = cpuset_getaffinity_nid_postfix(3, 1, -1, 33, set) == -1 && *__error_nid_postfix() == 34;
    result[7] = cpuset_getaffinity_nid_postfix(3, 1, -1, 8, nullptr) == -1 && *__error_nid_postfix() == 14;
    return nullptr;
}

struct SetCpusetResult {
    KernelCpumask cpu;
    int results[8];
    KernelCpumask affinity[3];
};

static void* APS5_VABI SetCpuset(void* arg) {
    auto* result = static_cast<SetCpusetResult*>(arg);
    const KernelCpumask cpu = result->cpu;
    std::uint64_t set[4] = {cpu | (1ull << 40), 0, 0, 0};
    result->results[0] = cpuset_setaffinity_nid_postfix(3, 1, -1, 8, set);
    scePthreadGetaffinity(scePthreadSelf(), &result->affinity[0]);

    set[1] = 1;
    result->results[1] = cpuset_setaffinity_nid_postfix(3, 1, -1, 16, set) == -1 && *__error_nid_postfix() == 22;
    set[1] = 0;
    result->results[2] = cpuset_setaffinity_nid_postfix(3, 1, -1, sizeof(set), set);
    result->results[3] = cpuset_setaffinity_nid_postfix(3, 1, -1, 4, set) == -1 && *__error_nid_postfix() == 34;
    result->results[4] = cpuset_setaffinity_nid_postfix(3, 1, -1, 33, set) == -1 && *__error_nid_postfix() == 34;
    result->results[5] = cpuset_setaffinity_nid_postfix(3, 1, -1, 8, nullptr) == -1 && *__error_nid_postfix() == 14;
    const std::uint64_t unavailable[1] = {1ull << 40};
    result->results[6] = cpuset_setaffinity_nid_postfix(3, 1, -1, 8, unavailable) == -1 && *__error_nid_postfix() == 11;
    scePthreadGetaffinity(scePthreadSelf(), &result->affinity[1]);

    const std::uint64_t process[1] = {cpu};
    result->results[7] = cpuset_setaffinity_nid_postfix(3, 2, getpid_nid_postfix(), 8, process);
    scePthreadGetaffinity(scePthreadSelf(), &result->affinity[2]);

    return nullptr;
}

static void* APS5_VABI ReportMask(void* arg) {
    *static_cast<KernelCpumask*>(arg) = sceKernelGetAvailableCpumask();
    return nullptr;
}

int main() {
    const KernelCpumask available = sceKernelGetAvailableCpumask();
    Require(available != 0);
    Require((available >> PS5_LOGICAL_CPUS) == 0);
    Require(sceKernelGetAvailableCpumask() == available);

    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    KernelCpumask defaultAffinity = 0;
    Require(scePthreadAttrGetaffinity(&attr, &defaultAffinity) == SCE_OK);
    Require(defaultAffinity == available);

    std::promise<void> release;
    auto released = release.get_future();
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Worker, &released, nullptr) == SCE_OK);
    KernelCpumask threadAffinity = 0;
    Require(scePthreadGetaffinity(thread, &threadAffinity) == SCE_OK);
    Require(threadAffinity == available);

    for (KernelCpumask cpu = 1; cpu != 0; cpu <<= 1) {
        if ((available & cpu) == 0) continue;
        Require(scePthreadSetaffinity(thread, cpu) == SCE_OK);
        Require(scePthreadGetaffinity(thread, &threadAffinity) == SCE_OK);
        Require(threadAffinity == cpu);
        Require(ReadCpuset(thread) == cpu);
    }
    release.set_value();
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    KernelCpumask fromThread = 0;
    Require(scePthreadCreate(&thread, &attr, ReportMask, &fromThread, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(fromThread == available);

    std::uint64_t cpuset[8] = {};
    Require(scePthreadCreate(&thread, &attr, ReportCpuset, cpuset, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(cpuset[0] == 0 && cpuset[1] == available && cpuset[2] == ~0ull);
    Require(cpuset[3] == 0 && cpuset[4] == available);
    Require(cpuset[5] == 1 && cpuset[6] == 1 && cpuset[7] == 1);

    KernelCpumask first = available & (~available + 1);
    KernelCpumask last = first;
    for (KernelCpumask cpu = first; cpu != 0; cpu <<= 1) if ((available & cpu) != 0) last = cpu;
    std::promise<void> hold;
    auto held = hold.get_future();
    Pthread bystander = nullptr;
    Require(scePthreadCreate(&bystander, &attr, Worker, &held, nullptr) == SCE_OK);
    SetCpusetResult set{last, {}, {}};
    Require(scePthreadCreate(&thread, &attr, SetCpuset, &set, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(set.results[0] == 0 && set.affinity[0] == last);
    Require(set.results[1] == 1 && set.results[2] == 0);
    Require(set.results[3] == 1 && set.results[4] == 1 && set.results[5] == 1);
    Require(set.results[6] == 1 && set.affinity[1] == last);
    Require(set.results[7] == 0 && set.affinity[2] == last);
    KernelCpumask bystanderAffinity = 0;
    Require(scePthreadGetaffinity(bystander, &bystanderAffinity) == SCE_OK);
    Require(bystanderAffinity == last);
    const std::uint64_t everything[1] = {available};
    Require(cpuset_setaffinity_nid_postfix(3, 2, -1, 8, everything) == 0);
    Require(scePthreadGetaffinity(bystander, &bystanderAffinity) == SCE_OK);
    Require(bystanderAffinity == available);
    hold.set_value();
    Require(scePthreadJoin(bystander, nullptr) == SCE_OK);

    if (first != available) {
        const std::uint64_t single[1] = {last};
        const bool changed = cpuset_setaffinity_nid_postfix(3, 2, -1, 8, single) == 0;
        bool restored = false;
        bool created = false;
        bool joined = false;
        bool explicitSet = false;
        bool explicitCreated = false;
        bool explicitJoined = false;
        std::uint64_t inherited[8]{};
        std::uint64_t explicitAffinity[8]{};
        if (changed) {
            RestoreProcessMask restore{available, &restored};
            Pthread child = nullptr;
            created = scePthreadCreate(&child, &attr, ReportCpuset, inherited, nullptr) == SCE_OK;
            joined = created && scePthreadJoin(child, nullptr) == SCE_OK;
            explicitSet = scePthreadAttrSetaffinity(&attr, available) == SCE_OK;
            child = nullptr;
            explicitCreated = explicitSet && scePthreadCreate(&child, &attr, ReportCpuset, explicitAffinity, nullptr) == SCE_OK;
            explicitJoined = explicitCreated && scePthreadJoin(child, nullptr) == SCE_OK;
        }
        Require(changed && restored && created && joined && inherited[0] == 0 && inherited[1] == last);
        Require(explicitSet && explicitCreated && explicitJoined && explicitAffinity[0] == 0 && explicitAffinity[1] == available);
    }

    std::uint64_t processSet[4] = {~0ull, ~0ull, ~0ull, ~0ull};
    Require(cpuset_getaffinity_nid_postfix(3, 2, -1, sizeof(processSet), processSet) == 0);
    Require(processSet[0] == available && processSet[1] == 0 && processSet[2] == 0 && processSet[3] == 0);
    processSet[0] = 0;
    Require(cpuset_getaffinity_nid_postfix(3, 2, getpid_nid_postfix(), 8, processSet) == 0 && processSet[0] == available);
    Require(cpuset_getaffinity_nid_postfix(3, 2, -1, 4, processSet) == -1 && *__error_nid_postfix() == GUEST_ERANGE);
    Require(cpuset_getaffinity_nid_postfix(3, 2, -1, 8, nullptr) == -1 && *__error_nid_postfix() == GUEST_EFAULT);
    if (first != last) {
        std::promise<void> park;
        auto parked = park.get_future();
        Pthread pinned = nullptr;
        Require(scePthreadCreate(&pinned, &attr, Worker, &parked, nullptr) == SCE_OK);
        Require(scePthreadSetaffinity(pinned, first) == SCE_OK);
        Require(scePthreadSetaffinity(scePthreadSelf(), last) == SCE_OK);
        std::uint64_t unionSet[1] = {0};
        const int unionResult = cpuset_getaffinity_nid_postfix(3, 2, -1, 8, unionSet);
        std::uint64_t ownSet[1] = {0};
        const int ownResult = cpuset_getaffinity_nid_postfix(3, 1, -1, 8, ownSet);
        Require(scePthreadSetaffinity(scePthreadSelf(), available) == SCE_OK);
        park.set_value();
        Require(scePthreadJoin(pinned, nullptr) == SCE_OK);
        Require(unionResult == 0 && unionSet[0] == (first | last));
        Require(ownResult == 0 && ownSet[0] == last);
    }
    std::uint64_t request[GUEST_CPUSET_BYTES / sizeof(std::uint64_t)] = {first, 0, 0, 0};
    Require(pthread_setaffinity_np_nid_postfix(pthread_self_nid_postfix(), GUEST_CPUSET_BYTES, request) == 0);
    Require(ReadCpuset(pthread_self_nid_postfix()) == first);
    std::promise<void> settle;
    auto settled = settle.get_future();
    Pthread target = nullptr;
    Require(scePthreadCreate(&target, &attr, Worker, &settled, nullptr) == SCE_OK);
    request[0] = last;
    Require(pthread_setaffinity_np_nid_postfix(target, GUEST_CPUSET_BYTES, request) == 0);
    Require(ReadCpuset(target) == last && ReadCpuset(pthread_self_nid_postfix()) == first);
    request[0] = available | (1ull << 40);
    request[3] = 1ull << 63;
    Require(pthread_setaffinity_np_nid_postfix(target, GUEST_CPUSET_BYTES, request) == 0);
    Require(ReadCpuset(target) == available);
    const std::uint64_t outside[GUEST_CPUSET_BYTES / sizeof(std::uint64_t)] = {1ull << 40, 1, 0, 0};
    Require(pthread_setaffinity_np_nid_postfix(target, GUEST_CPUSET_BYTES, outside) == GUEST_EDEADLK);
    Require(pthread_setaffinity_np_nid_postfix(pthread_self_nid_postfix(), GUEST_CPUSET_BYTES, outside) == GUEST_EDEADLK);
    Require(ReadCpuset(target) == available && ReadCpuset(pthread_self_nid_postfix()) == first);
    for (const std::size_t size : {std::size_t{0}, std::size_t{8}, std::size_t{31}, std::size_t{33}, std::size_t{64}}) {
        Require(pthread_setaffinity_np_nid_postfix(pthread_self_nid_postfix(), size, request) == GUEST_ERANGE);
        Require(pthread_setaffinity_np_nid_postfix(target, size, request) == GUEST_ERANGE);
    }
    Require(pthread_setaffinity_np_nid_postfix(target, GUEST_CPUSET_BYTES, nullptr) == GUEST_EFAULT);
    Require(pthread_setaffinity_np_nid_postfix(nullptr, GUEST_CPUSET_BYTES, request) == GUEST_EINVAL);
    std::uint64_t unknownThread[8]{};
    Require(pthread_setaffinity_np_nid_postfix(reinterpret_cast<Pthread>(unknownThread), GUEST_CPUSET_BYTES, request) == GUEST_ESRCH);
    Require(pthread_setaffinity_np_nid_postfix(reinterpret_cast<Pthread>(unknownThread), 8, request) == GUEST_ESRCH);
    settle.set_value();
    Require(scePthreadJoin(target, nullptr) == SCE_OK);
    Require(pthread_setaffinity_np_nid_postfix(target, GUEST_CPUSET_BYTES, request) == GUEST_ESRCH);
    request[0] = available;
    request[3] = 0;
    Require(pthread_setaffinity_np_nid_postfix(pthread_self_nid_postfix(), GUEST_CPUSET_BYTES, request) == 0);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    Require(ReadCpuset(pthread_self_nid_postfix()) == available);
    std::uint8_t rawSet[GUEST_CPUSET_BYTES]{};
    for (const std::size_t size : {std::size_t{0}, std::size_t{8}, std::size_t{31}, std::size_t{33}, std::size_t{64}})
        Require(pthread_getaffinity_np_nid_postfix(pthread_self_nid_postfix(), size, rawSet) == GUEST_ERANGE);
    Require(pthread_getaffinity_np_nid_postfix(nullptr, GUEST_CPUSET_BYTES, rawSet) == GUEST_EINVAL);
    Require(pthread_getaffinity_np_nid_postfix(nullptr, 0, rawSet) == GUEST_EINVAL);
    Require(pthread_getaffinity_np_nid_postfix(pthread_self_nid_postfix(), GUEST_CPUSET_BYTES, nullptr) == GUEST_EFAULT);
    Require(cpuset_getaffinity_nid_postfix(3, 1, scePthreadGetthreadid(), 8, processSet) == 0 && processSet[0] == available);
    Require(cpuset_getaffinity_nid_postfix(3, 1, 999999, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(cpuset_getaffinity_nid_postfix(3, 2, 999999, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(cpuset_getaffinity_nid_postfix(99, 1, -1, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    Require(cpuset_getaffinity_nid_postfix(99, 1, 999999, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(cpuset_getaffinity_nid_postfix(99, 2, 999999, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(cpuset_getaffinity_nid_postfix(3, 99, -1, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    Require(cpuset_setaffinity_nid_postfix(3, 1, scePthreadGetthreadid(), 8, processSet) == 0);
    Require(cpuset_setaffinity_nid_postfix(3, 1, 999999, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(cpuset_setaffinity_nid_postfix(3, 2, 999999, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(cpuset_setaffinity_nid_postfix(99, 1, -1, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    Require(cpuset_setaffinity_nid_postfix(99, 1, 999999, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(cpuset_setaffinity_nid_postfix(99, 2, 999999, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(cpuset_setaffinity_nid_postfix(3, 99, -1, 8, processSet) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    const auto throws = [](auto&& call) {
        try {
            call();
            return false;
        } catch (const std::runtime_error&) {
            return true;
        }
    };
    Require(throws([&] { cpuset_getaffinity_nid_postfix(1, 1, -1, 8, processSet); }));
    Require(throws([&] { cpuset_getaffinity_nid_postfix(2, 1, -1, 8, processSet); }));
    Require(throws([&] { cpuset_getaffinity_nid_postfix(3, 3, -1, 8, processSet); }));
    Require(throws([&] { cpuset_getaffinity_nid_postfix(3, 4, -1, 8, processSet); }));
    Require(throws([&] { cpuset_setaffinity_nid_postfix(1, 1, -1, 8, processSet); }));
    Require(throws([&] { cpuset_setaffinity_nid_postfix(2, 1, -1, 8, processSet); }));
    Require(throws([&] { cpuset_setaffinity_nid_postfix(3, 3, -1, 8, processSet); }));
    Require(throws([&] { cpuset_setaffinity_nid_postfix(3, 4, -1, 8, processSet); }));
    std::promise<void> holdOther;
    auto heldOther = holdOther.get_future();
    RemoteThread otherArg;
    otherArg.release = &heldOther;
    Pthread otherThread = nullptr;
    Require(scePthreadCreate(&otherThread, nullptr, RecordThreadId, &otherArg, nullptr) == SCE_OK);
    while (otherArg.tid.load(std::memory_order_acquire) == 0) std::this_thread::yield();
    const int otherTid = otherArg.tid.load(std::memory_order_acquire);
    std::uint64_t remoteSet[5] = {~0ull, ~0ull, ~0ull, ~0ull, ~0ull};
    Require(cpuset_getaffinity_nid_postfix(3, 1, otherTid, GUEST_CPUSET_BYTES, remoteSet) == 0);
    Require(remoteSet[0] == available && remoteSet[1] == 0 && remoteSet[2] == 0 && remoteSet[3] == 0 && remoteSet[4] == ~0ull);
    const std::uint64_t remoteMask[4] = {last | (1ull << 40), 0, 0, 0};
    Require(cpuset_setaffinity_nid_postfix(3, 1, otherTid, sizeof(remoteMask), remoteMask) == 0);
    Require(ReadCpuset(otherThread) == last);
    Require(ReadCpuset(pthread_self_nid_postfix()) == available);
    Require(cpuset_getaffinity_nid_postfix(3, 1, otherTid, 8, remoteSet) == 0 && remoteSet[0] == last);
    const std::uint64_t invalidRemote[4] = {first, 0, 0, 1};
    Require(cpuset_setaffinity_nid_postfix(3, 1, otherTid, sizeof(invalidRemote), invalidRemote) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    const std::uint64_t unavailableRemote[1] = {1ull << 40};
    Require(cpuset_setaffinity_nid_postfix(3, 1, otherTid, sizeof(unavailableRemote), unavailableRemote) == -1 && *__error_nid_postfix() == GUEST_EDEADLK);
    Require(cpuset_setaffinity_nid_postfix(3, 1, otherTid, 8, nullptr) == -1 && *__error_nid_postfix() == GUEST_EFAULT);
    Require(cpuset_setaffinity_nid_postfix(3, 1, otherTid, 4, remoteMask) == -1 && *__error_nid_postfix() == GUEST_ERANGE);
    Require(ReadCpuset(otherThread) == last);
    Require(cpuset_getaffinity_nid_postfix(99, 1, otherTid, 8, remoteSet) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    Require(cpuset_setaffinity_nid_postfix(99, 1, otherTid, 8, remoteMask) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    holdOther.set_value();
    Require(scePthreadJoin(otherThread, nullptr) == SCE_OK);
    Require(otherArg.observed == last && otherArg.inherited == last);
    Require(otherArg.cpu >= 0 && otherArg.cpu < PS5_LOGICAL_CPUS && (last & (KernelCpumask{1} << otherArg.cpu)) != 0);
    remoteSet[0] = ~0ull;
    Require(cpuset_getaffinity_nid_postfix(3, 1, otherTid, 8, remoteSet) == -1 && *__error_nid_postfix() == GUEST_ESRCH && remoteSet[0] == ~0ull);
    Require(cpuset_setaffinity_nid_postfix(3, 1, otherTid, 8, remoteMask) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
}
