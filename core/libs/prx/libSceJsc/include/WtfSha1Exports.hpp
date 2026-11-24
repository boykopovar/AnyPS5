#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfFileSystemExports.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Jsc::WtfAbi {

struct SHA1 {
    std::uint8_t buffer[64];
    std::size_t cursor;
    std::uint64_t totalBytes;
    std::uint32_t hash[5];
};

using SHA1Digest = std::array<std::uint8_t, 20>;

struct PersistenceDecoder {
    const std::uint8_t* buffer;
    const std::uint8_t* bufferPosition;
    const std::uint8_t* bufferEnd;
    SHA1 sha1;
};

struct PersistenceEncoder {
    std::uint8_t* buffer;
    unsigned capacity;
    unsigned size;
    std::uint8_t inlineBuffer[4096];
    SHA1 sha1;
};

struct PersistenceAllocator {
    void* (*allocate)(std::size_t size);
    void (*deallocate)(void* ptr);
};

struct OptionalSHA1Digest {
    bool hasValue;
    SHA1Digest value;
};

struct StringImpl;

struct OptionalCString {
    bool hasValue;
    CString value;
};

struct OptionalString {
    bool hasValue;
    StringImpl* value;
};

static_assert(sizeof(SHA1) == 104);
static_assert(alignof(SHA1) == 8);
static_assert(offsetof(SHA1, buffer) == 0);
static_assert(offsetof(SHA1, cursor) == 64);
static_assert(offsetof(SHA1, totalBytes) == 72);
static_assert(offsetof(SHA1, hash) == 80);
static_assert(sizeof(SHA1Digest) == 20);
static_assert(sizeof(PersistenceDecoder) == 128);
static_assert(offsetof(PersistenceDecoder, buffer) == 0);
static_assert(offsetof(PersistenceDecoder, bufferPosition) == 8);
static_assert(offsetof(PersistenceDecoder, bufferEnd) == 16);
static_assert(offsetof(PersistenceDecoder, sha1) == 24);
static_assert(sizeof(PersistenceEncoder) == 4216);
static_assert(offsetof(PersistenceEncoder, buffer) == 0);
static_assert(offsetof(PersistenceEncoder, capacity) == 8);
static_assert(offsetof(PersistenceEncoder, size) == 12);
static_assert(offsetof(PersistenceEncoder, inlineBuffer) == 16);
static_assert(offsetof(PersistenceEncoder, sha1) == 4112);
static_assert(sizeof(OptionalSHA1Digest) == 21);
static_assert(offsetof(OptionalSHA1Digest, hasValue) == 0);
static_assert(offsetof(OptionalSHA1Digest, value) == 1);
static_assert(sizeof(OptionalCString) == 16);
static_assert(offsetof(OptionalCString, value) == 8);
static_assert(sizeof(OptionalString) == 16);
static_assert(offsetof(OptionalString, value) == 8);

}  // namespace Jsc::WtfAbi

extern "C" {

void APS5_VABI _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(Jsc::WtfAbi::SHA1* self, Jsc::WtfAbi::SHA1Digest* digest);
Jsc::WtfAbi::OptionalSHA1Digest APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(Jsc::WtfAbi::PersistenceDecoder* decoder);
void APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(Jsc::WtfAbi::PersistenceEncoder* encoder, const Jsc::WtfAbi::SHA1Digest* digest);
Jsc::WtfAbi::OptionalCString* APS5_VABI _ZN3WTF11Persistence5CoderINS_7CStringEE6decodeERNS0_7DecoderE(
    Jsc::WtfAbi::OptionalCString* result, Jsc::WtfAbi::PersistenceDecoder* decoder);
Jsc::WtfAbi::OptionalString* APS5_VABI _ZN3WTF11Persistence5CoderINS_6StringEE6decodeERNS0_7DecoderE(
    Jsc::WtfAbi::OptionalString* result, Jsc::WtfAbi::PersistenceDecoder* decoder);
void RegisterPersistenceAllocator_nid_no_patch(const Jsc::WtfAbi::PersistenceAllocator* allocator);
const Jsc::WtfAbi::PersistenceAllocator* RegisteredPersistenceAllocator_nid_no_patch();

}
