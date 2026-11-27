#include "prx/libc/include/Api.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <thread>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename F>
static void Reject(F&& call, std::source_location location = std::source_location::current()) {
    bool rejected = false;
    try { call(); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, location);
}

int main() {
    std::array<std::byte, 32> initial{};
    for (std::size_t i = 0; i < initial.size(); ++i)
        initial[i] = static_cast<std::byte>(i + 1);
    GuestEmutlsControl control{initial.size(), 64, 0, initial.data()};
    errno = E2BIG;
    auto* first = static_cast<std::byte*>(__emutls_get_address_nid_postfix(&control));
    Require(first && reinterpret_cast<std::uintptr_t>(first) % 64 == 0);
    Require(control.index != 0 && errno == E2BIG);
    for (std::size_t i = 0; i < initial.size(); ++i) Require(first[i] == initial[i]);
    first[0] = std::byte{0xa5};
    Require(__emutls_get_address_nid_postfix(&control) == first && first[0] == std::byte{0xa5});

    GuestEmutlsControl zero{16, 0, 0, nullptr};
    auto* zeroBytes = static_cast<std::byte*>(__emutls_get_address_nid_postfix(&zero));
    Require(zeroBytes && zero.index != 0 && zero.index != control.index);
    for (std::size_t i = 0; i < zero.size; ++i) Require(zeroBytes[i] == std::byte{});

    GuestEmutlsControl concurrent{initial.size(), 64, 0, initial.data()};
    std::array<void*, 2> addresses{};
    std::atomic<bool> start{false};
    std::barrier hold{3};
    std::array<std::thread, 2> workers;
    for (std::size_t i = 0; i < workers.size(); ++i) {
        workers[i] = std::thread([&, i] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            auto* bytes = static_cast<std::byte*>(__emutls_get_address_nid_postfix(&concurrent));
            Require(bytes && bytes[0] == initial[0]);
            bytes[0] = static_cast<std::byte>(i + 0x40);
            addresses[i] = bytes;
            hold.arrive_and_wait();
            Require(__emutls_get_address_nid_postfix(&concurrent) == bytes);
            hold.arrive_and_wait();
        });
    }
    start.store(true, std::memory_order_release);
    hold.arrive_and_wait();
    Require(concurrent.index != 0 && addresses[0] && addresses[1]);
    Require(addresses[0] != addresses[1] && addresses[0] != first && addresses[1] != first);
    hold.arrive_and_wait();
    for (auto& worker : workers) worker.join();
    auto* mainBytes = static_cast<std::byte*>(__emutls_get_address_nid_postfix(&concurrent));
    Require(mainBytes && mainBytes != first && mainBytes[0] == initial[0]);

    Reject([&] { __emutls_get_address_nid_postfix(nullptr); });
    Reject([&] { __emutls_get_address_nid_postfix(reinterpret_cast<GuestEmutlsControl*>(1)); });
    GuestEmutlsControl badAlignment{8, 12, 0, nullptr};
    Reject([&] { __emutls_get_address_nid_postfix(&badAlignment); });
    GuestEmutlsControl badTemplate{8, 8, 0, reinterpret_cast<const void*>(1)};
    Reject([&] { __emutls_get_address_nid_postfix(&badTemplate); });

    GuestEmutlsControl tooLarge{std::numeric_limits<std::uintptr_t>::max() / 2, 64, 0, nullptr};
    bool allocationFailed = false;
    try { __emutls_get_address_nid_postfix(&tooLarge); }
    catch (const std::bad_alloc&) { allocationFailed = true; }
    Require(allocationFailed);

    const auto assignedIndex = tooLarge.index;
    tooLarge.size = 16;
    auto* retried = static_cast<std::byte*>(__emutls_get_address_nid_postfix(&tooLarge));
    Require(assignedIndex != 0 && tooLarge.index == assignedIndex);
    Require(retried && reinterpret_cast<std::uintptr_t>(retried) % 64 == 0);
    for (std::size_t i = 0; i < tooLarge.size; ++i) Require(retried[i] == std::byte{});
    Require(__emutls_get_address_nid_postfix(&tooLarge) == retried);

    constexpr std::size_t controlCount = 12;
    std::array<GuestEmutlsControl, controlCount> controls{};
    for (auto& entry : controls) {
        entry.size = 1;
        Require(__emutls_get_address_nid_postfix(&entry) != nullptr);
    }
    Require(__emutls_get_address_nid_postfix(&control) == first);
    std::thread reverseOrder([&] {
        std::array<void*, controlCount> reverseAddresses{};
        for (std::size_t i = controls.size(); i-- > 0;) {
            auto* bytes = static_cast<std::byte*>(__emutls_get_address_nid_postfix(&controls[i]));
            Require(bytes && bytes[0] == std::byte{});
            bytes[0] = static_cast<std::byte>(i + 1);
            reverseAddresses[i] = bytes;
        }
        for (std::size_t i = 0; i < controls.size(); ++i) {
            auto* bytes = static_cast<std::byte*>(__emutls_get_address_nid_postfix(&controls[i]));
            Require(bytes == reverseAddresses[i] && bytes[0] == static_cast<std::byte>(i + 1));
        }
    });
    reverseOrder.join();
}
