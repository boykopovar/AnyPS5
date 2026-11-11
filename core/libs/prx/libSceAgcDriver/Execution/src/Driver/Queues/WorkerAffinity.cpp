#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libSceAgcDriver/Execution/include/TracyZone.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/general/ThreadName.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <vector>

#ifdef _WIN32
#include <tlhelp32.h>
#else
#include <pthread.h>
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

void NameDriverThread(const char* name) {
#ifdef _WIN32
    const HRESULT nameResult = ThreadName::SetCurrent(name);
    if (FAILED(nameResult)) std::fprintf(stderr, "[thread-name] could not name %s (0x%08lx)\n", name, static_cast<unsigned long>(nameResult));
#elif defined(__APPLE__)
    const int nameResult = pthread_setname_np(name);
    if (nameResult != 0) std::fprintf(stderr, "[thread-name] could not name %s (%s)\n", name, std::strerror(nameResult));
#else
    const int nameResult = pthread_setname_np(pthread_self(), name);
    if (nameResult != 0) std::fprintf(stderr, "[thread-name] could not name %s (%s)\n", name, std::strerror(nameResult));
#endif
}

void PinWorkerThread(const char* role) {
    TracyZone::NameThread(role);
    const auto mask = WorkerAffinityMask();
    if (mask == 0) return;
    static std::once_flag summary;
    std::call_once(summary, [mask] {
        const auto& layout = CpuTopology::Get();
        std::fprintf(stderr, "[affinity] queue workers and presenter -> 0x%llx (hybrid=%d efficient=0x%llx performant=0x%llx process=0x%llx)\n", static_cast<unsigned long long>(mask), layout.hybrid ? 1 : 0, static_cast<unsigned long long>(layout.efficient), static_cast<unsigned long long>(layout.performant), static_cast<unsigned long long>(layout.process));
    });
    CpuTopology::PinTraced(role, nullptr, mask);
}

#ifdef _WIN32
namespace {

struct CoreSwitches {
    bool committer = false;
    bool finisher = false;
    std::optional<unsigned> logical;
};

struct Reservation {
    CpuTopology::CpuSet set;
    DWORD thread = 0;
};

constexpr std::size_t CommitterSlot = 0;
constexpr std::size_t FinisherSlot = 1;

std::mutex reservationMutex;
std::array<std::optional<Reservation>, 2> reservations;

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

bool HoldsReservation(DWORD thread) {
    for (const auto& held : reservations) {
        if (held.has_value() && held->thread == thread) return true;
    }
    return false;
}

void Reserve(std::size_t slot, const char* role, std::optional<unsigned> logical) {
    using SetCpuSets = BOOL(WINAPI*)(HANDLE, const ULONG*, ULONG);
    static const auto setDefault = ThreadName::Resolve<SetCpuSets>("SetProcessDefaultCpuSets");
    static const auto setSelected = ThreadName::Resolve<SetCpuSets>("SetThreadSelectedCpuSets");
    if (setDefault == nullptr || setSelected == nullptr) return;
    std::lock_guard lock(reservationMutex);
    auto& own = reservations[slot];
    if (own.has_value()) {
        own->thread = GetCurrentThreadId();
        const ULONG id = own->set.id;
        const bool selected = setSelected(GetCurrentThread(), &id, 1) != 0;
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
    own = Reservation{reservation->chosen, GetCurrentThreadId()};
    const std::vector<ULONG> others(reservation->others.begin(), reservation->others.end());
    const ULONG id = reservation->chosen.id;
    const bool process = setDefault(GetCurrentProcess(), others.data(), static_cast<ULONG>(others.size())) != 0;
    const bool thread = setSelected(GetCurrentThread(), &id, 1) != 0;
    std::size_t moved = 0;
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
    std::fprintf(stderr, "[affinity] %s tid=%lu -> core %u logical %u reserved (process default %s, thread %s, %zu running threads moved to the %zu other CPU sets)\n", role, CpuTopology::ThreadId(nullptr), static_cast<unsigned>(reservation->chosen.core), static_cast<unsigned>(reservation->chosen.logical), process ? "set" : "failed", thread ? "set" : "failed", moved, others.size());
}

}
#endif

void ReserveCommitterCore() {
#ifdef _WIN32
    if (Switches().committer) Reserve(CommitterSlot, "draw committer", Switches().logical);
#endif
}

void ReserveFinisherCore() {
#ifdef _WIN32
    if (Switches().finisher) Reserve(FinisherSlot, "draw finisher", std::nullopt);
#endif
}

}
