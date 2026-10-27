#define NOMINMAX
#include "prx/libc/include/WindowsMappings.hpp"
#include <array>
#include <atomic>
#include <iostream>
#include <set>
#include <string>
#include <thread>

namespace {
using Mappings = GuestArena::WindowsMappings;
using Collection = Mappings::Collection;
constexpr std::size_t Small = 4096;
constexpr std::size_t Bytes = 65536;
std::atomic<unsigned> faults{0};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

LONG CALLBACK HandleWrite(EXCEPTION_POINTERS* exception) {
    const auto* record = exception->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2 || record->ExceptionInformation[0] != 1) return EXCEPTION_CONTINUE_SEARCH;
    if (!Mappings::Get().HandleWrite(record->ExceptionInformation[1])) return EXCEPTION_CONTINUE_SEARCH;
    ++faults;
    return EXCEPTION_CONTINUE_EXECUTION;
}

struct Region {
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, Bytes, nullptr);
    unsigned char* first = static_cast<unsigned char*>(Mappings::Get().Reserve(nullptr, Bytes));
    unsigned char* alias = static_cast<unsigned char*>(Mappings::Get().Reserve(nullptr, Bytes));

    Region() {
        Require(section && first && alias, "cannot allocate shared test memory");
        Mappings::Get().Map(first, Bytes, section, 0, PAGE_READWRITE);
        Mappings::Get().Map(alias, Bytes, section, 0, PAGE_READWRITE);
    }
    ~Region() {
        Mappings::Get().Reset(first, Bytes);
        Mappings::Get().Reset(alias, Bytes);
        VirtualFree(first, 0, MEM_RELEASE);
        VirtualFree(alias, 0, MEM_RELEASE);
        CloseHandle(section);
    }
    void Write(std::size_t page, unsigned char value) {
        static_cast<volatile unsigned char*>(first)[page * Small] = value;
    }
    std::set<std::size_t> Collect(Collection mode, std::size_t begin = 0, std::size_t count = 16, bool clear = true, bool useAlias = false, std::size_t capacity = 32) {
        auto* base = useAlias ? alias : first;
        const auto address = reinterpret_cast<std::uintptr_t>(base);
        auto cursor = address + begin * Small;
        const auto end = cursor + count * Small;
        std::set<std::size_t> result;
        while (cursor < end) {
            std::array<void*, 32> pages{};
            auto available = capacity;
            Require(Mappings::Get().Collect(cursor, end - cursor, pages.data(), &available, clear, mode), "shared collection failed");
            for (std::size_t i = 0; i < available; ++i) result.insert((reinterpret_cast<std::uintptr_t>(pages[i]) - address) / Small);
            if (available < capacity) break;
            cursor = reinterpret_cast<std::uintptr_t>(pages[available - 1]) + Small;
        }
        return result;
    }
    DWORD Protection(std::size_t page, bool useAlias = false) {
        MEMORY_BASIC_INFORMATION info{};
        Require(VirtualQuery((useAlias ? alias : first) + page * Small, &info, sizeof(info)) == sizeof(info), "protection query failed");
        return info.Protect;
    }
};

std::set<std::size_t> Pages(std::size_t begin, std::size_t end) {
    std::set<std::size_t> result;
    for (auto page = begin; page < end; ++page) result.insert(page);
    return result;
}

void CheckArmed() {
    Region region;
    Require(region.Collect(Collection::Armed).empty(), "import collected fresh memory");
    const auto before = faults.load();
    region.Write(0, 1);
    region.Write(4, 2);
    Require(faults == before, "import armed fresh memory");
    Require(region.Collect(Collection::All, 0, 1) == Pages(0, 1), "first partial collect failed");
    Require(region.Protection(0) == PAGE_READONLY && region.Protection(3) == PAGE_READWRITE, "partial collect did not protect only its page");
    region.Write(0, 3);
    region.Write(4, 4);
    Require(region.Collect(Collection::Armed) == Pages(0, 1), "armed collect lost a write or consumed fresh pages");
    Require(region.Protection(4) == PAGE_READWRITE, "armed collect protected another fresh view");
    Require(region.Collect(Collection::All) == Pages(1, 16), "ordinary collect lost fresh pages");
    Require(region.Collect(Collection::All).empty(), "cleared memory reports writes");
}

void CheckFresh() {
    Region region;
    Require(region.Collect(Collection::All, 0, 1) == Pages(0, 1), "cannot collect initial page");
    region.Write(0, 7);
    Require(region.Collect(Collection::Fresh) == Pages(1, 16), "fresh collect consumed an existing dirty page");
    Require(region.Collect(Collection::Fresh).empty(), "fresh collect repeated initial writes");
    Require(region.Collect(Collection::Armed) == Pages(0, 1), "fresh collect cleared an existing write");
    region.Write(9, 9);
    Require(region.Collect(Collection::Armed) == Pages(9, 10), "write after activation was missed");
}

void CheckAliasesAndCapacity() {
    Region region;
    Require(region.Collect(Collection::All, 0, 1) == Pages(0, 1), "initial collect failed");
    Require(region.Protection(0, true) == PAGE_READONLY, "alias was not protected");
    Require(region.Collect(Collection::Armed, 0, 16, true, true).empty(), "alias protection was mistaken for collection history");
    Require(region.Collect(Collection::Fresh, 0, 16, true, true, 1) == Pages(0, 16), "small output buffers lost fresh pages");
    Require(region.Collect(Collection::Fresh, 0, 16, true, true).empty(), "small output buffers did not finish fresh collection");
    static_cast<volatile unsigned char*>(region.alias)[0] = 8;
    Require(region.Collect(Collection::Armed) == Pages(0, 1), "alias write was missed or fresh neighbors were consumed");
    Require(region.Collect(Collection::Fresh) == Pages(1, 16), "alias collection changed another view's history");
    Mappings::Get().Reset(region.first, Bytes);
    Mappings::Get().Map(region.first, Bytes, region.section, 0, PAGE_READWRITE);
    Require(region.Collect(Collection::Armed).empty(), "remapped address kept old collection history");
    Require(region.Collect(Collection::Fresh) == Pages(0, 16), "remapped memory was not fresh");
}

