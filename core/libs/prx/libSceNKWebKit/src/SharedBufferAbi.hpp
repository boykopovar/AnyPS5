#pragma once

#include "prx/libSceNKWebKit/src/WtfMemoryTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace NKWebKit::WebCoreAbi {

struct DataSegment {
    std::atomic<unsigned> refCount;
    union Storage {
        WtfAbi::Vector<char> vector;
        WTF::MappedFileData mappedFile;
        Storage() {}
        ~Storage() {}
    } storage;
    signed char index;
};

struct DataSegmentVectorEntry {
    std::size_t beginPosition;
    DataSegment* segment;
};

struct DataSegmentVector {
    DataSegmentVectorEntry* buffer;
    unsigned capacity;
    unsigned size;
    DataSegmentVectorEntry inlineBuffer[1];
};

struct SharedBuffer {
    unsigned refCount;
    std::size_t size;
    DataSegmentVector segments;
};

struct SharedBufferRef {
    SharedBuffer* ptr;
    explicit SharedBufferRef(SharedBuffer* value) : ptr(value) {}
    SharedBufferRef(SharedBufferRef&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
    SharedBufferRef(const SharedBufferRef&) = delete;
    SharedBufferRef& operator=(const SharedBufferRef&) = delete;
    ~SharedBufferRef();
};

static_assert(sizeof(std::atomic<unsigned>) == 4);
static_assert(sizeof(DataSegment) == 32);
static_assert(offsetof(DataSegment, refCount) == 0);
static_assert(offsetof(DataSegment, storage) == 8);
static_assert(offsetof(DataSegment, index) == 24);
static_assert(sizeof(DataSegmentVectorEntry) == 16);
static_assert(offsetof(DataSegmentVectorEntry, segment) == 8);
static_assert(sizeof(DataSegmentVector) == 32);
static_assert(offsetof(DataSegmentVector, capacity) == 8);
static_assert(offsetof(DataSegmentVector, size) == 12);
static_assert(offsetof(DataSegmentVector, inlineBuffer) == 16);
static_assert(sizeof(SharedBuffer) == 48);
static_assert(offsetof(SharedBuffer, size) == 8);
static_assert(offsetof(SharedBuffer, segments) == 16);
static_assert(sizeof(SharedBufferRef) == 8);
static_assert(!std::is_trivially_destructible_v<SharedBufferRef>);

}  // namespace NKWebKit::WebCoreAbi
