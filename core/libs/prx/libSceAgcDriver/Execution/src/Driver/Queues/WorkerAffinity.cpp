#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libSceAgcDriver/Execution/include/TracyZone.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/general/ThreadName.hpp"
#include <array>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <vector>

#ifdef _WIN32
#include <tlhelp32.h>
#elif defined(__linux__)
#include <dirent.h>
#include <string>
#include <sched.h>
#include <unistd.h>
#endif

namespace AgcDriver::DriverDetail {

std::uint64_t WorkerAffinityMask() {
    static const std::uint64_t mask = [] {
        if (std::getenv("APS5_NO_WORKER_AFFINITY") != nullptr) return std::uint64_t{0};
        const auto requested = CpuTopology::MaskFromEnvironment("APS5_WORKER_AFFINITY_MASK");
        if (requested != 0) return requested;
#ifndef __linux__
        if (std::getenv("APS5_WORKER_AFFINITY") == nullptr) return std::uint64_t{0};
#endif
        const auto& layout = CpuTopology::Get();
        return layout.hybrid ? layout.performant : std::uint64_t{0};
    }();
    return mask;
}

#ifdef __linux__
namespace {
void NoteWorkerThread();
}
#endif

void PinWorkerThread(const char* role) {
    TracyZone::NameThread(role);
#ifdef _WIN32
    const HRESULT nameResult = ThreadName::SetCurrent(role);
    if (FAILED(nameResult)) std::fprintf(stderr, "[thread-name] could not name %s (0x%08lx)\n", role, static_cast<unsigned long>(nameResult));
#endif
#ifdef __linux__
    NoteWorkerThread();
#endif
    const auto mask = WorkerAffinityMask();
    if (mask == 0) return;
    static std::once_flag summary;
    std::call_once(summary, [mask] {
        const auto& layout = CpuTopology::Get();
        std::fprintf(stderr, "[affinity] queue workers and presenter -> 0x%llx (hybrid=%d efficient=0x%llx performant=0x%llx process=0x%llx)\n", static_cast<unsigned long long>(mask), layout.hybrid ? 1 : 0, static_cast<unsigned long long>(layout.efficient), static_cast<unsigned long long>(layout.performant), static_cast<unsigned long long>(layout.process));
    });
    CpuTopology::PinTraced(role, nullptr, mask);
}

#if defined(_WIN32) || defined(__linux__)
namespace {

struct CoreSwitches {
    bool committer = false;
    bool finisher = false;
    std::optional<unsigned> logical;
};

struct Reservation {
    CpuTopology::CpuSet set;
    unsigned long thread = 0;
};

constexpr std::size_t CommitterSlot = 0;
constexpr std::size_t FinisherSlot = 1;

std::mutex reservationMutex;
std::array<std::optional<Reservation>, 2> reservations;
#ifdef __linux__
std::vector<unsigned long> workerThreads;
std::vector<std::uint32_t> pipelineCpus;
#endif

const CoreSwitches& Switches() {
    static const CoreSwitches switches = [] {
        CoreSwitches result;
        if (std::getenv("APS5_NO_COMMITTER_CORE") != nullptr) return result;
        const char* text = std::getenv("APS5_COMMITTER_CPU");
        if (text != nullptr && *text != '\0') result.logical = static_cast<unsigned>(std::strtoul(text, nullptr, 10));
        const bool workersPinned = WorkerAffinityMask() != 0;
        result.committer = result.logical.has_value() || !workersPinned;
        result.finisher = !workersPinned && std::getenv("APS5_NO_FINISHER_CORE") == nullptr;
        return result;
    }();
    return switches;
}

#ifdef _WIN32
std::vector<CpuTopology::CpuSet> SystemCpuSets() {
    using GetInformation = BOOL(WINAPI*)(PSYSTEM_CPU_SET_INFORMATION, ULONG, PULONG, HANDLE, ULONG);
    static const auto getInformation = ThreadName::Resolve<GetInformation>("GetSystemCpuSetInformation");
    std::vector<CpuTopology::CpuSet> sets;
    if (getInformation == nullptr) return sets;
    ULONG bytes = 0;
    getInformation(nullptr, 0, &bytes, GetCurrentProcess(), 0);
    std::vector<unsigned char> buffer(bytes);
    if (bytes == 0 || !getInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data()), bytes, &bytes, GetCurrentProcess(), 0)) return sets;
    const auto process = CpuTopology::Get().process;
    for (ULONG offset = 0; offset + sizeof(SYSTEM_CPU_SET_INFORMATION) <= bytes;) {
        const auto* entry = reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(buffer.data() + offset);
        if (entry->Size == 0 || offset + entry->Size > bytes) break;
        offset += entry->Size;
        if (entry->Type != CpuSetInformation) continue;
        const auto& set = entry->CpuSet;
        if (set.Group == 0 && process != 0 && (set.LogicalProcessorIndex >= 64 || ((process >> set.LogicalProcessorIndex) & 1u) == 0)) continue;
        sets.push_back({set.Id, set.Group, set.LogicalProcessorIndex, set.CoreIndex, set.EfficiencyClass, set.LastLevelCacheIndex, set.Parked != 0});
    }
    return sets;
}
#else
std::vector<CpuTopology::CpuSet> SystemCpuSets() {
    const auto& layout = CpuTopology::Get();
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0 || CPU_COUNT(&allowed) > __builtin_popcountll(layout.process)) return {};
    return CpuTopology::SysfsCpuSets("/sys/devices/system/cpu", layout.process, layout.hybrid ? layout.efficient : 0);
}