void CheckPinsAndHostWrites() {
    Region region;
    const auto address = reinterpret_cast<std::uintptr_t>(region.first);
    Mappings::Get().Pin(address, 16384);
    Require(region.Collect(Collection::Fresh) == Pages(0, 16), "pinned fresh collection failed");
    Require(region.Protection(0) == PAGE_READWRITE, "pinned page was protected");
    Require(region.Collect(Collection::Armed) == Pages(0, 4), "pinned writes were trusted as clean");
    Mappings::Get().Unpin(address, 16384);
    Require(region.Collect(Collection::Armed) == Pages(0, 4), "unpin was not invalidated");
    Require(region.Collect(Collection::Armed).empty(), "unpin did not restore tracking");
    Require(Mappings::Get().BeginHostWrite(address, 16384), "host write refused");
    region.Write(0, 3);
    region.Collect(Collection::Armed);
    region.Write(0, 4);
    Mappings::Get().EndHostWrite(address, 16384);
    Require(region.Collect(Collection::Armed) == Pages(0, 4), "host write completion was missed");
    Require(region.Collect(Collection::Armed).empty(), "completed host write stayed dirty");
}

void CheckFailureAndHoles() {
    Region region;
    const auto address = reinterpret_cast<std::uintptr_t>(region.first);
    DWORD previous;
    Mappings::Get().SetProtection(address + 16384, 16384, PAGE_NOACCESS);
    Require(VirtualProtect(region.first + 16384, 16384, PAGE_NOACCESS, &previous), "cannot protect test page");
    std::array<void*, 32> pages{};
    auto count = pages.size();
    Require(!Mappings::Get().Collect(address, Bytes, pages.data(), &count, true, Collection::Fresh), "fresh inaccessible page accepted");
    Require(region.Protection(0) == PAGE_READWRITE, "failed collect changed an earlier page");
    Mappings::Get().SetProtection(address + 16384, 16384, PAGE_READWRITE);
    Require(VirtualProtect(region.first + 16384, 16384, PAGE_READWRITE, &previous), "cannot restore test page");
    Mappings::Get().Reset(region.first + 16384, 16384);
    Require(region.Collect(Collection::Fresh) == std::set<std::size_t>({0, 1, 2, 3, 8, 9, 10, 11, 12, 13, 14, 15}), "fresh collection did not tolerate an unmapped block neighbor");
    Require(region.Collect(Collection::Fresh).empty(), "holes caused fresh pages to repeat");
}

void CheckPrivate() {
    auto& mappings = Mappings::Get();
    auto* memory = static_cast<unsigned char*>(mappings.Reserve(nullptr, Bytes));
    Require(memory != nullptr, "private reservation failed");
    mappings.Commit(memory, Bytes, PAGE_READWRITE, Bytes, true);
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    std::array<void*, 32> pages{};
    std::size_t count = pages.size();
    mappings.Collect(address, Bytes, pages.data(), &count, true);
    static_cast<volatile unsigned char*>(memory)[Small] = 3;
    count = pages.size();
    Require(mappings.Collect(address, Bytes, pages.data(), &count, true, Collection::Fresh) && count == 0, "fresh collect touched private memory");
    count = pages.size();
    Require(mappings.Collect(address, Bytes, pages.data(), &count, true, Collection::Armed) && count == 1 && pages[0] == memory + Small, "private write watch changed");
    mappings.Reset(memory, Bytes);
    VirtualFree(memory, 0, MEM_RELEASE);
}

MEMORY_BASIC_INFORMATION RegionAt(const void* address) {
    MEMORY_BASIC_INFORMATION info{};
    Require(VirtualQuery(address, &info, sizeof(info)) == sizeof(info), "private commit query failed");
    return info;
}

