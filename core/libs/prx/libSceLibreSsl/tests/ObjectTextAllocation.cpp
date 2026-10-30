#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <vector>

namespace {
thread_local bool armed = false, fired = false;
thread_local std::size_t failAt = 0, allocations = 0;
thread_local std::array<void*, 256> tracked{};
std::size_t failures = 0, checks = 0, faultCases = 0, escaped = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (value) return;
    ++failures;
    if (failures <= 8) std::fprintf(stderr, "FAIL: %s\n", label);
}
std::size_t Live() {
    return std::count_if(tracked.begin(), tracked.end(), [](void* p) { return p != nullptr; });
}
}

void* operator new(std::size_t size) {
    if (armed) {
        ++allocations;
        if (failAt == allocations) { fired = true; throw std::bad_alloc(); }
    }
    void* pointer = std::malloc(size ? size : 1);
    if (!pointer) throw std::bad_alloc();
    if (armed) {
        auto slot = std::find(tracked.begin(), tracked.end(), nullptr);
        if (slot == tracked.end()) std::abort();
        *slot = pointer;
    }
    return pointer;
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept {
    if (pointer) {
        auto slot = std::find(tracked.begin(), tracked.end(), pointer);
        if (slot != tracked.end()) *slot = nullptr;
    }
    std::free(pointer);
}
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

struct NumericRun {
    std::array<char, 512> buffer;
    int returned;
    bool threw, allocationFailed;
    std::size_t calls, live;
    std::uint64_t error, following;
};
NumericRun Run(const unsigned char* data, int length, int space, std::size_t fail, bool seed = false) {
    NumericRun result{};
    result.buffer.fill('Z');
    LibreSslAsn1Object object{nullptr, nullptr, 0, length, data, 0};
    ERR_clear_error_nid_postfix();
    if (seed) LibreSslPutError_nid_no_patch(8, 0xfff, 101);
    allocations = 0; failAt = fail; fired = false;
    armed = true;
    try { result.returned = i2t_ASN1_OBJECT_nid_postfix(result.buffer.data(), space, &object); }
    catch (const std::bad_alloc&) { result.threw = true; }
    armed = false;
    result.calls = allocations;
    result.allocationFailed = fired;
    result.live = Live();
    result.error = ERR_get_error_nid_postfix();
    result.following = ERR_get_error_nid_postfix();
    return result;
}

int main() {
    constexpr std::uint64_t allocationError = (3ull << 24) | (0xfffull << 12) | 65;
    std::size_t fastCases = 0;
    const unsigned char warmUp[] {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
    Run(warmUp, sizeof(warmUp), 0, 1);
    tracked.fill(nullptr);
    const unsigned char shortOid[] {0x2a, 0x03, 0x04};
    const unsigned char maxWord[] {0x81,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
    const unsigned char malformed[] {0x2a,0x81};
    for (const auto bytes : {std::pair{shortOid, sizeof(shortOid)}, std::pair{maxWord, sizeof(maxWord)}, std::pair{malformed, sizeof(malformed)}}) {
        for (int space : {0, 1, 2, 5, 32, 512}) {
            const auto expected = Run(bytes.first, static_cast<int>(bytes.second), space, 0);
            const auto result = Run(bytes.first, static_cast<int>(bytes.second), space, 1);
            ++fastCases;
            Check(!result.threw && result.returned == expected.returned && result.buffer == expected.buffer, "word/malformed path works when host new would fail");
            Check(result.calls == 0 && !result.allocationFailed, "word/malformed path does not allocate");
            Check(result.live == 0 && result.error == 0, "word/malformed path preserves empty error queue");
            escaped += result.threw;
        }
    }
    for (int size : {11, 20, 40}) {
        for (bool later : {false, true}) {
            std::vector<unsigned char> bytes;
            if (later) bytes = {0x2a, 0x03};
            bytes.insert(bytes.end(), static_cast<std::size_t>(size), 0xff);
            bytes.back() = 0x7f;
            for (int space : {0, 1, 5, 512}) {
                const auto normal = Run(bytes.data(), static_cast<int>(bytes.size()), space, 0);
                for (std::size_t point = 1; point <= normal.calls; ++point) {
                    const auto result = Run(bytes.data(), static_cast<int>(bytes.size()), space, point);
                    ++faultCases;
                    escaped += result.threw;
                    Check(result.allocationFailed, "selected big-number allocation actually failed");
                    Check(!result.threw && result.returned == 0, "big-number allocation failure returns zero");
                    Check(result.error == allocationError && result.following == 0, "big-number allocation failure uses BN malloc error");
                    Check(result.live == 0, "failed conversion releases scratch allocations");
                    if (space > 0) {
                        const void* end = std::memchr(result.buffer.data(), 0, static_cast<std::size_t>(space));
                        Check(end != nullptr, "failed conversion leaves output terminated");
                    }
                }
            }
        }
    }
    const unsigned char big[] {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
    const auto seeded = Run(big, sizeof(big), 64, 1, true);
    escaped += seeded.threw;
    Check(!seeded.threw && seeded.returned == 0 && seeded.error == ((8ull << 24) | (0xfffull << 12) | 101) && seeded.following == allocationError,
          "allocation error appends to existing queue entries");
    Check(ERR_get_error_nid_postfix() == 0 && seeded.live == 0, "seeded failure adds exactly one error and leaks nothing");
    std::printf("{\"fast_cases\":%zu,\"allocation_fault_cases\":%zu,\"checks\":%zu,\"failures\":%zu,\"escaped\":%zu,\"final_live\":%zu}\n",
                fastCases, faultCases, checks, failures, escaped, Live());
    return failures ? 1 : 0;
}
