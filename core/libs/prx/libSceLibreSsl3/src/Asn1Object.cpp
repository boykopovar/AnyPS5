#include <algorithm>
#include <array>
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
    Asn1Object_nid_no_patch fields{};
    bool copied = GuestCopyIn_nid_no_patch(&fields, object, sizeof(fields));
    if (!copied) throw std::invalid_argument(std::string(__func__) + ": unreadable object");
    const bool ownsData = (fields.flags & DynamicData) && fields.data;
    if (ownsData && fields.length < 0)
        throw std::invalid_argument(std::string(__func__) + ": negative data length");
    if (ownsData) {
        static constexpr std::array<unsigned char, 4096> zeros{};
        auto* data = const_cast<unsigned char*>(fields.data);
        for (std::size_t offset = 0, length = static_cast<std::size_t>(fields.length); offset < length; offset += zeros.size()) {
            copied = GuestCopyOut_nid_no_patch(data + offset, zeros.data(), std::min(zeros.size(), length - offset));
            if (!copied) throw std::invalid_argument(std::string(__func__) + ": unwritable data");
        }
    }
    Asn1Object_nid_no_patch released = fields;
    if (fields.flags & DynamicStrings) {
        released.shortName = nullptr;
        released.longName = nullptr;
    }
    if (fields.flags & DynamicData) {
        released.data = nullptr;
        released.length = 0;
    }
    if (!(fields.flags & Dynamic) && (fields.flags & (DynamicStrings | DynamicData))) {
        copied = GuestCopyOut_nid_no_patch(object, &released, sizeof(released));
        if (!copied) throw std::invalid_argument(std::string(__func__) + ": unwritable object");
    }
    if (fields.flags & DynamicStrings) {
        free_nid_postfix(const_cast<char*>(fields.shortName));
        free_nid_postfix(const_cast<char*>(fields.longName));
    }
    if (ownsData) free_nid_postfix(const_cast<unsigned char*>(fields.data));
    if (fields.flags & Dynamic) free_nid_postfix(object);
}
}