void CheckPrivateCommitCarvesPlaceholders() {
    auto& mappings = Mappings::Get();
    constexpr std::size_t Page = 16384;
    constexpr std::size_t Pages = 256;
    constexpr std::size_t Span = Pages * Page;
    constexpr std::size_t Reserved = std::size_t{1} << 30;
    auto* holder = static_cast<unsigned char*>(mappings.Reserve(nullptr, Reserved));
    Require(holder != nullptr, "cannot reserve a placeholder for private commits");
    auto* memory = holder + Page;
    const auto page = [&](std::size_t index) { return memory + index * Page; };
    const auto at = [](const unsigned char* pointer) { return reinterpret_cast<std::uintptr_t>(pointer); };
    auto before = Mappings::MemoryQueries();
    auto created = mappings.Commit(memory, Span, PAGE_READWRITE, Page, true);
    const auto queries = Mappings::MemoryQueries() - before;
    Require(queries == 1, ("committing 256 fresh pages queried the address space " + std::to_string(queries) + " times").c_str());
    Require(created.size() == 1 && created[0].first == at(memory) && created[0].second == Span, "a fresh commit did not report its range as one created range");
    for (std::size_t index = 0; index < Pages; ++index) {
        const auto info = RegionAt(page(index));
        Require(info.State == MEM_COMMIT && info.Type == MEM_PRIVATE && info.AllocationBase == page(index) && info.RegionSize == Page && info.Protect == PAGE_READWRITE, "a committed page is not a read-write allocation of its own");
    }
    const auto prefix = RegionAt(holder);
    Require(prefix.State == MEM_RESERVE && prefix.AllocationBase == holder && prefix.RegionSize == Page, "the placeholder before the commit changed");
    const auto suffix = RegionAt(page(Pages));
    Require(suffix.State == MEM_RESERVE && suffix.AllocationBase == page(Pages) && suffix.RegionSize == Reserved - Page - Span, "the placeholder after the commit changed");
    for (std::size_t index = 0; index < Pages; ++index) page(index)[8] = static_cast<unsigned char>(index + 1);
    mappings.Reset(page(11), Page);
    const auto released = RegionAt(page(11));
    Require(released.State == MEM_RESERVE && released.AllocationBase == page(11) && released.RegionSize == Page, "a released page did not become a placeholder of its own");
    before = Mappings::MemoryQueries();
    created = mappings.Commit(page(11), Page, PAGE_READWRITE, Page, true);
    Require(Mappings::MemoryQueries() - before <= 1 && created.size() == 1 && created[0].first == at(page(11)) && created[0].second == Page && page(11)[8] == 0, "recommitting a released page was not one fresh allocation");
    for (std::size_t index = 0; index < Pages; ++index) {
        if (index != 11) Require(page(index)[8] == static_cast<unsigned char>(index + 1), "a release or commit changed a neighbouring page");
    }
    created = mappings.Commit(memory, Span, PAGE_READONLY, Page, true);
    Require(created.empty() && RegionAt(page(0)).Protect == PAGE_READONLY && RegionAt(page(Pages - 1)).Protect == PAGE_READONLY, "a protection change reported fresh memory or missed a page");
    mappings.Commit(memory, Span, PAGE_READWRITE, Page, true);
    std::array<void*, 32> written{};
    std::size_t count = written.size();
    mappings.Collect(at(page(40)), 4 * Page, written.data(), &count, true);
    page(41)[4096 + 3] = 1;
    count = written.size();
    Require(mappings.Collect(at(page(40)), 4 * Page, written.data(), &count, true, Collection::Armed) && count == 1 && written[0] == page(41) + 4096, "a carved page lost its write watch");
    mappings.Reset(page(20), 2 * Page);
    mappings.Reset(page(22), 2 * Page);
    created = mappings.Commit(page(20), 4 * Page, PAGE_READWRITE, 4 * Page, true);
    Require(created.size() == 1 && created[0].second == 4 * Page && RegionAt(page(23)).AllocationBase == page(20), "a commit over two placeholders was not one allocation");
    mappings.Reset(memory, Span);
    Require(VirtualFree(holder, Reserved, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS) && VirtualFree(holder, 0, MEM_RELEASE), "cannot release the private commit placeholder");
}

void CheckPrivateCommitEdgeLayouts() {
    auto& mappings = Mappings::Get();
    constexpr std::size_t Page = 16384;
    constexpr std::size_t Pages = 32;
    auto* memory = static_cast<unsigned char*>(mappings.Reserve(nullptr, Pages * Page));
    Require(memory != nullptr, "cannot reserve a placeholder for private commit edges");
    const auto page = [&](std::size_t index) { return memory + index * Page; };
    const auto range = [&](std::size_t first, std::size_t count) { return std::make_pair(reinterpret_cast<std::uintptr_t>(page(first)), count * Page); };
    const auto region = [&](std::size_t first, std::size_t count, DWORD state) {
        const auto info = RegionAt(page(first));
        return info.State == state && info.AllocationBase == page(first) && info.RegionSize == count * Page;
    };
    const auto ownPages = [&](std::size_t first, std::size_t count) {
        for (auto index = first; index < first + count; ++index) {
            if (!region(index, 1, MEM_COMMIT)) return false;
        }
        return true;
    };
    auto before = Mappings::MemoryQueries();
    auto created = mappings.Commit(page(2), 4 * Page, PAGE_READWRITE, 4 * Page, true);
    Require(Mappings::MemoryQueries() - before <= 1 && created.size() == 1 && created[0] == range(2, 4), "a whole-range granule inside a placeholder was not one created range from one query");
    Require(region(0, 2, MEM_RESERVE) && region(2, 4, MEM_COMMIT) && region(6, Pages - 6, MEM_RESERVE), "a whole-range granule inside a placeholder was not one allocation between two placeholders");
    page(2)[8] = 0x5a;
    before = Mappings::MemoryQueries();
    created = mappings.Commit(page(0), 8 * Page, PAGE_READWRITE, Page, true);
    Require(Mappings::MemoryQueries() - before <= 3 && created.size() == 2 && created[0] == range(0, 2) && created[1] == range(6, 2), "a commit around a committed piece did not create exactly the placeholders around it");
    Require(ownPages(0, 2) && region(2, 4, MEM_COMMIT) && page(2)[8] == 0x5a && ownPages(6, 2) && region(8, Pages - 8, MEM_RESERVE), "a commit around a committed piece changed the piece or the layout");
    for (std::size_t index = 0; index < 8; ++index) page(index)[8] = static_cast<unsigned char>(index + 1);
    mappings.Reset(page(6), 2 * Page);
    Require(region(6, 2, MEM_RESERVE) && ownPages(0, 2) && region(2, 4, MEM_COMMIT) && page(1)[8] == 2 && page(5)[8] == 6 && region(8, Pages - 8, MEM_RESERVE), "a partial release of carved pages changed a neighbour");
    mappings.Reset(page(2), 4 * Page);
    before = Mappings::MemoryQueries();
    created = mappings.Commit(page(2), 4 * Page, PAGE_READWRITE, Page, true);
    Require(Mappings::MemoryQueries() - before <= 1 && created.size() == 1 && created[0] == range(2, 4) && ownPages(2, 4) && page(2)[8] == 0 && page(5)[8] == 0, "a fixed remap was not carved into fresh pages");
    Require(page(1)[8] == 2 && region(6, 2, MEM_RESERVE), "a fixed remap changed a neighbour");
    before = Mappings::MemoryQueries();
    created = mappings.Commit(page(6), 4 * Page, PAGE_READWRITE, Page, true);
    Require(Mappings::MemoryQueries() - before <= 2 && created.size() == 1 && created[0] == range(6, 4) && ownPages(6, 4) && region(10, Pages - 10, MEM_RESERVE), "a commit over adjacent placeholders was not carved page by page");
    bool refused = false;
    try {
        mappings.Commit(page(12), Page, PAGE_READWRITE, 0, true);
    } catch (const std::invalid_argument&) {
        refused = true;
    }
    Require(refused && region(10, Pages - 10, MEM_RESERVE), "a zero commit granule was accepted or changed the placeholder");
    mappings.Commit(page(10), 3 * Page, PAGE_READWRITE, 3 * Page, true);
    mappings.Reset(page(10), 3 * Page);
    created = mappings.Commit(page(10), 4 * Page, PAGE_READWRITE, 2 * Page, true);
    Require(created.size() == 1 && created[0] == range(10, 4) && region(10, 2, MEM_COMMIT) && region(12, 2, MEM_COMMIT) && region(14, Pages - 14, MEM_RESERVE), "a granule crossing the end of a carved placeholder did not take the coalescing path");
    mappings.Reset(memory, 14 * Page);
    Require(VirtualFree(memory, Pages * Page, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS) && VirtualFree(memory, 0, MEM_RELEASE), "cannot release the private commit edge placeholder");
}

