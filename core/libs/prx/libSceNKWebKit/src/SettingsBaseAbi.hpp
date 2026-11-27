#pragma once

#include "prx/libSceNKWebKit/src/ContentTypeAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::SettingsBaseAbi {

using WebCoreAbi::ContentType;
using WtfAbi::String;
using WtfAbi::StringImpl;

static_assert(sizeof(StringImpl) == 24);
static_assert(sizeof(WTF::Seconds) == 8);
static_assert(sizeof(WtfAbi::Vector<ContentType>) == 16);
static_assert(offsetof(WtfAbi::Vector<ContentType>, buffer) == 0);
static_assert(offsetof(WtfAbi::Vector<ContentType>, capacity) == 8);
static_assert(offsetof(WtfAbi::Vector<ContentType>, size) == 12);
static_assert(sizeof(ContentType) == 8);
static_assert(alignof(ContentType) == 8);

constexpr std::size_t kPageOffset = 8;
constexpr std::size_t kMinimumDOMTimerIntervalOffset = 16;
constexpr std::size_t kTimerSize = 48;
constexpr std::size_t kMediaContentTypesRequiringHardwareSupportOffset =
    kMinimumDOMTimerIntervalOffset + sizeof(double) + kTimerSize;

static_assert(kMinimumDOMTimerIntervalOffset == kPageOffset + 8);
static_assert(kMediaContentTypesRequiringHardwareSupportOffset == kMinimumDOMTimerIntervalOffset + sizeof(double) + kTimerSize);

constexpr unsigned kFontGenericFamilyMapCount = 7;
constexpr std::size_t kFontGenericFamiliesSize = kFontGenericFamilyMapCount * 8;

constexpr std::size_t kStandardFontFamilyMapOffset = 0;
constexpr std::size_t kSerifFontFamilyMapOffset = 8;
constexpr std::size_t kFixedFontFamilyMapOffset = 16;
constexpr std::size_t kSansSerifFontFamilyMapOffset = 24;
constexpr std::size_t kCursiveFontFamilyMapOffset = 32;
constexpr std::size_t kFantasyFontFamilyMapOffset = 40;
constexpr std::size_t kPictographFontFamilyMapOffset = 48;

struct ScriptFontFamilyEntry {
    int key;
    String value;
};

static_assert(sizeof(ScriptFontFamilyEntry) == 16);
static_assert(offsetof(ScriptFontFamilyEntry, key) == 0);
static_assert(offsetof(ScriptFontFamilyEntry, value) == 8);
static_assert(alignof(ScriptFontFamilyEntry) == 8);

constexpr int kEmptyKey = -2;
constexpr int kDeletedKey = -3;
constexpr unsigned kMinimumTableSize = 8;
constexpr std::size_t kTableMetadataSize = 16;
constexpr std::size_t kTableSizeSlot = 1;
constexpr std::size_t kTableSizeMaskSlot = 2;
constexpr std::size_t kKeyCountSlot = 3;
constexpr std::size_t kDeletedCountSlot = 4;

}  // namespace NKWebKit::SettingsBaseAbi
