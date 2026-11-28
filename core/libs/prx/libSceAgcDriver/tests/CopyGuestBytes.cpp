#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef ANYPS5_ENABLE_ASAN
#include <sanitizer/asan_interface.h>
#endif

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

#if defined(ANYPS5_ENABLE_ASAN) || defined(ANYPS5_ENABLE_TSAN)
template <typename Write, typename Read>
static void RunAgainstWriter(int reads, Write&& write, Read&& read) {
    constexpr std::uint64_t MinimumWrites = 64;
    std::atomic<bool> started{false};
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> writes{0};
    std::thread writer([&] {
        started.store(true, std::memory_order_release);
        for (unsigned round = 1; !stop.load(std::memory_order_relaxed); ++round) {
            write(round);
            writes.fetch_add(1, std::memory_order_relaxed);
        }
    });
    while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
    const auto first = writes.load(std::memory_order_relaxed);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    for (int round = 0; round < reads || writes.load(std::memory_order_relaxed) - first < MinimumWrites; ++round) {
        read();
        if (std::chrono::steady_clock::now() > deadline) break;
    }
    const auto during = writes.load(std::memory_order_relaxed) - first;
    stop.store(true, std::memory_order_relaxed);
    writer.join();
    Require(during >= MinimumWrites, "the writer did not run while the reader read");
}

static void CheckConcurrentAccess() {
    constexpr std::size_t Stable = 3;
    alignas(64) static std::array<unsigned char, 4096> shared{};
    shared.fill(0x01);
    for (std::size_t index = 0; index < Stable; ++index) shared[index] = 0x5a;
    std::array<unsigned char, 4096> pattern{};
    std::array<unsigned char, Stable> prefix{};
    prefix.fill(0x5a);
    std::array<unsigned char, 4096> copy{};
    RunAgainstWriter(
        2000,
        [&](unsigned round) {
            pattern.fill(static_cast<unsigned char>(round * 2u + 1u));
            AgcDriver::GuestMemory::CopyToGuestBytes(shared.data() + Stable, pattern.data(), pattern.size() - Stable);
        },
        [&] {
            AgcDriver::GuestMemory::CopyGuestBytes(copy.data(), shared.data(), copy.size());
            Require(std::memcmp(copy.data(), prefix.data(), Stable) == 0, "a byte the writer never stores was copied wrongly");
            for (std::size_t index = Stable; index < copy.size(); ++index) Require((copy[index] & 1u) != 0, "a copied byte is not one the writer stored");
            Require(AgcDriver::GuestMemory::GuestBytesEqual(shared.data(), prefix.data(), Stable), "bytes the writer never stores compared unequal");
            copy[0] ^= 0x40u;
            Require(!AgcDriver::GuestMemory::GuestBytesEqual(shared.data(), copy.data(), copy.size()), "a byte the writer never stores was not found to differ");
            Require(!AgcDriver::GuestMemory::GuestBytesEqual(shared.data() + Stable + 1, prefix.data(), 1), "a byte the writer stores compared equal to a value it never stores");
        });
}

static void CheckAlignedWordsDoNotTear() {
    alignas(64) static std::array<std::uint64_t, 512> shared{};
    shared.fill(0x0101010101010101ull);
    std::array<std::uint64_t, 512> pattern{};
    std::array<std::uint64_t, 512> copy{};
    RunAgainstWriter(
        4000,
        [&](unsigned round) {
            pattern.fill(0x0101010101010101ull * ((round & 0x7fu) * 2u + 1u));
            AgcDriver::GuestMemory::CopyToGuestBytes(shared.data(), pattern.data(), sizeof(pattern));
        },
        [&] {
            AgcDriver::GuestMemory::CopyGuestBytes(copy.data(), shared.data(), sizeof(copy));
            for (const std::uint64_t word : copy) Require(word == 0x0101010101010101ull * (word & 0xffu) && (word & 1u) != 0, "an aligned 8-byte word was torn");
        });
}
#endif

#ifdef ANYPS5_ENABLE_ASAN
static void CheckReadsPastHostObject() {
    constexpr std::size_t Object = 5;
    auto* const buffer = new unsigned char[64]{1, 2, 3, 4, 5};
    ASAN_POISON_MEMORY_REGION(buffer + Object, 64 - Object);
    Require(__asan_address_is_poisoned(buffer + Object) != 0 && __asan_address_is_poisoned(buffer + Object - 1) == 0, "the bytes past the object were not poisoned");
    std::array<unsigned char, 32> copy{};
    AgcDriver::GuestMemory::CopyGuestBytes(copy.data(), buffer + 1, 31);
    Require(copy[0] == 2 && copy[3] == 5, "the bytes of an object were not copied when the read ran past its end");
    AgcDriver::GuestMemory::CopyGuestBytes(copy.data(), buffer, copy.size());
    Require(copy[0] == 1 && copy[4] == 5, "the bytes of an object were not copied when a word read ran past its end");
    ASAN_UNPOISON_MEMORY_REGION(buffer + Object, 64 - Object);
    delete[] buffer;
}
#endif

int main() {
    CheckCopies();
    CheckComparisons();
#if defined(ANYPS5_ENABLE_ASAN) || defined(ANYPS5_ENABLE_TSAN)
    CheckConcurrentAccess();
    CheckAlignedWordsDoNotTear();
#endif
#ifdef ANYPS5_ENABLE_ASAN
    CheckReadsPastHostObject();
#endif
    std::puts("copy guest bytes: ok");
}