void CheckPrivateCommitTails() {
    auto& mappings = Mappings::Get();
    constexpr std::size_t Page = 16384;
    constexpr std::size_t Pages = 8;
    auto* memory = static_cast<unsigned char*>(mappings.Reserve(nullptr, Pages * Page));
    Require(memory != nullptr, "cannot reserve a placeholder for private commit tails");
    const auto page = [&](std::size_t index) { return memory + index * Page; };
    const auto at = [](const unsigned char* pointer) { return reinterpret_cast<std::uintptr_t>(pointer); };
    const auto allocation = [](const unsigned char* base, std::size_t bytes, DWORD state) {
        const auto info = RegionAt(base);
        return info.State == state && info.AllocationBase == base && info.RegionSize == bytes;
    };
    const auto unwatched = [](unsigned char* base) {
        std::array<void*, 4> written{};
        ULONG_PTR count = written.size();
        DWORD granularity = 0;
        return GetWriteWatch(0, base, Small, written.data(), &count, &granularity) != 0;
    };
    auto before = Mappings::MemoryQueries();
    auto created = mappings.Commit(page(1), 2 * Page + Small, PAGE_READWRITE, Page, false);
    Require(Mappings::MemoryQueries() - before == 1 && created.size() == 1 && created[0].first == at(page(1)) && created[0].second == 2 * Page + Small, "a commit with a short last piece was not one created range from one query");
    Require(allocation(page(0), Page, MEM_RESERVE) && allocation(page(1), Page, MEM_COMMIT) && allocation(page(2), Page, MEM_COMMIT) && allocation(page(3), Small, MEM_COMMIT) && allocation(page(3) + Small, (Pages - 3) * Page - Small, MEM_RESERVE), "a short last piece was not its own allocation before the rest of the placeholder");
    page(3)[Small - 1] = 7;
    Require(unwatched(page(1)) && unwatched(page(3)), "an unwatched commit carved write-watched pages");
    before = Mappings::MemoryQueries();
    created = mappings.Commit(page(5), 3 * Page, PAGE_READWRITE, Page, true);
    Require(Mappings::MemoryQueries() - before == 1 && created.size() == 1 && created[0].first == at(page(5)) && created[0].second == 3 * Page, "a commit to the end of a placeholder was not one created range from one query");
    Require(allocation(page(3) + Small, 2 * Page - Small, MEM_RESERVE) && allocation(page(5), Page, MEM_COMMIT) && allocation(page(6), Page, MEM_COMMIT) && allocation(page(7), Page, MEM_COMMIT) && page(3)[Small - 1] == 7, "a commit to the end of a placeholder changed the placeholder before it or a neighbour");
    mappings.Reset(page(1), 2 * Page + Small);
    mappings.Reset(page(5), 3 * Page);
    Require(VirtualFree(memory, Pages * Page, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS) && VirtualFree(memory, 0, MEM_RELEASE), "cannot release the private commit tail placeholder");
}

void CheckForgottenSectionKeepsNoPages() {
    auto& mappings = Mappings::Get();
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, Bytes, nullptr);
    auto* first = static_cast<unsigned char*>(mappings.Reserve(nullptr, Bytes));
    auto* second = static_cast<unsigned char*>(mappings.Reserve(nullptr, Bytes));
    Require(section && first && second, "cannot allocate forgotten section test memory");
    const auto resident = [&](unsigned char* address) {
        MEMORY_BASIC_INFORMATION info{};
        Require(VirtualQuery(address, &info, sizeof(info)) == sizeof(info), "resident query failed");
        return info.State == MEM_COMMIT && info.Type == MEM_PRIVATE;
    };
    mappings.Map(first, Bytes, section, 0, PAGE_READWRITE, true, true);
    mappings.Map(second, Bytes, section, 0, PAGE_READWRITE, true, true);
    Require(!resident(first) && !resident(second), "a second mapping kept the first resident");
    mappings.ForgetSection(section);
    mappings.Reset(first, Bytes);
    mappings.Reset(second, Bytes);
    mappings.Map(first, Bytes, section, 0, PAGE_READWRITE, true, true);
    Require(resident(first), "a forgotten section's handle value kept its pages in the section");
    first[0] = 7;
    mappings.ForgetSection(section);
    mappings.Reset(first, Bytes);
    mappings.Map(first, Bytes, section, 0, PAGE_READWRITE, true, true);
    Require(resident(first) && first[0] == 0, "a forgotten section's handle value kept its unmapped bytes");
    mappings.Reset(first, Bytes);
    VirtualFree(first, 0, MEM_RELEASE);
    VirtualFree(second, 0, MEM_RELEASE);
    CloseHandle(section);
}

