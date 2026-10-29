#include "prx/libSceNpCommon/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"

namespace {

constexpr int NP_RING_BUF_ERROR_NO_SPACE = static_cast<int>(0x80559E0A);
constexpr int NP_RING_BUF_ERROR_NO_DATA = static_cast<int>(0x80559E0B);
constexpr int NP_RING_BUF_ERROR_OUT_OF_MEMORY = static_cast<int>(0x80559E09);

static_assert(sizeof(NpRingBuf) == 0x30);
static_assert(sizeof(NpRingBufMemory) == 0x38);

void* const RingBufVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np7RingBufD1Ev), reinterpret_cast<void*>(&_ZN3sce2np7RingBufD0Ev),
};

void* const RingBufMemoryVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np13RingBufMemoryD1Ev), reinterpret_cast<void*>(&_ZN3sce2np13RingBufMemoryD0Ev),
};

void Require(bool condition, const char* failure) {
    if (!condition) throw std::invalid_argument(failure);
}

void ClearFields(NpRingBuf& ringBuf) {
    ringBuf.buf = nullptr;
    ringBuf.readPointer = nullptr;
    ringBuf.writePointer = nullptr;
    ringBuf.size = 0;
    ringBuf.bufSize = 0;
}

void Attach(NpRingBuf& ringBuf, std::uint8_t* buf, std::size_t bufSize) {
    ringBuf.buf = buf;
    ringBuf.readPointer = buf;
    ringBuf.writePointer = buf;
    ringBuf.bufSize = bufSize;
    ringBuf.size = 0;
}

std::uint8_t* End(const NpRingBuf& ringBuf) {
    return ringBuf.buf + ringBuf.bufSize;
}

std::uint8_t* WriteBlock(NpRingBuf& ringBuf, std::size_t& blockSize) {
    if (ringBuf.size == ringBuf.bufSize) return nullptr;
    if (ringBuf.readPointer == ringBuf.writePointer) {
        ringBuf.readPointer = ringBuf.buf;
        ringBuf.writePointer = ringBuf.buf;
    }
    std::uint8_t* const end = ringBuf.writePointer < ringBuf.readPointer ? ringBuf.readPointer : End(ringBuf);
    blockSize = static_cast<std::size_t>(end - ringBuf.writePointer);
    return ringBuf.writePointer;
}

void CommitWrite(NpRingBuf& ringBuf, std::size_t size) {
    ringBuf.writePointer += size;
    Require(ringBuf.writePointer <= End(ringBuf), "sce::np::RingBuf::CheckinForWrite: the write passes the end of the buffer");
    if (ringBuf.writePointer == End(ringBuf)) ringBuf.writePointer = ringBuf.buf;
    ringBuf.size += size;
}

std::uint8_t* ReadBlock(NpRingBuf& ringBuf, std::size_t& blockSize) {
    if (ringBuf.size == 0) return nullptr;
    if (ringBuf.readPointer < ringBuf.writePointer) {
        blockSize = static_cast<std::size_t>(ringBuf.writePointer - ringBuf.readPointer);
        Require(blockSize == ringBuf.size, "sce::np::RingBuf::CheckoutForRead: the data size does not match the pointers");
    } else {
        blockSize = static_cast<std::size_t>(End(ringBuf) - ringBuf.readPointer);
    }
    return ringBuf.readPointer;
}

void CommitRead(NpRingBuf& ringBuf, std::size_t size) {
    const std::size_t tail = static_cast<std::size_t>(End(ringBuf) - ringBuf.readPointer);
    ringBuf.readPointer = size < tail ? ringBuf.readPointer + size : ringBuf.buf + (size - tail);
    ringBuf.size -= size;
}

void ReleaseBuffer(NpRingBufMemory& ringBuf) {
    ringBuf.allocator->allocator.release(ringBuf.ringBuf.buf, ringBuf.allocator->allocator.context);
}

} // namespace

