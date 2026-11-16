#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <tuple>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

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

constexpr int OcspLibrary = 39;
constexpr int OcspFunction = 0xfff;
constexpr int ErrorInNextUpdate = 122;
constexpr int ErrorInThisUpdate = 123;
constexpr int NextUpdateBeforeThisUpdate = 124;
constexpr int StatusExpired = 125;
constexpr int StatusNotYetValid = 126;
constexpr int StatusTooOld = 127;
constexpr int GeneralizedTimeLength = 15;

int Fail(int reason) {
    LibreSslPutError_nid_no_patch(OcspLibrary, OcspFunction, reason);
    return 0;
}

bool ParseGeneralizedTime(const Asn1String& time, GuestTm& result) {
    if (!time.data || time.length != GeneralizedTimeLength || time.data[GeneralizedTimeLength - 1] != 'Z') return false;
    for (int i = 0; i < GeneralizedTimeLength - 1; ++i) {
        if (time.data[i] < '0' || time.data[i] > '9') return false;
    }
    const auto pair = [&time](int index) { return (time.data[index] - '0') * 10 + (time.data[index + 1] - '0'); };
    result.tm_year = pair(0) * 100 + pair(2) - 1900;
    result.tm_mon = pair(4) - 1;
    result.tm_mday = pair(6);
    result.tm_hour = pair(8);
    result.tm_min = pair(10);
    result.tm_sec = pair(12);
    return result.tm_mon <= 11 && result.tm_mon >= 0 && result.tm_mday >= 1 && result.tm_mday <= 31 && result.tm_hour <= 23 && result.tm_min <= 59 && result.tm_sec <= 59;
}

bool TimeAt(std::int64_t now, std::uint64_t offset, GuestTm& result) {
    const auto time = static_cast<std::int64_t>(static_cast<std::uint64_t>(now) + offset);
    return gmtime_r_nid_postfix(&time, &result) != nullptr;
}

auto Fields(const GuestTm& time) {
    return std::tie(time.tm_year, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
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

int APS5_VABI OCSP_check_validity_nid_postfix(const Asn1String* thisUpdate, const Asn1String* nextUpdate, std::int64_t slack, std::int64_t maxAge) {
    if (!thisUpdate) NotImplemented_nid_no_patch(__func__);
    std::int64_t now = 0;
    time_nid_postfix(&now);
    GuestTm thisTime{};
    GuestTm limit{};
    if (!ParseGeneralizedTime(*thisUpdate, thisTime)) return Fail(ErrorInThisUpdate);
    if (!TimeAt(now, static_cast<std::uint64_t>(slack), limit)) return 0;
    if (Fields(thisTime) > Fields(limit)) return Fail(StatusNotYetValid);
    if (maxAge >= 0) {
        if (!TimeAt(now, 0 - static_cast<std::uint64_t>(maxAge), limit)) return 0;
        if (Fields(thisTime) < Fields(limit)) return Fail(StatusTooOld);
    }
    if (!nextUpdate) return 1;
    GuestTm nextTime{};
    if (!ParseGeneralizedTime(*nextUpdate, nextTime)) return Fail(ErrorInNextUpdate);
    if (!TimeAt(now, 0 - static_cast<std::uint64_t>(slack), limit)) return 0;
    if (Fields(nextTime) < Fields(limit)) return Fail(StatusExpired);
    if (Fields(nextTime) < Fields(thisTime)) return Fail(NextUpdateBeforeThisUpdate);
    return 1;
}

}