void CheckReleasedPrivateAllocation() {
    auto& mappings = Mappings::Get();
    auto* memory = static_cast<unsigned char*>(mappings.Reserve(nullptr, Bytes));
    Require(memory != nullptr, "private reservation failed");
    mappings.Commit(memory, Bytes, PAGE_READWRITE, Bytes, true);
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    std::array<void*, 32> pages{};
    std::size_t count = pages.size();
    Require(mappings.Collect(address, Bytes, pages.data(), &count, true), "a collect over a committed private allocation failed");
    Require(VirtualFree(memory, 0, MEM_RELEASE) != 0, "cannot release the private allocation behind the mappings");
    bool collected = true;
    try {
        count = pages.size();
        collected = mappings.Collect(address, Bytes, pages.data(), &count, true);
    } catch (const std::exception& error) {
        Require(false, (std::string("a collect over a private allocation released behind the mappings threw: ") + error.what()).c_str());
    }
    Require(!collected, "a collect over a private allocation released behind the mappings succeeded");
}

void CheckVerifiedPrivateCollect() {
    auto& mappings = Mappings::Get();
    auto* memory = static_cast<unsigned char*>(mappings.Reserve(nullptr, Bytes));
    Require(memory != nullptr, "private reservation failed");
    mappings.Commit(memory, Bytes, PAGE_READWRITE, Bytes, true);
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    std::array<void*, 64> pages{};
    std::size_t available = pages.size();
    Require(mappings.Collect(address, Bytes, pages.data(), &available, true), "a verified collect over a private allocation failed");
    static_cast<volatile unsigned char*>(memory)[Small] = 1;
    available = pages.size();
    Require(mappings.Collect(address, Bytes, pages.data(), &available, true) && available == 1 && reinterpret_cast<std::uintptr_t>(pages[0]) == address + Small, "a verified collect over a recorded private allocation lost its write");
    Require(VirtualFree(memory + 2 * Small, Small, MEM_DECOMMIT) != 0, "cannot decommit a private page in place");
    bool collected = false;
    try {
        available = pages.size();
        collected = mappings.Collect(address, Bytes, pages.data(), &available, true);
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find("APS5_VERIFY_DIRECT_COLLECT") != std::string::npos, error.what());
    }
    Require(!collected, "a collect over a recorded private allocation with a page decommitted in place succeeded under APS5_VERIFY_DIRECT_COLLECT");
    mappings.Reset(memory, Bytes);
    VirtualFree(memory, 0, MEM_RELEASE);
}

void CheckConcurrentWrites() {
    Region region;
    std::atomic<bool> start{false};
    std::thread writer([&] {
        while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for (std::size_t page = 0; page < 16; ++page) region.Write(page, static_cast<unsigned char>(page + 1));
    });
    start.store(true, std::memory_order_release);
    region.Collect(Collection::Fresh);
    writer.join();
    for (std::size_t page = 0; page < 16; ++page) Require(region.first[page * Small] == page + 1, "activation lost a concurrent write");
    region.Collect(Collection::Armed);
    Require(region.Collect(Collection::Armed).empty(), "concurrent writes did not settle");
}

void CheckComparedWhenShared() {
    auto& mappings = Mappings::Get();
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, 2 * Bytes, nullptr);
    auto* first = static_cast<unsigned char*>(mappings.Reserve(nullptr, 2 * Bytes));
    auto* second = static_cast<unsigned char*>(mappings.Reserve(nullptr, 2 * Bytes));
    Require(section && first && second, "cannot allocate compared sharing test memory");
    mappings.ForgetSection(section);
    const auto collect = [&](unsigned char* address, std::size_t bytes) {
        std::array<void*, 32> pages{};
        std::size_t count = pages.size();
        return mappings.Collect(reinterpret_cast<std::uintptr_t>(address), bytes, pages.data(), &count, true);
    };
    const auto protection = [](unsigned char* address) {
        MEMORY_BASIC_INFORMATION info{};
        Require(VirtualQuery(address, &info, sizeof(info)) == sizeof(info), "compared sharing protection query failed");
        return info;
    };
    mappings.Map(first, 2 * Bytes, section, 0, PAGE_READWRITE, true, true);
    Require(protection(first).Type == MEM_PRIVATE, "the compared sharing test memory is not resident");
    mappings.CompareWhenShared(reinterpret_cast<std::uintptr_t>(first), Bytes);
    Require(collect(first, 2 * Bytes), "a resident page to be compared when shared stopped being collected");
    mappings.Map(second, 2 * Bytes, section, 0, PAGE_READWRITE, true, true);
    Require(protection(first).Type == MEM_MAPPED && protection(second).Type == MEM_MAPPED, "a second mapping kept the first resident");
    Require(!collect(first, Bytes) && !collect(second, Bytes), "a page to be compared when shared was collected once shared");
    Require(!collect(first, 2 * Bytes), "a collect over a page to be compared when shared succeeded once shared");
    Require(protection(first).Protect == PAGE_READWRITE && protection(second).Protect == PAGE_READWRITE, "a page to be compared when shared was armed once shared");
    const auto before = faults.load();
    first[8] = 0x5a;
    second[Small + 8] = 0xa5;
    Require(faults.load() == before && second[8] == 0x5a && first[Small + 8] == 0xa5, "a store into a page to be compared when shared faulted once shared");
    Require(collect(first + Bytes, Bytes), "a shared page that was never to be compared stopped being collected");
    mappings.Reset(first, 2 * Bytes);
    mappings.Reset(second, 2 * Bytes);
    for (auto* reserved : {first, second}) {
        VirtualFree(reserved, 2 * Bytes, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
        VirtualFree(reserved, 0, MEM_RELEASE);
    }
    mappings.ForgetSection(section);
    CloseHandle(section);
}