extern "C" {

void APS5_VABI _ZN3sce2np7RingBufC1Ev(NpRingBuf* self) {
    self->vtable = &RingBufVtable[2];
    ClearFields(*self);
}

void APS5_VABI _ZN3sce2np7RingBufC2Ev(NpRingBuf* self) {
    _ZN3sce2np7RingBufC1Ev(self);
}

void APS5_VABI _ZN3sce2np7RingBufD1Ev(NpRingBuf* self) {
    self->vtable = &RingBufVtable[2];
}

void APS5_VABI _ZN3sce2np7RingBufD2Ev(NpRingBuf* self) {
    _ZN3sce2np7RingBufD1Ev(self);
}

void APS5_VABI _ZN3sce2np7RingBufD0Ev(NpRingBuf* self) {
    _ZN3sce2np7RingBufD1Ev(self);
    _ZN3sce2np6ObjectdlEPv(self);
}

void APS5_VABI _ZN3sce2np7RingBuf4ctorEv(NpRingBuf* self) {
    ClearFields(*self);
}

void APS5_VABI _ZN3sce2np7RingBuf4dtorEv(NpRingBuf*) {}

int APS5_VABI _ZN3sce2np7RingBuf4InitEPvm(NpRingBuf* self, void* buf, std::size_t bufSize) {
    Require(self->buf == nullptr, "sce::np::RingBuf::Init: the ring buffer already has a buffer");
    Require(buf != nullptr, "sce::np::RingBuf::Init: null buffer");
    Require(bufSize != 0, "sce::np::RingBuf::Init: empty buffer");
    Attach(*self, static_cast<std::uint8_t*>(buf), bufSize);
    return 0;
}

void APS5_VABI _ZN3sce2np7RingBuf7DestroyEv(NpRingBuf* self) {
    ClearFields(*self);
}

void APS5_VABI _ZN3sce2np7RingBuf5ClearEv(NpRingBuf* self) {
    self->writePointer = self->buf;
    self->readPointer = self->buf;
    self->size = 0;
}

bool APS5_VABI _ZNK3sce2np7RingBuf7IsEmptyEv(const NpRingBuf* self) {
    return self->size == 0;
}

std::size_t APS5_VABI _ZNK3sce2np7RingBuf11GetDataSizeEv(const NpRingBuf* self) {
    return self->size;
}

std::size_t APS5_VABI _ZNK3sce2np7RingBuf11GetFreeSizeEv(const NpRingBuf* self) {
    return self->bufSize - self->size;
}

void* APS5_VABI _ZN3sce2np7RingBuf16CheckoutForWriteEPm(NpRingBuf* self, std::size_t* blockSize) {
    Require(blockSize != nullptr, "sce::np::RingBuf::CheckoutForWrite: null block size");
    return WriteBlock(*self, *blockSize);
}

void APS5_VABI _ZN3sce2np7RingBuf15CheckinForWriteEm(NpRingBuf* self, std::size_t size) {
    CommitWrite(*self, size);
}

int APS5_VABI _ZN3sce2np7RingBuf5WriteEPKvm(NpRingBuf* self, const void* data, std::size_t size) {
    if (self->bufSize - self->size < size) return NP_RING_BUF_ERROR_NO_SPACE;
    std::size_t blockSize = 0;
    std::uint8_t* block = WriteBlock(*self, blockSize);
    Require(block != nullptr, "sce::np::RingBuf::Write: the ring buffer is full");
    const std::size_t first = size <= blockSize ? size : blockSize;
    std::memcpy(block, data, first);
    CommitWrite(*self, first);
    if (first == size) return 0;
    block = WriteBlock(*self, blockSize);
    Require(block != nullptr && blockSize >= size - first, "sce::np::RingBuf::Write: the rest does not fit after the wrap");
    std::memcpy(block, static_cast<const std::uint8_t*>(data) + first, size - first);
    CommitWrite(*self, size - first);
    return 0;
}

void* APS5_VABI _ZN3sce2np7RingBuf15CheckoutForReadEPm(NpRingBuf* self, std::size_t* blockSize) {
    if (self->size == 0) return nullptr;
    return ReadBlock(*self, *blockSize);
}

void APS5_VABI _ZN3sce2np7RingBuf14CheckinForReadEm(NpRingBuf* self, std::size_t size) {
    CommitRead(*self, size);
}

int APS5_VABI _ZN3sce2np7RingBuf4ReadEPvm(NpRingBuf* self, void* data, std::size_t size) {
    if (size == 0) return 0;
    if (self->size < size) return NP_RING_BUF_ERROR_NO_DATA;
    auto* out = static_cast<std::uint8_t*>(data);
    std::size_t blockSize = 0;
    const std::uint8_t* block = ReadBlock(*self, blockSize);
    const std::size_t first = size <= blockSize ? size : blockSize;
    if (out != nullptr) std::memcpy(out, block, first);
    CommitRead(*self, first);
    if (first == size) return 0;
    block = ReadBlock(*self, blockSize);
    Require(block != nullptr && blockSize >= size - first, "sce::np::RingBuf::Read: the rest is not readable after the wrap");
    if (out != nullptr) std::memcpy(out + first, block, size - first);
    CommitRead(*self, size - first);
    return 0;
}

int APS5_VABI _ZN3sce2np7RingBuf4PeekEmPvm(NpRingBuf* self, std::size_t offset, void* data, std::size_t size) {
    Require(data != nullptr, "sce::np::RingBuf::Peek: null buffer");
    Require(size != 0, "sce::np::RingBuf::Peek: empty read");
    if (offset + size > self->size || offset >= self->size) return NP_RING_BUF_ERROR_NO_SPACE;
    auto* out = static_cast<std::uint8_t*>(data);
    const std::uint8_t* source = self->readPointer + offset;
    if (source < End(*self)) {
        const std::size_t tail = static_cast<std::size_t>(End(*self) - source);
        if (tail <= size) {
            std::memcpy(out, source, tail);
            source = self->buf;
            out += tail;
            size -= tail;
        }
    } else {
        source -= self->bufSize;
    }
    std::memcpy(out, source, size);
    return 0;
}

void APS5_VABI _ZN3sce2np13RingBufMemoryC1EP16SceNpAllocatorEx(NpRingBufMemory* self, NpAllocatorEx* allocator) {
    ClearFields(self->ringBuf);
    self->ringBuf.vtable = &RingBufMemoryVtable[2];
    self->allocator = allocator;
}

void APS5_VABI _ZN3sce2np13RingBufMemoryC2EP16SceNpAllocatorEx(NpRingBufMemory* self, NpAllocatorEx* allocator) {
    _ZN3sce2np13RingBufMemoryC1EP16SceNpAllocatorEx(self, allocator);
}

void APS5_VABI _ZN3sce2np13RingBufMemoryD1Ev(NpRingBufMemory* self) {
    self->ringBuf.vtable = &RingBufMemoryVtable[2];
    ReleaseBuffer(*self);
    self->ringBuf.vtable = &RingBufVtable[2];
}

void APS5_VABI _ZN3sce2np13RingBufMemoryD2Ev(NpRingBufMemory* self) {
    _ZN3sce2np13RingBufMemoryD1Ev(self);
}

void APS5_VABI _ZN3sce2np13RingBufMemoryD0Ev(NpRingBufMemory* self) {
    _ZN3sce2np13RingBufMemoryD1Ev(self);
    _ZN3sce2np6ObjectdlEPv(self);
}

void APS5_VABI _ZN3sce2np13RingBufMemory4ctorEv(NpRingBufMemory*) {}

void APS5_VABI _ZN3sce2np13RingBufMemory4dtorEv(NpRingBufMemory* self) {
    ReleaseBuffer(*self);
}

int APS5_VABI _ZN3sce2np13RingBufMemory4InitEm(NpRingBufMemory* self, std::size_t bufSize) {
    auto* buf = static_cast<std::uint8_t*>(self->allocator->allocator.allocate(bufSize, self->allocator->allocator.context));
    if (buf == nullptr) return self->allocator->outOfMemoryError;
    Require(self->ringBuf.buf == nullptr, "sce::np::RingBufMemory::Init: the ring buffer already has a buffer");
    Require(bufSize != 0, "sce::np::RingBufMemory::Init: empty buffer");
    Attach(self->ringBuf, buf, bufSize);
    return 0;
}

void APS5_VABI _ZN3sce2np13RingBufMemory7DestroyEv(NpRingBufMemory* self) {
    ReleaseBuffer(*self);
    ClearFields(self->ringBuf);
}

int APS5_VABI _ZN3sce2np13RingBufMemory6ExpandEm(NpRingBufMemory* self, std::size_t extraSize) {
    NpRingBuf& ringBuf = self->ringBuf;
    auto* buf = static_cast<std::uint8_t*>(self->allocator->allocator.reallocate(ringBuf.buf, ringBuf.bufSize + extraSize,
        self->allocator->allocator.context));
    if (buf == nullptr) return NP_RING_BUF_ERROR_OUT_OF_MEMORY;
    const std::size_t readOffset = static_cast<std::size_t>(ringBuf.readPointer - ringBuf.buf);
    const std::size_t writeOffset = static_cast<std::size_t>(ringBuf.writePointer - ringBuf.buf);
    std::size_t newReadOffset = readOffset;
    if (ringBuf.writePointer < ringBuf.readPointer) {
        std::memmove(buf + readOffset + extraSize, buf + readOffset, ringBuf.bufSize - readOffset);
        newReadOffset += extraSize;
    }
    ringBuf.writePointer = buf + writeOffset;
    ringBuf.readPointer = buf + newReadOffset;
    ringBuf.bufSize += extraSize;
    ringBuf.buf = buf;
    return 0;
}

}
