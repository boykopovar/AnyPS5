#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using StringFunction = const char* (APS5_VABI*)(std::int64_t);

void Check(StringFunction function, const char* name, std::int64_t value, const char* expected) {
    const char* actual = function(value);
    if (actual == nullptr || std::strcmp(actual, expected) != 0) {
        std::fprintf(stderr, "%s(%lld): expected %s, got %s\n", name, static_cast<long long>(value), expected, actual ? actual : "(null)");
        std::abort();
    }
}

int main() {
    constexpr auto Unknown = "(UNKNOWN)";
    constexpr std::int64_t Outside[] {-1, 4, 7, 9, 0x100, 0x100000000, std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()};

    const char* responses[] {"successful", "malformedrequest", "internalerror", "trylater", nullptr, "sigrequired", "unauthorized"};
    for (std::int64_t code = 0; code < 7; ++code) {
        Check(OCSP_response_status_str_nid_postfix, "OCSP_response_status_str", code, responses[code] ? responses[code] : Unknown);
    }

    const char* certificates[] {"good", "revoked", "unknown"};
    for (std::int64_t code = 0; code < 3; ++code) {
        Check(OCSP_cert_status_str_nid_postfix, "OCSP_cert_status_str", code, certificates[code]);
    }

    const char* reasons[] {"unspecified", "keyCompromise", "cACompromise", "affiliationChanged", "superseded", "cessationOfOperation", "certificateHold", nullptr, "removeFromCRL"};
    for (std::int64_t code = 0; code < 9; ++code) {
        Check(OCSP_crl_reason_str_nid_postfix, "OCSP_crl_reason_str", code, reasons[code] ? reasons[code] : Unknown);
    }

    for (const auto value : Outside) {
        Check(OCSP_response_status_str_nid_postfix, "OCSP_response_status_str", value, Unknown);
        Check(OCSP_cert_status_str_nid_postfix, "OCSP_cert_status_str", value, Unknown);
        if (value != 4) Check(OCSP_crl_reason_str_nid_postfix, "OCSP_crl_reason_str", value, Unknown);
    }
    Check(OCSP_response_status_str_nid_postfix, "OCSP_response_status_str", 0x100000000 | 1, Unknown);
    Check(OCSP_crl_reason_str_nid_postfix, "OCSP_crl_reason_str", -0x100000000 | 8, Unknown);
    return 0;
}
