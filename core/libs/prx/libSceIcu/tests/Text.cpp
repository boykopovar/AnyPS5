#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Text = Icu::Text;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU text check failed at line %u\n", location.line());
    std::exit(1);
}

template<class TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

struct Provider {
    std::u16string contents{char16_t{0xdc00}, u'A', char16_t{0xd83d}, char16_t{0xde00}, u'B',
        char16_t{0xd800}, u'C', char16_t{0xdc00}, char16_t{0xd800}};
    int chunkSize = 1;
    int scale = 1;
    std::vector<std::pair<std::int64_t, int>> accesses;
    std::vector<std::int32_t> mappedOffsets;
    std::vector<std::int64_t> mappedIndexes;
    std::array<std::int64_t, 6> arguments{};
    const void* buffer = nullptr;
    std::int32_t* statusPointer = nullptr;
    const Text* source = nullptr;
    int calls = 0;
    int cloneMode = 0;
};

Provider& Context(const Text* text) {
    return *static_cast<Provider*>(const_cast<void*>(text->context));
}

std::int8_t APS5_VABI Access_nid_no_patch(Text* text, std::int64_t index, std::int8_t forward) {
    auto& provider = Context(text);
    provider.accesses.emplace_back(index, forward);
    const auto length = static_cast<std::int64_t>(provider.contents.size()) * provider.scale;
    const auto pinned = std::clamp<std::int64_t>(index, 0, length);
    const auto unit = static_cast<int>(pinned / provider.scale);
    const auto preceding = !forward && unit > 0 ? unit - 1 : std::min<int>(unit, provider.contents.size() - 1);
    const auto first = (preceding / provider.chunkSize) * provider.chunkSize;
    const auto last = std::min<int>(first + provider.chunkSize, provider.contents.size());
    text->chunkNativeStart = first * provider.scale;
    text->chunkNativeLimit = last * provider.scale;
    text->chunkLength = last - first;
    text->chunkOffset = unit - first;
    text->chunkContents = provider.contents.data() + first;
    text->nativeIndexingLimit = provider.scale == 1 ? text->chunkLength : 0;
    return (forward ? unit < static_cast<int>(provider.contents.size()) : unit > 0) ? -7 : 0;
}

std::int64_t APS5_VABI NativeLength_nid_no_patch(Text* text) {
    auto& provider = Context(text);
    ++provider.calls;
    return provider.contents.size() * provider.scale;
}

std::int64_t APS5_VABI MapOffset_nid_no_patch(const Text* text) {
    auto& provider = Context(text);
    provider.mappedOffsets.push_back(text->chunkOffset);
    return text->chunkNativeStart + text->chunkOffset * provider.scale;
}

std::int32_t APS5_VABI MapIndex_nid_no_patch(const Text* text, std::int64_t index) {
    auto& provider = Context(text);
    provider.mappedIndexes.push_back(index);
    return (index - text->chunkNativeStart) / provider.scale;
}

std::int32_t APS5_VABI Extract_nid_no_patch(Text* text, std::int64_t start, std::int64_t limit,
    char16_t* destination, std::int32_t capacity, std::int32_t* status) {
    auto& provider = Context(text);
    ++provider.calls;
    provider.arguments = {start, limit, capacity, status ? *status : 0, 0, 0};
    provider.buffer = destination;
    provider.statusPointer = status;
    return 41;
}

std::int32_t APS5_VABI Replace_nid_no_patch(Text* text, std::int64_t start, std::int64_t limit,
    const char16_t* replacement, std::int32_t length, std::int32_t* status) {
    auto& provider = Context(text);
    ++provider.calls;
    provider.arguments = {start, limit, length, *status, 0, 0};
    provider.buffer = replacement;
    provider.statusPointer = status;
    *status = 5;
    return -42;
}

void APS5_VABI Copy_nid_no_patch(Text* text, std::int64_t start, std::int64_t limit,
    std::int64_t destination, std::int8_t move, std::int32_t* status) {
    auto& provider = Context(text);
    ++provider.calls;
    provider.arguments = {start, limit, destination, move, *status, 0};
    provider.statusPointer = status;
    *status = 6;
}

Text* APS5_VABI Clone_nid_no_patch(Text* destination, const Text* source, std::int8_t deep, std::int32_t* status) {
    auto& provider = Context(source);
    ++provider.calls;
    provider.source = source;
    provider.arguments = {deep, *status, 0, 0, 0, 0};
    provider.statusPointer = status;
    if (provider.cloneMode == 1) return nullptr;
    if (provider.cloneMode == 2) {
        *status = 4;
        return destination;
    }
    *destination = *source;
    return destination;
}

const Icu::TextFunctions Functions{sizeof(Icu::TextFunctions), 0, 0, 0, Clone_nid_no_patch, NativeLength_nid_no_patch,
    Access_nid_no_patch, Extract_nid_no_patch, Replace_nid_no_patch, Copy_nid_no_patch, MapOffset_nid_no_patch,
    MapIndex_nid_no_patch, nullptr, nullptr, nullptr, nullptr};

