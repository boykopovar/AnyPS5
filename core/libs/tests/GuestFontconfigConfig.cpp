#include "prx/libfontconfig/include/Config.hpp"

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* name) {
    throw std::runtime_error(name);
}

static_assert(std::is_same_v<decltype(FcConfigCreate()), FcConfig*>);
static_assert(std::is_same_v<decltype(FcConfigReference(nullptr)), FcConfig*>);
static_assert(std::is_same_v<decltype(FcConfigDestroy(nullptr)), void>);

int main() {
    FcConfigDestroy(nullptr);
#ifndef _WIN32
    bool unsupportedCurrent = false;
    try {
        FcConfigReference(nullptr);
    } catch (const std::runtime_error&) {
        unsupportedCurrent = true;
    }
    assert(unsupportedCurrent);
#endif
    FcConfig* initial = FcConfigCreate();
    assert(initial);
    constexpr int kCopies = 128;
    std::array<FcConfig*, kCopies> copies{};
    for (int i = 0; i < kCopies; ++i)
        copies[i] = FcConfigReference(initial);
    for (FcConfig* copy : copies) assert(copy == initial);
    FcConfigDestroy(initial);
    for (int i = 0; i < kCopies - 1; ++i)
        FcConfigDestroy(copies[i]);
    FcConfig* retained = FcConfigReference(copies.back());
    assert(retained == copies.back());
    FcConfigDestroy(copies.back());
    FcConfigDestroy(copies.back());

    constexpr int kWorkers = 8;
    constexpr int kIterations = 2000;
    FcConfig* shared = FcConfigCreate();
    assert(shared);
    std::vector<std::thread> workers;
    workers.reserve(kWorkers);
    for (int i = 0; i < kWorkers; ++i)
        workers.emplace_back([shared] {
            for (int n = 0; n < kIterations; ++n) {
                FcConfig* copy = FcConfigReference(shared);
                assert(copy == shared);
                FcConfigDestroy(copy);
            }
        });
    for (std::thread& worker : workers) worker.join();
    FcConfigDestroy(shared);

    for (int i = 0; i < 100; ++i) {
        FcConfig* config = FcConfigCreate();
        assert(config);
        FcConfigDestroy(config);
    }
}