void CheckResidentOnlyComparedWhenShared() {
    auto& mappings = Mappings::Get();
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, Bytes, nullptr);
    auto* first = static_cast<unsigned char*>(mappings.Reserve(nullptr, Bytes));
    auto* second = static_cast<unsigned char*>(mappings.Reserve(nullptr, Bytes));
    Require(section && first && second, "cannot allocate resident-only sharing test memory");
    mappings.ForgetSection(section);
    mappings.Map(first, Bytes, section, 0, PAGE_READWRITE, true, true);
    MEMORY_BASIC_INFORMATION info{};
    Require(VirtualQuery(first, &info, sizeof(info)) == sizeof(info) && info.Type == MEM_PRIVATE, "the resident-only sharing test memory is not resident");
    mappings.CompareWhenShared(reinterpret_cast<std::uintptr_t>(first), Bytes, true);
    mappings.Map(second, Bytes, section, 0, PAGE_READWRITE, true, true);
    std::array<void*, 32> pages{};
    std::size_t count = pages.size();
    Require(!mappings.Collect(reinterpret_cast<std::uintptr_t>(first), Bytes, pages.data(), &count, true), "a resident page marked to be compared was collected once shared");
    const auto before = faults.load();
    first[8] = 0x3c;
    Require(faults.load() == before && second[8] == 0x3c, "a store into a resident page marked to be compared faulted once shared");
    mappings.Reset(first, Bytes);
    mappings.Reset(second, Bytes);
    for (auto* reserved : {first, second}) {
        VirtualFree(reserved, Bytes, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
        VirtualFree(reserved, 0, MEM_RELEASE);
    }
    mappings.ForgetSection(section);
    CloseHandle(section);
    Region region;
    mappings.CompareWhenShared(reinterpret_cast<std::uintptr_t>(region.first), Bytes, true);
    Require(region.Collect(Collection::All, 0, 1) == Pages(0, 1), "marking only resident pages changed a shared view");
    Require(region.Protection(0) == PAGE_READONLY, "a shared view outside a resident-only mark was not armed");
}

void CheckComparedWhenSharedRemapped() {
    auto& mappings = Mappings::Get();
    constexpr std::size_t Page = 16384;
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, Bytes, nullptr);
    std::array<unsigned char*, 3> reserved{};
    for (auto& address : reserved) address = static_cast<unsigned char*>(mappings.Reserve(nullptr, Bytes));
    Require(section && reserved[0] && reserved[1] && reserved[2], "cannot allocate remapped compared sharing test memory");
    mappings.ForgetSection(section);
    const auto collect = [&](unsigned char* address) {
        std::array<void*, 32> pages{};
        std::size_t count = pages.size();
        return mappings.Collect(reinterpret_cast<std::uintptr_t>(address), Page, pages.data(), &count, true);
    };
    const auto memory = [](unsigned char* address) {
        MEMORY_BASIC_INFORMATION info{};
        Require(VirtualQuery(address, &info, sizeof(info)) == sizeof(info), "remapped compared sharing query failed");
        return info;
    };
    const auto share = [&] {
        mappings.Map(reserved[1], Page, section, 0, PAGE_READWRITE, true, true);
        mappings.Map(reserved[2], Page, section, 0, PAGE_READWRITE, true, true);
        Require(memory(reserved[1]).Type == MEM_MAPPED && memory(reserved[2]).Type == MEM_MAPPED, "the remapped page is not shared");
    };
    const auto unmapShared = [&] {
        mappings.Reset(reserved[1], Page);
        mappings.Reset(reserved[2], Page);
    };
    mappings.Map(reserved[0], Page, section, 0, PAGE_READWRITE, true, true);
    Require(memory(reserved[0]).Type == MEM_PRIVATE, "the remapped compared sharing test memory is not resident");
    mappings.CompareWhenShared(reinterpret_cast<std::uintptr_t>(reserved[0]), Page);
    mappings.Reset(reserved[0], Page);
    share();
    Require(!collect(reserved[1]) && !collect(reserved[2]), "a page to be compared when shared was collected after every view was unmapped and it was mapped again");
    Require(memory(reserved[1]).Protect == PAGE_READWRITE && memory(reserved[2]).Protect == PAGE_READWRITE, "a page to be compared when shared was armed after every view was unmapped and it was mapped again");
    const auto before = faults.load();
    reserved[1][8] = 0x6b;
    Require(faults.load() == before && reserved[2][8] == 0x6b, "a store into a remapped page to be compared faulted once shared");
    unmapShared();
    mappings.ForgetSection(section);
    share();
    Require(collect(reserved[1]) && memory(reserved[2]).Protect == PAGE_READONLY, "a forgotten section's handle value kept its pages compared when shared");
    unmapShared();
    for (auto* address : reserved) {
        VirtualFree(address, Bytes, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
        VirtualFree(address, 0, MEM_RELEASE);
    }
    mappings.ForgetSection(section);
    CloseHandle(section);
}

