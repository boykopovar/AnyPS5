#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/SharedBufferExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>

using NKWebKit::WebCoreAbi::DataSegment;
using NKWebKit::WebCoreAbi::SharedBuffer;
using CharVector = NKWebKit::WtfAbi::Vector<char>;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SharedBuffer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Exception, typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

static constexpr std::size_t TooLong = std::size_t{std::numeric_limits<unsigned>::max()} + 1;

static std::string_view Bytes(const DataSegment& segment) {
    return {_ZNK7WebCore12SharedBuffer11DataSegment4dataEv(&segment), _ZNK7WebCore12SharedBuffer11DataSegment4sizeEv(&segment)};
}

static CharVector MakeVector(std::string_view text, unsigned capacity) {
    CharVector vector(capacity);
    std::memcpy(vector.buffer, text.data(), text.size());
    vector.size = static_cast<unsigned>(text.size());
    return vector;
}

static const CharVector& SegmentVector(const SharedBuffer* buffer, unsigned index) {
    Require(buffer->segments.buffer[index].segment->index == 0);
    return buffer->segments.buffer[index].segment->storage.vector;
}

static void RequireSegments(const SharedBuffer* buffer, std::initializer_list<std::string_view> parts) {
    const auto* entry = _ZNK7WebCore12SharedBuffer5beginEv(buffer);
    Require(entry == buffer->segments.buffer);
    Require(static_cast<std::size_t>(_ZNK7WebCore12SharedBuffer3endEv(buffer) - entry) == parts.size());
    std::size_t position = 0;
    for (auto part : parts) {
        Require(entry->beginPosition == position && Bytes(*entry->segment) == part);
        position += part.size();
        ++entry;
    }
    Require(_ZNK7WebCore12SharedBuffer4sizeEv(buffer) == position);
    Require(_ZNK7WebCore12SharedBuffer7isEmptyEv(buffer) == !position);
}

static bool UsesInlineSegment(const SharedBuffer* buffer) {
    return buffer->segments.buffer == buffer->segments.inlineBuffer && buffer->segments.capacity == 1;
}

static void TestCreateFromBytes() {
    auto empty = _ZN7WebCore12SharedBuffer6createEv();
    Require(empty.ptr->refCount == 1 && UsesInlineSegment(empty.ptr));
    Require(_ZN3WTF14fastMallocSizeEPKv(empty.ptr) == sizeof(SharedBuffer));
    RequireSegments(empty.ptr, {});
    Require(!_ZNK7WebCore12SharedBuffer4dataEv(empty.ptr) && !_ZNK7WebCore12SharedBuffer14dataAsUInt8PtrEv(empty.ptr));

    auto chars = _ZN7WebCore12SharedBuffer6createEPKcm("hello", 5);
    RequireSegments(chars.ptr, {"hello"});
    const auto* segment = chars.ptr->segments.buffer[0].segment;
    Require(segment->refCount == 1 && _ZN3WTF14fastMallocSizeEPKv(segment) == sizeof(DataSegment));
    Require(SegmentVector(chars.ptr, 0).capacity == 5 && _ZN3WTF14fastMallocSizeEPKv(SegmentVector(chars.ptr, 0).buffer) == 5);
    Require(_ZNK7WebCore12SharedBuffer4dataEv(chars.ptr) == SegmentVector(chars.ptr, 0).buffer);

    const unsigned char raw[] = {0x00, 0xFF, 0x80};
    auto bytes = _ZN7WebCore12SharedBuffer6createEPKhm(raw, sizeof(raw));
    RequireSegments(bytes.ptr, {std::string_view("\x00\xFF\x80", 3)});
    Require(_ZNK7WebCore12SharedBuffer14dataAsUInt8PtrEv(bytes.ptr)[1] == 0xFF);

    auto nothing = _ZN7WebCore12SharedBuffer6createEPKcm(nullptr, 0);
    RequireSegments(nothing.ptr, {""});
    Require(!SegmentVector(nothing.ptr, 0).buffer && !_ZNK7WebCore12SharedBuffer4dataEv(nothing.ptr));
}

