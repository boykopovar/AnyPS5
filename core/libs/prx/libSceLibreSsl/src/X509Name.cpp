#include "prx/libSceLibreSsl/include/Api.hpp"
#include <climits>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslX509NameEntry) == 24);
static_assert(offsetof(LibreSslX509NameEntry, value) == 8);
static_assert(sizeof(LibreSslX509Name) == 40);
static_assert(offsetof(LibreSslX509Name, entries) == 0);

namespace {

struct Asn1Object {
    const char* shortName;
    const char* longName;
    int nid;
    int length;
    const unsigned char* data;
    int flags;
};

static_assert(sizeof(Asn1Object) == 40 && offsetof(Asn1Object, length) == 20 && offsetof(Asn1Object, data) == 24);

struct NameAttribute {
    int nid;
    int length;
    unsigned char der[10];
};

constexpr NameAttribute NameAttributes[] {
    {0, 0, {}},
    {13, 3, {0x55, 0x04, 0x03}},
    {14, 3, {0x55, 0x04, 0x06}},
    {15, 3, {0x55, 0x04, 0x07}},
    {16, 3, {0x55, 0x04, 0x08}},
    {17, 3, {0x55, 0x04, 0x0a}},
    {18, 3, {0x55, 0x04, 0x0b}},
    {48, 9, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01}},
    {99, 3, {0x55, 0x04, 0x2a}},
    {100, 3, {0x55, 0x04, 0x04}},
    {101, 3, {0x55, 0x04, 0x2b}},
    {105, 3, {0x55, 0x04, 0x05}},
    {106, 3, {0x55, 0x04, 0x0c}},
    {107, 3, {0x55, 0x04, 0x0d}},
    {173, 3, {0x55, 0x04, 0x29}},
    {174, 3, {0x55, 0x04, 0x2e}},
    {391, 10, {0x09, 0x92, 0x26, 0x89, 0x93, 0xf2, 0x2c, 0x64, 0x01, 0x19}},
    {400, 3, {0x55, 0x04, 0x48}},
    {458, 10, {0x09, 0x92, 0x26, 0x89, 0x93, 0xf2, 0x2c, 0x64, 0x01, 0x01}},
    {503, 3, {0x55, 0x04, 0x2d}},
    {509, 3, {0x55, 0x04, 0x2c}},
    {510, 3, {0x55, 0x04, 0x41}},
    {660, 3, {0x55, 0x04, 0x09}},
    {661, 3, {0x55, 0x04, 0x11}},
    {859, 3, {0x55, 0x04, 0x0e}},
    {860, 3, {0x55, 0x04, 0x0f}},
    {861, 3, {0x55, 0x04, 0x10}},
    {862, 3, {0x55, 0x04, 0x12}},
    {863, 3, {0x55, 0x04, 0x13}},
    {864, 3, {0x55, 0x04, 0x14}},
    {865, 3, {0x55, 0x04, 0x15}},
    {866, 3, {0x55, 0x04, 0x16}},
    {867, 3, {0x55, 0x04, 0x17}},
    {868, 3, {0x55, 0x04, 0x18}},
    {869, 3, {0x55, 0x04, 0x19}},
    {870, 3, {0x55, 0x04, 0x1a}},
    {871, 3, {0x55, 0x04, 0x1b}},
    {872, 3, {0x55, 0x04, 0x1c}},
    {873, 3, {0x55, 0x04, 0x1d}},
    {874, 3, {0x55, 0x04, 0x1e}},
    {875, 3, {0x55, 0x04, 0x1f}},
    {876, 3, {0x55, 0x04, 0x20}},
    {877, 3, {0x55, 0x04, 0x21}},
    {878, 3, {0x55, 0x04, 0x22}},
    {879, 3, {0x55, 0x04, 0x23}},
    {880, 3, {0x55, 0x04, 0x24}},
    {881, 3, {0x55, 0x04, 0x25}},
    {882, 3, {0x55, 0x04, 0x26}},
    {883, 3, {0x55, 0x04, 0x27}},
    {884, 3, {0x55, 0x04, 0x28}},
    {885, 3, {0x55, 0x04, 0x2f}},
    {886, 3, {0x55, 0x04, 0x30}},
    {887, 3, {0x55, 0x04, 0x31}},
    {888, 3, {0x55, 0x04, 0x32}},
    {889, 3, {0x55, 0x04, 0x33}},
    {890, 3, {0x55, 0x04, 0x34}},
    {891, 3, {0x55, 0x04, 0x35}},
    {892, 3, {0x55, 0x04, 0x36}},
};

