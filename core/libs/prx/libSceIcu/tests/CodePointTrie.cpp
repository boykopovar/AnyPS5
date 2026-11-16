#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/ucptrie.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <random>
#include <vector>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidFormat = 3;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t Warning = -124;
constexpr std::int32_t CodePoints = 0x110000;
constexpr std::uint32_t InitialValue = 0x11;
constexpr std::uint32_t ErrorValue = 0xBAD;
constexpr std::int32_t TypeAny = -1;
constexpr std::int32_t TypeFast = 0;
constexpr std::int32_t TypeSmall = 1;
constexpr std::int32_t WidthAny = -1;
constexpr std::int32_t Width16 = 0;
constexpr std::int32_t Width32 = 1;
constexpr std::int32_t Width8 = 2;
constexpr std::int32_t RangeNormal = 0;
constexpr std::int32_t RangeFixedLeadSurrogates = 1;
constexpr std::int32_t RangeFixedAllSurrogates = 2;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

template<class F>
bool Throws(F call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct FilterContext {
    std::uint32_t mask;
    std::int32_t calls;
};

std::uint32_t APS5_VABI MaskValue_nid_no_patch(const void* context, std::uint32_t value) {
    auto* filter = const_cast<FilterContext*>(static_cast<const FilterContext*>(context));
    ++filter->calls;
    return value & filter->mask;
}

using Model = std::vector<std::uint32_t>;

std::uint32_t Filtered(const Model& model, std::int32_t c, std::int32_t option, std::uint32_t surrogateValue, const FilterContext* filter) {
    const bool lead = c >= 0xD800 && c <= 0xDBFF;
    const bool trail = c >= 0xDC00 && c <= 0xDFFF;
    if ((option == RangeFixedLeadSurrogates && lead) || (option == RangeFixedAllSurrogates && (lead || trail))) return surrogateValue;
    return filter ? model[c] & filter->mask : model[c];
}

template<class GetRange>
void ExpectRanges(GetRange getRange, const Model& model, std::int32_t option, std::uint32_t surrogateValue, bool useFilter, const char* what) {
    FilterContext filter{0xF0F, 0};
    std::int32_t start = 0;
    while (start < CodePoints) {
        std::uint32_t value = 0xFFFFFFFF;
        const std::int32_t end = getRange(start, option, surrogateValue, useFilter ? MaskValue_nid_no_patch : nullptr, useFilter ? &filter : nullptr, &value);
        Expect(end >= start && end < CodePoints, what);
        const std::uint32_t expected = Filtered(model, start, option, surrogateValue, useFilter ? &filter : nullptr);
        Expect(value == expected, what);
        for (std::int32_t c = start; c <= end; ++c) Expect(Filtered(model, c, option, surrogateValue, useFilter ? &filter : nullptr) == expected, what);
        if (end + 1 < CodePoints) Expect(Filtered(model, end + 1, option, surrogateValue, useFilter ? &filter : nullptr) != expected, what);
        start = end + 1;
    }
    Expect(getRange(CodePoints, option, surrogateValue, nullptr, nullptr, nullptr) < 0, what);
    Expect(!useFilter || filter.calls > 0, "the guest filter is called with its context");
}

Model InitialModel(std::uint32_t mask) {
    return Model(CodePoints, InitialValue & mask);
}

UMutableCPTrie* Fill(Model& model, std::uint32_t seed, std::uint32_t mask) {
    std::mt19937 random(seed);
    std::int32_t status = Success;
    UMutableCPTrie* trie = umutablecptrie_open_nid_postfix(InitialValue & mask, ErrorValue, &status);
    Expect(trie != nullptr && status == Success, "umutablecptrie_open");
    for (std::int32_t step = 0; step < 300; ++step) {
        const std::int32_t start = static_cast<std::int32_t>(random() % CodePoints);
        const std::int32_t length = static_cast<std::int32_t>(random() % (step % 3 == 0 ? 0x3000 : 0x80));
        const std::int32_t end = std::min(CodePoints - 1, start + length);
        const std::uint32_t value = (static_cast<std::uint32_t>(random() % 40) * 0x9E3779B1u) & mask;
        umutablecptrie_setRange_nid_postfix(trie, start, end, value, &status);
        for (std::int32_t c = start; c <= end; ++c) model[c] = value;
    }
    for (std::int32_t step = 0; step < 2000; ++step) {
        const std::int32_t c = step < 300 ? static_cast<std::int32_t>(random() % 0x800) : static_cast<std::int32_t>(random() % CodePoints);
        const std::uint32_t value = (static_cast<std::uint32_t>(random() % 40) * 0x9E3779B1u) & mask;
        umutablecptrie_set_nid_postfix(trie, c, value, &status);
        model[c] = value;
    }
    Expect(status == Success, "set and setRange succeed");
    return trie;
}

void ExpectMutable(const UMutableCPTrie* trie, const Model& model, const char* what, std::uint32_t error = ErrorValue) {
    for (std::int32_t c = 0; c < CodePoints; ++c) Expect(umutablecptrie_get_nid_postfix(trie, c) == model[c], what);
    Expect(umutablecptrie_get_nid_postfix(trie, -1) == error && umutablecptrie_get_nid_postfix(trie, CodePoints) == error, "out-of-range code points give the error value");
}

void ExpectImmutable(const UCPTrie* trie, const Model& model, std::int32_t type, std::int32_t width, const char* what) {
    Expect(ucptrie_getType_nid_postfix(trie) == type && ucptrie_getValueWidth_nid_postfix(trie) == width, "type and value width");
    for (std::int32_t c = 0; c < CodePoints; ++c) Expect(ucptrie_get_nid_postfix(trie, c) == model[c], what);
    const std::uint32_t error = ErrorValue & (width == Width8 ? 0xFFu : width == Width16 ? 0xFFFFu : 0xFFFFFFFFu);
    Expect(ucptrie_get_nid_postfix(trie, -1) == error && ucptrie_get_nid_postfix(trie, CodePoints) == error, "an immutable trie keeps the error value in its width");
}

std::uint32_t DataAt(const UCPTrie* trie, std::int32_t index) {
    switch (trie->valueWidth) {
    case UCPTRIE_VALUE_BITS_16: return trie->data.ptr16[index];
    case UCPTRIE_VALUE_BITS_32: return trie->data.ptr32[index];
    default: return trie->data.ptr8[index];
    }
}

void ExpectInternalIndexes(const UCPTrie* trie, const Model& model) {
    const std::int32_t fastLimit = trie->type == UCPTRIE_TYPE_FAST ? 0x10000 : 0x1000;
    for (std::int32_t c = fastLimit; c < trie->highStart; c += 37) {
        const std::int32_t index = ucptrie_internalSmallIndex_nid_postfix(trie, c);
        Expect(index == ucptrie_internalSmallIndex(trie, c) && DataAt(trie, index) == model[c], "internalSmallIndex finds the value");
    }
    if (trie->type == UCPTRIE_TYPE_SMALL) {
        for (std::int32_t c = 0x1000; c < 0x10000; c += 29) {
            const std::int32_t index = ucptrie_internalSmallU8Index_nid_postfix(trie, c >> 12, static_cast<std::uint8_t>((c >> 6) & 0x3F), static_cast<std::uint8_t>(c & 0x3F));
            Expect(index == ucptrie_internalSmallU8Index(trie, c >> 12, static_cast<std::uint8_t>((c >> 6) & 0x3F), static_cast<std::uint8_t>(c & 0x3F)), "internalSmallU8Index matches ICU");
            if (c < trie->highStart) Expect(DataAt(trie, index) == model[c], "internalSmallU8Index finds the value");
        }
    }
    if (trie->type != UCPTRIE_TYPE_FAST) return;
    const std::uint8_t text[] = {'a', 0xC3, 0xA9, 0xE2, 0x82, 0xAC, 0xF0, 0x9F, 0x98, 0x80, 0xE2, 0x82};
    for (std::size_t position = 1; position < sizeof(text); ++position) {
        const std::uint8_t* src = text + position;
        Expect(ucptrie_internalU8PrevIndex_nid_postfix(trie, *src, text, src) == ucptrie_internalU8PrevIndex(trie, *src, text, src), "internalU8PrevIndex matches ICU");
    }
    const std::int32_t packed = ucptrie_internalU8PrevIndex_nid_postfix(trie, 0x80, text, text + 9);
    Expect((packed & 7) == 3 && DataAt(trie, packed >> 3) == model[0x1F600], "internalU8PrevIndex decodes the previous code point");
    const std::int32_t euro = ucptrie_internalU8PrevIndex_nid_postfix(trie, 0xAC, text, text + 5);
    Expect((euro & 7) == 2 && DataAt(trie, euro >> 3) == model[0x20AC], "internalU8PrevIndex decodes a three-byte code point");
}

void ExpectBinary(const UCPTrie* trie, const Model& model, std::int32_t type, std::int32_t width) {
    std::int32_t status = Success;
    const std::int32_t length = ucptrie_toBinary_nid_postfix(trie, nullptr, 0, &status);
    Expect(status == BufferOverflow && length > 16, "toBinary preflights its length");
    std::vector<std::uint32_t> buffer((length + 3) / 4 + 1, 0xA5A5A5A5u);
    status = Success;
    Expect(ucptrie_toBinary_nid_postfix(trie, buffer.data(), length - 1, &status) == length && status == BufferOverflow, "a short buffer overflows");
    status = Success;
    Expect(ucptrie_toBinary_nid_postfix(trie, buffer.data(), length, &status) == length && status == Success, "toBinary writes the trie");
    std::int32_t actual = 0;
    UCPTrie* reopened = ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, buffer.data(), length + 4, &actual, &status);
    Expect(reopened != nullptr && status == Success && actual == length, "openFromBinary reads back the length it wrote");
    ExpectImmutable(reopened, model, type, width, "a reopened trie gives the same values");
    std::vector<std::uint32_t> again(buffer.size(), 0);
    Expect(ucptrie_toBinary_nid_postfix(reopened, again.data(), length, &status) == length && std::memcmp(again.data(), buffer.data(), length) == 0, "the round trip is byte-identical");
    ucptrie_close_nid_postfix(reopened);
    const std::int32_t otherType = type == TypeFast ? TypeSmall : TypeFast;
    Expect(ucptrie_openFromBinary_nid_postfix(otherType, width, buffer.data(), length, nullptr, &status) == nullptr && status == InvalidFormat, "a different type is refused");
    status = Success;
    Expect(ucptrie_openFromBinary_nid_postfix(type, width == Width8 ? Width16 : Width8, buffer.data(), length, nullptr, &status) == nullptr && status == InvalidFormat, "a different value width is refused");
    status = Success;
    Expect(ucptrie_openFromBinary_nid_postfix(type, width, buffer.data(), 8, nullptr, &status) == nullptr && status == InvalidFormat, "a truncated trie is refused");
    status = Success;
    Expect(ucptrie_openFromBinary_nid_postfix(type, width, buffer.data(), 0, nullptr, &status) == nullptr && status == IllegalArgument, "an empty buffer is refused");
    status = IllegalArgument;
    Expect(ucptrie_openFromBinary_nid_postfix(type, width, buffer.data(), length, nullptr, &status) == nullptr && status == IllegalArgument, "openFromBinary with an error set");
    Expect(ucptrie_toBinary_nid_postfix(trie, buffer.data(), length, &status) == 0 && status == IllegalArgument, "toBinary with an error set");
}

