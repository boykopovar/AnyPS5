#pragma once

#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::IsoAbi {

using ByteVector = WtfAbi::Vector<std::uint8_t>;

struct OptionalInt8 {
    std::int8_t value;
    bool engaged;
};

struct Box {
    const void* const* vtable;
    std::uint64_t size;
    std::uint32_t boxType;
    ByteVector extendedType;
};

struct FullBox {
    Box box;
    std::uint8_t version;
    std::uint32_t flags;
};

struct TrackEncryptionBox {
    FullBox fullBox;
    OptionalInt8 defaultCryptByteBlock;
    OptionalInt8 defaultSkipByteBlock;
    std::int8_t defaultIsProtected;
    std::int8_t defaultPerSampleIVSize;
    ByteVector defaultKID;
    ByteVector defaultConstantIV;
};

struct ProtectionSystemSpecificHeaderBox {
    FullBox fullBox;
    ByteVector systemID;
    WtfAbi::Vector<ByteVector> keyIDs;
    ByteVector data;
};

struct WebVTTCue {
    Box box;
    MediaAbi::MediaTime presentationTime;
    MediaAbi::MediaTime duration;
    WtfAbi::String sourceID;
    WtfAbi::String identifier;
    WtfAbi::String originalStartTime;
    WtfAbi::String settings;
    WtfAbi::String cueText;
};

static_assert(sizeof(OptionalInt8) == 2 && alignof(OptionalInt8) == 1 && std::is_trivially_copyable_v<OptionalInt8>);
static_assert(sizeof(ByteVector) == 16);
static_assert(std::is_standard_layout_v<Box> && std::is_standard_layout_v<TrackEncryptionBox> && std::is_standard_layout_v<ProtectionSystemSpecificHeaderBox> && std::is_standard_layout_v<WebVTTCue>);
static_assert(sizeof(Box) == 0x28);
static_assert(offsetof(Box, size) == 0x08);
static_assert(offsetof(Box, boxType) == 0x10);
static_assert(offsetof(Box, extendedType) == 0x18);
static_assert(sizeof(FullBox) == 0x30);
static_assert(offsetof(FullBox, version) == 0x28);
static_assert(offsetof(FullBox, flags) == 0x2c);
static_assert(sizeof(TrackEncryptionBox) == 0x58);
static_assert(offsetof(TrackEncryptionBox, defaultCryptByteBlock) == 0x30);
static_assert(offsetof(TrackEncryptionBox, defaultSkipByteBlock) == 0x32);
static_assert(offsetof(TrackEncryptionBox, defaultIsProtected) == 0x34);
static_assert(offsetof(TrackEncryptionBox, defaultPerSampleIVSize) == 0x35);
static_assert(offsetof(TrackEncryptionBox, defaultKID) == 0x38);
static_assert(offsetof(TrackEncryptionBox, defaultConstantIV) == 0x48);
static_assert(sizeof(ProtectionSystemSpecificHeaderBox) == 0x60);
static_assert(offsetof(ProtectionSystemSpecificHeaderBox, systemID) == 0x30);
static_assert(offsetof(ProtectionSystemSpecificHeaderBox, keyIDs) == 0x40);
static_assert(offsetof(ProtectionSystemSpecificHeaderBox, data) == 0x50);
static_assert(sizeof(WebVTTCue) == 0x70);
static_assert(offsetof(WebVTTCue, presentationTime) == 0x28);
static_assert(offsetof(WebVTTCue, duration) == 0x38);
static_assert(offsetof(WebVTTCue, sourceID) == 0x48);
static_assert(offsetof(WebVTTCue, identifier) == 0x50);
static_assert(offsetof(WebVTTCue, originalStartTime) == 0x58);
static_assert(offsetof(WebVTTCue, settings) == 0x60);
static_assert(offsetof(WebVTTCue, cueText) == 0x68);

}  // namespace NKWebKit::IsoAbi