Text Initial(Provider& provider) {
    Text text{};
    text.magic = 0x345ad82c;
    text.sizeOfStruct = sizeof(Text);
    text.context = &provider;
    text.functions = &Functions;
    return text;
}

void Iteration() {
    const std::array<int, 8> codePoints{0xdc00, 0x41, 0x1f600, 0x42, 0xd800, 0x43, 0xdc00, 0xd800};
    const std::array<int, 9> boundaries{0, 1, 2, 4, 5, 6, 7, 8, 9};
    for (int scale : {1, 2}) {
        for (int chunkSize : {1, 2, 3, 9}) {
            Provider provider;
            provider.chunkSize = chunkSize;
            provider.scale = scale;
            auto text = Initial(provider);
            Require(utext_nativeLength_nid_postfix(&text) == 9 * scale && provider.calls == 1);
            Require(utext_getNativeIndex_nid_postfix(&text) == 0);
            Require(utext_getPreviousNativeIndex_nid_postfix(&text) == 0);
            for (int position = 0; position < 8; ++position) {
                Require(utext_current32_nid_postfix(&text) == codePoints[position]);
                Require(utext_getNativeIndex_nid_postfix(&text) == boundaries[position] * scale);
                Require(utext_next32_nid_postfix(&text) == codePoints[position]);
                Require(utext_getNativeIndex_nid_postfix(&text) == boundaries[position + 1] * scale);
                Require(utext_getPreviousNativeIndex_nid_postfix(&text) == boundaries[position] * scale);
                Require(utext_getNativeIndex_nid_postfix(&text) == boundaries[position + 1] * scale);
            }
            Require(utext_current32_nid_postfix(&text) == -1);
            Require(utext_next32_nid_postfix(&text) == -1);
            for (int position = 7; position >= 0; --position) {
                Require(utext_previous32_nid_postfix(&text) == codePoints[position]);
                Require(utext_getNativeIndex_nid_postfix(&text) == boundaries[position] * scale);
            }
            Require(utext_previous32_nid_postfix(&text) == -1);
            for (int unit = 0; unit < 9; ++unit) {
                const auto found = std::upper_bound(boundaries.begin(), boundaries.end(), unit) - boundaries.begin() - 1;
                for (int interior = 0; interior < scale; ++interior) {
                    const auto native = unit * scale + interior;
                    const auto at = utext_char32At_nid_postfix(&text, native);
                    const auto expectedAt = unit == 7 && unit % chunkSize == 0 ? -1 : codePoints[found];
                    Require(at == expectedAt);
                    Require(utext_getNativeIndex_nid_postfix(&text) == boundaries[found] * scale);
                    Require(utext_next32From_nid_postfix(&text, native) == codePoints[found]);
                    Require(utext_getNativeIndex_nid_postfix(&text) == boundaries[found + 1] * scale);
                    const auto previous = found == 0 ? -1 : codePoints[found - 1];
                    Require(utext_previous32From_nid_postfix(&text, native) == previous);
                    Require(utext_getNativeIndex_nid_postfix(&text) == (found == 0 ? 0 : boundaries[found - 1] * scale));
                }
            }
            utext_setNativeIndex_nid_postfix(&text, 0);
            Require(utext_moveIndex32_nid_postfix(&text, 3) == 1 && utext_getNativeIndex_nid_postfix(&text) == 4 * scale);
            Require(utext_moveIndex32_nid_postfix(&text, -2) == 1 && utext_getNativeIndex_nid_postfix(&text) == scale);
            Require(utext_moveIndex32_nid_postfix(&text, 100) == 0 && utext_getNativeIndex_nid_postfix(&text) == 9 * scale);
            Require(utext_moveIndex32_nid_postfix(&text, -100) == 0 && utext_getNativeIndex_nid_postfix(&text) == 0);
            Require(utext_char32At_nid_postfix(&text, -1) == -1);
            Require(utext_next32From_nid_postfix(&text, -1) == 0xdc00);
            Require(utext_char32At_nid_postfix(&text, 100) == -1);
            Require(utext_previous32From_nid_postfix(&text, 100) == 0xd800);
        }
    }
    Provider provider;
    auto text = Initial(provider);
    utext_setNativeIndex_nid_postfix(&text, 2);
    provider.accesses.clear();
    Require(utext_current32_nid_postfix(&text) == 0x1f600);
    Require(provider.accesses == std::vector<std::pair<std::int64_t, int>>({{3, 1}, {3, 0}}));
    Require(utext_getNativeIndex_nid_postfix(&text) == 2);
    utext_setNativeIndex_nid_postfix(&text, 8);
    provider.accesses.clear();
    Require(utext_current32_nid_postfix(&text) == 0xd800);
    Require(provider.accesses == std::vector<std::pair<std::int64_t, int>>({{9, 1}, {9, 0}}));
    Require(utext_getNativeIndex_nid_postfix(&text) == 8);
}

