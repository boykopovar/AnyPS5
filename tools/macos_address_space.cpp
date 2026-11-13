#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <libproc.h>
#include <unistd.h>

namespace {

constexpr std::uintptr_t ArenaStart = 0x0000000200000000ull;
constexpr std::uintptr_t SystemReservedStart = 0x00000007FFFFC000ull;
constexpr std::uintptr_t SystemReservedEnd = 0x0000001000000000ull;
constexpr std::uintptr_t ApplicationAreaEnd = 0x000000FC00000000ull;

const char* KernName(kern_return_t result) {
    switch (result) {
    case KERN_SUCCESS: return "SUCCESS";
    case KERN_INVALID_ADDRESS: return "INVALID_ADDRESS";
    case KERN_PROTECTION_FAILURE: return "PROTECTION_FAILURE";
    case KERN_NO_SPACE: return "NO_SPACE";
    case KERN_FAILURE: return "FAILURE";
    default: return "OTHER";
    }
}

bool ReserveFixed(std::uintptr_t start, std::uintptr_t bytes, std::vector<std::pair<std::uintptr_t, std::uintptr_t>>& kept) {
    mach_vm_address_t address = start;
    const kern_return_t allocated = mach_vm_allocate(mach_task_self(), &address, bytes, VM_FLAGS_FIXED);
    if (allocated != KERN_SUCCESS || address != start) {
        if (allocated == KERN_SUCCESS) mach_vm_deallocate(mach_task_self(), address, bytes);
        return false;
    }
    if (mach_vm_protect(mach_task_self(), start, bytes, FALSE, VM_PROT_NONE) != KERN_SUCCESS) {
        mach_vm_deallocate(mach_task_self(), start, bytes);
        return false;
    }
    kept.emplace_back(start, bytes);
    return true;
}

void ReleaseAll(const std::vector<std::pair<std::uintptr_t, std::uintptr_t>>& kept) {
    for (const auto& [start, bytes] : kept) mach_vm_deallocate(mach_task_self(), start, bytes);
}

void Probe(std::uintptr_t address, std::uintptr_t bytes) {
    mach_vm_address_t placed = address;
    const kern_return_t result = mach_vm_allocate(mach_task_self(), &placed, bytes, VM_FLAGS_FIXED);
    const bool exact = result == KERN_SUCCESS && placed == address;
    std::printf("probe 0x%llx +0x%llx -> %s", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), exact ? "OK" : KernName(result));
    if (result == KERN_SUCCESS && placed != address) std::printf(" moved to 0x%llx", static_cast<unsigned long long>(placed));
    std::printf("\n");
    if (result == KERN_SUCCESS) mach_vm_deallocate(mach_task_self(), placed, bytes);
}

void TryReserve(std::vector<std::pair<std::uintptr_t, std::uintptr_t>>& kept, std::uintptr_t from, std::uintptr_t to, std::uintptr_t& reservedBytes) {
    const auto page = static_cast<std::uintptr_t>(vm_page_size);
    if (page == 0 || from >= to) return;
    constexpr std::uintptr_t sizes[] = {std::uintptr_t{1} << 30, std::uintptr_t{64} << 20, std::uintptr_t{4} << 20, std::uintptr_t{256} << 10};
    std::uintptr_t cursor = from;
    int pageFailures = 0;
    while (cursor < to) {
        bool placed = false;
        for (const auto size : sizes) {
            if (size > to - cursor || (cursor & (page - 1)) != 0) continue;
            if (!ReserveFixed(cursor, size, kept)) continue;
            reservedBytes += size;
            cursor += size;
            placed = true;
            pageFailures = 0;
            break;
        }
        if (placed) continue;
        if ((cursor & (page - 1)) == 0 && page <= to - cursor && ReserveFixed(cursor, page, kept)) {
            reservedBytes += page;
            cursor += page;
            pageFailures = 0;
            continue;
        }
        if (++pageFailures >= 4) {
            std::printf("stop  0x%llx..0x%llx after four page failures\n", static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(to));
            return;
        }
        const auto skip = page < to - cursor ? page : to - cursor;
        cursor += skip;
    }
}

}

int main() {
    std::printf("arch %s\n",
#if defined(__x86_64__)
        "x86_64"
#elif defined(__aarch64__)
        "arm64"
#else
        "other"
#endif
    );
    std::printf("page 0x%llx\n", static_cast<unsigned long long>(vm_page_size));
    std::printf("arena 0x%llx..0x%llx system-reserved 0x%llx..0x%llx\n",
        static_cast<unsigned long long>(ArenaStart), static_cast<unsigned long long>(ApplicationAreaEnd),
        static_cast<unsigned long long>(SystemReservedStart), static_cast<unsigned long long>(SystemReservedEnd));

    constexpr std::uintptr_t ImageBase = 0x0000000140000000ull;
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> host;
    mach_vm_address_t address = ImageBase;
    int seen = 0;
    int printed = 0;
    while (address < ApplicationAreaEnd) {
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        const kern_return_t result = mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object);
        if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
        if (result != KERN_SUCCESS || size == 0 || address >= ApplicationAreaEnd) break;
        const auto begin = address < ArenaStart ? ArenaStart : address;
        const auto end = address + size > ApplicationAreaEnd ? ApplicationAreaEnd : address + size;
        const bool overlapsArena = address >= ArenaStart || size > ArenaStart - address;
        if (overlapsArena && begin < end)
            host.emplace_back(static_cast<std::uintptr_t>(begin), static_cast<std::uintptr_t>(end));
        ++seen;
        if (printed < 32) {
            char path[PROC_PIDPATHINFO_MAXSIZE] = {};
            const int length = proc_regionfilename(getpid(), address, path, sizeof(path));
            std::printf("host  0x%llx..0x%llx prot 0x%x %s\n", static_cast<unsigned long long>(address), static_cast<unsigned long long>(address + size), info.protection, length > 0 ? path : "");
            ++printed;
        }
        if (address > std::numeric_limits<mach_vm_address_t>::max() - size) break;
        address += size;
    }
    if (seen > printed) std::printf("host  ... %d regions, printed %d\n", seen, printed);

    const std::uintptr_t probes[] = {ImageBase, ArenaStart, SystemReservedEnd, 0x0000001000000000ull, 0x0000008000000000ull, 0x000000F000000000ull};
    for (const auto probe : probes) {
        Probe(probe, static_cast<std::uintptr_t>(vm_page_size));
        Probe(probe, std::uintptr_t{2} << 20);
        Probe(probe, std::uintptr_t{1} << 30);
    }

    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> used = host;
    used.emplace_back(SystemReservedStart, SystemReservedEnd);
    std::sort(used.begin(), used.end());
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> kept;
    std::uintptr_t reservedBytes = 0;
    std::uintptr_t cursor = ArenaStart;
    for (const auto& [usedStart, usedEnd] : used) {
        if (usedStart > ApplicationAreaEnd) break;
        if (usedStart > cursor) TryReserve(kept, cursor, usedStart < ApplicationAreaEnd ? usedStart : ApplicationAreaEnd, reservedBytes);
        if (usedEnd > cursor) cursor = usedEnd;
        if (cursor >= ApplicationAreaEnd) break;
    }
    if (cursor < ApplicationAreaEnd) TryReserve(kept, cursor, ApplicationAreaEnd, reservedBytes);
    std::printf("reserved 0x%llx bytes across %zu mappings\n", static_cast<unsigned long long>(reservedBytes), kept.size());
    ReleaseAll(kept);
    std::printf("released\n");
    return 0;
}
