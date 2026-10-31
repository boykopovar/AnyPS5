#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/WindowsMappings.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("owned guest memory regression at " + std::to_string(location.line()));
}

#ifdef _WIN32
using Mappings = GuestArena::WindowsMappings;
constexpr std::size_t blockBytes = Mappings::ownedBlockBytes;

struct Memory {
    Mappings& mappings = Mappings::Get();
    unsigned char* base;
    std::size_t bytes;
    HANDLE section = nullptr;

    explicit Memory(std::size_t bytes = blockBytes, bool resident = true, bool watched = true) : bytes(bytes) {
        base = static_cast<unsigned char*>(mappings.Reserve(nullptr, bytes));
        Require(base != nullptr);
        if (resident) {
            section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, static_cast<DWORD>(bytes), nullptr);
            Require(section != nullptr);
            mappings.Map(base, bytes, section, 0, PAGE_READWRITE, watched, true);
        } else mappings.Commit(base, bytes, PAGE_READWRITE, bytes, watched);
    }

    ~Memory() {
        mappings.Reset(base, bytes);
        VirtualFree(base, 0, MEM_RELEASE);
        if (section != nullptr) {
            mappings.ForgetSection(section);
            CloseHandle(section);
        }
    }

    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(base); }

    bool Arm(std::uint64_t* words, unsigned flags = 0) { return mappings.ArmOwned(Address(), bytes, words, bytes / blockBytes, flags); }
    bool Same(const std::uint64_t* words) { return mappings.OwnedUnchanged(Address(), bytes, words, bytes / blockBytes); }

    std::vector<void*> Collect(bool clear, Mappings::Collection collection = Mappings::Collection::All, std::size_t capacity = 0) {
        std::vector<void*> pages(capacity == 0 ? bytes / 4096 : capacity);
        auto count = pages.size();
        Require(mappings.Collect(Address(), bytes, pages.data(), &count, clear, collection));
        pages.resize(count);
        return pages;
    }
};

static void CheckWrites(bool resident) {
    Memory memory(blockBytes, resident);
    const unsigned flags = resident ? 0 : Mappings::OwnedPrivate;
    std::uint64_t word = 0;
    MEMORY_BASIC_INFORMATION before{};
    Require(VirtualQuery(memory.base, &before, sizeof(before)) == sizeof(before));
    Require(memory.Arm(&word, flags));
    Require(word != 0 && memory.Same(&word));
    MEMORY_BASIC_INFORMATION after{};
    Require(VirtualQuery(memory.base, &after, sizeof(after)) == sizeof(after));
    Require(before.Protect == after.Protect && after.Protect == PAGE_READWRITE);
    memory.base[4095] = 37;
    Require(!memory.Same(&word));
    const auto old = word;
    Require(memory.Arm(&word, flags));
    Require(word != old && memory.Same(&word) && !memory.Same(&old));
    memory.base[blockBytes - 1] = 81;
    Require(!memory.Same(&word));
    Require(memory.Arm(&word, flags));
    Require(memory.mappings.OwnedUnchanged(memory.Address() + 4095, blockBytes - 8190, &word, 1));
    Require(memory.base[4095] == 37 && memory.base[blockBytes - 1] == 81);
}

static void CheckPending(bool resident) {
    Memory memory(2 * 1024 * 1024, resident);
    memory.Collect(true);
    for (std::size_t at = 0; at < memory.bytes; at += 4096) memory.base[at] = 17;
    std::vector<std::uint64_t> words(memory.bytes / blockBytes);
    Require(memory.Arm(words.data(), resident ? 0 : Mappings::OwnedPrivate));
    Require(memory.Same(words.data()));
    const auto peek = memory.Collect(false, Mappings::Collection::All, 3);
    Require(peek.size() == 3 && memory.Collect(false, Mappings::Collection::All, 3) == peek);
    Require(memory.Same(words.data()));
    std::vector<bool> seen(memory.bytes / 4096);
    std::size_t total = 0;
    for (;;) {
        const auto pages = memory.Collect(true, Mappings::Collection::All, 3);
        if (pages.empty()) break;
        for (auto* page : pages) {
            const auto index = (reinterpret_cast<std::uintptr_t>(page) - memory.Address()) / 4096;
            Require(index < seen.size() && !seen[index]);
            seen[index] = true;
            ++total;
        }
    }
    Require(total == seen.size() && !memory.Same(words.data()));
    Require(memory.Arm(words.data(), resident ? 0 : Mappings::OwnedPrivate));
    memory.base[7 * 4096] = 23;
    Require(!memory.Same(words.data()));
    Require(memory.Collect(true).size() == 1);
}

