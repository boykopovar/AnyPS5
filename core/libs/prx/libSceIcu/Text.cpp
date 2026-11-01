// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <limits>
#include <stdexcept>

namespace {

using Text = Icu::Text;

const Text& Checked(const Text* text) {
    if (!text) throw std::invalid_argument("UText: null text");
    return *text;
}

Text& Checked(Text* text) {
    return const_cast<Text&>(Checked(static_cast<const Text*>(text)));
}

template<class TCallback>
TCallback Callback(const Text& text, TCallback Icu::TextFunctions::*member) {
    if (!text.functions || !(text.functions->*member)) throw std::invalid_argument("UText: null provider callback");
    return text.functions->*member;
}

bool Access(Text& text, std::int64_t index, std::int8_t forward) {
    return Callback(text, &Icu::TextFunctions::access)(&text, index, forward) != 0;
}

std::int32_t Unit(const Text& text, std::int32_t offset) {
    if (!text.chunkContents || offset < 0 || offset >= text.chunkLength)
        throw std::invalid_argument("UText: invalid chunk access");
    return text.chunkContents[offset];
}

bool IsLead(std::int32_t value) {
    return value >= 0xd800 && value <= 0xdbff;
}

bool IsTrail(std::int32_t value) {
    return value >= 0xdc00 && value <= 0xdfff;
}

bool IsSurrogate(std::int32_t value) {
    return value >= 0xd800 && value <= 0xdfff;
}

std::int32_t Supplementary(std::int32_t lead, std::int32_t trail) {
    return 0x10000 + ((lead - 0xd800) << 10) + trail - 0xdc00;
}

std::int64_t Add(std::int64_t first, std::int64_t second) {
    if ((second > 0 && first > std::numeric_limits<std::int64_t>::max() - second) ||
        (second < 0 && first < std::numeric_limits<std::int64_t>::min() - second))
        throw std::invalid_argument("UText: overflowing native index");
    return first + second;
}

std::int64_t Difference(std::int64_t first, std::int64_t second) {
    if ((second < 0 && first > std::numeric_limits<std::int64_t>::max() + second) ||
        (second > 0 && first < std::numeric_limits<std::int64_t>::min() + second))
        throw std::invalid_argument("UText: overflowing native index difference");
    return first - second;
}

std::int32_t Before(std::int32_t offset) {
    if (offset == std::numeric_limits<std::int32_t>::min()) throw std::invalid_argument("UText: overflowing chunk offset");
    return offset - 1;
}

bool Failed(std::int32_t* status) {
    if (!status) throw std::invalid_argument("UText: null status");
    return *status > 0;
}

}

