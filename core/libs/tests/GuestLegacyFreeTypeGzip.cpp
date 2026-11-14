#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libfreetype/include/GuestStream.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

constexpr std::int32_t InvalidArgument = 0x06;
constexpr std::int32_t InvalidTable = 0x08;
constexpr std::int32_t ArrayTooLarge = 0x0a;

constexpr char Text[] = "AnyPS5 relinks PS5 executables.";
constexpr std::uint64_t TextLength = sizeof(Text) - 1;

constexpr std::array<std::uint8_t, 48> Gzip{
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x73, 0xcc, 0xab, 0x0c, 0x08, 0x36,
    0x55, 0x28, 0x4a, 0xcd, 0xc9, 0xcc, 0xcb, 0x2e, 0x56, 0x00, 0xb1, 0x53, 0x2b, 0x52, 0x93, 0x4b,
    0x4b, 0x12, 0x93, 0x72, 0x52, 0x8b, 0xf5, 0x00, 0x8e, 0xfd, 0x1b, 0x9a, 0x1f, 0x00, 0x00, 0x00};

constexpr std::array<std::uint8_t, 36> Zlib{
    0x78, 0x9c, 0x73, 0xcc, 0xab, 0x0c, 0x08, 0x36, 0x55, 0x28, 0x4a, 0xcd, 0xc9, 0xcc, 0xcb, 0x2e,
    0x56, 0x00, 0xb1, 0x53, 0x2b, 0x52, 0x93, 0x4b, 0x4b, 0x12, 0x93, 0x72, 0x52, 0x8b, 0xf5, 0x00,
    0xab, 0xac, 0x0a, 0xf4};

constexpr std::array<std::uint8_t, 81> LargeGzip{
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0a, 0xed, 0xc1, 0x01, 0x0d, 0x00, 0x00,
    0x00, 0xc2, 0xa0, 0x6c, 0xef, 0x5f, 0xca, 0x1c, 0x6e, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xbf, 0x01, 0x66, 0xc0, 0x21, 0x15, 0x00, 0xa0, 0x00,
    0x00};

struct Counter {
    int allocations = 0;
    int frees = 0;
    int limit = -1;
};

void Require(bool value) {
    if (!value) std::abort();
}

void* APS5_VABI GuestAlloc(FreeType::GuestMemoryRec* memory, std::int64_t size) {
    auto* counter = static_cast<Counter*>(memory->user);
    if (counter->allocations == counter->limit) return nullptr;
    ++counter->allocations;
    return std::malloc(static_cast<std::size_t>(size));
}

void APS5_VABI GuestFree(FreeType::GuestMemoryRec* memory, void* block) {
    ++static_cast<Counter*>(memory->user)->frees;
    std::free(block);
}

void* APS5_VABI GuestRealloc(FreeType::GuestMemoryRec*, std::int64_t, std::int64_t, void*) {
    std::abort();
}

template <std::size_t N>
std::int32_t Uncompress(Counter& counter, const std::array<std::uint8_t, N>& input, std::uint8_t* output, std::uint64_t& length) {
    FreeType::GuestMemoryRec memory{&counter, GuestAlloc, GuestFree, GuestRealloc};
    return FT_Gzip_Uncompress_nid_postfix(&memory, output, &length, input.data(), input.size());
}

void CheckDecodes(const auto& input) {
    Counter counter;
    std::array<std::uint8_t, 64> output{};
    std::uint64_t length = output.size();
    Require(Uncompress(counter, input, output.data(), length) == 0);
    Require(length == TextLength && std::memcmp(output.data(), Text, TextLength) == 0);
    Require(counter.allocations > 0 && counter.allocations == counter.frees);
}

std::uint64_t APS5_VABI ReadSource(LegacyFreeType::GuestStreamRec* stream, std::uint64_t offset, unsigned char* buffer, std::uint64_t count) {
    if (offset > stream->size) return count == 0 ? 1 : 0;
    if (count == 0) return 0;
    const auto length = std::min(count, stream->size - offset);
    std::memcpy(buffer, static_cast<const unsigned char*>(stream->descriptor.pointer) + offset, static_cast<std::size_t>(length));
    return length;
}

