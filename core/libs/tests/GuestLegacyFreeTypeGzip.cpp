#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libfreetype/include/GuestStream.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t InvalidFileFormat = 0x03;
constexpr std::int32_t InvalidArgument = 0x06;
constexpr std::int32_t InvalidTable = 0x08;
constexpr std::int32_t ArrayTooLarge = 0x0a;
constexpr std::int32_t OutOfMemory = 0x40;
constexpr std::int32_t InvalidStreamOperation = 0x55;

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

constexpr std::array<std::uint8_t, 8> Plain{'n', 'o', 't', ' ', 'g', 'z', 'i', 'p'};

constexpr std::array<std::uint8_t, 5> Stored{0x41, 0x00, 0xff, 0xff, 0x00};

constexpr std::array<std::uint8_t, 21> StoredThenInvalid{
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x05, 0x00, 0xfa, 0xff, 0x41,
    0x00, 0xff, 0xff, 0x00, 0x07};

struct Counter {
    int allocations = 0;
    int frees = 0;
    int limit = -1;
    int failures = 0;
    std::set<void*> live;
};

void Require(bool value) {
    if (!value) std::abort();
}

template <class F>
bool Rejects(F call) {
    errno = 71;
    try {
        call();
    } catch (const std::runtime_error&) {
        return errno == 71;
    }
    return false;
}