void CheckComparedPagesReportedOnce() {
    auto& mappings = Mappings::Get();
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, 2 * Bytes, nullptr);
    std::array<unsigned char*, 3> reserved{};
    for (auto& address : reserved) address = static_cast<unsigned char*>(mappings.Reserve(nullptr, 2 * Bytes));
    Require(section && reserved[0] && reserved[1] && reserved[2], "cannot allocate compared page report test memory");
    mappings.ForgetSection(section);
    const auto listed = [&](unsigned char* base, std::size_t begin, std::size_t count, bool unreported) {
        const auto address = reinterpret_cast<std::uintptr_t>(base);
        auto cursor = address + begin * Small;
        const auto end = cursor + count * Small;
        std::set<std::size_t> result;
        while (cursor < end) {
            std::array<void*, 3> pages{};
            std::size_t available = pages.size();
            mappings.ComparedPages(cursor, end - cursor, pages.data(), &available, unreported);
            for (std::size_t i = 0; i < available; ++i) result.insert((reinterpret_cast<std::uintptr_t>(pages[i]) - address) / Small);
            if (available < pages.size()) break;
            cursor = reinterpret_cast<std::uintptr_t>(pages[available - 1]) + Small;
        }
        return result;
    };
    auto* first = reserved[0];
    auto* second = reserved[1];
    mappings.Map(first, 2 * Bytes, section, 0, PAGE_READWRITE, true, true);
    mappings.CompareWhenShared(reinterpret_cast<std::uintptr_t>(first), Bytes);
    Require(listed(first, 0, 32, false).empty() && listed(first, 0, 32, true).empty(), "a resident page to be compared was listed before it was shared");
    mappings.Map(second, 2 * Bytes, section, 0, PAGE_READWRITE, true, true);
    Require(listed(first, 0, 32, false) == Pages(0, 16) && listed(second, 0, 32, false) == Pages(0, 16), "the compared pages of both mappings were not listed once shared");
    Require(listed(first, 2, 4, true) == Pages(2, 6), "a partial take did not report exactly its compared pages");
    std::set<std::size_t> rest = Pages(0, 2);
    rest.insert(6);
    for (std::size_t page = 7; page < 16; ++page) rest.insert(page);
    Require(listed(first, 0, 32, true) == rest, "a compared page was reported twice or not at all");
    Require(listed(first, 0, 32, true).empty() && listed(first, 0, 32, false) == Pages(0, 16), "a compared page was reported again, or a listing consumed its mark");
    Require(listed(second, 0, 32, true) == Pages(0, 16) && listed(second, 0, 32, true).empty(), "the second mapping's compared pages were not reported exactly once");
    mappings.CompareWhenShared(reinterpret_cast<std::uintptr_t>(first), Bytes);
    Require(listed(first, 0, 32, true).empty() && listed(second, 0, 32, true).empty(), "marking compared pages again reported them again");
    mappings.CompareWhenShared(reinterpret_cast<std::uintptr_t>(first + Bytes), Bytes);
    Require(listed(first, 16, 16, true) == Pages(16, 32) && listed(second, 16, 16, true) == Pages(16, 32), "shared pages newly marked to be compared were not reported in every mapping");
    mappings.Map(reserved[2], 2 * Bytes, section, 0, PAGE_READWRITE, true, true);
    Require(listed(reserved[2], 0, 32, true) == Pages(0, 32) && listed(first, 0, 32, true).empty(), "a new mapping of compared pages was not reported, or reported the others again");
    for (auto* address : reserved) mappings.Reset(address, 2 * Bytes);
    for (auto* address : reserved) {
        VirtualFree(address, 2 * Bytes, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
        VirtualFree(address, 0, MEM_RELEASE);
    }
    mappings.ForgetSection(section);
    CloseHandle(section);
}
}

namespace {
void CheckSharedMapQueriesPerMapping() {
    constexpr std::size_t Pages = 256;
    constexpr std::size_t Span = Pages * 16384;
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, Span, nullptr);
    auto* memory = static_cast<unsigned char*>(Mappings::Get().Reserve(nullptr, Span));
    Require(section && memory, "cannot allocate shared test memory");
    const auto before = Mappings::MemoryQueries();
    Mappings::Get().Map(memory, Span, section, 0, PAGE_READWRITE);
    const auto queries = Mappings::MemoryQueries() - before;
    for (std::size_t page = 0; page < Pages; ++page) memory[page * 16384 + 8] = static_cast<unsigned char>(page);
    for (std::size_t page = 0; page < Pages; ++page) Require(memory[page * 16384 + 8] == static_cast<unsigned char>(page), "a shared page lost its store");
    Mappings::Get().Reset(memory, Span);
    VirtualFree(memory, 0, MEM_RELEASE);
    CloseHandle(section);
    Require(queries < Pages, ("a shared mapping queried the address space for every page: " + std::to_string(queries) + " queries for " + std::to_string(Pages) + " pages").c_str());
}

std::size_t MappedViews(const unsigned char* begin, std::size_t bytes) {
    std::set<void*> bases;
    for (auto cursor = begin; cursor < begin + bytes;) {
        MEMORY_BASIC_INFORMATION info{};
        Require(VirtualQuery(cursor, &info, sizeof(info)) == sizeof(info), "view query failed");
        if (info.Type == MEM_MAPPED) bases.insert(info.AllocationBase);
        cursor = static_cast<const unsigned char*>(info.BaseAddress) + info.RegionSize;
    }
    return bases.size();
}

DWORD ProtectionAt(const unsigned char* address) {
    MEMORY_BASIC_INFORMATION info{};
    Require(VirtualQuery(address, &info, sizeof(info)) == sizeof(info), "protection query failed");
    return info.State == MEM_COMMIT ? info.Protect : 0;
}

