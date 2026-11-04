#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

constexpr int invalidNumber = 187;
constexpr int invalidSeparator = 131;
constexpr int invalidDigit = 130;
constexpr int missingSecondNumber = 138;
constexpr int firstNumberTooLarge = 122;
constexpr int secondNumberTooLarge = 147;
constexpr int tooLong = 155;
constexpr int allocationFailure = 65;

int readDecimalArc(const char*& cursor, std::uint64_t& value, int characterError) {
    const char* begin = cursor;
    value = 0;
    while (*cursor != '\0' && *cursor != '.' && *cursor != ' ') {
        const char character = *cursor;
        if (character < '0' || character > '9') {
            return characterError;
        }
        if (cursor != begin && value == 0 && character == '0') {
            return invalidNumber;
        }
        if (value > std::numeric_limits<std::uint64_t>::max() / 10) {
            return tooLong;
        }
        value = value * 10 + static_cast<std::uint64_t>(character - '0');
        ++cursor;
    }
    return cursor == begin ? invalidNumber : 0;
}

int appendArc(std::vector<unsigned char>& output, std::uint64_t value) {
    unsigned char digits[10];
    std::size_t count = 0;
    do {
        digits[count++] = static_cast<unsigned char>(value & 0x7f);
        value >>= 7;
    } while (value != 0);
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max()) - output.size()) {
        return tooLong;
    }
    while (count != 0) {
        --count;
        output.push_back(static_cast<unsigned char>(digits[count] | (count == 0 ? 0 : 0x80)));
    }
    return 0;
}

int parseNumericObject(const char* text, std::vector<unsigned char>& output) {
    const char* cursor = text;
    std::uint64_t first = 0;
    int error = readDecimalArc(cursor, first, invalidSeparator);
    if (error != 0) {
        return error;
    }
    if (*cursor == '\0') {
        return missingSecondNumber;
    }
    const char separator = *cursor++;
    std::uint64_t second = 0;
    error = readDecimalArc(cursor, second, invalidDigit);
    if (error != 0) {
        return error;
    }
    if (first > 2) {
        return firstNumberTooLarge;
    }
    const std::uint64_t secondLimit = first == 2 ? std::numeric_limits<std::uint64_t>::max() - 80 : 39;
    if (second > secondLimit) {
        return secondNumberTooLarge;
    }
    error = appendArc(output, first * 40 + second);
    if (error != 0) {
        return error;
    }
    while (*cursor != '\0') {
        if (*cursor != separator) {
            return invalidSeparator;
        }
        ++cursor;
        std::uint64_t arc = 0;
        error = readDecimalArc(cursor, arc, invalidDigit);
        if (error != 0) {
            return error;
        }
        error = appendArc(output, arc);
        if (error != 0) {
            return error;
        }
    }
    return 0;
}

LibreSslAsn1Object* rejectNumericObject(int reason) {
    LibreSslPutError_nid_no_patch(13, 0xfff, reason);
    return nullptr;
}

}

extern "C" LibreSslAsn1Object* APS5_VABI OBJ_txt2obj_nid_postfix(const char* text, int noName) {
    if (text == nullptr) {
        throw std::invalid_argument("OBJ_txt2obj requires a non-null text pointer");
    }
    if (noName == 0 && (text[0] < '0' || text[0] > '9')) {
        NotImplemented_nid_no_patch(__func__);
    }
    try {
        std::vector<unsigned char> encoded;
        const int error = parseNumericObject(text, encoded);
        if (error != 0) {
            return rejectNumericObject(error);
        }
        auto* data = static_cast<unsigned char*>(malloc_nid_postfix(encoded.size()));
        if (data == nullptr) {
            return nullptr;
        }
        auto* object = static_cast<LibreSslAsn1Object*>(calloc_nid_postfix(1, sizeof(LibreSslAsn1Object)));
        if (object == nullptr) {
            free_nid_postfix(data);
            return rejectNumericObject(allocationFailure);
        }
        std::memcpy(data, encoded.data(), encoded.size());
        object->shortName = nullptr;
        object->longName = nullptr;
        object->nid = 0;
        object->length = static_cast<int>(encoded.size());
        object->data = data;
        object->flags = 9;
        return object;
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}