class ReadOnlyPage {
    void* page = nullptr;
    std::size_t size = 4096;

public:
    explicit ReadOnlyPage(std::uint64_t initialValue) {
#ifdef _WIN32
        page = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(page != nullptr);
        std::memcpy(page, &initialValue, sizeof(initialValue));
        DWORD oldProtect = 0;
        Require(VirtualProtect(page, size, PAGE_READONLY, &oldProtect) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096;
        page = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(page != MAP_FAILED);
        std::memcpy(page, &initialValue, sizeof(initialValue));
        Require(mprotect(page, size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!page) return;
#ifdef _WIN32
        VirtualFree(page, 0, MEM_RELEASE);
#else
        munmap(page, size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    template <typename T>
    T* As() const {
        return static_cast<T*>(page);
    }
};

void* APS5_VABI GuestAlloc(FreeType::GuestMemoryRec* memory, std::int64_t size) {
    auto* counter = static_cast<Counter*>(memory->user);
    if (counter->allocations == counter->limit) return nullptr;
    if (counter->failures > 0) {
        --counter->failures;
        return nullptr;
    }
    ++counter->allocations;
    void* block = std::malloc(static_cast<std::size_t>(size));
    counter->live.insert(block);
    return block;
}

void APS5_VABI GuestFree(FreeType::GuestMemoryRec* memory, void* block) {
    auto* counter = static_cast<Counter*>(memory->user);
    ++counter->frees;
    counter->live.erase(block);
    std::free(block);
}

void ReleaseLeaks(Counter& counter) {
    for (void* block : counter.live) std::free(block);
    counter.live.clear();
}

template <class TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
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

using Output = std::array<std::uint8_t, 64>;

bool Holds(const Output& output, const void* bytes, std::size_t count) {
    return std::memcmp(output.data(), bytes, count) == 0 &&
        std::all_of(output.begin() + static_cast<std::ptrdiff_t>(count), output.end(), [](std::uint8_t byte) { return byte == 0x5a; });
}

void CheckUncompressOutput() {
    Counter counter;
    FreeType::GuestMemoryRec memory{&counter, GuestAlloc, GuestFree, GuestRealloc};
    Output untouched{};
    untouched.fill(0x5a);

    Output output = untouched;
    std::uint64_t length = output.size();
    Require(FT_Gzip_Uncompress_nid_postfix(&memory, output.data(), &length, nullptr, Gzip.size()) == 0);
    Require(length == output.size() && output == untouched);
    Require(counter.allocations > 0 && counter.allocations == counter.frees);
    Require(FT_Gzip_Uncompress_nid_postfix(&memory, output.data(), &length, nullptr, 0) == ArrayTooLarge);
    Require(length == output.size() && output == untouched && counter.allocations == counter.frees);
    Counter initFails;
    initFails.limit = 0;
    FreeType::GuestMemoryRec failing{&initFails, GuestAlloc, GuestFree, GuestRealloc};
    Require(FT_Gzip_Uncompress_nid_postfix(&failing, output.data(), &length, nullptr, Gzip.size()) == InvalidArgument);
    Require(length == output.size() && output == untouched && initFails.frees == 0);

    length = TextLength;
    Require(Uncompress(counter, Gzip, output.data(), length) == 0);
    Require(length == TextLength && Holds(output, Text, TextLength));

    output = untouched;
    length = TextLength - 1;
    Require(Uncompress(counter, Gzip, output.data(), length) == ArrayTooLarge);
    Require(length == TextLength - 1 && Holds(output, Text, TextLength - 1));
    Require(counter.allocations == counter.frees);

    output = untouched;
    length = 0;
    Require(Uncompress(counter, Gzip, output.data(), length) == ArrayTooLarge);
    Require(length == 0 && output == untouched && counter.allocations == counter.frees);

    std::array<std::uint8_t, 30> truncated{};
    std::copy_n(Gzip.begin(), truncated.size(), truncated.begin());
    length = output.size();
    Require(Uncompress(counter, truncated, output.data(), length) == ArrayTooLarge);
    Require(length == output.size() && Holds(output, Text, 21));
    Require(counter.allocations == counter.frees);

    auto badCheck = Gzip;
    badCheck[40] ^= 0xff;
    output = untouched;
    length = output.size();
    Require(Uncompress(counter, badCheck, output.data(), length) == InvalidTable);
    Require(length == output.size() && Holds(output, Text, TextLength));
    Require(counter.allocations == counter.frees);

    output = untouched;
    length = output.size();
    Require(Uncompress(counter, StoredThenInvalid, output.data(), length) == InvalidTable);
    Require(length == output.size() && Holds(output, Stored.data(), Stored.size()));
    Require(counter.allocations == counter.frees);

    Counter windowFails;
    windowFails.limit = 1;
    output = untouched;
    length = TextLength - 1;
    Require(Uncompress(windowFails, Gzip, output.data(), length) == OutOfMemory);
    Require(length == TextLength - 1 && Holds(output, Text, TextLength - 1));
    Require(windowFails.allocations == 1 && windowFails.frees == 1);

    auto corrupt = Gzip;
    corrupt[10] = 0xff;
    output = untouched;
    length = output.size();
    Require(Uncompress(counter, corrupt, output.data(), length) == InvalidTable);
    Require(length == output.size() && output == untouched && counter.allocations == counter.frees);
}

std::uint64_t APS5_VABI ReadSource(LegacyFreeType::GuestStreamRec* stream, std::uint64_t offset, unsigned char* buffer, std::uint64_t count) {
    if (offset > stream->size) return count == 0 ? 1 : 0;
    if (count == 0) return 0;
    const auto length = std::min(count, stream->size - offset);
    std::memcpy(buffer, static_cast<const unsigned char*>(stream->descriptor.pointer) + offset, static_cast<std::size_t>(length));
    return length;
}

std::uint64_t APS5_VABI RefuseSeeks(LegacyFreeType::GuestStreamRec* stream, std::uint64_t offset, unsigned char* buffer, std::uint64_t count) {
    return count == 0 ? 1 : ReadSource(stream, offset, buffer, count);
}

std::uint64_t APS5_VABI ReadTooMuch(LegacyFreeType::GuestStreamRec* stream, std::uint64_t offset, unsigned char* buffer, std::uint64_t count) {
    return count == 0 ? 0 : ReadSource(stream, offset, buffer, count) + 1;
}

LegacyFreeType::GuestStreamRec MemorySource(unsigned char* bytes, std::size_t size, FreeType::GuestMemoryRec* memory) {
    LegacyFreeType::GuestStreamRec source{};
    source.base = bytes;
    source.size = size;
    source.memory = memory;
    return source;
}

LegacyFreeType::GuestStreamRec CallbackSource(unsigned char* bytes, std::size_t size, FreeType::GuestMemoryRec* memory) {
    LegacyFreeType::GuestStreamRec source{};
    source.descriptor.pointer = bytes;
    source.size = size;
    source.read = ReadSource;
    source.memory = memory;
    return source;
}

LegacyFreeType::GuestStreamRec Filled() {
    LegacyFreeType::GuestStreamRec stream;
    std::memset(&stream, 0x5a, sizeof(stream));
    return stream;
}

bool Same(const LegacyFreeType::GuestStreamRec& first, const LegacyFreeType::GuestStreamRec& second) {
    return std::memcmp(&first, &second, sizeof(first)) == 0;
}

void CheckUnusedMemory() {
    auto plain = Plain;
    auto small = Gzip;
    FreeType::GuestMemoryRec empty{};
    for (FreeType::GuestMemoryRec* memory : {static_cast<FreeType::GuestMemoryRec*>(nullptr), &empty}) {
        auto source = MemorySource(plain.data(), plain.size(), memory);
        auto output = Filled();
        const auto untouched = output;
        Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == InvalidFileFormat);
        Require(Same(output, untouched) && source.pos == 4);
        source = MemorySource(small.data(), small.size(), memory);
        Require(Throws([&] { FT_Stream_OpenGzip_nid_postfix(&output, &source); }));
        Require(Same(output, untouched));
    }

    Counter counter;
    FreeType::GuestMemoryRec noRealloc{&counter, GuestAlloc, GuestFree, nullptr};
    auto source = MemorySource(small.data(), small.size(), &noRealloc);
    LegacyFreeType::GuestStreamRec output{};
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == 0);
    Require(output.base != nullptr && std::memcmp(output.base, Text, TextLength) == 0);
    output.close(&output);
    auto large = LargeGzip;
    source = CallbackSource(large.data(), large.size(), &noRealloc);
    output = {};
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == 0);
    std::array<unsigned char, 64> decoded{};
    Require(output.read(&output, 40000, decoded.data(), decoded.size()) == decoded.size());
    Require(std::all_of(decoded.begin(), decoded.end(), [](unsigned char byte) { return byte == 'A'; }));
    output.close(&output);
    Require(counter.allocations > 0 && counter.allocations == counter.frees);
}

void CheckAliasedStream() {
    Counter counter;
    FreeType::GuestMemoryRec memory{&counter, GuestAlloc, GuestFree, GuestRealloc};
    auto small = Gzip;
    auto stream = MemorySource(small.data(), small.size(), &memory);
    Require(FT_Stream_OpenGzip_nid_postfix(&stream, &stream) == InvalidStreamOperation);
    LegacyFreeType::GuestStreamRec zeroed{};
    zeroed.memory = &memory;
    Require(Same(stream, zeroed) && counter.allocations == 1 && counter.frees == 1);

    auto plain = Plain;
    stream = MemorySource(plain.data(), plain.size(), &memory);
    auto expected = stream;
    expected.pos = 4;
    Require(FT_Stream_OpenGzip_nid_postfix(&stream, &stream) == InvalidFileFormat);
    Require(Same(stream, expected) && counter.allocations == 1);
}

void CheckReopenedStream() {
    Counter counter;
    FreeType::GuestMemoryRec memory{&counter, GuestAlloc, GuestFree, GuestRealloc};
    auto large = LargeGzip;
    auto source = CallbackSource(large.data(), large.size(), &memory);
    LegacyFreeType::GuestStreamRec output{};
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == 0);
    const auto opened = output;
    auto plain = Plain;
    auto other = MemorySource(plain.data(), plain.size(), &memory);
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &other) == InvalidFileFormat);
    Require(Same(output, opened));
    std::array<unsigned char, 16> decoded{};
    Require(output.read(&output, 0, decoded.data(), decoded.size()) == decoded.size());
    const int frees = counter.frees;
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == 0);
    Require(counter.frees == frees && output.read != nullptr);
    decoded.fill(0);
    Require(output.read(&output, 0, decoded.data(), decoded.size()) == decoded.size());
    Require(std::all_of(decoded.begin(), decoded.end(), [](unsigned char byte) { return byte == 'A'; }));
    output.close(&output);
    Require(counter.allocations > counter.frees);
    ReleaseLeaks(counter);
}

