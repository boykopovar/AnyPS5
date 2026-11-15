#include "prx/libSceLibreSsl/include/Api.hpp"
#include <climits>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"

static_assert(sizeof(LibreSslX509NameEntry) == 24);
static_assert(offsetof(LibreSslX509NameEntry, value) == 8);
static_assert(sizeof(LibreSslX509Name) == 40);
static_assert(offsetof(LibreSslX509Name, entries) == 0);

namespace {

constexpr int ObjectLibrary = 8;
constexpr int AnyFunction = 0xfff;
constexpr int UnknownNid = 101;

const LibreSslAsn1Object* ObjectByNid(int nid) {
    if (nid >= 0 && nid < LibreSslObjectCount) {
        const LibreSslAsn1Object& object = LibreSslObjects_nid_no_patch[nid];
        if (nid != 0 && object.nid == 0) {
            LibreSslPutError_nid_no_patch(ObjectLibrary, AnyFunction, UnknownNid);
            return nullptr;
        }
        return &object;
    }
    if (!LibreSslAddedObjectsExist_nid_no_patch()) return nullptr;
    const LibreSslAsn1Object* object = LibreSslAddedObjectByNid_nid_no_patch(nid);
    if (!object) LibreSslPutError_nid_no_patch(ObjectLibrary, AnyFunction, UnknownNid);
    return object;
}

int IndexOf(const LibreSslX509Name* name, const LibreSslAsn1Object& wanted, int lastpos, const char* function) {
    if (!name) return -1;
    if (lastpos == INT_MAX) throw std::invalid_argument(std::string(function) + ": last position overflows");
    if (lastpos < 0) lastpos = -1;
    const int count = name->entries ? name->entries->num : -1;
    for (++lastpos; lastpos < count; ++lastpos) {
        const auto* entry = X509_NAME_get_entry_nid_postfix(name, lastpos);
        if (!entry || !entry->object) throw std::invalid_argument(std::string(function) + ": null name entry or object");
        const auto* object = static_cast<const LibreSslAsn1Object*>(entry->object);
        if (object->length != wanted.length) continue;
        if (wanted.length == 0) return lastpos;
        if (!object->data) throw std::invalid_argument(std::string(function) + ": null object data");
        if (std::memcmp(object->data, wanted.data, static_cast<std::size_t>(wanted.length)) == 0) return lastpos;
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
    const LibreSslAsn1Object* object = ObjectByNid(nid);
    if (!object) return -2;
    return IndexOf(name, *object, lastpos, __func__);
}

int APS5_VABI X509_NAME_get_text_by_NID_nid_postfix(const LibreSslX509Name* name, int nid, char* buf, int len) {
    const LibreSslAsn1Object* object = ObjectByNid(nid);
    if (!object) return -1;
    const int index = IndexOf(name, *object, -1, __func__);
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