static void CheckFresh() {
    Memory memory;
    memory.base[4096] = 9;
    std::uint64_t word = 0;
    Require(memory.Arm(&word));
    Require(memory.Collect(false, Mappings::Collection::Armed).empty());
    Require(memory.Collect(true, Mappings::Collection::Fresh).size() == blockBytes / 4096);
    Require(memory.Collect(true, Mappings::Collection::Armed).empty());
    Require(memory.Arm(&word));
    memory.base[8192] = 11;
    Require(memory.Arm(&word));
    Require(memory.Collect(false, Mappings::Collection::Fresh).empty());
    Require(memory.Collect(true, Mappings::Collection::Armed).size() == 1);
}

static void CheckEviction() {
    Memory memory;
    memory.base[7] = 73;
    std::uint64_t word = 0;
    Require(memory.Arm(&word));
    const auto old = word;
    memory.mappings.Reset(memory.base, memory.bytes);
    Require(!memory.Same(&word));
    memory.mappings.Map(memory.base, memory.bytes, memory.section, 0, PAGE_READWRITE, true, true);
    Require(memory.base[7] == 73 && memory.Arm(&word) && word != old);
    Require(!memory.Same(&old));
    auto* alias = static_cast<unsigned char*>(memory.mappings.MapAlias(memory.Address(), memory.bytes));
    Require(alias != nullptr && !memory.Same(&word) && !memory.Arm(&word));
    alias[19] = 41;
    Require(memory.base[19] == 41);
    memory.mappings.UnmapAlias(alias);
    Require(!memory.Arm(&word));
}

static void CheckAccess(bool resident) {
    Memory memory(blockBytes, resident);
    const auto flags = resident ? 0 : Mappings::OwnedPrivate;
    std::uint64_t word = 0;
    Require(memory.Arm(&word, flags));
    memory.mappings.Pin(memory.Address() + 31, 9);
    memory.mappings.Pin(memory.Address() + 31, 9);
    Require(!memory.Same(&word) && !memory.Arm(&word, flags));
    memory.mappings.Unpin(memory.Address() + 31, 9);
    Require(!memory.Arm(&word, flags));
    memory.mappings.Unpin(memory.Address() + 31, 9);
    Require(memory.Arm(&word, flags));
    for (const bool store : {true, false}) {
        Require(memory.mappings.BeginHostWrite(memory.Address() + 4096, 1, store));
        Require(!memory.Same(&word) && !memory.Arm(&word, flags));
        memory.base[4096] = 59;
        memory.mappings.EndHostWrite(memory.Address() + 4096, 1, store);
        Require(memory.Arm(&word, flags));
    }
    memory.mappings.SetProtection(memory.Address(), memory.bytes, PAGE_READONLY);
    Require(!memory.Same(&word));
    DWORD old = 0;
    Require(VirtualProtect(memory.base, memory.bytes, PAGE_READONLY, &old));
    Require(!memory.Arm(&word, flags));
    memory.mappings.SetProtection(memory.Address(), memory.bytes, PAGE_READWRITE);
    Require(!memory.Arm(&word, flags));
    Require(VirtualProtect(memory.base, memory.bytes, PAGE_READWRITE, &old));
    Require(memory.Arm(&word, flags));
    memory.mappings.Commit(memory.base, memory.bytes, PAGE_READWRITE, memory.bytes, true);
    Require(!memory.Same(&word));
}