void CheckFailedAllocation() {
    Counter counter;
    counter.limit = 0;
    FreeType::GuestMemoryRec memory{&counter, GuestAlloc, GuestFree, GuestRealloc};
    auto small = Gzip;
    auto source = MemorySource(small.data(), small.size(), &memory);
    LegacyFreeType::GuestStreamRec output{};
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == 0);
    Require(output.read != nullptr && output.close != nullptr && output.base == nullptr && output.descriptor.pointer == nullptr);
    Require(output.size == TextLength && output.memory == &memory);
    std::array<unsigned char, 4> decoded{};
    Require(Throws([&] { output.read(&output, 0, decoded.data(), decoded.size()); }));
    output.close(&output);
    auto large = LargeGzip;
    source = CallbackSource(large.data(), large.size(), &memory);
    output = {};
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == OutOfMemory);
    Require(output.read != nullptr && output.close != nullptr && output.descriptor.pointer == nullptr && output.size == 40960);
    output.close(&output);
    Require(counter.allocations == 0 && counter.frees == 0);

    Counter recordOnly;
    recordOnly.limit = 1;
    FreeType::GuestMemoryRec failsSecond{&recordOnly, GuestAlloc, GuestFree, GuestRealloc};
    source = MemorySource(small.data(), small.size(), &failsSecond);
    output = Filled();
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == InvalidFileFormat);
    Require(output.read == nullptr && output.close == nullptr && output.descriptor.pointer == nullptr && output.memory == &failsSecond);
    Require(recordOnly.allocations == 1 && recordOnly.frees == 1 && recordOnly.live.empty());

    Counter once;
    once.failures = 1;
    FreeType::GuestMemoryRec failsOnce{&once, GuestAlloc, GuestFree, GuestRealloc};
    source = MemorySource(small.data(), small.size(), &failsOnce);
    output = Filled();
    const auto untouched = output;
    Require(Throws([&] { FT_Stream_OpenGzip_nid_postfix(&output, &source); }));
    Require(Same(output, untouched) && once.allocations == 1 && once.frees == 1 && once.live.empty());

    Counter kept;
    kept.failures = 1;
    FreeType::GuestMemoryRec cannotFree{&kept, GuestAlloc, nullptr, GuestRealloc};
    source = MemorySource(small.data(), small.size(), &cannotFree);
    output = Filled();
    Require(Throws([&] { FT_Stream_OpenGzip_nid_postfix(&output, &source); }));
    Require(Same(output, untouched) && kept.allocations == 1 && kept.frees == 0 && kept.live.size() == 1);
    ReleaseLeaks(kept);
}

