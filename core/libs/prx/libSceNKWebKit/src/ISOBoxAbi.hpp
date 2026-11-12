#pragma once

#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct FourCC {
    std::uint32_t value;
};

static_assert(sizeof(FourCC) == 4);
static_assert(alignof(FourCC) == 4);
static_assert(std::is_trivially_copyable_v<FourCC>);

struct ISOBox {
    const void* vtable;
    std::uint64_t m_size;
    FourCC m_boxType;
    WtfAbi::Vector<std::uint8_t> m_extendedType;
};

static_assert(sizeof(WtfAbi::Vector<std::uint8_t>) == 16);
static_assert(alignof(WtfAbi::Vector<std::uint8_t>) == 8);
static_assert(sizeof(ISOBox) == 40);
static_assert(alignof(ISOBox) == 8);
static_assert(offsetof(ISOBox, vtable) == 0);
static_assert(offsetof(ISOBox, m_size) == 8);
static_assert(offsetof(ISOBox, m_boxType) == 16);
static_assert(offsetof(ISOBox, m_extendedType) == 24);

struct ISOFullBox {
    ISOBox base;
    std::uint8_t m_version;
    std::uint32_t m_flags;
};

static_assert(sizeof(ISOFullBox) == 48);
static_assert(alignof(ISOFullBox) == 8);
static_assert(offsetof(ISOFullBox, m_version) == 40);
static_assert(offsetof(ISOFullBox, m_flags) == 44);

struct ISOWebVTTCue {
    ISOBox base;
    MediaAbi::MediaTime m_presentationTime;
    MediaAbi::MediaTime m_duration;
    WtfAbi::String m_sourceID;
    WtfAbi::String m_identifier;
    WtfAbi::String m_originalStartTime;
    WtfAbi::String m_settings;
    WtfAbi::String m_cueText;
};

static_assert(sizeof(WtfAbi::String) == 8);
static_assert(sizeof(ISOWebVTTCue) == 112);
static_assert(alignof(ISOWebVTTCue) == 8);
static_assert(offsetof(ISOWebVTTCue, m_presentationTime) == 40);
static_assert(offsetof(ISOWebVTTCue, m_duration) == 56);
static_assert(offsetof(ISOWebVTTCue, m_sourceID) == 72);
static_assert(offsetof(ISOWebVTTCue, m_identifier) == 80);
static_assert(offsetof(ISOWebVTTCue, m_originalStartTime) == 88);
static_assert(offsetof(ISOWebVTTCue, m_settings) == 96);
static_assert(offsetof(ISOWebVTTCue, m_cueText) == 104);

}
