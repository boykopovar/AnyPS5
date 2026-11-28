#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::ExceptionData;
using NKWebKit::WtfAbi::String;

using IsolatedCopy = ExceptionData* (APS5_VABI *)(void*, const ExceptionData*);

const auto isolatedCopy = std::bit_cast<IsolatedCopy>(&_ZNK7WebCore13ExceptionData12isolatedCopyEv);

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ExceptionData line %u: %s\n", location.line(), message);
        std::abort();
    }
}

String Text(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), std::strlen(text));
}

String Wide(const std::u16string& text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), text.size());
}

std::u16string Units(const String& text) {
    std::u16string result;
    if (!text.impl) return result;
    for (unsigned i = 0; i < text.impl->length; ++i) {
        if (text.impl->hashAndFlags & 4) result.push_back(static_cast<const unsigned char*>(text.impl->data)[i]);
        else result.push_back(static_cast<const char16_t*>(text.impl->data)[i]);
    }
    return result;
}

void Check(std::uint32_t code, const String& message) {
    ExceptionData source { code, String(message) };
    const auto references = message.impl ? message.impl->refCount : 0;
    struct {
        std::array<unsigned char, 16> before;
        alignas(ExceptionData) std::array<unsigned char, sizeof(ExceptionData)> storage;
        std::array<unsigned char, 16> after;
    } guarded;
    guarded.before.fill(0xA5);
    guarded.after.fill(0x5A);
    guarded.storage.fill(0xCC);
    auto* copy = isolatedCopy(guarded.storage.data(), &source);
    Require(static_cast<void*>(copy) == guarded.storage.data(), "result is written through the hidden pointer");
    Require(std::all_of(guarded.before.begin(), guarded.before.end(), [](auto x) { return x == 0xA5; }), "leading canary");
    Require(std::all_of(guarded.after.begin(), guarded.after.end(), [](auto x) { return x == 0x5A; }), "trailing canary");
    Require(copy->code == code, "code is copied");
    Require(source.code == code && source.message.impl == message.impl, "source is left as it was");
    Require(!copy->message.impl == !message.impl, "null message stays null");
    Require(Units(copy->message) == Units(message), "message keeps its text");
    const bool hasText = message.impl && message.impl->length;
    if (hasText) {
        Require(message.impl->refCount == references, "source string is not retained");
        Require(copy->message.impl != message.impl, "message is a new string");
        Require(copy->message.impl->refCount == 2, "new string has one owner");
        Require((copy->message.impl->hashAndFlags & 4) == (message.impl->hashAndFlags & 4), "message keeps its width");
    }
    std::destroy_at(copy);
    Require(!hasText || message.impl->refCount == references, "destroying the copy leaves the source string");
}

}

int main() {
    try {
        Check(0, String());
        Check(8, Text("The object can not be found here."));
        Check(18, Wide(u"s\u00E9curit\u00E9 \u20AC \U0001F600"));
        Check(24, Text(""));
        Check(31, Text(std::string(5000, 'x').c_str()));
        Check(0xFFFFFFFFu, Text("out of range code"));
        bool threw = false;
        try {
            std::array<unsigned char, sizeof(ExceptionData)> storage {};
            isolatedCopy(storage.data(), nullptr);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw, "null object throws");
        std::puts("ExceptionData: isolatedCopy code, message, ownership and hidden-return checks passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ExceptionData unexpected exception: %s\n", error.what());
        return 1;
    }
}