extern "C" {

std::int8_t APS5_VABI utext_isLengthExpensive_nid_postfix(const Icu::Text* text) {
    return (Checked(text).providerProperties & 2) != 0;
}

std::int8_t APS5_VABI utext_isWritable_nid_postfix(const Icu::Text* text) {
    return (Checked(text).providerProperties & 8) != 0;
}

std::int8_t APS5_VABI utext_hasMetaData_nid_postfix(const Icu::Text* text) {
    return (Checked(text).providerProperties & 16) != 0;
}

void APS5_VABI utext_freeze_nid_postfix(Icu::Text* text) {
    Checked(text).providerProperties &= ~8;
}

std::int64_t APS5_VABI utext_nativeLength_nid_postfix(Icu::Text* text) {
    return Callback(Checked(text), &Icu::TextFunctions::nativeLength)(text);
}

std::int64_t APS5_VABI utext_getNativeIndex_nid_postfix(const Icu::Text* text) {
    const auto& value = Checked(text);
    return value.chunkOffset <= value.nativeIndexingLimit ? Add(value.chunkNativeStart, value.chunkOffset) :
        Callback(value, &Icu::TextFunctions::mapOffsetToNative)(text);
}

std::int8_t APS5_VABI utext_equals_nid_postfix(const Icu::Text* first, const Icu::Text* second) {
    if (!first || !second || first->magic != 0x345ad82c || second->magic != 0x345ad82c) return 0;
    return first->functions == second->functions && first->context == second->context &&
        utext_getNativeIndex_nid_postfix(first) == utext_getNativeIndex_nid_postfix(second);
}

void APS5_VABI utext_setNativeIndex_nid_postfix(Icu::Text* pointer, std::int64_t index) {
    auto& text = Checked(pointer);
    if (index < text.chunkNativeStart || index >= text.chunkNativeLimit) Access(text, index, 1);
    else if (static_cast<std::int32_t>(Difference(index, text.chunkNativeStart)) <= text.nativeIndexingLimit)
        text.chunkOffset = static_cast<std::int32_t>(Difference(index, text.chunkNativeStart));
    else text.chunkOffset = Callback(text, &Icu::TextFunctions::mapNativeIndexToUTF16)(pointer, index);
    if (text.chunkOffset < text.chunkLength && IsTrail(Unit(text, text.chunkOffset))) {
        if (text.chunkOffset == 0) Access(text, text.chunkNativeStart, 0);
        if (text.chunkOffset > 0 && IsLead(Unit(text, Before(text.chunkOffset)))) --text.chunkOffset;
    }
}

std::int32_t APS5_VABI utext_current32_nid_postfix(Icu::Text* pointer) {
    auto& text = Checked(pointer);
    if (text.chunkOffset == text.chunkLength && !Access(text, text.chunkNativeLimit, 1)) return -1;
    const auto lead = Unit(text, text.chunkOffset);
    if (!IsLead(lead)) return lead;
    std::int32_t trail = 0;
    if (static_cast<std::int64_t>(text.chunkOffset) + 1 < text.chunkLength) trail = Unit(text, text.chunkOffset + 1);
    else {
        const auto position = text.chunkNativeLimit;
        const auto offset = text.chunkOffset;
        if (Access(text, position, 1)) trail = Unit(text, text.chunkOffset);
        const auto restored = Access(text, position, 0);
        text.chunkOffset = offset;
        if (!restored) return -1;
    }
    return IsTrail(trail) ? Supplementary(lead, trail) : lead;
}

std::int32_t APS5_VABI utext_next32_nid_postfix(Icu::Text* pointer) {
    auto& text = Checked(pointer);
    if (text.chunkOffset >= text.chunkLength && !Access(text, text.chunkNativeLimit, 1)) return -1;
    const auto lead = Unit(text, text.chunkOffset);
    ++text.chunkOffset;
    if (!IsLead(lead)) return lead;
    if (text.chunkOffset >= text.chunkLength && !Access(text, text.chunkNativeLimit, 1)) return lead;
    const auto trail = Unit(text, text.chunkOffset);
    if (!IsTrail(trail)) return lead;
    ++text.chunkOffset;
    return Supplementary(lead, trail);
}

std::int32_t APS5_VABI utext_previous32_nid_postfix(Icu::Text* pointer) {
    auto& text = Checked(pointer);
    if (text.chunkOffset <= 0 && !Access(text, text.chunkNativeStart, 0)) return -1;
    text.chunkOffset = Before(text.chunkOffset);
    const auto trail = Unit(text, text.chunkOffset);
    if (!IsTrail(trail)) return trail;
    if (text.chunkOffset <= 0 && !Access(text, text.chunkNativeStart, 0)) return trail;
    const auto lead = Unit(text, Before(text.chunkOffset));
    if (!IsLead(lead)) return trail;
    --text.chunkOffset;
    return Supplementary(lead, trail);
}

std::int32_t APS5_VABI utext_char32At_nid_postfix(Icu::Text* pointer, std::int64_t index) {
    auto& text = Checked(pointer);
    if (index >= text.chunkNativeStart && index < Add(text.chunkNativeStart, text.nativeIndexingLimit)) {
        text.chunkOffset = static_cast<std::int32_t>(Difference(index, text.chunkNativeStart));
        const auto value = Unit(text, text.chunkOffset);
        if (!IsSurrogate(value)) return value;
    }
    utext_setNativeIndex_nid_postfix(pointer, index);
    if (index < text.chunkNativeStart || text.chunkOffset >= text.chunkLength) return -1;
    const auto value = Unit(text, text.chunkOffset);
    return IsSurrogate(value) ? utext_current32_nid_postfix(pointer) : value;
}

std::int32_t APS5_VABI utext_next32From_nid_postfix(Icu::Text* pointer, std::int64_t index) {
    auto& text = Checked(pointer);
    if (index < text.chunkNativeStart || index >= text.chunkNativeLimit) {
        if (!Access(text, index, 1)) return -1;
    } else if (Difference(index, text.chunkNativeStart) <= text.nativeIndexingLimit)
        text.chunkOffset = static_cast<std::int32_t>(Difference(index, text.chunkNativeStart));
    else text.chunkOffset = Callback(text, &Icu::TextFunctions::mapNativeIndexToUTF16)(pointer, index);
    const auto value = Unit(text, text.chunkOffset);
    ++text.chunkOffset;
    if (!IsSurrogate(value)) return value;
    utext_setNativeIndex_nid_postfix(pointer, index);
    return utext_next32_nid_postfix(pointer);
}

std::int32_t APS5_VABI utext_previous32From_nid_postfix(Icu::Text* pointer, std::int64_t index) {
    auto& text = Checked(pointer);
    if (index <= text.chunkNativeStart || index > text.chunkNativeLimit) {
        if (!Access(text, index, 0)) return -1;
    } else if (Difference(index, text.chunkNativeStart) <= text.nativeIndexingLimit)
        text.chunkOffset = static_cast<std::int32_t>(Difference(index, text.chunkNativeStart));
    else {
        text.chunkOffset = Callback(text, &Icu::TextFunctions::mapNativeIndexToUTF16)(pointer, index);
        if (text.chunkOffset == 0 && !Access(text, index, 0)) return -1;
    }
    text.chunkOffset = Before(text.chunkOffset);
    const auto value = Unit(text, text.chunkOffset);
    if (!IsSurrogate(value)) return value;
    utext_setNativeIndex_nid_postfix(pointer, index);
    return utext_previous32_nid_postfix(pointer);
}

std::int8_t APS5_VABI utext_moveIndex32_nid_postfix(Icu::Text* pointer, std::int32_t delta) {
    if (delta == 0) return 1;
    auto& text = Checked(pointer);
    while (delta > 0) {
        if (text.chunkOffset >= text.chunkLength && !Access(text, text.chunkNativeLimit, 1)) return 0;
        if (IsSurrogate(Unit(text, text.chunkOffset))) {
            if (utext_next32_nid_postfix(pointer) == -1) return 0;
        } else ++text.chunkOffset;
        --delta;
    }
    while (delta < 0) {
        if (text.chunkOffset <= 0 && !Access(text, text.chunkNativeStart, 0)) return 0;
        if (IsSurrogate(Unit(text, Before(text.chunkOffset)))) {
            if (utext_previous32_nid_postfix(pointer) == -1) return 0;
        } else --text.chunkOffset;
        ++delta;
    }
    return 1;
}

std::int64_t APS5_VABI utext_getPreviousNativeIndex_nid_postfix(Icu::Text* pointer) {
    auto& text = Checked(pointer);
    const auto previous = Before(text.chunkOffset);
    if (previous >= 0 && !IsTrail(Unit(text, previous))) {
        if (previous <= text.nativeIndexingLimit) return Add(text.chunkNativeStart, previous);
        text.chunkOffset = previous;
        const auto result = Callback(text, &Icu::TextFunctions::mapOffsetToNative)(pointer);
        if (text.chunkOffset == std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("UText: overflowing chunk offset");
        ++text.chunkOffset;
        return result;
    }
    if (text.chunkOffset == 0 && text.chunkNativeStart == 0) return 0;
    utext_previous32_nid_postfix(pointer);
    const auto result = utext_getNativeIndex_nid_postfix(pointer);
    utext_next32_nid_postfix(pointer);
    return result;
}

std::int32_t APS5_VABI utext_extract_nid_postfix(Icu::Text* text, std::int64_t start, std::int64_t limit,
    char16_t* destination, std::int32_t capacity, std::int32_t* status) {
    return Callback(Checked(text), &Icu::TextFunctions::extract)(text, start, limit, destination, capacity, status);
}

std::int32_t APS5_VABI utext_replace_nid_postfix(Icu::Text* text, std::int64_t start, std::int64_t limit,
    const char16_t* replacement, std::int32_t length, std::int32_t* status) {
    if (Failed(status)) return 0;
    if ((Checked(text).providerProperties & 8) == 0) {
        *status = 30;
        return 0;
    }
    return Callback(*text, &Icu::TextFunctions::replace)(text, start, limit, replacement, length, status);
}

void APS5_VABI utext_copy_nid_postfix(Icu::Text* text, std::int64_t start, std::int64_t limit,
    std::int64_t destination, std::int8_t move, std::int32_t* status) {
    if (Failed(status)) return;
    if ((Checked(text).providerProperties & 8) == 0) {
        *status = 30;
        return;
    }
    Callback(*text, &Icu::TextFunctions::copy)(text, start, limit, destination, move, status);
}

Icu::Text* APS5_VABI utext_clone_nid_postfix(Icu::Text* destination, const Icu::Text* source,
    std::int8_t deep, std::int8_t readOnly, std::int32_t* status) {
    if (Failed(status)) return destination;
    auto* result = Callback(Checked(source), &Icu::TextFunctions::clone)(destination, source, deep, status);
    if (*status > 0) return result;
    if (!result) {
        *status = 7;
        return nullptr;
    }
    if (readOnly) utext_freeze_nid_postfix(result);
    return result;
}

}
