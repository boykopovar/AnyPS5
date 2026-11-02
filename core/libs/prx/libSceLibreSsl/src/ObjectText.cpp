#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

static_assert(sizeof(LibreSslAsn1Object) == 40);
static_assert(offsetof(LibreSslAsn1Object, longName) == 8);
static_assert(offsetof(LibreSslAsn1Object, nid) == 16);
static_assert(offsetof(LibreSslAsn1Object, length) == 20);
static_assert(offsetof(LibreSslAsn1Object, data) == 24);
static_assert(offsetof(LibreSslAsn1Object, flags) == 32);

namespace {

constexpr int ObjectLibrary = 8;
constexpr int BigNumberLibrary = 3;
constexpr int AllocationFailure = 65;
constexpr int AnyFunction = 0xfff;
constexpr int UnknownNid = 101;

class BigNumber {
public:
    explicit BigNumber(std::uint64_t value) : limbs{static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32)} {}

    void ShiftLeft7() {
        std::uint32_t carry = 0;
        for (auto& limb : limbs) {
            const std::uint32_t next = limb >> 25;
            limb = (limb << 7) | carry;
            carry = next;
        }
        if (carry) limbs.push_back(carry);
    }

    void Add(std::uint32_t value) {
        for (auto& limb : limbs) {
            const std::uint64_t sum = static_cast<std::uint64_t>(limb) + value;
            limb = static_cast<std::uint32_t>(sum);
            value = static_cast<std::uint32_t>(sum >> 32);
            if (!value) return;
        }
        limbs.push_back(value);
    }

    void Subtract(std::uint32_t value) {
        for (auto& limb : limbs) {
            const std::uint32_t before = limb;
            limb -= value;
            if (before >= value) return;
            value = 1;
        }
    }

    std::string Decimal() const {
        std::vector<std::uint32_t> number = limbs;
        std::string digits;
        while (std::any_of(number.begin(), number.end(), [](std::uint32_t limb) { return limb != 0; })) {
            std::uint64_t remainder = 0;
            for (auto limb = number.rbegin(); limb != number.rend(); ++limb) {
                const std::uint64_t current = (remainder << 32) | *limb;
                *limb = static_cast<std::uint32_t>(current / 10);
                remainder = current % 10;
            }
            digits.push_back(static_cast<char>('0' + remainder));
        }
        if (digits.empty()) digits = "0";
        std::reverse(digits.begin(), digits.end());
        return digits;
    }

private:
    std::vector<std::uint32_t> limbs;
};

int CompareObject(const LibreSslAsn1Object* key, const LibreSslAsn1Object* entry) {
    const int difference = key->length - entry->length;
    if (difference) return difference;
    return key->length > 0 ? std::memcmp(key->data, entry->data, static_cast<std::size_t>(key->length)) : 0;
}

const char* Name(int nid, bool longName) {
    if (nid < 0 || nid >= LibreSslObjectCount) return LibreSslAddedObjectName_nid_no_patch(nid, longName);
    const auto& object = LibreSslObjects_nid_no_patch[nid];
    if (nid != 0 && object.nid == 0) {
        LibreSslPutError_nid_no_patch(ObjectLibrary, AnyFunction, UnknownNid);
        return nullptr;
    }
    return longName ? object.longName : object.shortName;
}

void Write(char*& buffer, int& space, std::string_view text) {
    if (space > 0) {
        const std::size_t copied = std::min(text.size(), static_cast<std::size_t>(space - 1));
        std::memcpy(buffer, text.data(), copied);
        buffer[copied] = '\0';
    }
    const int written = static_cast<int>(text.size());
    if (written >= space) {
        space = 0;
    } else {
        buffer += written;
        space -= written;
    }
}

int NumericObjectText(char* buffer, int space, const LibreSslAsn1Object* object) {
    int result = 0;
    int remaining = object->length;
    const unsigned char* bytes = object->data;
    bool first = true;
    while (remaining > 0) {
        std::uint64_t value = 0;
        bool big = false;
        std::optional<BigNumber> number;
        for (;;) {
            const unsigned char byte = *bytes++;
            --remaining;
            if (remaining == 0 && (byte & 0x80)) return 0;
            if (big) number->Add(byte & 0x7f);
            else value |= byte & 0x7f;
            if (!(byte & 0x80)) break;
            if (!big && value > (std::numeric_limits<std::uint64_t>::max() >> 7)) {
                number.emplace(value);
                big = true;
            }
            if (big) number->ShiftLeft7();
            else value <<= 7;
        }
        if (first) {
            first = false;
            int arc;
            if (value >= 80) {
                arc = 2;
                if (big) number->Subtract(80);
                else value -= 80;
            } else {
                arc = static_cast<int>(value / 40);
                value -= static_cast<std::uint64_t>(arc) * 40;
            }
            if (space > 1) {
                *buffer++ = static_cast<char>('0' + arc);
                *buffer = '\0';
                --space;
            }
            ++result;
        }
        if (big) {
            const std::string text = "." + number->Decimal();
            Write(buffer, space, text);
            result += static_cast<int>(text.size());
        } else {
            char text[32];
            text[0] = '.';
            const auto converted = std::to_chars(text + 1, text + sizeof(text), value);
            const std::string_view part(text, static_cast<std::size_t>(converted.ptr - text));
            Write(buffer, space, part);
            result += static_cast<int>(part.size());
        }
    }
    return result;
}

int ObjectText(char* buffer, int space, const LibreSslAsn1Object* object, const char* function) {
    if (space < 0 || (space > 0 && !buffer)) NotImplemented_nid_no_patch(function);
    if (space > 0) buffer[0] = '\0';
    if (!object || !object->data) return 0;
    const int nid = LibreSslObjectNid_nid_no_patch(object);
    if (nid != 0) {
        const char* name = LibreSslObjectLongName_nid_no_patch(nid);
        if (!name) name = LibreSslObjectShortName_nid_no_patch(nid);
        if (name) {
            const std::size_t length = std::strlen(name);
            if (space > 0) {
                const std::size_t copied = std::min(length, static_cast<std::size_t>(space - 1));
                std::memcpy(buffer, name, copied);
                buffer[copied] = '\0';
            }
            return static_cast<int>(length);
        }
    }
    try {
        return NumericObjectText(buffer, space, object);
    } catch (const std::bad_alloc&) {
        LibreSslPutError_nid_no_patch(BigNumberLibrary, AnyFunction, AllocationFailure);
        return 0;
    }
}

} // namespace

extern "C" {

int LibreSslObjectNid_nid_no_patch(const LibreSslAsn1Object* object) {
    if (!object) return 0;
    if (object->nid != 0) return object->nid;
    if (const int added = LibreSslAddedObjectNid_nid_no_patch(object)) return added;
    int low = 0;
    int high = LibreSslObjectOrderCount;
    while (low < high) {
        const int middle = (low + high) / 2;
        const auto& entry = LibreSslObjects_nid_no_patch[LibreSslObjectOrder_nid_no_patch[middle]];
        const int comparison = CompareObject(object, &entry);
        if (comparison < 0) high = middle;
        else if (comparison > 0) low = middle + 1;
        else return entry.nid;
    }
    return 0;
}

const char* LibreSslObjectShortName_nid_no_patch(int nid) {
    return Name(nid, false);
}

const char* LibreSslObjectLongName_nid_no_patch(int nid) {
    return Name(nid, true);
}

int APS5_VABI i2t_ASN1_OBJECT_nid_postfix(char* buffer, int length, const LibreSslAsn1Object* object) {
    return ObjectText(buffer, length, object, __func__);
}

}