void CheckStreams() {
    Counter counter;
    FreeType::GuestMemoryRec memory{&counter, GuestAlloc, GuestFree, GuestRealloc};
    auto small = Gzip;
    LegacyFreeType::GuestStreamRec source{};
    source.base = small.data();
    source.size = small.size();
    source.memory = &memory;
    LegacyFreeType::GuestStreamRec output{};
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == 0);
    Require(output.size == TextLength && output.base != nullptr && output.read == nullptr && output.close != nullptr);
    Require(std::memcmp(output.base, Text, TextLength) == 0);
    output.close(&output);
    Require(counter.allocations > 0 && counter.allocations == counter.frees);

    auto large = LargeGzip;
    source = {};
    source.descriptor.pointer = large.data();
    source.size = large.size();
    source.read = ReadSource;
    source.memory = &memory;
    output = {};
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == 0);
    Require(output.base == nullptr && output.read != nullptr && output.close != nullptr && output.size == 40960);
    std::array<unsigned char, 128> decoded{};
    Require(output.read(&output, 0, decoded.data(), decoded.size()) == decoded.size());
    Require(std::all_of(decoded.begin(), decoded.end(), [](unsigned char byte) { return byte == 'A'; }));
    Require(output.read(&output, 40900, decoded.data(), 60) == 60);
    Require(std::all_of(decoded.begin(), decoded.begin() + 60, [](unsigned char byte) { return byte == 'A'; }));
    output.close(&output);
    Require(counter.allocations == counter.frees);
    Require(FT_Stream_OpenGzip_nid_postfix(nullptr, &source) == FT_Err_Invalid_Stream_Handle);
    Require(FT_Stream_OpenGzip_nid_postfix(&output, nullptr) == FT_Err_Invalid_Stream_Handle);
}

}

int main() {
    CheckDecodes(Gzip);
    CheckDecodes(Zlib);
    CheckStreams();

    Counter counter;
    FreeType::GuestMemoryRec memory{&counter, GuestAlloc, GuestFree, GuestRealloc};
    std::array<std::uint8_t, 64> output{};
    std::uint64_t length = output.size();
    Require(FT_Gzip_Uncompress_nid_postfix(nullptr, output.data(), &length, Gzip.data(), Gzip.size()) == InvalidArgument);
    Require(FT_Gzip_Uncompress_nid_postfix(&memory, nullptr, &length, Gzip.data(), Gzip.size()) == InvalidArgument);
    Require(FT_Gzip_Uncompress_nid_postfix(&memory, output.data(), nullptr, Gzip.data(), Gzip.size()) == InvalidArgument);
    Require(length == output.size() && counter.allocations == 0);

    length = TextLength - 1;
    Require(Uncompress(counter, Gzip, output.data(), length) == ArrayTooLarge);
    Require(length == TextLength - 1 && counter.allocations == counter.frees);

    auto corrupt = Gzip;
    corrupt[10] = 0xff;
    length = output.size();
    Require(Uncompress(counter, corrupt, output.data(), length) == InvalidTable);
    Require(length == output.size() && counter.allocations == counter.frees);

    if constexpr (std::numeric_limits<unsigned long>::max() < std::numeric_limits<std::uint64_t>::max()) {
        const std::uint64_t tooLarge = std::uint64_t{std::numeric_limits<unsigned long>::max()} + 1;
        length = tooLarge;
        Require(Uncompress(counter, Gzip, output.data(), length) == InvalidArgument && length == tooLarge);
        length = output.size();
        Require(FT_Gzip_Uncompress_nid_postfix(&memory, output.data(), &length, Gzip.data(), tooLarge) == InvalidArgument);
        Require(length == output.size() && counter.allocations == counter.frees);
    }

    Counter initFails;
    initFails.limit = 0;
    length = output.size();
    Require(Uncompress(initFails, Gzip, output.data(), length) == InvalidArgument);
    Require(length == output.size());
    return 0;
}