static void TestCreateFromVectors() {
    auto moved = MakeVector("vector", 32);
    auto fromChars = _ZN7WebCore12SharedBuffer6createEON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&moved);
    Require(!moved.buffer && !moved.capacity && !moved.size);
    RequireSegments(fromChars.ptr, {"vector"});
    Require(SegmentVector(fromChars.ptr, 0).capacity == 6 && _ZN3WTF14fastMallocSizeEPKv(SegmentVector(fromChars.ptr, 0).buffer) == 6);

    CharVector reserved(8);
    auto fromEmpty = _ZN7WebCore12SharedBuffer6createEON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&reserved);
    Require(!reserved.buffer && !reserved.capacity);
    RequireSegments(fromEmpty.ptr, {""});
    Require(!SegmentVector(fromEmpty.ptr, 0).buffer && !SegmentVector(fromEmpty.ptr, 0).capacity);

    NKWebKit::WtfAbi::Vector<unsigned char> kept(4);
    std::memcpy(kept.buffer, "abc", 3);
    kept.size = 3;
    auto* keptBuffer = kept.buffer;
    auto fromBytes = _ZN7WebCore12SharedBuffer6createEON3WTF6VectorIhLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&kept);
    Require(kept.buffer == keptBuffer && kept.size == 3 && kept.capacity == 4);
    RequireSegments(fromBytes.ptr, {"abc"});
    Require(reinterpret_cast<unsigned char*>(SegmentVector(fromBytes.ptr, 0).buffer) != keptBuffer);
}

static void TestConstructors() {
    alignas(SharedBuffer) unsigned char storage[sizeof(SharedBuffer)];
    auto* buffer = reinterpret_cast<SharedBuffer*>(storage);
    const unsigned char raw[] = {'u', 'v'};
    for (int base = 0; base < 2; ++base) {
        (base ? _ZN7WebCore12SharedBufferC2Ev : _ZN7WebCore12SharedBufferC1Ev)(buffer);
        Require(buffer->refCount == 1 && UsesInlineSegment(buffer));
        RequireSegments(buffer, {});
        (base ? _ZN7WebCore12SharedBufferD2Ev : _ZN7WebCore12SharedBufferD1Ev)(buffer);
        (base ? _ZN7WebCore12SharedBufferC2EPKcm : _ZN7WebCore12SharedBufferC1EPKcm)(buffer, "st", 2);
        RequireSegments(buffer, {"st"});
        (base ? _ZN7WebCore12SharedBufferD2Ev : _ZN7WebCore12SharedBufferD1Ev)(buffer);
        (base ? _ZN7WebCore12SharedBufferC2EPKhm : _ZN7WebCore12SharedBufferC1EPKhm)(buffer, raw, 2);
        RequireSegments(buffer, {"uv"});
        (base ? _ZN7WebCore12SharedBufferD1Ev : _ZN7WebCore12SharedBufferD2Ev)(buffer);
        auto vector = MakeVector("wx", 4);
        (base ? _ZN7WebCore12SharedBufferC2EON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE
              : _ZN7WebCore12SharedBufferC1EON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE)(buffer, &vector);
        Require(!vector.buffer && buffer->refCount == 1);
        RequireSegments(buffer, {"wx"});
        Require(SegmentVector(buffer, 0).capacity == 2);
        _ZN7WebCore12SharedBufferD1Ev(buffer);
    }
}