void CheckSourceReads() {
    Counter counter;
    FreeType::GuestMemoryRec memory{&counter, GuestAlloc, GuestFree, GuestRealloc};
    auto large = LargeGzip;
    auto source = CallbackSource(large.data(), large.size(), &memory);
    source.read = RefuseSeeks;
    auto output = Filled();
    const auto untouched = output;
    Require(FT_Stream_OpenGzip_nid_postfix(&output, &source) == InvalidStreamOperation);
    Require(Same(output, untouched));
    source.read = ReadTooMuch;
    Require(Throws([&] { FT_Stream_OpenGzip_nid_postfix(&output, &source); }));
    Require(Same(output, untouched) && counter.allocations == 0);
}

void CheckOverlappingRecords() {
    Counter counter;
    FreeType::GuestMemoryRec memory{&counter, GuestAlloc, GuestFree, GuestRealloc};
    std::array<LegacyFreeType::GuestStreamRec, 2> records{};
    auto* source = records.data();
    auto* output = reinterpret_cast<LegacyFreeType::GuestStreamRec*>(reinterpret_cast<unsigned char*>(records.data()) + 8);
    auto plain = Plain;
    *source = MemorySource(plain.data(), plain.size(), &memory);
    Require(FT_Stream_OpenGzip_nid_postfix(output, source) == InvalidFileFormat);
    auto small = Gzip;
    *source = MemorySource(small.data(), small.size(), &memory);
    Require(Throws([&] { FT_Stream_OpenGzip_nid_postfix(output, source); }));
    ReleaseLeaks(counter);
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
    Require(FT_Stream_OpenGzip_nid_postfix(nullptr, nullptr) == FT_Err_Invalid_Stream_Handle);
    errno = 71;
    Require(FT_Stream_OpenGzip_nid_postfix(reinterpret_cast<LegacyFreeType::GuestStreamRec*>(0x10), &source) == FT_Err_Invalid_Stream_Handle);
    Require(errno == 71);
    errno = 71;
    Require(FT_Stream_OpenGzip_nid_postfix(&output, reinterpret_cast<LegacyFreeType::GuestStreamRec*>(0x10)) == FT_Err_Invalid_Stream_Handle);
    Require(errno == 71);
    errno = 71;
    Require(FT_Stream_OpenGzip_nid_postfix(reinterpret_cast<LegacyFreeType::GuestStreamRec*>(0x10), reinterpret_cast<LegacyFreeType::GuestStreamRec*>(0x10)) == FT_Err_Invalid_Stream_Handle);
    Require(errno == 71);
}

}

