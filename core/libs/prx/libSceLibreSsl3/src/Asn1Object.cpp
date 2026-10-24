#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct Asn1Object_nid_no_patch {
    const char* shortName;
    const char* longName;
    int nid;
    int length;
    const unsigned char* data;
    int flags;
};

static_assert(sizeof(Asn1Object_nid_no_patch) == 40);
static_assert(offsetof(Asn1Object_nid_no_patch, nid) == 16);
static_assert(offsetof(Asn1Object_nid_no_patch, length) == 20);
static_assert(offsetof(Asn1Object_nid_no_patch, data) == 24);
static_assert(offsetof(Asn1Object_nid_no_patch, flags) == 32);

extern "C" {
void APS5_VABI free_nid_postfix(void* pointer);
int APS5_VABI memset_s_nid_postfix(void* destination, std::size_t destinationSize, int value, std::size_t count);

void APS5_VABI ASN1_OBJECT_free_nid_postfix(Asn1Object_nid_no_patch* object) {
    if (!object) return;
    constexpr int Dynamic = 0x01;
    constexpr int DynamicStrings = 0x04;
    constexpr int DynamicData = 0x08;
    if ((object->flags & DynamicData) && object->data && object->length < 0)
        throw std::invalid_argument(std::string(__func__) + ": negative data length");
    if (object->flags & DynamicStrings) {
        free_nid_postfix(const_cast<char*>(object->shortName));
        free_nid_postfix(const_cast<char*>(object->longName));
        object->shortName = nullptr;
        object->longName = nullptr;
    }
    if (object->flags & DynamicData) {
        if (object->data) {
            memset_s_nid_postfix(const_cast<unsigned char*>(object->data), static_cast<std::size_t>(object->length), 0, static_cast<std::size_t>(object->length));
            free_nid_postfix(const_cast<unsigned char*>(object->data));
        }
        object->data = nullptr;
        object->length = 0;
    }
    if (object->flags & Dynamic) free_nid_postfix(object);
}
}
