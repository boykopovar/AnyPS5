#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::YarrAbi {

inline constexpr int TextCaseInsensitive = 1;
inline constexpr int MultilineEnabled = 1;
inline constexpr int UnicodeAwareMode = 1;

struct StringReference {
    Jsc::WtfAbi::StringImpl* impl;
};

struct RegularExpressionPrivate {
    std::uint32_t refCount;
    std::uint8_t constructionErrorCode;
    void* regexAllocator;
    std::int32_t lastMatchLength;
    std::uint32_t numSubpatterns;
    void* regExpByteCode;
};

struct RegularExpression {
    RegularExpressionPrivate* d;
};

static_assert(sizeof(StringReference) == 8);
static_assert(sizeof(RegularExpressionPrivate) == 32);
static_assert(offsetof(RegularExpressionPrivate, refCount) == 0);
static_assert(offsetof(RegularExpressionPrivate, constructionErrorCode) == 4);
static_assert(offsetof(RegularExpressionPrivate, regexAllocator) == 8);
static_assert(offsetof(RegularExpressionPrivate, lastMatchLength) == 16);
static_assert(offsetof(RegularExpressionPrivate, numSubpatterns) == 20);
static_assert(offsetof(RegularExpressionPrivate, regExpByteCode) == 24);
static_assert(sizeof(RegularExpression) == 8);
static_assert(std::is_trivially_copyable_v<RegularExpression>);

} // namespace Jsc::YarrAbi

extern "C" {
void APS5_VABI _ZN3JSC4Yarr17RegularExpressionC1ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE(Jsc::YarrAbi::RegularExpression* self, const Jsc::YarrAbi::StringReference* pattern, int caseSensitivity, int multilineMode, int unicodeMode);
void APS5_VABI _ZN3JSC4Yarr17RegularExpressionC2ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE(Jsc::YarrAbi::RegularExpression* self, const Jsc::YarrAbi::StringReference* pattern, int caseSensitivity, int multilineMode, int unicodeMode);
int APS5_VABI _ZNK3JSC4Yarr17RegularExpression5matchERKN3WTF6StringEiPi(const Jsc::YarrAbi::RegularExpression* self, const Jsc::YarrAbi::StringReference* string, int startFrom, int* matchLength);
int APS5_VABI _ZNK3JSC4Yarr17RegularExpression9searchRevERKN3WTF6StringE(const Jsc::YarrAbi::RegularExpression* self, const Jsc::YarrAbi::StringReference* string);
bool APS5_VABI _ZNK3JSC4Yarr17RegularExpression7isValidEv(const Jsc::YarrAbi::RegularExpression* self);
void APS5_VABI JscRegularExpressionConstruct_nid_no_patch(Jsc::YarrAbi::RegularExpression* self, const Jsc::YarrAbi::StringReference* pattern, int caseSensitivity, int multilineMode, int unicodeMode);
int APS5_VABI JscRegularExpressionMatch_nid_no_patch(const Jsc::YarrAbi::RegularExpression* self, const Jsc::YarrAbi::StringReference* string, int startFrom, int* matchLength);
void APS5_VABI JscRegularExpressionRelease_nid_no_patch(Jsc::YarrAbi::RegularExpressionPrivate* d);
}