int main() {
    CheckDecodes(Gzip);
    CheckDecodes(Zlib);
    CheckStreams();
    CheckUnusedMemory();
    CheckAliasedStream();
    CheckReopenedStream();
    CheckFailedAllocation();
    CheckSourceReads();
    CheckOverlappingRecords();
    CheckUncompressOutput();

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

    auto* badMemory = reinterpret_cast<FreeType::GuestMemoryRec*>(std::uintptr_t{0x10});
    auto* badLength = reinterpret_cast<std::uint64_t*>(std::uintptr_t{0x10});
    auto* badBuffer = reinterpret_cast<std::uint8_t*>(std::uintptr_t{0x10});
    length = output.size();
    Require(Rejects([&] { FT_Gzip_Uncompress_nid_postfix(badMemory, output.data(), &length, Gzip.data(), Gzip.size()); }));
    Require(length == output.size());
    Require(Rejects([&] { FT_Gzip_Uncompress_nid_postfix(&memory, output.data(), badLength, Gzip.data(), Gzip.size()); }));
    Require(Rejects([&] { FT_Gzip_Uncompress_nid_postfix(&memory, output.data(), &length, badBuffer, Gzip.size()); }));
    Require(length == output.size() && counter.allocations == counter.frees);
    Require(Rejects([&] { FT_Gzip_Uncompress_nid_postfix(&memory, badBuffer, &length, Gzip.data(), Gzip.size()); }));
    Require(length == output.size() && counter.allocations == counter.frees);

    const ReadOnlyPage readOnlyOutput(output.size());
    std::array<std::uint8_t, 64> untouched{};
    untouched.fill(0x5a);
    output = untouched;
    Require(Rejects([&] { FT_Gzip_Uncompress_nid_postfix(&memory, output.data(), readOnlyOutput.As<std::uint64_t>(), Gzip.data(), Gzip.size()); }));
    Require(output == untouched && counter.allocations == counter.frees);
    length = output.size();
    Require(Rejects([&] { FT_Gzip_Uncompress_nid_postfix(&memory, readOnlyOutput.As<std::uint8_t>(), &length, Gzip.data(), Gzip.size()); }));
    Require(length == output.size() && counter.allocations == counter.frees);
    return 0;
}