unsigned long CurrentThread() {
    return static_cast<unsigned long>(gettid());
}

bool PinTo(pid_t thread, const std::vector<std::uint32_t>& cpus) {
    cpu_set_t set;
    CPU_ZERO(&set);
    for (const auto cpu : cpus) CPU_SET(cpu, &set);
    return sched_setaffinity(thread, sizeof(set), &set) == 0;
}

bool OwnThread(unsigned long tid) {
    return access(("/proc/self/task/" + std::to_string(tid)).c_str(), F_OK) == 0;
}

void NoteWorkerThread() {
    std::lock_guard lock(reservationMutex);
    workerThreads.push_back(CurrentThread());
    if (!pipelineCpus.empty()) static_cast<void>(PinTo(0, pipelineCpus));
}
#endif

bool HoldsReservation(unsigned long thread) {
    for (const auto& held : reservations) {
        if (held.has_value() && held->thread == thread) return true;
    }
    return false;
}

void Reserve(std::size_t slot, const char* role, std::optional<unsigned> logical) {
#ifdef _WIN32
    using SetCpuSets = BOOL(WINAPI*)(HANDLE, const ULONG*, ULONG);
    static const auto setDefault = ThreadName::Resolve<SetCpuSets>("SetProcessDefaultCpuSets");
    static const auto setSelected = ThreadName::Resolve<SetCpuSets>("SetThreadSelectedCpuSets");
    if (setDefault == nullptr || setSelected == nullptr) return;
#endif
    std::lock_guard lock(reservationMutex);
    auto& own = reservations[slot];
    if (own.has_value()) {
#ifdef _WIN32
        own->thread = GetCurrentThreadId();
        const ULONG id = own->set.id;
        const bool selected = setSelected(GetCurrentThread(), &id, 1) != 0;
#else
        own->thread = CurrentThread();
        const bool selected = PinTo(0, {own->set.id});
#endif
        std::fprintf(stderr, "[affinity] %s tid=%lu -> core %u logical %u, kept from its previous thread (thread %s)\n", role, CpuTopology::ThreadId(nullptr), static_cast<unsigned>(own->set.core), static_cast<unsigned>(own->set.logical), selected ? "set" : "failed");
        return;
    }
    const auto sets = SystemCpuSets();
    std::vector<CpuTopology::CpuSet> taken;
    for (const auto& held : reservations) {
        if (held.has_value()) taken.push_back(held->set);
    }
    if (slot == FinisherSlot && !reservations[CommitterSlot].has_value() && Switches().committer) {
        if (const auto ahead = CpuTopology::ChooseReservedCore(sets, taken, Switches().logical)) taken.push_back(ahead->chosen);
    }
    const auto reservation = CpuTopology::ChooseReservedCore(sets, taken, logical);
    if (!reservation.has_value()) {
        if (logical.has_value()) std::fprintf(stderr, "[affinity] %s: APS5_COMMITTER_CPU=%u names no free logical processor of the process, no core reserved\n", role, *logical);
        return;
    }
    std::size_t moved = 0;
#ifdef _WIN32
    own = Reservation{reservation->chosen, GetCurrentThreadId()};
    const std::vector<ULONG> others(reservation->others.begin(), reservation->others.end());
    const ULONG id = reservation->chosen.id;
    const bool process = setDefault(GetCurrentProcess(), others.data(), static_cast<ULONG>(others.size())) != 0;
    const bool thread = setSelected(GetCurrentThread(), &id, 1) != 0;
    if (const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); snapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        const DWORD processId = GetCurrentProcessId();
        for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID != processId || HoldsReservation(entry.th32ThreadID)) continue;
            const HANDLE other = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
            if (other == nullptr) continue;
            if (setSelected(other, others.data(), static_cast<ULONG>(others.size())) != 0) ++moved;
            CloseHandle(other);
        }
        CloseHandle(snapshot);
    }
    const char* processState = process ? "set" : "failed";
