#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {

using namespace AgcDriver::GuestMemory;
constexpr std::size_t Unit = 65536;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class Range {
public:
    Range() {
        data = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(2 * Unit, Unit));
        Require(data != nullptr, "cannot reserve compare-hook fixture");
        GuestArena::GuestArenaCommit_nid_postfix(data, 2 * Unit, 0x04, Unit);
        std::memset(data, 0x5b, 2 * Unit);
    }

    ~Range() {
        GuestArena::GuestArenaReset_nid_postfix(data, 2 * Unit);
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(Address(), 2 * Unit);
        GuestArena::GuestArenaRelease_nid_postfix(data, 2 * Unit);
    }

    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }

private:
    std::byte* data = nullptr;
};

unsigned hookCalls = 0;
std::uint64_t hookAddress = 0;
std::size_t hookBytes = 0;

void CountCompare(std::uint64_t address, std::size_t bytes) {
    ++hookCalls;
    hookAddress = address;
    hookBytes = bytes;
}

void Check(std::string_view mode) {
    Range range;
    std::array<std::byte, 256> expected{};
    expected.fill(std::byte{0x5b});
    if (mode == "unhooked") {
        Require(CompareMapped(range.Address(), expected) == Compare::Equal, "an equal range with no hook installed did not compare equal");
        expected.back() = std::byte{0x5c};
        Require(CompareMapped(range.Address(), expected) == Compare::Differs, "a last-byte difference with no hook installed was missed");
        Require(hookCalls == 0, "a compare ran a hook nobody installed");
        return;
    }
    if (mode == "installed") {
        SetCompareMappedHook(&CountCompare);
        const auto outcome = CompareMapped(range.Address(), expected);
        SetCompareMappedHook(nullptr);
        Require(outcome == Compare::Equal, "a compare with a hook installed did not compare equal");
        Require(hookCalls == 1, "the compare hook did not run once for a one-run compare");
        Require(hookAddress == range.Address() && hookBytes == expected.size(), "the compare hook did not see its run");
        return;
    }
    if (mode == "cleared") {
        SetCompareMappedHook(&CountCompare);
        Require(CompareMapped(range.Address(), expected) == Compare::Equal, "a compare with a hook installed did not compare equal");
        SetCompareMappedHook(nullptr);
        const auto installedCalls = hookCalls;
        Require(CompareMapped(range.Address(), expected) == Compare::Equal && CompareMapped(range.Address(), expected) == Compare::Equal, "a compare after clearing the hook did not compare equal");
        Require(installedCalls == 1 && hookCalls == 1, "a cleared compare hook still ran");
        return;
    }
    if (mode == "other-thread") {
        SetCompareMappedHook(&CountCompare);
        auto other = Compare::Unmapped;
        std::thread([&] { other = CompareMapped(range.Address(), expected); }).join();
        const auto otherCalls = hookCalls;
        const auto own = CompareMapped(range.Address(), expected);
        SetCompareMappedHook(nullptr);
        Require(other == Compare::Equal && own == Compare::Equal, "a compare beside an installed hook did not compare equal");
        Require(otherCalls == 0, "a hook installed on one thread ran for another thread's compare");
        Require(hookCalls == 1, "the installing thread's compare did not run its hook");
        return;
    }
    if (mode == "cleared-elsewhere") {
        SetCompareMappedHook(&CountCompare);
        std::thread([] {
            SetCompareMappedHook(&CountCompare);
            SetCompareMappedHook(nullptr);
        }).join();
        const auto own = CompareMapped(range.Address(), expected);
        SetCompareMappedHook(nullptr);
        Require(own == Compare::Equal, "a compare beside a hook cleared on another thread did not compare equal");
        Require(hookCalls == 1, "clearing a hook on another thread silenced this thread's hook");
        return;
    }
    Require(false, "unknown compare-hook fixture mode");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "compare-hook fixture requires a mode");
        Check(argv[1]);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
