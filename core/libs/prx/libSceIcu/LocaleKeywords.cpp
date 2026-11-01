// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
using namespace Icu::Abi;
struct KeywordContext { char* keywords; char* current; };
static_assert(sizeof(KeywordContext) == 16 && alignof(KeywordContext) == 8);
static_assert(offsetof(KeywordContext, keywords) == 0 && offsetof(KeywordContext, current) == 8);
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU keyword list: null pointer");
    return *value;
}
void APS5_VABI Close_nid_no_patch(Icu::UEnumeration* enumeration) {
    auto* context = static_cast<KeywordContext*>(Require(enumeration).context);
    Free_nid_no_patch(Require(context).keywords);
    Free_nid_no_patch(context);
    Free_nid_no_patch(enumeration);
}
std::int32_t APS5_VABI Count_nid_no_patch(Icu::UEnumeration* enumeration, std::int32_t*) {
    const auto* context = static_cast<const KeywordContext*>(Require(enumeration).context);
    const char* keyword = &Require(Require(context).keywords);
    std::int32_t count = 0;
    while (*keyword) {
        count = CheckedInt_nid_no_patch(static_cast<std::int64_t>(count) + 1);
        keyword += std::strlen(keyword) + 1;
    }
    return count;
}
const char* APS5_VABI Next_nid_no_patch(Icu::UEnumeration* enumeration, std::int32_t* length, std::int32_t*) {
    auto* context = static_cast<KeywordContext*>(Require(enumeration).context);
    const char* result = &Require(Require(context).current);
    std::int32_t size = 0;
    if (*result) {
        size = CheckedInt_nid_no_patch(std::strlen(result));
        context->current += static_cast<std::size_t>(size) + 1;
    } else result = nullptr;
    if (length) *length = size;
    return result;
}
void APS5_VABI Reset_nid_no_patch(Icu::UEnumeration* enumeration, std::int32_t*) {
    auto* context = static_cast<KeywordContext*>(Require(enumeration).context);
    Require(context).current = context->keywords;
}
}

extern "C" {
Icu::UEnumeration* APS5_VABI uloc_openKeywordList_nid_postfix(const char* keywords, std::int32_t size, std::int32_t* status) {
    if (Require(status) > 0) return nullptr;
    if (size < 0 || size == std::numeric_limits<std::int32_t>::max() || (size && !keywords)) throw std::invalid_argument("ICU keyword list: invalid copy size or source");
    if (size && keywords[size - 1] != 0) throw std::invalid_argument("ICU keyword list: missing list terminator");
    auto* context = static_cast<KeywordContext*>(Allocate_nid_no_patch(sizeof(KeywordContext)));
    auto* result = static_cast<Icu::UEnumeration*>(Allocate_nid_no_patch(sizeof(Icu::UEnumeration)));
    if (!context || !result) {
        Free_nid_no_patch(context); Free_nid_no_patch(result); *status = 7; return nullptr;
    }
    context->keywords = static_cast<char*>(Allocate_nid_no_patch(static_cast<std::size_t>(size) + 1));
    if (!context->keywords) { Free_nid_no_patch(context); Free_nid_no_patch(result); *status = 7; return nullptr; }
    if (size) std::memcpy(context->keywords, keywords, size);
    context->keywords[size] = 0;
    context->current = context->keywords;
    *result = {nullptr, context, Close_nid_no_patch, Count_nid_no_patch, uenum_unextDefault_nid_postfix, Next_nid_no_patch, Reset_nid_no_patch};
    return result;
}
}