#else
    own = Reservation{reservation->chosen, CurrentThread()};
    const bool thread = PinTo(0, {reservation->chosen.id});
    if (DIR* tasks = opendir("/proc/self/task"); tasks != nullptr) {
        while (const dirent* entry = readdir(tasks)) {
            char* end = nullptr;
            const unsigned long tid = std::strtoul(entry->d_name, &end, 10);
            if (end == entry->d_name || *end != '\0' || HoldsReservation(tid)) continue;
            if (PinTo(static_cast<pid_t>(tid), reservation->others)) ++moved;
        }
        closedir(tasks);
    }
    const char* processState = "set for helper threads";
    std::vector<CpuTopology::CpuSet> held;
    for (const auto& reserved : reservations) {
        if (reserved.has_value()) held.push_back(reserved->set);
    }
    std::uint64_t unreserved = 0;
    for (const auto cpu : reservation->others) unreserved |= std::uint64_t{1} << cpu;
    CpuTopology::UnreservedMask().store(unreserved, std::memory_order_release);
    pipelineCpus = WorkerAffinityMask() != 0 ? std::vector<std::uint32_t>{} : CpuTopology::PipelineCacheSet(sets, held);
    if (!pipelineCpus.empty()) {
        std::size_t kept = 0;
        for (const auto worker : workerThreads) {
            if (!OwnThread(worker)) continue;
            kept += PinTo(static_cast<pid_t>(worker), pipelineCpus) ? 1u : 0u;
        }
        std::fprintf(stderr, "[affinity] %zu queue workers kept on the %zu other CPUs of the reserved cores' last-level cache\n", kept, pipelineCpus.size());
    }
#endif
    std::fprintf(stderr, "[affinity] %s tid=%lu -> core %u logical %u reserved (process default %s, thread %s, %zu running threads moved to the %zu other CPU sets)\n", role, CpuTopology::ThreadId(nullptr), static_cast<unsigned>(reservation->chosen.core), static_cast<unsigned>(reservation->chosen.logical), processState, thread ? "set" : "failed", moved, reservation->others.size());
}


}
#endif

void ReserveCommitterCore() {
#if defined(_WIN32) || defined(__linux__)
    if (Switches().committer) Reserve(CommitterSlot, "draw committer", Switches().logical);
#endif
}

void ReserveFinisherCore() {
#if defined(_WIN32) || defined(__linux__)
    if (Switches().finisher) Reserve(FinisherSlot, "draw finisher", std::nullopt);
#endif
}

}