static void TestAppendAndCombine() {
    auto buffer = _ZN7WebCore12SharedBuffer6createEv();
    _ZN7WebCore12SharedBuffer6appendEPKcm(buffer.ptr, "abc", 3);
    Require(UsesInlineSegment(buffer.ptr));
    _ZN7WebCore12SharedBuffer6appendEPKcm(buffer.ptr, "de", 2);
    Require(buffer.ptr->segments.buffer != buffer.ptr->segments.inlineBuffer && buffer.ptr->segments.capacity == 16);
    auto tail = MakeVector("fgh", 8);
    _ZN7WebCore12SharedBuffer6appendEON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(buffer.ptr, &tail);
    Require(!tail.buffer && SegmentVector(buffer.ptr, 2).capacity == 3);
    _ZN7WebCore12SharedBuffer6appendEPKcm(buffer.ptr, nullptr, 0);
    RequireSegments(buffer.ptr, {"abc", "de", "fgh", ""});
    Require(SegmentVector(buffer.ptr, 1).capacity == 2 && _ZN3WTF14fastMallocSizeEPKv(SegmentVector(buffer.ptr, 1).buffer) == 2);

    auto snapshot = _ZNK7WebCore12SharedBuffer4copyEv(buffer.ptr);
    auto* first = buffer.ptr->segments.buffer[0].segment;
    Require(first->refCount == 2);
    _ZNK7WebCore12SharedBuffer21combineIntoOneSegmentEv(buffer.ptr);
    RequireSegments(buffer.ptr, {"abcdefgh"});
    Require(UsesInlineSegment(buffer.ptr) && first->refCount == 1);
    const auto* combined = SegmentVector(buffer.ptr, 0).buffer;
    Require(SegmentVector(buffer.ptr, 0).capacity == 8 && _ZN3WTF14fastMallocSizeEPKv(combined) == 8);
    _ZNK7WebCore12SharedBuffer21combineIntoOneSegmentEv(buffer.ptr);
    Require(SegmentVector(buffer.ptr, 0).buffer == combined && _ZNK7WebCore12SharedBuffer4dataEv(buffer.ptr) == combined);

    RequireSegments(snapshot.ptr, {"abc", "de", "fgh", ""});
    Require(std::string_view(_ZNK7WebCore12SharedBuffer4dataEv(snapshot.ptr), 8) == "abcdefgh");
    RequireSegments(snapshot.ptr, {"abcdefgh"});

    auto empties = _ZN7WebCore12SharedBuffer6createEPKcm(nullptr, 0);
    _ZN7WebCore12SharedBuffer6appendEPKcm(empties.ptr, "", 0);
    Require(!_ZNK7WebCore12SharedBuffer4dataEv(empties.ptr));
    RequireSegments(empties.ptr, {""});

    auto many = _ZN7WebCore12SharedBuffer6createEv();
    const unsigned capacities[] = {1, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 21};
    for (unsigned capacity : capacities) {
        _ZN7WebCore12SharedBuffer6appendEPKcm(many.ptr, "z", 1);
        Require(many.ptr->segments.capacity == capacity);
    }
    Require(many.ptr->segments.size == 17 && many.ptr->segments.buffer[16].beginPosition == 16);
}

static void TestAppendBuffer() {
    auto left = _ZN7WebCore12SharedBuffer6createEPKcm("ab", 2);
    auto right = _ZN7WebCore12SharedBuffer6createEPKcm("cd", 2);
    _ZN7WebCore12SharedBuffer6appendEPKcm(right.ptr, "e", 1);
    _ZN7WebCore12SharedBuffer6appendERKS0_(left.ptr, right.ptr);
    RequireSegments(left.ptr, {"ab", "cd", "e"});
    Require(left.ptr->segments.capacity == 3 && left.ptr->segments.buffer[1].segment == right.ptr->segments.buffer[0].segment);
    Require(right.ptr->segments.buffer[0].segment->refCount == 2);
    _ZN7WebCore12SharedBuffer6appendERKS0_(left.ptr, left.ptr);
    RequireSegments(left.ptr, {"ab", "cd", "e", "ab", "cd", "e"});
    Require(left.ptr->segments.capacity == 6 && left.ptr->segments.buffer[0].segment->refCount == 2);
    Require(right.ptr->segments.buffer[1].segment->refCount == 3);
}

