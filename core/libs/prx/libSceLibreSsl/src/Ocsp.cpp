#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>

namespace {
struct OcspString {
    std::int64_t code;
    const char* text;
};

template <std::size_t Count>
const char* TableToString(std::int64_t code, const OcspString (&table)[Count]) {
    for (const auto& entry : table) {
        if (entry.code == code) return entry.text;
    }
    return "(UNKNOWN)";
}
}

extern "C" {

const char* APS5_VABI OCSP_response_status_str_nid_postfix(std::int64_t status) {
    static constexpr OcspString table[] {
        {0, "successful"},
        {1, "malformedrequest"},
        {2, "internalerror"},
        {3, "trylater"},
        {5, "sigrequired"},
        {6, "unauthorized"},
    };
    return TableToString(status, table);
}

const char* APS5_VABI OCSP_cert_status_str_nid_postfix(std::int64_t status) {
    static constexpr OcspString table[] {
        {0, "good"},
        {1, "revoked"},
        {2, "unknown"},
    };
    return TableToString(status, table);
}

const char* APS5_VABI OCSP_crl_reason_str_nid_postfix(std::int64_t reason) {
    static constexpr OcspString table[] {
        {0, "unspecified"},
        {1, "keyCompromise"},
        {2, "cACompromise"},
        {3, "affiliationChanged"},
        {4, "superseded"},
        {5, "cessationOfOperation"},
        {6, "certificateHold"},
        {8, "removeFromCRL"},
    };
    return TableToString(reason, table);
}

}
