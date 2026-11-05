#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "../../src/WtfTextAbi.hpp"

namespace {

constexpr std::size_t GuardSize = 16;
constexpr std::size_t LargestExpectedAllocation = std::size_t(1) << 24;
constexpr unsigned char GuardByte = 0xA5;

std::unordered_map<void*, std::size_t> liveAllocations;
int allocationsBeforeFailure = -1;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF text allocation test failed at line %u\n", location.line());
        std::abort();
    }
}

void* Place(std::size_t size) {
    Check(size <= LargestExpectedAllocation);
    if (allocationsBeforeFailure == 0) return nullptr;
    if (allocationsBeforeFailure > 0) --allocationsBeforeFailure;
    auto* allocation = static_cast<unsigned char*>(std::malloc(size + GuardSize));
    Check(allocation != nullptr);
    std::memset(allocation + size, GuardByte, GuardSize);
    liveAllocations.emplace(allocation, size);
    return allocation;
}

}  // namespace

namespace NKWebKit {

void* Allocate(std::size_t size, std::size_t) {
    void* allocation = Place(size);
    if (!allocation) {
        std::fprintf(stderr, "WTF text allocation test: fatal allocation failed\n");
        std::abort();
    }
    return allocation;
}

void* TryAllocate(std::size_t size, std::size_t) {
    return Place(size);
}

void Deallocate(void* ptr) {
    if (!ptr) return;
    auto allocation = liveAllocations.find(ptr);
    Check(allocation != liveAllocations.end());
    const auto* guard = static_cast<const unsigned char*>(ptr) + allocation->second;
    for (std::size_t i = 0; i < GuardSize; ++i) Check(guard[i] == GuardByte);
    liveAllocations.erase(allocation);
    std::free(ptr);
}

}  // namespace NKWebKit

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;

template<typename T> void Throws(T action, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try { action(); } catch (const std::length_error&) { threw = true; }
    Check(threw, location);
}

StringBuilder NewBuilder(bool crashOnOverflow) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = crashOnOverflow;
    builder.is8Bit = true;
    return builder;
}

void Free(StringBuilder& builder) {
    NKWebKit::WtfAbi::Release(builder.buffer);
    builder.buffer = nullptr;
    builder.string = String();
}

void Append8(StringBuilder& builder, const char* data) {
    _ZN3WTF13StringBuilder16appendCharactersEPKhj(&builder, reinterpret_cast<const unsigned char*>(data), static_cast<unsigned>(std::strlen(data)));
}

void Append16(StringBuilder& builder, const char16_t* data) {
    _ZN3WTF13StringBuilder16appendCharactersEPKDsj(&builder, data, static_cast<unsigned>(std::char_traits<char16_t>::length(data)));
}

void Reserve(StringBuilder& builder, unsigned capacity) {
    _ZN3WTF13StringBuilder15reserveCapacityEj(&builder, capacity);
}

void FailFrom(int successfulAllocations) {
    allocationsBeforeFailure = successfulAllocations;
}

void Restore() {
    allocationsBeforeFailure = -1;
}

template<typename TSetup, typename TFailing, typename TRecorded> void Case(TSetup setup, TFailing failing, TRecorded recorded, String* kept = nullptr) {
    const auto baseline = liveAllocations.size();
    auto recording = NewBuilder(false);
    setup(recording);
    FailFrom(0);
    failing(recording);
    Restore();
    Check(recording.overflowed);
    recorded(recording);
    Free(recording);
    if (kept) *kept = String();
    Check(liveAllocations.size() == baseline);

    auto crashing = NewBuilder(true);
    setup(crashing);
    FailFrom(0);
    Throws([&] { failing(crashing); });
    Restore();
    Check(crashing.overflowed);
    Free(crashing);
    if (kept) *kept = String();
    Check(liveAllocations.size() == baseline);
}

void TestInitialAllocation() {
    Case([](StringBuilder&) {}, [](StringBuilder& builder) { Append8(builder, "abc"); },
        [](StringBuilder& builder) { Check(!builder.buffer && !builder.length && builder.is8Bit && !builder.characters8); });
}

void TestGrowth() {
    Case([](StringBuilder& builder) { Append8(builder, "abcdefghijklmnop"); }, [](StringBuilder& builder) { Append8(builder, "q"); },
        [](StringBuilder& builder) { Check(!builder.buffer && builder.length == 16); });
}

void TestSharedGrowth() {
    String shared;
    Case([&](StringBuilder& builder) {
            Append8(builder, "abcdefghijklmnop");
            shared = NKWebKit::WtfAbi::ToString(builder);
            Check(shared.impl == builder.buffer);
        },
        [](StringBuilder& builder) { Append8(builder, "q"); },
        [&](StringBuilder& builder) {
            Check(builder.buffer == shared.impl && builder.length == 16 && !builder.string.impl && !builder.characters8);
            Check(shared.impl->length == 16 && std::memcmp(shared.impl->data, "abcdefghijklmnop", 16) == 0);
        }, &shared);
}

void TestWideSharedGrowth() {
    String shared;
    Case([&](StringBuilder& builder) {
            Append16(builder, u"\x263a\x263b\x263c\x263d\x263e\x263f\x2640\x2641\x2642\x2643\x2644\x2645\x2646\x2647\x2648\x2649");
            shared = NKWebKit::WtfAbi::ToString(builder);
            Check(!builder.is8Bit && shared.impl == builder.buffer && builder.length == 16);
        },
        [](StringBuilder& builder) { Append16(builder, u"\x264a"); },
        [&](StringBuilder& builder) {
            Check(builder.buffer == shared.impl && builder.length == 16 && !builder.string.impl && !builder.characters16);
            Check(static_cast<const char16_t*>(shared.impl->data)[15] == u'\x2649');
        }, &shared);
}

void TestLengthLimit() {
    const auto baseline = liveAllocations.size();
    auto recording = NewBuilder(false);
    Append16(recording, u"\x263a");
    Reserve(recording, 0x7FFFFFFF);
    Check(recording.overflowed);
    Free(recording);
    auto crashing = NewBuilder(true);
    Append16(crashing, u"\x263a");
    Throws([&] { Reserve(crashing, 0x7FFFFFFF); });
    Check(crashing.overflowed);
    Free(crashing);
    Check(liveAllocations.size() == baseline);
}

void TestReserveCapacity() {
    Case([](StringBuilder&) {}, [](StringBuilder& builder) { Reserve(builder, 100); },
        [](StringBuilder& builder) { Check(!builder.buffer && !builder.length && !builder.characters8); });
    Case([](StringBuilder& builder) { Append8(builder, "abc"); }, [](StringBuilder& builder) { Reserve(builder, 100); },
        [](StringBuilder& builder) { Check(!builder.buffer && builder.length == 3); });
}

void TestUpConversion() {
    Case([](StringBuilder& builder) { Append8(builder, "abc"); }, [](StringBuilder& builder) { Append16(builder, u"\x263a"); },
        [](StringBuilder& builder) { Check(builder.is8Bit && builder.buffer && builder.buffer->length == 16 && builder.length == 3 && !builder.characters16); });
}

}  // namespace

int main() {
    TestInitialAllocation();
    TestGrowth();
    TestSharedGrowth();
    TestWideSharedGrowth();
    TestReserveCapacity();
    TestLengthLimit();
    TestUpConversion();
    Check(liveAllocations.empty());
    return 0;
}
