#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

static void CheckCopies() {
    std::array<unsigned char, 96> source{};
    for (std::size_t index = 0; index < source.size(); ++index) source[index] = static_cast<unsigned char>(index * 37 + 11);
    for (std::size_t from = 0; from < 8; ++from) {
        for (std::size_t to = 0; to < 8; ++to) {
            for (std::size_t bytes = 0; bytes <= 65; ++bytes) {
                for (const bool store : {false, true}) {
                    std::array<unsigned char, 96> destination{};
                    destination.fill(0xa5);
                    if (store) AgcDriver::GuestMemory::CopyToGuestBytes(destination.data() + 1 + to, source.data() + from, bytes);
                    else AgcDriver::GuestMemory::CopyGuestBytes(destination.data() + 1 + to, source.data() + from, bytes);
                    for (std::size_t index = 0; index < 1 + to; ++index) Require(destination[index] == 0xa5, "a byte before the destination was changed");
                    Require(std::memcmp(destination.data() + 1 + to, source.data() + from, bytes) == 0, "the bytes were not copied exactly from every source and destination offset");
                    for (std::size_t index = 1 + to + bytes; index < destination.size(); ++index) Require(destination[index] == 0xa5, "a byte after the destination was changed");
                }
            }
        }
    }
}

static void CheckComparisons() {
    std::array<unsigned char, 80> guest{};
    for (std::size_t index = 0; index < guest.size(); ++index) guest[index] = static_cast<unsigned char>(index * 13 + 5);
    for (std::size_t from = 0; from < 8; ++from) {
        for (std::size_t bytes = 0; bytes <= 65; ++bytes) {
            std::array<unsigned char, 80> expected{};
            std::memcpy(expected.data(), guest.data() + from, bytes);
            Require(AgcDriver::GuestMemory::GuestBytesEqual(guest.data() + from, expected.data(), bytes), "equal bytes compared unequal");
            for (std::size_t at = 0; at < bytes; ++at) {
                expected[at] ^= 0x40u;
                Require(!AgcDriver::GuestMemory::GuestBytesEqual(guest.data() + from, expected.data(), bytes), "a byte that differs was not found");
                expected[at] ^= 0x40u;
            }
        }
    }
}

static void CheckConcurrentAccess() {
    alignas(64) static std::array<unsigned char, 4096> shared{};
    std::atomic<bool> stop{false};
    std::thread writer([&] {
        std::array<unsigned char, 4096> pattern{};
        for (unsigned round = 0; !stop.load(std::memory_order_relaxed); ++round) {
            pattern.fill(static_cast<unsigned char>(round));
            AgcDriver::GuestMemory::CopyToGuestBytes(shared.data() + 3, pattern.data(), pattern.size() - 3);
        }
    });
    std::array<unsigned char, 4096> copy{};
    for (int round = 0; round < 2000; ++round) {
        AgcDriver::GuestMemory::CopyGuestBytes(copy.data(), shared.data(), copy.size());
        static_cast<void>(AgcDriver::GuestMemory::GuestBytesEqual(shared.data(), copy.data(), copy.size()));
    }
    stop.store(true, std::memory_order_relaxed);
    writer.join();
}

int main() {
    CheckCopies();
    CheckComparisons();
    CheckConcurrentAccess();
    std::puts("copy guest bytes: ok");
}