static void TestCopyAndClear() {
    auto original = _ZN7WebCore12SharedBuffer6createEPKcm("one", 3);
    _ZN7WebCore12SharedBuffer6appendEPKcm(original.ptr, "two", 3);
    auto clone = _ZNK7WebCore12SharedBuffer4copyEv(original.ptr);
    Require(clone.ptr != original.ptr && clone.ptr->refCount == 1 && clone.ptr->segments.capacity == 2);
    RequireSegments(clone.ptr, {"one", "two"});
    Require(clone.ptr->segments.buffer[1].segment == original.ptr->segments.buffer[1].segment);
    Require(clone.ptr->segments.buffer[1].segment->refCount == 2);
    _ZN7WebCore12SharedBuffer5clearEv(original.ptr);
    RequireSegments(original.ptr, {});
    Require(UsesInlineSegment(original.ptr) && clone.ptr->segments.buffer[1].segment->refCount == 1);

    auto single = _ZN7WebCore12SharedBuffer6createEPKcm("x", 1);
    auto singleCopy = _ZNK7WebCore12SharedBuffer4copyEv(single.ptr);
    Require(UsesInlineSegment(singleCopy.ptr));
    RequireSegments(singleCopy.ptr, {"x"});
}

static DataSegment* MappedSegment(void* data, unsigned size) {
    auto* segment = new (_ZN3WTF10fastMallocEm(sizeof(DataSegment))) DataSegment;
    segment->refCount.store(1);
    segment->storage.mappedFile = {data, size};
    segment->index = 1;
    return segment;
}

static void TestMappedSegments() {
    char mapped[] = "map";
    auto* segment = MappedSegment(mapped, 3);
    Require(Bytes(*segment) == "map" && Bytes(*segment).data() == mapped);
    segment->index = -1;
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore12SharedBuffer11DataSegment4dataEv(segment); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore12SharedBuffer11DataSegment4sizeEv(segment); }));
    _ZN3WTF8fastFreeEPv(segment);

    alignas(SharedBuffer) unsigned char storage[sizeof(SharedBuffer)];
    auto* buffer = reinterpret_cast<SharedBuffer*>(storage);
    _ZN7WebCore12SharedBufferC1Ev(buffer);
    const unsigned large = std::numeric_limits<unsigned>::max();
    auto* segments = static_cast<NKWebKit::WebCoreAbi::DataSegmentVectorEntry*>(_ZN3WTF10fastMallocEm(2 * sizeof(NKWebKit::WebCoreAbi::DataSegmentVectorEntry)));
    segments[0] = {0, MappedSegment(nullptr, large)};
    segments[1] = {large, MappedSegment(nullptr, 1)};
    buffer->segments.buffer = segments;
    buffer->segments.capacity = buffer->segments.size = 2;
    buffer->size = TooLong;
    Require(Throws<std::length_error>([&] { _ZNK7WebCore12SharedBuffer21combineIntoOneSegmentEv(buffer); }));
    Require(buffer->segments.size == 2 && buffer->segments.buffer == segments);
    _ZN7WebCore12SharedBufferD1Ev(buffer);
}

static void TestThrowingPaths() {
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore12SharedBuffer6createEPKcm(nullptr, 1); }));
    Require(Throws<std::length_error>([] { _ZN7WebCore12SharedBuffer6createEPKcm("x", TooLong); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore12SharedBuffer6createEON3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore12SharedBufferC1Ev(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore12SharedBuffer4sizeEv(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore12SharedBuffer11DataSegment4dataEv(nullptr); }));
    auto buffer = _ZN7WebCore12SharedBuffer6createEv();
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore12SharedBuffer6appendEPKcm(buffer.ptr, nullptr, 2); }));
    Require(Throws<std::length_error>([&] { _ZN7WebCore12SharedBuffer6appendEPKcm(buffer.ptr, "x", TooLong); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore12SharedBuffer6appendERKS0_(buffer.ptr, nullptr); }));
    RequireSegments(buffer.ptr, {});
}

int main() {
    TestCreateFromBytes();
    TestCreateFromVectors();
    TestConstructors();
    TestAppendAndCombine();
    TestAppendBuffer();
    TestCopyAndClear();
    TestMappedSegments();
    TestThrowingPaths();
    return 0;
}
