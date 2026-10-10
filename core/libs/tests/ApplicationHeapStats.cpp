#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

extern "C" {
int APS5_VABI malloc_stats_fast_nid_postfix(void*);
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

template<typename TValue, std::size_t TSize>
void write(std::array<std::byte, TSize>& data, std::size_t offset, TValue value) {
    require(offset <= data.size() && sizeof(value) <= data.size() - offset);
    std::memcpy(data.data() + offset, &value, sizeof(value));
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
    if (argc > 1 && std::strcmp(argv[1], "default") == 0) {
        std::memset(replacement.data() + 0x20, 0, 10 * sizeof(void*));
        ApplicationHeapInitialize_nid_no_patch(process.data());
        require(ApplicationHeapAllocate_nid_no_patch(16) != nullptr);
        reject([&] { malloc_stats_fast_nid_postfix(stats.data()); });
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "missing") == 0) {
        write(replacement, 0x60, static_cast<void*>(nullptr));
        ApplicationHeapInitialize_nid_no_patch(process.data());
        require(ApplicationHeapAllocate_nid_no_patch(16) == storage.data());
        reject([&] { malloc_stats_fast_nid_postfix(stats.data()); });
        require(statsCalls == 0);
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
}
