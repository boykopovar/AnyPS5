#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/IsoBoxAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace NKWebKit::IsoParse {

using IsoAbi::Box;
using IsoAbi::ByteVector;
using IsoAbi::FullBox;

struct DataView {
    unsigned refCount;
    std::uint8_t type;
    bool isDetachable;
    std::uint8_t flags;
    std::uint8_t padding;
    std::size_t byteOffset;
    std::size_t byteLength;
    void* baseAddress;
    const void* buffer;
};

static_assert(sizeof(DataView) == 40);
static_assert(offsetof(DataView, type) == 4);
static_assert(offsetof(DataView, flags) == 6);
static_assert(offsetof(DataView, byteOffset) == 8);
static_assert(offsetof(DataView, byteLength) == 16);
static_assert(offsetof(DataView, baseAddress) == 24);
static_assert(offsetof(DataView, buffer) == 32);

inline constexpr std::uint8_t ViewResizableNonShared = 1;
inline constexpr std::uint8_t ViewGrowableShared = 2;
inline constexpr std::uint8_t ViewAutoLength = 4;

inline constexpr std::size_t ArrayBufferDataOffset = 16;
inline constexpr std::size_t ArrayBufferSharedOffset = 32;
inline constexpr std::size_t ArrayBufferSizeOffset = 48;
inline constexpr std::size_t ArrayBufferHasMaxOffset = 64;
inline constexpr std::size_t SharedContentsSizeOffset = 32;

struct PeekResult {
    std::uint32_t type;
    std::uint64_t size;
    bool engaged;
};

static_assert(sizeof(PeekResult) == 24);
static_assert(offsetof(PeekResult, size) == 8);
static_assert(offsetof(PeekResult, engaged) == 16);

using Destructor = void (APS5_VABI*)(void* self);
using ParseFunction = bool (APS5_VABI*)(void* self, DataView* view, unsigned* offset);

struct Vtable {
    Destructor completeDestructor;
    Destructor deletingDestructor;
    ParseFunction parse;
};

const DataView& CheckedView(const DataView* view, const char* function);
std::size_t ViewByteLength(const DataView& view);
const void* ViewBuffer(const DataView& view);
std::size_t BufferByteLength(const void* buffer);
std::vector<std::uint8_t> Slice(const void* buffer, double begin, double end);

bool ReadU8(const DataView& view, unsigned& offset, std::int8_t& value);
bool ReadU32(const DataView& view, unsigned& offset, std::uint32_t& value);
bool ReadU64(const DataView& view, unsigned& offset, std::uint64_t& value);

void Resize(ByteVector& vector, unsigned size);
void ReserveInitialCapacity(ByteVector& vector, unsigned capacity);
void Assign(ByteVector& target, const ByteVector& source);
void MoveAssign(ByteVector& target, ByteVector&& source);
ByteVector Copy(const ByteVector& source);
void Destroy(ByteVector& vector);

void ConstructBox(Box& box, const Vtable* vtable);
void CopyBox(Box& box, const Box& other, const Vtable* vtable);
void AssignBox(Box& box, const Box& other);
void DestroyBox(Box& box, const Vtable* vtable);
void ConstructFullBox(FullBox& box, const Vtable* vtable);
void CopyFullBox(FullBox& box, const FullBox& other, const Vtable* vtable);
void AssignFullBox(FullBox& box, const FullBox& other);

bool ParseBox(Box& box, const DataView& view, unsigned& offset);
bool ParseFullBox(FullBox& box, const DataView& view, unsigned& offset);
bool ParseVersionAndFlags(FullBox& box, const DataView& view, unsigned& offset);
PeekResult PeekBox(const DataView& view, unsigned offset);

bool VirtualParse(Box& box, const DataView& view, unsigned& offset);
bool VirtualRead(Box& box, const DataView& view, unsigned& offset);
void DeleteBox(Box* box);
void* AllocateBox(std::size_t size);
void FreeBox(void* box);

const Vtable* BoxVtable();
const Vtable* FullBoxVtable();


inline constexpr std::uint32_t FourCC(const char (&text)[5]) {
    return (std::uint32_t(std::uint8_t(text[0])) << 24) | (std::uint32_t(std::uint8_t(text[1])) << 16)
        | (std::uint32_t(std::uint8_t(text[2])) << 8) | std::uint8_t(text[3]);
}

}  // namespace NKWebKit::IsoParse
