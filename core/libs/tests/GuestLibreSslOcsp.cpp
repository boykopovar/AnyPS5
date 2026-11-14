#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <string>

using StringFunction = const char* (APS5_VABI*)(std::int64_t);

void Check(StringFunction function, const char* name, std::int64_t value, const char* expected) {
    const char* actual = function(value);
    if (actual == nullptr || std::strcmp(actual, expected) != 0) {
        std::fprintf(stderr, "%s(%lld): expected %s, got %s\n", name, static_cast<long long>(value), expected, actual ? actual : "(null)");
        std::abort();
    }
}

constexpr std::int32_t GeneralizedTime = 24;
constexpr std::uint64_t OcspError = 0x27fffull << 12;
constexpr std::uint64_t ErrorInNextUpdate = OcspError | 122;
constexpr std::uint64_t ErrorInThisUpdate = OcspError | 123;
constexpr std::uint64_t NextUpdateBeforeThisUpdate = OcspError | 124;
constexpr std::uint64_t StatusExpired = OcspError | 125;
constexpr std::uint64_t StatusNotYetValid = OcspError | 126;
constexpr std::uint64_t StatusTooOld = OcspError | 127;
constexpr std::int64_t Century = 3155760000;

struct TimeString {
    std::string text;
    Asn1String value;

    explicit TimeString(const char* input, std::int32_t type = GeneralizedTime)
        : text(input), value{static_cast<std::int32_t>(text.size()), type, reinterpret_cast<unsigned char*>(text.data()), 0} {}
    TimeString(const TimeString&) = delete;
    TimeString& operator=(const TimeString&) = delete;
};

void CheckValidity(const char* name, const Asn1String* thisUpdate, const Asn1String* nextUpdate, std::int64_t slack, std::int64_t maxAge, int expected, std::uint64_t error) {
    ERR_clear_error_nid_postfix();
    const int actual = OCSP_check_validity_nid_postfix(thisUpdate, nextUpdate, slack, maxAge);
    const std::uint64_t first = ERR_get_error_nid_postfix();
    const std::uint64_t second = ERR_get_error_nid_postfix();
    if (actual != expected || first != error || second != 0) {
        std::fprintf(stderr, "OCSP_check_validity %s: expected %d and 0x%llx, got %d, 0x%llx and 0x%llx\n", name, expected, static_cast<unsigned long long>(error), actual,
            static_cast<unsigned long long>(first), static_cast<unsigned long long>(second));
        std::abort();
    }
}

void CheckValidity() {
    constexpr auto Maximum = std::numeric_limits<std::int64_t>::max();
    constexpr auto Minimum = std::numeric_limits<std::int64_t>::min();
    const TimeString past("20000101000000Z");
    const TimeString pastSecond("20000101000001Z");
    const TimeString future("99991231235959Z");
    const TimeString yearZero("00000101000000Z");
    const TimeString february("20000231000000Z");
    const TimeString march("20000301000000Z");
    const TimeString utcType("20000101000000Z", 23);
    const TimeString octetType("20000101000000Z", 4);
    const TimeString untyped("99991231235959Z", 0);

    CheckValidity("valid window", &past.value, &future.value, 0, -1, 1, 0);
    CheckValidity("no next update", &past.value, nullptr, 0, -1, 1, 0);
    CheckValidity("minimum age skips the age check", &past.value, &future.value, 0, Minimum, 1, 0);
    CheckValidity("year zero", &yearZero.value, &future.value, 0, -1, 1, 0);
    CheckValidity("UTCTime type", &utcType.value, &untyped.value, 0, -1, 1, 0);
    CheckValidity("octet string type", &octetType.value, &untyped.value, 0, -1, 1, 0);
    CheckValidity("not yet valid", &future.value, &future.value, 0, -1, 0, StatusNotYetValid);
    CheckValidity("not yet valid before expired", &future.value, &past.value, 0, -1, 0, StatusNotYetValid);
    CheckValidity("expired", &past.value, &past.value, 0, -1, 0, StatusExpired);
    CheckValidity("too old", &past.value, &future.value, 0, 3600, 0, StatusTooOld);
    CheckValidity("zero age", &past.value, &future.value, 0, 0, 0, StatusTooOld);
    CheckValidity("next update before this update", &pastSecond.value, &past.value, Century, -1, 0, NextUpdateBeforeThisUpdate);
    CheckValidity("next update equal to this update", &past.value, &past.value, Century, -1, 1, 0);
    CheckValidity("expired before next update before this update", &pastSecond.value, &past.value, 0, -1, 0, StatusExpired);
    CheckValidity("too old before expired", &past.value, &past.value, 0, 3600, 0, StatusTooOld);
    CheckValidity("not yet valid before too old", &past.value, &future.value, -Century, 0, 0, StatusNotYetValid);
    CheckValidity("February 31 before March 1", &february.value, &march.value, Century, -1, 1, 0);
    CheckValidity("March 1 after February 31", &march.value, &february.value, Century, -1, 0, NextUpdateBeforeThisUpdate);

    const char* invalid[] {"000101000000Z", "20000101000000.5Z", "2000010100000aZ", "200001010000000", "20000101000000z", "20001301000000Z", "20000001000000Z",
        "20000100000000Z", "20000132000000Z", "20000101240000Z", "20000101006000Z", "20000101000060Z", " 0000101000000Z", "-0000101000000Z", "+0000101000000Z",
        "\xb2" "0000101000000Z", "20000101000000Z0", ""};
    for (const char* text : invalid) {
        const TimeString time(text);
        CheckValidity(text, &time.value, &future.value, 0, -1, 0, ErrorInThisUpdate);
        CheckValidity(text, &past.value, &time.value, 0, -1, 0, ErrorInNextUpdate);
    }
    const TimeString month("20001301000000Z");
    CheckValidity("invalid this update without next update", &month.value, nullptr, 0, -1, 0, ErrorInThisUpdate);
    CheckValidity("not yet valid before invalid next update", &future.value, &month.value, 0, -1, 0, StatusNotYetValid);
    const Asn1String missing{15, GeneralizedTime, nullptr, 0};
    CheckValidity("null data", &missing, &future.value, 0, -1, 0, ErrorInThisUpdate);
    CheckValidity("null next update data", &past.value, &missing, 0, -1, 0, ErrorInNextUpdate);
    const Asn1String negative{-1, GeneralizedTime, past.value.data, 0};
    CheckValidity("negative length", &negative, &future.value, 0, -1, 0, ErrorInThisUpdate);

    CheckValidity("wrapping slack", &past.value, &future.value, Maximum, -1, 0, 0);
    CheckValidity("wrapping slack without next update", &past.value, nullptr, Maximum, -1, 0, 0);
    CheckValidity("wrapping negative slack", &past.value, &future.value, Minimum, -1, 0, 0);
    CheckValidity("slack beyond the calendar", &past.value, &future.value, 70000000000000000, -1, 0, 0);
    CheckValidity("negative slack beyond the calendar", &past.value, &future.value, -70000000000000000, -1, 0, 0);
    CheckValidity("age beyond the calendar", &past.value, &future.value, 0, Maximum, 0, 0);
    CheckValidity("slack inside the calendar", &past.value, &future.value, 60000000000000000, -1, 1, 0);
    CheckValidity("age inside the calendar", &past.value, nullptr, 0, 60000000000000000, 1, 0);

    try {
        OCSP_check_validity_nid_postfix(nullptr, &future.value, 0, -1);
    } catch (const std::exception&) {
        return;
    }
    std::fprintf(stderr, "OCSP_check_validity: a null this update must throw\n");
    std::abort();
}

int main() {
    CheckValidity();

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