const NameAttribute* FindAttribute(int nid, const char* function) {
    for (const auto& attribute : NameAttributes) {
        if (attribute.nid == nid) return &attribute;
    }
    NotImplemented_nid_no_patch((std::string(function) + ": NID " + std::to_string(nid) + " is not a name attribute type in the object identifier subset").c_str());
}

int IndexOf(const LibreSslX509Name* name, const NameAttribute& attribute, int lastpos, const char* function) {
    if (!name) return -1;
    if (lastpos == INT_MAX) throw std::invalid_argument(std::string(function) + ": last position overflows");
    if (lastpos < 0) lastpos = -1;
    const int count = name->entries ? name->entries->num : -1;
    for (++lastpos; lastpos < count; ++lastpos) {
        const auto* entry = X509_NAME_get_entry_nid_postfix(name, lastpos);
        if (!entry || !entry->object) throw std::invalid_argument(std::string(function) + ": null name entry or object");
        const auto* object = static_cast<const Asn1Object*>(entry->object);
        if (object->length != attribute.length) continue;
        if (attribute.length == 0) return lastpos;
        if (!object->data) throw std::invalid_argument(std::string(function) + ": null object data");
        if (std::memcmp(object->data, attribute.der, attribute.length) == 0) return lastpos;
    }
    return -1;
}

}

extern "C" {

LibreSslX509NameEntry* APS5_VABI X509_NAME_get_entry_nid_postfix(const LibreSslX509Name* name, int loc) {
    if (!name || loc < 0 || !name->entries || name->entries->num <= loc) return nullptr;
    if (!name->entries->data) throw std::invalid_argument(std::string(__func__) + ": null entry array");
    return reinterpret_cast<LibreSslX509NameEntry*>(name->entries->data[loc]);
}

void* APS5_VABI X509_NAME_ENTRY_get_data_nid_postfix(const LibreSslX509NameEntry* entry) {
    if (!entry) return nullptr;
    return entry->value;
}

int APS5_VABI X509_NAME_get_index_by_NID_nid_postfix(const LibreSslX509Name* name, int nid, int lastpos) {
    const auto* attribute = FindAttribute(nid, __func__);
    return IndexOf(name, *attribute, lastpos, __func__);
}

int APS5_VABI X509_NAME_get_text_by_NID_nid_postfix(const LibreSslX509Name* name, int nid, char* buf, int len) {
    const auto* attribute = FindAttribute(nid, __func__);
    const int index = IndexOf(name, *attribute, -1, __func__);
    if (index < 0) return -1;
    const auto* data = static_cast<const Asn1String*>(X509_NAME_ENTRY_get_data_nid_postfix(X509_NAME_get_entry_nid_postfix(name, index)));
    if (!data) throw std::invalid_argument(std::string(__func__) + ": null entry value");
    if (len == INT_MIN) throw std::invalid_argument(std::string(__func__) + ": buffer length overflows");
    const int count = data->length > len - 1 ? len - 1 : data->length;
    if (!buf) return data->length;
    if (count >= 0) {
        if (count) {
            if (!data->data) throw std::invalid_argument(std::string(__func__) + ": null entry string");
            std::memcpy(buf, data->data, count);
        }
        buf[count] = '\0';
    }
    return count;
}

}