static void CheckRefusals() {
    Memory memory(2 * blockBytes, false);
    std::array<std::uint64_t, 2> words{7, 9};
    Require(!memory.Arm(words.data()));
    Require(!memory.mappings.ArmOwned(memory.Address(), 0, words.data(), 0, Mappings::OwnedPrivate));
    Require(!memory.mappings.ArmOwned(memory.Address(), memory.bytes, nullptr, 2, Mappings::OwnedPrivate));
    Require(!memory.mappings.ArmOwned(memory.Address(), memory.bytes, words.data(), 1, Mappings::OwnedPrivate));
    Require(!memory.mappings.ArmOwned(memory.Address(), memory.bytes, words.data(), 2, 2));
    Require(!memory.mappings.ArmOwned(std::numeric_limits<std::uintptr_t>::max() - 7, 16, words.data(), 2));
    Require(words[0] == 7 && words[1] == 9);
    Require(memory.mappings.ArmOwned(memory.Address(), blockBytes, words.data(), 1, Mappings::OwnedPrivate));
    const auto old = words[0];
    DWORD previous;
    Require(VirtualProtect(memory.base + blockBytes, 4096, PAGE_READONLY, &previous));
    Require(!memory.Arm(words.data(), Mappings::OwnedPrivate));
    Require(memory.mappings.OwnedUnchanged(memory.Address(), blockBytes, &old, 1));
    Require(VirtualProtect(memory.base + blockBytes, 4096, PAGE_READWRITE, &previous));
    Require(memory.mappings.ArmOwned(memory.Address() + blockBytes - 1, 2, words.data(), 2, Mappings::OwnedPrivate));
    memory.base[blockBytes] = 5;
    Require(!memory.mappings.OwnedUnchanged(memory.Address() + blockBytes - 1, 2, words.data(), 2));
    Memory untracked(blockBytes, false, false);
    Require(!untracked.Arm(words.data(), Mappings::OwnedPrivate));
    Memory unwatched(blockBytes, true, false);
    Require(!unwatched.Arm(words.data()));
    memory.mappings.Reset(memory.base, memory.bytes);
    Require(!memory.Arm(words.data(), Mappings::OwnedPrivate));
}

static void CheckWriterAndCollector() {
    Memory memory;
    memory.Collect(true);
    std::atomic<unsigned> request{0};
    std::atomic<unsigned> done{0};
    const auto wait = [](const std::atomic<unsigned>& value, unsigned expected) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (value.load(std::memory_order_acquire) != expected) {
            if (std::chrono::steady_clock::now() >= until) return false;
            std::this_thread::yield();
        }
        return true;
    };
    std::thread writer([&] {
        for (unsigned step = 1; step <= 64; ++step) {
            if (!wait(request, step)) return;
            memory.base[4096] = static_cast<unsigned char>(step);
            if ((step & 1) != 0) memory.Collect(true);
            done.store(step, std::memory_order_release);
        }
    });
    bool exact = true;
    for (unsigned step = 1; step <= 64; ++step) {
        std::uint64_t word = 0;
        exact &= memory.Arm(&word) && memory.Same(&word);
        request.store(step, std::memory_order_release);
        if (!wait(done, step)) {
            exact = false;
            break;
        }
        exact &= !memory.Same(&word);
    }
    writer.join();
    Require(exact);
}
#endif

static void CheckExports() {
    std::uint64_t word = 0;
    Require(!GuestArena::GuestArenaArmOwned_nid_no_patch(0, 0, &word, 1, 0));
    Require(!GuestArena::GuestArenaOwnedUnchanged_nid_no_patch(0, 0, &word, 1));
#ifdef _WIN32
    auto* pointer = GuestArena::GuestArenaAllocate_nid_postfix(blockBytes, blockBytes);
    Require(pointer != nullptr);
    GuestArena::GuestArenaCommit_nid_postfix(pointer, blockBytes, PAGE_READWRITE, blockBytes);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    Require(GuestArena::GuestArenaArmOwned_nid_no_patch(address, blockBytes, &word, 1, Mappings::OwnedPrivate));
    Require(GuestArena::GuestArenaOwnedUnchanged_nid_no_patch(address, blockBytes, &word, 1));
    static_cast<unsigned char*>(pointer)[11] = 77;
    Require(!GuestArena::GuestArenaOwnedUnchanged_nid_no_patch(address, blockBytes, &word, 1));
    GuestArena::GuestArenaReset_nid_postfix(pointer, blockBytes);
    GuestArena::GuestArenaRelease_nid_postfix(pointer, blockBytes);
#else
    Require(!GuestArena::GuestArenaArmOwned_nid_no_patch(0x100000, 65536, &word, 1, 1));
    Require(!GuestArena::GuestArenaOwnedUnchanged_nid_no_patch(0x100000, 65536, &word, 1));
#endif
}

int main() {
    try {
#ifdef _WIN32
        for (const bool resident : {true, false}) {
            CheckWrites(resident);
            CheckPending(resident);
            CheckAccess(resident);
        }
        CheckFresh();
        CheckEviction();
        CheckRefusals();
        CheckWriterAndCollector();
#endif
        CheckExports();
        std::puts("owned guest memory checks passed");
    } catch (const std::exception& failure) {
        std::fprintf(stderr, "%s\n", failure.what());
        return 1;
    }
    return 0;
}