void Builds() {
    const std::int32_t widths[] = {Width16, Width32, Width8};
    const std::uint32_t masks[] = {0xFFFF, 0xFFFFFFFF, 0xFF};
    std::uint32_t seed = 1;
    for (std::int32_t widthIndex = 0; widthIndex < 3; ++widthIndex) {
        for (std::int32_t type : {TypeFast, TypeSmall}) {
            Model model = InitialModel(masks[widthIndex]);
            UMutableCPTrie* mutableTrie = Fill(model, seed++, masks[widthIndex]);
            ExpectMutable(mutableTrie, model, "umutablecptrie_get matches the model");
            ExpectRanges([&](std::int32_t start, std::int32_t option, std::uint32_t surrogate, Icu::CodePointValueFilter filter, const void* context, std::uint32_t* value) {
                return umutablecptrie_getRange_nid_postfix(mutableTrie, start, option, surrogate, filter, context, value);
            }, model, RangeNormal, 0, false, "umutablecptrie_getRange matches the model");
            std::int32_t status = Success;
            UMutableCPTrie* clone = umutablecptrie_clone_nid_postfix(mutableTrie, &status);
            Expect(clone != nullptr && status == Success, "umutablecptrie_clone");
            UCPTrie* trie = umutablecptrie_buildImmutable_nid_postfix(clone, type, widths[widthIndex], &status);
            Expect(trie != nullptr && status == Success, "buildImmutable");
            ExpectImmutable(trie, model, type, widths[widthIndex], "ucptrie_get matches the model");
            ExpectMutable(mutableTrie, model, "the original survives building from its clone");
            for (std::int32_t option : {RangeNormal, RangeFixedLeadSurrogates, RangeFixedAllSurrogates}) {
                for (bool useFilter : {false, true}) {
                    ExpectRanges([&](std::int32_t start, std::int32_t o, std::uint32_t surrogate, Icu::CodePointValueFilter filter, const void* context, std::uint32_t* value) {
                        return ucptrie_getRange_nid_postfix(trie, start, o, surrogate, filter, context, value);
                    }, model, option, 0x5A & masks[widthIndex], useFilter, "ucptrie_getRange matches the model");
                    ExpectRanges([&](std::int32_t start, std::int32_t o, std::uint32_t surrogate, Icu::CodePointValueFilter filter, const void* context, std::uint32_t* value) {
                        return ucpmap_getRange_nid_postfix(reinterpret_cast<const UCPMap*>(trie), start, o, surrogate, filter, context, value);
                    }, model, option, 0x5A & masks[widthIndex], useFilter, "ucpmap_getRange over a trie matches the model");
                }
            }
            for (std::int32_t c = 0; c < CodePoints; c += 7) Expect(ucpmap_get_nid_postfix(reinterpret_cast<const UCPMap*>(trie), c) == model[c], "ucpmap_get over a trie");
            ExpectInternalIndexes(trie, model);
            ExpectBinary(trie, model, type, widths[widthIndex]);
            UMutableCPTrie* fromTrie = umutablecptrie_fromUCPTrie_nid_postfix(trie, &status);
            Expect(fromTrie != nullptr && status == Success, "fromUCPTrie");
            ExpectMutable(fromTrie, model, "fromUCPTrie copies every value", ucptrie_get_nid_postfix(trie, -1));
            UMutableCPTrie* fromMap = umutablecptrie_fromUCPMap_nid_postfix(reinterpret_cast<const UCPMap*>(trie), &status);
            Expect(fromMap != nullptr && status == Success, "fromUCPMap");
            ExpectMutable(fromMap, model, "fromUCPMap copies every value", ucptrie_get_nid_postfix(trie, -1));
            umutablecptrie_close_nid_postfix(fromMap);
            umutablecptrie_close_nid_postfix(fromTrie);
            umutablecptrie_close_nid_postfix(clone);
            umutablecptrie_close_nid_postfix(mutableTrie);
            ucptrie_close_nid_postfix(trie);
        }
    }
}

