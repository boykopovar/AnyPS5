#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>



namespace {

std::unique_ptr<GuestJSString> Decode(const char* text) {
    auto result = std::make_unique<GuestJSString>();
    const auto* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor) {
        std::uint32_t codepoint = *cursor++;
        unsigned following = 0;
        std::uint32_t minimum = 0;
        if (codepoint >= 0xC2 && codepoint <= 0xDF) {
            codepoint &= 0x1F;
            following = 1;
            minimum = 0x80;
        } else if (codepoint >= 0xE0 && codepoint <= 0xEF) {
            codepoint &= 0x0F;
            following = 2;
            minimum = 0x800;
        } else if (codepoint >= 0xF0 && codepoint <= 0xF4) {
            codepoint &= 7;
            following = 3;
            minimum = 0x10000;
        } else if (codepoint >= 0x80) {
            result->nullString = true;
            break;
        }
        while (following > 0) {
            --following;
            if ((*cursor & 0xC0) != 0x80) {
                result->nullString = true;
                break;
            }
            codepoint = codepoint << 6 | (*cursor++ & 0x3F);
        }
        if (result->nullString || codepoint < minimum || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            result->nullString = true;
            break;
        }
        if (codepoint > 0xFFFF) {
            const auto supplementary = codepoint - 0x10000;
            result->characters.push_back(static_cast<std::uint16_t>(0xD800 + (supplementary >> 10)));
            result->characters.push_back(static_cast<std::uint16_t>(0xDC00 + (supplementary & 0x3FF)));
        } else result->characters.push_back(static_cast<std::uint16_t>(codepoint));
    }
    if (result->nullString) result->characters.clear();
    result->characters.push_back(0);
    return result;
}

bool Equal(const GuestJSString* first, const GuestJSString* second) {
    if (first == second) return true;
    if (!first || !second) return false;
    return first->nullString == second->nullString && first->characters == second->characters;
}

}

extern "C" {

GuestJSString* APS5_VABI JSStringCreateWithCharacters(const std::uint16_t* characters, std::size_t count) {
    if (!characters && count) NotImplemented_nid_no_patch(__func__);
    if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    auto string = std::make_unique<GuestJSString>();
    string->nullString = !characters;
    if (count) string->characters.assign(characters, characters + count);
    string->characters.push_back(0);
    return string.release();
}

GuestJSString* APS5_VABI JSStringCreateWithUTF8CString(const char* text) {
    if (!text) NotImplemented_nid_no_patch(__func__);
    return Decode(text).release();
}

GuestJSString* APS5_VABI JSStringRetain(GuestJSString* string) {
    if (!string) NotImplemented_nid_no_patch(__func__);
    auto references = string->references.load(std::memory_order_relaxed);
    for (;;) {
        if (references == std::numeric_limits<std::size_t>::max()) NotImplemented_nid_no_patch(__func__);
        if (string->references.compare_exchange_weak(references, references + 1, std::memory_order_relaxed)) return string;
    }
}

void APS5_VABI JSStringRelease(GuestJSString* string) {
    if (!string) NotImplemented_nid_no_patch(__func__);
    if (string->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete string;
}

std::size_t APS5_VABI JSStringGetLength(const GuestJSString* string) {
    return string ? string->characters.size() - 1 : 0;
}

const std::uint16_t* APS5_VABI JSStringGetCharactersPtr(const GuestJSString* string) {
    return string && !string->nullString ? string->characters.data() : nullptr;
}

std::size_t APS5_VABI JSStringGetMaximumUTF8CStringSize(const GuestJSString* string) {
    if (!string) NotImplemented_nid_no_patch(__func__);
    const auto length = string->characters.size() - 1;
    if (length > (std::numeric_limits<std::size_t>::max() - 1) / 3) NotImplemented_nid_no_patch(__func__);
    return length * 3 + 1;
}

std::size_t APS5_VABI JSStringGetUTF8CString(const GuestJSString* string, char* buffer, std::size_t bufferSize) {
    if (!string || !buffer || !bufferSize) return 0;
    std::size_t written = 0;
    const auto count = string->characters.size() - 1;
    for (std::size_t index = 0; index < count; ++index) {
        std::uint32_t codepoint = string->characters[index];
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            if (index + 1 == count) break;
            const auto low = string->characters[index + 1];
            if (low < 0xDC00 || low > 0xDFFF) break;
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + low - 0xDC00;
            ++index;
        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) break;
        const std::size_t bytes = codepoint < 0x80 ? 1 : codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
        if (bytes > bufferSize - 1 - written) break;
        if (bytes == 1) buffer[written++] = static_cast<char>(codepoint);
        else {
            const unsigned prefix = bytes == 2 ? 0xC0 : bytes == 3 ? 0xE0 : 0xF0;
            buffer[written++] = static_cast<char>(prefix | (codepoint >> (6 * (bytes - 1))));
            for (auto remaining = bytes - 1; remaining > 0; --remaining) {
                buffer[written++] = static_cast<char>(0x80 | ((codepoint >> (6 * (remaining - 1))) & 0x3F));
            }
        }
    }
    buffer[written] = '\0';
    return written + 1;
}

bool APS5_VABI JSStringIsEqual(const GuestJSString* first, const GuestJSString* second) {
    return Equal(first, second);
}

bool APS5_VABI JSStringIsEqualToUTF8CString(const GuestJSString* string, const char* text) {
    if (!text) NotImplemented_nid_no_patch(__func__);
    const auto other = Decode(text);
    return Equal(string, other.get());
}

}
