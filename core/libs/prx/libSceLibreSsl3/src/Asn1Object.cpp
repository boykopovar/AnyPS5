#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

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
