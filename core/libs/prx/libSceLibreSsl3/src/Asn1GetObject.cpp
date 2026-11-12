#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int ConstructedBit = 0x20;
constexpr int ClassMask = 0xc0;
constexpr int PrimitiveTag = 0x1f;
constexpr int ErrorResult = 0x80;

bool Consume(std::uint64_t& remaining) {
    if (remaining == 1) return false;
    --remaining;
    return true;
}

bool ReadLength(const unsigned char*& cursor, std::uint64_t maximum, bool& indefinite, std::int64_t& length) {
    if (maximum < 1) return false;
    --maximum;
    const unsigned char* position = cursor;
    std::uint64_t value = 0;
    if (*position == 0x80) {
        indefinite = true;
        ++position;
    } else {
        indefinite = false;
        unsigned int count = *position & 0x7f;
        if ((*position++ & 0x80) != 0) {
            if (maximum < static_cast<std::uint64_t>(count)) return false;
            while (count != 0 && *position == 0) {
                ++position;
                --count;
            }
            if (count > sizeof(value)) return false;
            while (count != 0) {
                --count;
                value = (value << 8) | *position++;
            }
        } else {
            value = count;
        }
    }
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
    cursor = position;
    length = static_cast<std::int64_t>(value);
    return true;
}

}

extern "C" {

int APS5_VABI ASN1_get_object_nid_postfix(const unsigned char** pointer, std::int64_t* length,
                                          int* tag, int* objectClass, std::int64_t maximum) {
    if (!pointer || !length || !tag || !objectClass)
        throw std::invalid_argument(std::string(__func__) + ": null argument");
    const unsigned char* start = *pointer;
    if (!start) throw std::invalid_argument(std::string(__func__) + ": null pointer");
    if (maximum <= 0) return ErrorResult;

    std::uint64_t remaining = static_cast<std::uint64_t>(maximum);
    int result = *start & ConstructedBit;
    const int parsedClass = *start & ClassMask;
    const int identifier = *start & PrimitiveTag;
    const unsigned char* cursor = start;
    int parsedTag = identifier;
    if (identifier == PrimitiveTag) {
        ++cursor;
        if (!Consume(remaining)) return ErrorResult;
        std::int64_t value = 0;
        while (*cursor & 0x80) {
            value = (value << 7) | (*cursor++ & 0x7f);
            if (!Consume(remaining)) return ErrorResult;
            if (value > (std::numeric_limits<int>::max() >> 7)) return ErrorResult;
        }
        value = (value << 7) | (*cursor++ & 0x7f);
        parsedTag = static_cast<int>(value);
        if (!Consume(remaining)) return ErrorResult;
    } else {
        ++cursor;
        if (!Consume(remaining)) return ErrorResult;
    }
    *tag = parsedTag;
    *objectClass = parsedClass;
    bool indefinite = false;
    if (!ReadLength(cursor, remaining, indefinite, *length)) return ErrorResult;
    if (indefinite && (result & ConstructedBit) == 0) return ErrorResult;
    const auto headerLength = static_cast<std::int64_t>(cursor - start);
    if (headerLength > maximum || *length > maximum - headerLength) result |= ErrorResult;
    *pointer = cursor;
    return result | (indefinite ? 1 : 0);
}

}
