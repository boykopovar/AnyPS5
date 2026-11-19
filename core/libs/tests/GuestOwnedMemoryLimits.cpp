#include "prx/libc/include/WindowsMappings.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

using Mappings = GuestArena::WindowsMappings;

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    try {
        constexpr auto block = Mappings::ownedBlockBytes;
        constexpr auto cap = Mappings::ownedEntryLimit;
        static_assert(cap == 8);
        auto& mappings = Mappings::Get();
        auto* base = static_cast<unsigned char*>(mappings.Reserve(nullptr, (cap + 1) * block));
        Require(base != nullptr, "reserve failed");
        struct Release {
            Mappings& mappings;
            unsigned char* base;
            ~Release() {
                mappings.Reset(base, (cap + 1) * block);
                VirtualFree(base, 0, MEM_RELEASE);
            }
        } release{mappings, base};
        mappings.Commit(base, (cap + 1) * block, PAGE_READWRITE, (cap + 1) * block, true);
        const auto address = reinterpret_cast<std::uintptr_t>(base);
        std::array<std::uint64_t, cap + 1> words{};
        for (std::size_t i = 0; i < cap; ++i) {
            base[i * block] = 1;
            Require(mappings.ArmOwned(address + i * block, block, &words[i], 1, Mappings::OwnedPrivate), "cannot fill pending budget");
        }
        Require(mappings.ArmOwned(address, block, &words[0], 1, Mappings::OwnedPrivate), "existing pending block was refused");
        std::array<std::uint64_t, 2> pair{};
        Require(mappings.ArmOwned(address, 2 * block, pair.data(), pair.size(), Mappings::OwnedPrivate), "existing pending range was refused");
        words[0] = pair[0];
        words[1] = pair[1];
        base[cap * block] = 2;
        Require(!mappings.ArmOwned(address + (cap - 1) * block, 2 * block, pair.data(), pair.size(), Mappings::OwnedPrivate), "pending budget accepted a partly new range");
        Require(mappings.OwnedUnchanged(address + (cap - 1) * block, block, &words[cap - 1], 1), "mixed refusal invalidated an existing token");
        Require(!mappings.ArmOwned(address + cap * block, block, &words[cap], 1, Mappings::OwnedPrivate), "pending budget accepted a new block");
        Require(mappings.OwnedUnchanged(address, block, &words[0], 1), "refusal invalidated an existing token");
        std::array<void*, 16> reported{};
        ULONG_PTR reportedCount = reported.size();
        DWORD granularity = 0;
        Require(GetWriteWatch(0, base + cap * block, block, reported.data(), &reportedCount, &granularity) == 0 && reportedCount == 1, "refusal reset an untracked write");
        std::array<void*, 16> first{};
        std::size_t firstCount = first.size();
        Require(mappings.Collect(address, block, first.data(), &firstCount, true) && firstCount == 1 && first[0] == base, "pending page was lost on collection");
        Require(mappings.ArmOwned(address + cap * block, block, &words[cap], 1, Mappings::OwnedPrivate), "released pending capacity was not reused");
        std::vector<void*> all((cap + 1) * 16);
        auto allCount = all.size();
        Require(mappings.Collect(address, (cap + 1) * block, all.data(), &allCount, true) && allCount == cap, "pending or refused writes were lost");
        for (std::size_t i = 1; i <= cap; ++i) Require(all[i - 1] == base + i * block, "collected pending page mismatch");
        for (std::size_t i = 0; i < cap; ++i)
            Require(mappings.ArmOwned(address + i * block, block, &words[i], 1, Mappings::OwnedPrivate), "cannot fill token budget");
        const auto oldest = words[0];
        Require(mappings.ArmOwned(address + cap * block, block, &words[cap], 1, Mappings::OwnedPrivate), "token budget did not evict");
        Require(!mappings.OwnedUnchanged(address, block, &oldest, 1), "evicted proof still held");
        Require(mappings.OwnedUnchanged(address + block, block, &words[1], 1), "unrelated proof was evicted");
        std::array<std::uint64_t, cap + 1> oversized{};
        Require(!mappings.ArmOwned(address, (cap + 1) * block, oversized.data(), oversized.size(), Mappings::OwnedPrivate), "oversized arm was accepted");
        Require(mappings.OwnedUnchanged(address + cap * block, block, &words[cap], 1), "oversized refusal invalidated a token");
        std::puts("owned memory limit checks passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
