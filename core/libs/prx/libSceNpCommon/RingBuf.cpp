#include "prx/libSceNpCommon/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

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

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": the guest memory is not readable");
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": the guest memory is not writable");
}

template <typename Value>
Value Load(const Value* source, const char* function) {
    Value value {};
    CopyIn(&value, source, sizeof(value), function);
    return value;
}

template <typename Value>
void Store(Value* destination, const Value& value, const char* function) {
    CopyOut(destination, &value, sizeof(value), function);
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
    Require(size <= static_cast<std::size_t>(End(ringBuf) - ringBuf.writePointer),
        "sce::np::RingBuf::CheckinForWrite: the write passes the end of the buffer");
    ringBuf.writePointer += size;
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

void ReleaseBuffer(const NpRingBufMemory& ringBuf, const char* function) {
    const NpAllocatorEx allocator = Load(ringBuf.allocator, function);
    allocator.allocator.release(ringBuf.ringBuf.buf, allocator.allocator.context);
}

} // namespace

extern "C" {

void APS5_VABI _ZN3sce2np7RingBufC1Ev(NpRingBuf* self) {
    if (self == nullptr) return;
    NpRingBuf ring {};
    ring.vtable = &RingBufVtable[2];
    ClearFields(ring);
    Store(self, ring, "sce::np::RingBuf::RingBuf");
}

void APS5_VABI _ZN3sce2np7RingBufC2Ev(NpRingBuf* self) {
    _ZN3sce2np7RingBufC1Ev(self);
}

void APS5_VABI _ZN3sce2np7RingBufD1Ev(NpRingBuf* self) {
    if (self == nullptr) return;
    constexpr const char* Function = "sce::np::RingBuf::~RingBuf";
    NpRingBuf ring = Load(self, Function);
    ring.vtable = &RingBufVtable[2];
    Store(self, ring, Function);
}

void APS5_VABI _ZN3sce2np7RingBufD2Ev(NpRingBuf* self) {
    _ZN3sce2np7RingBufD1Ev(self);
}

void APS5_VABI _ZN3sce2np7RingBufD0Ev(NpRingBuf* self) {
    _ZN3sce2np7RingBufD1Ev(self);
    _ZN3sce2np6ObjectdlEPv(self);
}

void APS5_VABI _ZN3sce2np7RingBuf4ctorEv(NpRingBuf* self) {
    if (self == nullptr) return;
    constexpr const char* Function = "sce::np::RingBuf::ctor";
    NpRingBuf ring = Load(self, Function);
    ClearFields(ring);
    Store(self, ring, Function);
}

void APS5_VABI _ZN3sce2np7RingBuf4dtorEv(NpRingBuf*) {}

int APS5_VABI _ZN3sce2np7RingBuf4InitEPvm(NpRingBuf* self, void* buf, std::size_t bufSize) {
    constexpr const char* Function = "sce::np::RingBuf::Init";
    NpRingBuf ring = Load(self, Function);
    Require(ring.buf == nullptr, "sce::np::RingBuf::Init: the ring buffer already has a buffer");
    Require(buf != nullptr, "sce::np::RingBuf::Init: null buffer");
    Require(bufSize != 0, "sce::np::RingBuf::Init: empty buffer");
    Attach(ring, static_cast<std::uint8_t*>(buf), bufSize);
    Store(self, ring, Function);
    return 0;
}

void APS5_VABI _ZN3sce2np7RingBuf7DestroyEv(NpRingBuf* self) {
    if (self == nullptr) return;
    constexpr const char* Function = "sce::np::RingBuf::Destroy";
    NpRingBuf ring = Load(self, Function);
    ClearFields(ring);
    Store(self, ring, Function);
}

void APS5_VABI _ZN3sce2np7RingBuf5ClearEv(NpRingBuf* self) {
    constexpr const char* Function = "sce::np::RingBuf::Clear";
    NpRingBuf ring = Load(self, Function);
    ring.writePointer = ring.buf;
    ring.readPointer = ring.buf;
    ring.size = 0;
    Store(self, ring, Function);
}

bool APS5_VABI _ZNK3sce2np7RingBuf7IsEmptyEv(const NpRingBuf* self) {
    return Load(self, "sce::np::RingBuf::IsEmpty").size == 0;
}

std::size_t APS5_VABI _ZNK3sce2np7RingBuf11GetDataSizeEv(const NpRingBuf* self) {
    return Load(self, "sce::np::RingBuf::GetDataSize").size;
}

std::size_t APS5_VABI _ZNK3sce2np7RingBuf11GetFreeSizeEv(const NpRingBuf* self) {
    const NpRingBuf ring = Load(self, "sce::np::RingBuf::GetFreeSize");
    return ring.bufSize - ring.size;
}

void* APS5_VABI _ZN3sce2np7RingBuf16CheckoutForWriteEPm(NpRingBuf* self, std::size_t* blockSize) {
    constexpr const char* Function = "sce::np::RingBuf::CheckoutForWrite";
    Require(blockSize != nullptr, "sce::np::RingBuf::CheckoutForWrite: null block size");
    NpRingBuf ring = Load(self, Function);
    std::size_t size = 0;
    std::uint8_t* const block = WriteBlock(ring, size);
    if (block != nullptr) Store(blockSize, size, Function);
    Store(self, ring, Function);
    return block;
}

void APS5_VABI _ZN3sce2np7RingBuf15CheckinForWriteEm(NpRingBuf* self, std::size_t size) {
    constexpr const char* Function = "sce::np::RingBuf::CheckinForWrite";
    NpRingBuf ring = Load(self, Function);
    CommitWrite(ring, size);
    Store(self, ring, Function);
}

int APS5_VABI _ZN3sce2np7RingBuf5WriteEPKvm(NpRingBuf* self, const void* data, std::size_t size) {
    constexpr const char* Function = "sce::np::RingBuf::Write";
    NpRingBuf ring = Load(self, Function);
    if (ring.bufSize - ring.size < size) return NP_RING_BUF_ERROR_NO_SPACE;
    std::vector<std::uint8_t> bytes(size);
    CopyIn(bytes.data(), data, size, Function);
    std::size_t blockSize = 0;
    std::uint8_t* block = WriteBlock(ring, blockSize);
    Require(block != nullptr, "sce::np::RingBuf::Write: the ring buffer is full");
    const std::size_t first = size <= blockSize ? size : blockSize;
    CopyOut(block, bytes.data(), first, Function);
    CommitWrite(ring, first);
    if (first != size) {
        block = WriteBlock(ring, blockSize);
        Require(block != nullptr && blockSize >= size - first, "sce::np::RingBuf::Write: the rest does not fit after the wrap");
        CopyOut(block, bytes.data() + first, size - first, Function);
        CommitWrite(ring, size - first);
    }
    Store(self, ring, Function);
    return 0;
}

void* APS5_VABI _ZN3sce2np7RingBuf15CheckoutForReadEPm(NpRingBuf* self, std::size_t* blockSize) {
    constexpr const char* Function = "sce::np::RingBuf::CheckoutForRead";
    Require(blockSize != nullptr, "sce::np::RingBuf::CheckoutForRead: null block size");
    NpRingBuf ring = Load(self, Function);
    if (ring.size == 0) return nullptr;
    std::size_t size = 0;
    std::uint8_t* const block = ReadBlock(ring, size);
    Store(blockSize, size, Function);
    return block;
}

void APS5_VABI _ZN3sce2np7RingBuf14CheckinForReadEm(NpRingBuf* self, std::size_t size) {
    constexpr const char* Function = "sce::np::RingBuf::CheckinForRead";
    NpRingBuf ring = Load(self, Function);
    CommitRead(ring, size);
    Store(self, ring, Function);
}

int APS5_VABI _ZN3sce2np7RingBuf4ReadEPvm(NpRingBuf* self, void* data, std::size_t size) {
    constexpr const char* Function = "sce::np::RingBuf::Read";
    if (size == 0) return 0;
    NpRingBuf ring = Load(self, Function);
    if (ring.size < size) return NP_RING_BUF_ERROR_NO_DATA;
    std::vector<std::uint8_t> bytes(data != nullptr ? size : 0);
    std::size_t blockSize = 0;
    const std::uint8_t* block = ReadBlock(ring, blockSize);
    const std::size_t first = size <= blockSize ? size : blockSize;
    if (data != nullptr) CopyIn(bytes.data(), block, first, Function);
    CommitRead(ring, first);
    if (first != size) {
        block = ReadBlock(ring, blockSize);
        Require(block != nullptr && blockSize >= size - first, "sce::np::RingBuf::Read: the rest is not readable after the wrap");
        if (data != nullptr) CopyIn(bytes.data() + first, block, size - first, Function);
        CommitRead(ring, size - first);
    }
    if (data != nullptr) CopyOut(data, bytes.data(), size, Function);
    Store(self, ring, Function);
    return 0;
}

int APS5_VABI _ZN3sce2np7RingBuf4PeekEmPvm(NpRingBuf* self, std::size_t offset, void* data, std::size_t size) {
    constexpr const char* Function = "sce::np::RingBuf::Peek";
    Require(data != nullptr, "sce::np::RingBuf::Peek: null buffer");
    Require(size != 0, "sce::np::RingBuf::Peek: empty read");
    const NpRingBuf ring = Load(self, Function);
    if (offset >= ring.size || size > ring.size - offset) return NP_RING_BUF_ERROR_NO_SPACE;
    std::vector<std::uint8_t> bytes(size);
    std::size_t copied = 0;
    const std::uint8_t* source = ring.readPointer + offset;
    if (source < End(ring)) {
        const std::size_t tail = static_cast<std::size_t>(End(ring) - source);
        if (tail <= size) {
            CopyIn(bytes.data(), source, tail, Function);
            source = ring.buf;
            copied = tail;
        }
    } else {
        source -= ring.bufSize;
    }
    CopyIn(bytes.data() + copied, source, size - copied, Function);
    CopyOut(data, bytes.data(), size, Function);
    return 0;
}

void APS5_VABI _ZN3sce2np13RingBufMemoryC1EP16SceNpAllocatorEx(NpRingBufMemory* self, NpAllocatorEx* allocator) {
    NpRingBufMemory memory {};
    ClearFields(memory.ringBuf);
    memory.ringBuf.vtable = &RingBufMemoryVtable[2];
    memory.allocator = allocator;
    Store(self, memory, "sce::np::RingBufMemory::RingBufMemory");
}

void APS5_VABI _ZN3sce2np13RingBufMemoryC2EP16SceNpAllocatorEx(NpRingBufMemory* self, NpAllocatorEx* allocator) {
    _ZN3sce2np13RingBufMemoryC1EP16SceNpAllocatorEx(self, allocator);
}

void APS5_VABI _ZN3sce2np13RingBufMemoryD1Ev(NpRingBufMemory* self) {
    constexpr const char* Function = "sce::np::RingBufMemory::~RingBufMemory";
    NpRingBufMemory memory = Load(self, Function);
    memory.ringBuf.vtable = &RingBufMemoryVtable[2];
    Store(self, memory, Function);
    ReleaseBuffer(memory, Function);
    memory = Load(self, Function);
    memory.ringBuf.vtable = &RingBufVtable[2];
    Store(self, memory, Function);
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
    constexpr const char* Function = "sce::np::RingBufMemory::dtor";
    ReleaseBuffer(Load(self, Function), Function);
}

int APS5_VABI _ZN3sce2np13RingBufMemory4InitEm(NpRingBufMemory* self, std::size_t bufSize) {
    constexpr const char* Function = "sce::np::RingBufMemory::Init";
    NpRingBufMemory memory = Load(self, Function);
    const NpAllocatorEx allocator = Load(memory.allocator, Function);
    auto* buf = static_cast<std::uint8_t*>(allocator.allocator.allocate(bufSize, allocator.allocator.context));
    if (buf == nullptr) return allocator.outOfMemoryError;
    Require(memory.ringBuf.buf == nullptr, "sce::np::RingBufMemory::Init: the ring buffer already has a buffer");
    Require(bufSize != 0, "sce::np::RingBufMemory::Init: empty buffer");
    Attach(memory.ringBuf, buf, bufSize);
    Store(self, memory, Function);
    return 0;
}

void APS5_VABI _ZN3sce2np13RingBufMemory7DestroyEv(NpRingBufMemory* self) {
    constexpr const char* Function = "sce::np::RingBufMemory::Destroy";
    NpRingBufMemory memory = Load(self, Function);
    ReleaseBuffer(memory, Function);
    ClearFields(memory.ringBuf);
    Store(self, memory, Function);
}

int APS5_VABI _ZN3sce2np13RingBufMemory6ExpandEm(NpRingBufMemory* self, std::size_t extraSize) {
    constexpr const char* Function = "sce::np::RingBufMemory::Expand";
    NpRingBufMemory memory = Load(self, Function);
    NpRingBuf& ringBuf = memory.ringBuf;
    Require(extraSize <= SIZE_MAX - ringBuf.bufSize, "sce::np::RingBufMemory::Expand: the new size overflows");
    const NpAllocatorEx allocator = Load(memory.allocator, Function);
    auto* buf = static_cast<std::uint8_t*>(allocator.allocator.reallocate(ringBuf.buf, ringBuf.bufSize + extraSize,
        allocator.allocator.context));
    if (buf == nullptr) return NP_RING_BUF_ERROR_OUT_OF_MEMORY;
    const std::size_t readOffset = static_cast<std::size_t>(ringBuf.readPointer - ringBuf.buf);
    const std::size_t writeOffset = static_cast<std::size_t>(ringBuf.writePointer - ringBuf.buf);
    std::size_t newReadOffset = readOffset;
    if (ringBuf.writePointer < ringBuf.readPointer) {
        std::vector<std::uint8_t> moved(ringBuf.bufSize - readOffset);
        CopyIn(moved.data(), buf + readOffset, moved.size(), Function);
        CopyOut(buf + readOffset + extraSize, moved.data(), moved.size(), Function);
        newReadOffset += extraSize;
    }
    ringBuf.writePointer = buf + writeOffset;
    ringBuf.readPointer = buf + newReadOffset;
    ringBuf.bufSize += extraSize;
    ringBuf.buf = buf;
    Store(self, memory, Function);
    return 0;
}

}