void Dispatch() {
    Provider provider;
    auto text = Initial(provider);
    for (int properties = 0; properties < 256; ++properties) {
        text.providerProperties = properties;
        Require(utext_isLengthExpensive_nid_postfix(&text) == ((properties & 2) != 0));
        Require(utext_isWritable_nid_postfix(&text) == ((properties & 8) != 0));
        Require(utext_hasMetaData_nid_postfix(&text) == ((properties & 16) != 0));
        utext_freeze_nid_postfix(&text);
        Require(text.providerProperties == (properties & ~8));
    }
    text.providerProperties = 31;
    auto other = text;
    Require(utext_equals_nid_postfix(&text, &other));
    other.magic = 0;
    Require(!utext_equals_nid_postfix(&text, &other));
    other = text;
    other.context = nullptr;
    Require(!utext_equals_nid_postfix(&text, &other));
    other = text;
    other.functions = nullptr;
    Require(!utext_equals_nid_postfix(&text, &other));
    other = text;
    other.chunkOffset = 1;
    other.nativeIndexingLimit = 1;
    Require(!utext_equals_nid_postfix(&text, &other));
    Require(!utext_equals_nid_postfix(nullptr, nullptr));
    std::int32_t status = 7;
    Require(utext_replace_nid_postfix(nullptr, 0, 1, nullptr, -1, &status) == 0 && status == 7);
    utext_copy_nid_postfix(nullptr, 0, 1, 0, 1, &status);
    Require(utext_clone_nid_postfix(&other, nullptr, 0, 0, &status) == &other && status == 7);
    text.providerProperties = 0;
    status = -128;
    Require(utext_replace_nid_postfix(&text, -1, -2, nullptr, -1, &status) == 0 && status == 30);
    status = -128;
    utext_copy_nid_postfix(&text, -1, -2, -3, -7, &status);
    Require(status == 30 && provider.calls == 0);
    char16_t buffer[2]{};
    status = 7;
    Require(utext_extract_nid_postfix(&text, -1, 10000000000, buffer, -3, &status) == 41);
    Require(provider.calls == 1 && provider.arguments == std::array<std::int64_t, 6>{-1, 10000000000, -3, 7, 0, 0});
    Require(provider.buffer == buffer && provider.statusPointer == &status);
    Require(utext_extract_nid_postfix(&text, 0, 0, nullptr, 0, nullptr) == 41 && provider.statusPointer == nullptr);
    text.providerProperties = 31;
    status = -128;
    Require(utext_replace_nid_postfix(&text, -5, 10000000000, buffer, -7, &status) == -42 && status == 5);
    Require(provider.arguments == std::array<std::int64_t, 6>{-5, 10000000000, -7, -128, 0, 0});
    status = -128;
    utext_copy_nid_postfix(&text, -5, 10000000000, -8, -7, &status);
    Require(status == 6 && provider.arguments == std::array<std::int64_t, 6>{-5, 10000000000, -8, -7, -128, 0});
    status = -128;
    Require(utext_clone_nid_postfix(&other, &text, -7, -8, &status) == &other && status == -128);
    Require(provider.source == &text && provider.arguments[0] == -7 && provider.arguments[1] == -128);
    Require(other.providerProperties == 23 && text.providerProperties == 31);
    status = 0;
    Require(utext_clone_nid_postfix(&other, &text, 0, 0, &status) == &other && other.providerProperties == 31);
    provider.cloneMode = 1;
    Require(!utext_clone_nid_postfix(&other, &text, 0, 0, &status) && status == 7);
    provider.cloneMode = 2;
    status = 0;
    Require(utext_clone_nid_postfix(&other, &text, 0, 1, &status) == &other && status == 4 && other.providerProperties == 31);
}

void Errors() {
    Require(utext_moveIndex32_nid_postfix(nullptr, 0) == 1);
    Invalid([] { utext_current32_nid_postfix(nullptr); });
    Invalid([] { utext_isWritable_nid_postfix(nullptr); });
    Invalid([] { utext_copy_nid_postfix(nullptr, 0, 0, 0, 0, nullptr); });
    Text text{};
    Invalid([&] { utext_next32_nid_postfix(&text); });
    Invalid([&] { utext_nativeLength_nid_postfix(&text); });
    Invalid([&] { utext_extract_nid_postfix(&text, 0, 0, nullptr, 0, nullptr); });
    text.chunkNativeStart = std::numeric_limits<std::int64_t>::max();
    text.chunkOffset = 1;
    text.nativeIndexingLimit = 1;
    Invalid([&] { utext_getNativeIndex_nid_postfix(&text); });
    text.chunkOffset = std::numeric_limits<std::int32_t>::min();
    Invalid([&] { utext_getPreviousNativeIndex_nid_postfix(&text); });
}

}

int main() {
    Iteration();
    Dispatch();
    Errors();
    std::puts("ICU text tests passed");
}