void PropertyMap() {
    std::int32_t status = Success;
    const UCPMap* map = u_getIntPropertyMap_nid_postfix(0x1005, &status);
    Expect(map != nullptr && status == Success, "u_getIntPropertyMap(gc)");
    Model model(CodePoints);
    for (std::int32_t c = 0; c < CodePoints; ++c) {
        model[c] = static_cast<std::uint32_t>(u_charType_nid_postfix(c));
        Expect(ucpmap_get_nid_postfix(map, c) == model[c], "ucpmap_get on the general category map is u_charType");
    }
    ExpectRanges([&](std::int32_t start, std::int32_t option, std::uint32_t surrogate, Icu::CodePointValueFilter filter, const void* context, std::uint32_t* value) {
        return ucpmap_getRange_nid_postfix(map, start, option, surrogate, filter, context, value);
    }, model, RangeNormal, 0, true, "ucpmap_getRange on the general category map");
    UMutableCPTrie* copy = umutablecptrie_fromUCPMap_nid_postfix(map, &status);
    Expect(copy != nullptr && status == Success && umutablecptrie_get_nid_postfix(copy, 0x41) == 1 && umutablecptrie_get_nid_postfix(copy, 0x301) == 6, "a property map copies into a mutable trie");
    umutablecptrie_close_nid_postfix(copy);
}

