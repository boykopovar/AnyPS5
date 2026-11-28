#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int64_t Asn1StringFlagNdef = 0x010;

void Checked(bool copied, int savedErrno, const char* function, const char* what) {
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": " + what);
}

Asn1String ReadString(const Asn1String* string, const char* function) {
    Asn1String copy{};
    const int savedErrno = errno;
    Checked(GuestCopyIn_nid_no_patch(&copy, string, sizeof(copy)), savedErrno, function, "unreadable string");
    return copy;
}

}

extern "C" {
const unsigned char* APS5_VABI ASN1_STRING_get0_data_nid_postfix(const Asn1String* string) {
    if (!string) throw std::invalid_argument(std::string(__func__) + ": null string");
    return ReadString(string, __func__).data;
}

void APS5_VABI ASN1_INTEGER_free_nid_postfix(Asn1String* integer) {
    if (!integer) return;
    const Asn1String fields = ReadString(integer, __func__);
    if (!(fields.flags & Asn1StringFlagNdef) && fields.data) {
        if (fields.length < 0) throw std::invalid_argument(std::string(__func__) + ": negative length");
        static constexpr std::array<unsigned char, 4096> zeros{};
        const int savedErrno = errno;
        for (std::size_t offset = 0, length = static_cast<std::size_t>(fields.length); offset < length; offset += zeros.size()) {
            const std::size_t chunk = std::min(zeros.size(), length - offset);
            Checked(GuestCopyOut_nid_no_patch(fields.data + offset, zeros.data(), chunk), savedErrno, __func__, "unwritable data");
        }
        free_nid_postfix(fields.data);
    }
    free_nid_postfix(integer);
}

}