void CheckSharedRunIsOneView() {
    constexpr std::size_t Gib = std::size_t{1} << 30;
    HANDLE large = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, static_cast<DWORD>(Gib), nullptr);
    auto* whole = static_cast<unsigned char*>(Mappings::Get().Reserve(nullptr, Gib));
    Require(large && whole, "cannot allocate a shared gibibyte");
    Mappings::Get().Map(whole, Gib, large, 0, PAGE_READWRITE);
    const auto wholeViews = MappedViews(whole, Gib);
    whole[Gib - 1] = 1;
    Mappings::Get().Reset(whole, Gib);
    VirtualFree(whole, 0, MEM_RELEASE);
    CloseHandle(large);
    Require(wholeViews == 1, ("a shared gibibyte was mapped as " + std::to_string(wholeViews) + " views instead of one").c_str());

    constexpr std::size_t Page = 16384;
    constexpr std::size_t Pages = 64;
    constexpr std::size_t Span = Pages * Page;
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, Span + Page, nullptr);
    auto* memory = static_cast<unsigned char*>(Mappings::Get().Reserve(nullptr, Span));
    auto* other = static_cast<unsigned char*>(Mappings::Get().Reserve(nullptr, Page));
    Require(section && memory && other, "cannot allocate shared test memory");
    Mappings::Get().Map(memory, Span, section, 0, PAGE_READWRITE);
    for (std::size_t page = 0; page < Pages; ++page) memory[page * Page + 8] = static_cast<unsigned char>(page + 1);
    Require(MappedViews(memory, Span) == 1, "a shared run was not mapped as one view");
    Mappings::Get().SetProtection(reinterpret_cast<std::uintptr_t>(memory + 5 * Page), Page, PAGE_READONLY);
    DWORD previous;
    Require(VirtualProtect(memory + 5 * Page, Page, PAGE_READONLY, &previous), "guest protection of one page failed");
    Require(MappedViews(memory, Span) == 1, "a per-page protection split the run");
    Mappings::Get().Map(other, Page, section, 30 * Page, PAGE_READWRITE);
    Require(MappedViews(memory, Span) == 1, "an alias of one page split the run");
    other[16] = 0x5a;
    Require(memory[30 * Page + 16] == 0x5a, "an alias store did not reach the run");
    std::array<void*, 32> collected{};
    std::size_t available = collected.size();
    Require(Mappings::Get().Collect(reinterpret_cast<std::uintptr_t>(memory + 50 * Page), Page, collected.data(), &available, true, Collection::All), "run collection failed");
    Require(ProtectionAt(memory + 50 * Page) == PAGE_READONLY, "collection did not arm the run's page");
    Mappings::Get().Reset(memory + 10 * Page, Page);
    Require(MappedViews(memory, Span) == 2, ("a partial unmap left " + std::to_string(MappedViews(memory, Span)) + " views instead of two").c_str());
    Require(ProtectionAt(memory + 10 * Page) == 0, "the unmapped page is still mapped");
    Mappings::Get().Map(memory + 20 * Page, Page, section, Span, PAGE_READWRITE);
    Require(MappedViews(memory, Span) == 4, ("a remap of one page left " + std::to_string(MappedViews(memory, Span)) + " views instead of four").c_str());
    Require(memory[20 * Page + 8] == 0, "the remapped page shows the old page");
    for (std::size_t page = 0; page < Pages; ++page) {
        if (page == 10 || page == 20) continue;
        Require(memory[page * Page + 8] == static_cast<unsigned char>(page + 1), ("a split lost page " + std::to_string(page)).c_str());
    }
    Require(ProtectionAt(memory + 5 * Page) == PAGE_READONLY && ProtectionAt(memory + 6 * Page) == PAGE_READWRITE, "a split lost a page protection");
    Require(ProtectionAt(memory + 50 * Page) == PAGE_READONLY, "a split dropped a write-tracking protection");
    const auto before = faults.load();
    memory[50 * Page + 8] = 0x77;
    Require(faults == before + 1, "a write to an armed page of a split run did not fault");
    memory[51 * Page + 8] = 0x78;
    available = collected.size();
    Require(Mappings::Get().Collect(reinterpret_cast<std::uintptr_t>(memory + 50 * Page), 2 * Page, collected.data(), &available, true, Collection::Armed), "armed collection failed");
    Require(available == 1 && collected[0] == memory + 50 * Page, "the split run lost a tracked write");
    Mappings::Get().Reset(memory, Span);
    Mappings::Get().Reset(other, Page);
    VirtualFree(memory, 0, MEM_RELEASE);
    VirtualFree(other, 0, MEM_RELEASE);
    CloseHandle(section);
}
}

int main(int argc, char** argv) {
    auto* handler = AddVectoredExceptionHandler(1, HandleWrite);
    try {
        Require(handler != nullptr, "cannot install test handler");
        if (argc > 1 && std::string(argv[1]) == "--verify-direct") {
            CheckVerifiedPrivateCollect();
            std::cout << "Windows verified private collect tests passed\n";
            RemoveVectoredExceptionHandler(handler);
            return 0;
        }
        CheckArmed();
        CheckFresh();
        CheckAliasesAndCapacity();
        CheckPinsAndHostWrites();
        CheckFailureAndHoles();
        CheckPrivate();
        CheckConcurrentWrites();
        CheckForgottenSectionKeepsNoPages();
        CheckSharedMapQueriesPerMapping();
        CheckSharedRunIsOneView();
        CheckReleasedPrivateAllocation();
        CheckComparedWhenShared();
        CheckResidentOnlyComparedWhenShared();
        CheckComparedWhenSharedRemapped();
        CheckComparedPagesReportedOnce();
        CheckPrivateCommitCarvesPlaceholders();
        CheckPrivateCommitEdgeLayouts();
        CheckPrivateCommitTails();
        std::cout << "Windows deferred write-watch tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    RemoveVectoredExceptionHandler(handler);
}