void Refusals() {
    std::int32_t status = Success;
    UMutableCPTrie* trie = umutablecptrie_open_nid_postfix(1, 2, &status);
    umutablecptrie_set_nid_postfix(trie, CodePoints, 5, &status);
    Expect(status == IllegalArgument, "set beyond U+10FFFF");
    status = Success;
    umutablecptrie_setRange_nid_postfix(trie, 20, 10, 5, &status);
    Expect(status == IllegalArgument, "setRange with start after end");
    status = Success;
    umutablecptrie_setRange_nid_postfix(trie, -1, 10, 5, &status);
    Expect(status == IllegalArgument && umutablecptrie_get_nid_postfix(trie, 5) == 1, "setRange from a negative code point");
    status = Success;
    Expect(umutablecptrie_buildImmutable_nid_postfix(trie, 7, Width16, &status) == nullptr && status == IllegalArgument, "an unknown trie type");
    status = Success;
    Expect(umutablecptrie_buildImmutable_nid_postfix(trie, TypeFast, 9, &status) == nullptr && status == IllegalArgument, "an unknown value width");
    status = IllegalArgument;
    umutablecptrie_set_nid_postfix(trie, 5, 9, &status);
    umutablecptrie_setRange_nid_postfix(trie, 5, 6, 9, &status);
    Expect(umutablecptrie_get_nid_postfix(trie, 5) == 1, "an error set stops set and setRange");
    Expect(umutablecptrie_open_nid_postfix(1, 2, &status) == nullptr && umutablecptrie_clone_nid_postfix(trie, &status) == nullptr, "an error set opens and clones nothing");
    Expect(umutablecptrie_buildImmutable_nid_postfix(trie, TypeFast, Width16, &status) == nullptr && status == IllegalArgument, "an error set builds nothing");
    umutablecptrie_set_nid_postfix(nullptr, 5, 9, &status);
    umutablecptrie_setRange_nid_postfix(nullptr, 5, 6, 9, &status);
    Expect(umutablecptrie_buildImmutable_nid_postfix(nullptr, TypeFast, Width16, &status) == nullptr, "an error set does not read a null trie");
    Expect(ucptrie_toBinary_nid_postfix(nullptr, nullptr, 0, &status) == 0, "an error set does not read a null trie for toBinary");
    Expect(ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, nullptr, 64, nullptr, &status) == nullptr && status == IllegalArgument, "an error set does not read a null buffer");
    status = Warning;
    umutablecptrie_set_nid_postfix(trie, 5, 9, &status);
    Expect(umutablecptrie_get_nid_postfix(trie, 5) == 9 && status == Warning, "a warning does not stop set");
    status = Success;
    Expect(umutablecptrie_clone_nid_postfix(nullptr, &status) == nullptr && status == Success, "a null clone source gives null without an error");
    Expect(umutablecptrie_fromUCPMap_nid_postfix(nullptr, &status) == nullptr && status == IllegalArgument, "a null map is an illegal argument");
    status = Success;
    Expect(umutablecptrie_fromUCPTrie_nid_postfix(nullptr, &status) == nullptr && status == IllegalArgument, "a null trie is an illegal argument");
    status = Success;
    UCPTrie* built = umutablecptrie_buildImmutable_nid_postfix(trie, TypeSmall, Width8, &status);
    Expect(built != nullptr && ucptrie_get_nid_postfix(built, 5) == 9, "an 8-bit build");
    std::uint32_t buffer[4] = {};
    Expect(ucptrie_toBinary_nid_postfix(built, buffer, -1, &status) == 0 && status == IllegalArgument, "a negative capacity");

    Expect(Throws([&] { umutablecptrie_open_nid_postfix(1, 2, nullptr); }), "open with a null status");
    Expect(Throws([&] { umutablecptrie_set_nid_postfix(trie, 1, 1, nullptr); }), "set with a null status");
    status = Success;
    Expect(Throws([&] { umutablecptrie_set_nid_postfix(nullptr, 1, 1, &status); }), "set on a null trie");
    Expect(Throws([&] { umutablecptrie_setRange_nid_postfix(nullptr, 1, 2, 1, &status); }), "setRange on a null trie");
    Expect(Throws([&] { umutablecptrie_buildImmutable_nid_postfix(nullptr, TypeFast, Width16, &status); }), "build from a null trie");
    Expect(Throws([&] { umutablecptrie_get_nid_postfix(nullptr, 1); }), "get from a null mutable trie");
    Expect(Throws([&] { umutablecptrie_getRange_nid_postfix(nullptr, 0, RangeNormal, 0, nullptr, nullptr, nullptr); }), "getRange of a null mutable trie");
    Expect(Throws([&] { ucptrie_get_nid_postfix(nullptr, 1); }), "get from a null trie");
    Expect(Throws([&] { ucptrie_getType_nid_postfix(nullptr); }) && Throws([&] { ucptrie_getValueWidth_nid_postfix(nullptr); }), "type and width of a null trie");
    Expect(Throws([&] { ucptrie_getRange_nid_postfix(nullptr, 0, RangeNormal, 0, nullptr, nullptr, nullptr); }), "getRange of a null trie");
    Expect(Throws([&] { ucptrie_toBinary_nid_postfix(nullptr, buffer, 16, &status); }), "toBinary of a null trie");
    Expect(Throws([&] { ucptrie_toBinary_nid_postfix(built, buffer, 16, nullptr); }), "toBinary with a null status");
    Expect(Throws([&] { ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, nullptr, 64, nullptr, &status); }), "openFromBinary of a null buffer");
    Expect(Throws([&] { ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, buffer, 16, nullptr, nullptr); }), "openFromBinary with a null status");
    Expect(Throws([&] { ucptrie_internalSmallIndex_nid_postfix(nullptr, 0x20000); }), "internalSmallIndex of a null trie");
    Expect(Throws([&] { ucptrie_internalSmallU8Index_nid_postfix(nullptr, 1, 0, 0); }), "internalSmallU8Index of a null trie");
    Expect(Throws([&] { ucptrie_internalU8PrevIndex_nid_postfix(nullptr, 0x80, nullptr, nullptr); }), "internalU8PrevIndex of a null trie");
    Expect(Throws([&] { ucpmap_get_nid_postfix(nullptr, 1); }), "get from a null map");
    Expect(Throws([&] { ucpmap_getRange_nid_postfix(nullptr, 0, RangeNormal, 0, nullptr, nullptr, nullptr); }), "getRange of a null map");
    std::uint32_t value = 0;
    Expect(ucptrie_getRange_nid_postfix(built, -1, RangeNormal, 0, nullptr, nullptr, &value) < 0 && ucptrie_getRange_nid_postfix(built, 0, RangeNormal, 0, nullptr, nullptr, nullptr) >= 0, "getRange refuses a negative start and accepts a null value slot");
    ucptrie_close_nid_postfix(built);
    ucptrie_close_nid_postfix(nullptr);
    umutablecptrie_close_nid_postfix(trie);
    umutablecptrie_close_nid_postfix(nullptr);
}

}

int main() {
    Builds();
    PropertyMap();
    Refusals();
    std::puts("ICU code point trie tests passed");
}
