#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace LibreSsl3 {
namespace {

constexpr int Asn1Library = 13;
constexpr int ObjectLibrary = 8;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int InvalidObjectEncoding = 216;
constexpr int UnknownObject = 101;
constexpr int ObjectDynamic = 0x01;
constexpr int ObjectDynamicData = 0x08;

void PutError(int library, int reason) {
    LibreSslPutError_nid_no_patch(library, AnyFunction, reason);
}

bool DecimalArc(const char*& cursor, std::vector<unsigned char>& arc) {
    arc.clear();
    if (*cursor < '0' || *cursor > '9') return false;
    std::size_t digits = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        if (digits > 0 && arc.empty() && *cursor == '0') return false;
        unsigned int carry = static_cast<unsigned int>(*cursor - '0');
        for (unsigned char& byte : arc) {
            const unsigned int value = static_cast<unsigned int>(byte) * 10u + carry;
            byte = static_cast<unsigned char>(value & 0xffu);
            carry = value >> 8;
        }
        while (carry != 0) {
            arc.push_back(static_cast<unsigned char>(carry & 0xffu));
            carry >>= 8;
        }
        ++cursor;
        ++digits;
    }
    return true;
}

void MultiplyAdd(std::vector<unsigned char>& value, unsigned int multiplier, const std::vector<unsigned char>& addend) {
    unsigned int carry = 0;
    for (unsigned char& byte : value) {
        const unsigned int product = static_cast<unsigned int>(byte) * multiplier + carry;
        byte = static_cast<unsigned char>(product & 0xffu);
        carry = product >> 8;
    }
    while (carry != 0) {
        value.push_back(static_cast<unsigned char>(carry & 0xffu));
        carry >>= 8;
    }
    unsigned int add = 0;
    for (std::size_t index = 0; index < addend.size() || add != 0; ++index) {
        if (index >= value.size()) value.push_back(0);
        const unsigned int sum = static_cast<unsigned int>(value[index]) +
            (index < addend.size() ? addend[index] : 0u) + add;
        value[index] = static_cast<unsigned char>(sum & 0xffu);
        add = sum >> 8;
    }
    while (!value.empty() && value.back() == 0) value.pop_back();
}

void Base128(const std::vector<unsigned char>& arc, std::vector<unsigned char>& content) {
    std::vector<unsigned char> work = arc;
    std::vector<unsigned char> groups;
    while (!work.empty()) {
        unsigned int remainder = 0;
        for (auto byte = work.rbegin(); byte != work.rend(); ++byte) {
            const unsigned int value = (remainder << 8) | *byte;
            *byte = static_cast<unsigned char>(value / 128u);
            remainder = value % 128u;
        }
        groups.push_back(static_cast<unsigned char>(remainder));
        while (!work.empty() && work.back() == 0) work.pop_back();
    }
    if (groups.empty()) groups.push_back(0);
    for (auto group = groups.rbegin(); group != groups.rend(); ++group)
        content.push_back(static_cast<unsigned char>(*group | (std::next(group) == groups.rend() ? 0x00 : 0x80)));
}

bool EncodeText(const char* text, std::vector<unsigned char>& content) {
    content.clear();
    const char* cursor = text;
    std::vector<unsigned char> first;
    std::vector<unsigned char> second;
    if (!DecimalArc(cursor, first)) return false;
    if (*cursor != '.') return false;
    ++cursor;
    if (!DecimalArc(cursor, second)) return false;
    std::uint64_t lowFirst = 0;
    for (auto byte = first.rbegin(); byte != first.rend(); ++byte) lowFirst = (lowFirst << 8) | *byte;
    if (first.size() > 8 || lowFirst > 2) return false;
    std::uint64_t lowSecond = 0;
    for (auto byte = second.rbegin(); byte != second.rend(); ++byte) lowSecond = (lowSecond << 8) | *byte;
    if (lowFirst < 2 && (second.size() > 8 || lowSecond >= 40)) return false;
    MultiplyAdd(first, 40, second);
    Base128(first, content);
    std::vector<unsigned char> arc;
    while (*cursor == '.') {
        ++cursor;
        if (!DecimalArc(cursor, arc)) return false;
        Base128(arc, content);
    }
    return *cursor == '\0' && !content.empty();
}

Asn1Object_nid_no_patch* NamedObject(int nid) {
    if (nid <= 0 || nid >= ObjectCount || ObjectAt(nid).nid == 0) {
        PutError(ObjectLibrary, UnknownObject);
        return nullptr;
    }
    return reinterpret_cast<Asn1Object_nid_no_patch*>(const_cast<LibreSslAsn1Object*>(&ObjectAt(nid)));
}

int LongNameNid(const char* text) {
    for (int nid = 1; nid < ObjectCount; ++nid) {
        const LibreSslAsn1Object& entry = ObjectAt(nid);
        if (entry.longName != nullptr && std::strcmp(entry.longName, text) == 0) return nid;
    }
    return 0;
}

} // namespace

Asn1Object_nid_no_patch* ObjectFromText(const char* text, int noName) {
    if (text == nullptr) return nullptr;
    if (noName == 0) {
        if (const int nid = OBJ_sn2nid_nid_postfix(text)) return NamedObject(nid);
        if (const int nid = LongNameNid(text)) return NamedObject(nid);
    }
    std::vector<unsigned char> content;
    if (!EncodeText(text, content)) {
        PutError(Asn1Library, InvalidObjectEncoding);
        return nullptr;
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(content.size()));
    if (data == nullptr) {
        PutError(ObjectLibrary, MallocFailure);
        return nullptr;
    }
    std::memcpy(data, content.data(), content.size());
    auto* object = static_cast<Asn1Object_nid_no_patch*>(calloc_nid_postfix(1, sizeof(Asn1Object_nid_no_patch)));
    if (object == nullptr) {
        free_nid_postfix(data);
        PutError(ObjectLibrary, MallocFailure);
        return nullptr;
    }
    object->length = static_cast<int>(content.size());
    object->data = data;
    object->flags = ObjectDynamic | ObjectDynamicData;
    return object;
}

} // namespace LibreSsl3
