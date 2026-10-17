#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

extern "C" {
int APS5_VABI malloc_stats_fast_nid_postfix(void*);
std::size_t APS5_VABI malloc_usable_size_nid_postfix(void*);
}

namespace {

alignas(64) std::array<std::byte, 256> storage{};
unsigned statsCalls = 0;
void* lastStats = nullptr;
bool statsAllocate = false;

void require(bool condition) {
    if (!condition) throw std::runtime_error("application heap stats test failed");
}

template<typename TAction>
void reject(TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected);
}

void* APS5_VABI allocate(std::size_t) { return storage.data(); }
void APS5_VABI release(void*) {}
void* APS5_VABI reallocate(void*, std::size_t) { return storage.data(); }
void* APS5_VABI allocateZeroed(std::size_t, std::size_t) { return storage.data(); }
void* APS5_VABI align(std::size_t, std::size_t) { return storage.data(); }
void* APS5_VABI realign(void*, std::size_t, std::size_t) { return storage.data(); }
int APS5_VABI posixAlign(void** pointer, std::size_t, std::size_t) {
    *pointer = storage.data();
    return 0;
}

int APS5_VABI statsFast(void* stats) {
    ++statsCalls;
    lastStats = stats;
    if (statsAllocate) ApplicationHeapAllocate_nid_no_patch(16);
    return 0x2a;
}

unsigned usableCalls = 0;
void* lastUsable = nullptr;

std::size_t APS5_VABI usableSize(void* pointer) {
    ++usableCalls;
    lastUsable = pointer;
    return 0x55;
}

template<typename TValue, std::size_t TSize>
void write(std::array<std::byte, TSize>& data, std::size_t offset, TValue value) {
    require(offset <= data.size() && sizeof(value) <= data.size() - offset);
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

template<typename TValue, std::size_t TSize>
TValue read(const std::array<std::byte, TSize>& data, std::size_t offset) {
    TValue value{};
    require(offset <= data.size() && sizeof(value) <= data.size() - offset);
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

}

int main(int argc, char** argv) {
    std::array<std::byte, 0x40> stats{};
    reject([&] { malloc_stats_fast_nid_postfix(stats.data()); });
    std::array<std::byte, 0x40> process{};
    std::array<std::byte, 0x38> libc{};
    std::array<std::byte, 0x78> replacement{};
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, &allocate);
    write(replacement, 0x28, &release);
    write(replacement, 0x30, &allocateZeroed);
    write(replacement, 0x38, &reallocate);
    write(replacement, 0x40, &align);
    write(replacement, 0x48, &realign);
    write(replacement, 0x50, &posixAlign);
    write(replacement, 0x60, &statsFast);
    write(replacement, 0x68, &usableSize);
    if (argc > 1 && std::strcmp(argv[1], "default") == 0) {
        std::memset(replacement.data() + 0x20, 0, 10 * sizeof(void*));
        ApplicationHeapInitialize_nid_no_patch(process.data());
        require(ApplicationHeapAllocate_nid_no_patch(16) != nullptr);
        const auto measure = [&] {
            stats.fill(std::byte{0x5a});
            write(stats, 0, std::uint16_t{0x28});
            write(stats, 2, std::uint16_t{1});
            require(malloc_stats_fast_nid_postfix(stats.data()) == 0);
            const auto maxSystem = read<std::uint64_t>(stats, 0x08), system = read<std::uint64_t>(stats, 0x10);
            const auto maxInUse = read<std::uint64_t>(stats, 0x18), inUse = read<std::uint64_t>(stats, 0x20);
            require(maxSystem >= system && system >= inUse && maxInUse >= inUse && read<std::uint64_t>(stats, 0x28) == 0x5a5a5a5a5a5a5a5aull);
            return std::array<std::uint64_t, 4>{maxSystem, system, maxInUse, inUse};
        };
        const auto initial = measure();
        require(initial[3] >= 16);
        auto* small = ApplicationHeapAllocate_nid_no_patch(16);
        auto* large = ApplicationHeapAllocate_nid_no_patch(100000);
        const auto holding = measure();
        require(holding[3] >= initial[3] + 100016 && holding[1] >= initial[1]);
        require(malloc_usable_size_nid_postfix(small) == 16 && malloc_usable_size_nid_postfix(large) == 100000);
        require(malloc_usable_size_nid_postfix(nullptr) == 0 && usableCalls == 0);
        ApplicationHeapFree_nid_no_patch(small);
        ApplicationHeapFree_nid_no_patch(large);
        const auto released = measure();
        require(released[3] == initial[3] && released[2] == holding[2]);
        write(stats, 0, std::uint16_t{0x27});
        require(malloc_stats_fast_nid_postfix(stats.data()) == 22);
        require(malloc_stats_fast_nid_postfix(nullptr) == 22 && statsCalls == 0);
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "missing") == 0) {
        write(replacement, 0x60, static_cast<void*>(nullptr));
        write(replacement, 0x68, static_cast<void*>(nullptr));
        ApplicationHeapInitialize_nid_no_patch(process.data());
        require(ApplicationHeapAllocate_nid_no_patch(16) == storage.data());
        reject([&] { malloc_stats_fast_nid_postfix(stats.data()); });
        reject([&] { malloc_usable_size_nid_postfix(storage.data()); });
        reject([&] { malloc_usable_size_nid_postfix(nullptr); });
        require(statsCalls == 0 && usableCalls == 0);
        return 0;
    }
    ApplicationHeapInitialize_nid_no_patch(process.data());
    require(malloc_stats_fast_nid_postfix(stats.data()) == 0x2a && statsCalls == 1 && lastStats == stats.data());
    require(malloc_stats_fast_nid_postfix(nullptr) == 0x2a && statsCalls == 2 && lastStats == nullptr);
    statsAllocate = true;
    reject([&] { malloc_stats_fast_nid_postfix(stats.data()); });
    statsAllocate = false;
    require(statsCalls == 3);
    require(malloc_stats_fast_nid_postfix(stats.data()) == 0x2a && statsCalls == 4);
    require(ApplicationHeapAllocate_nid_no_patch(16) == storage.data());
    require(malloc_usable_size_nid_postfix(storage.data()) == 0x55 && usableCalls == 1 && lastUsable == storage.data());
    require(malloc_usable_size_nid_postfix(nullptr) == 0x55 && usableCalls == 2 && lastUsable == nullptr);
}
