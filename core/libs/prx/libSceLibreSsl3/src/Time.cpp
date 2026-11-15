#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <stdexcept>

namespace {

static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, data) == 8);

bool CopyGuestInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool CopyGuestOutPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool ReadTime(const Asn1String* value, std::int64_t& seconds) {
    if (!value) {
        seconds = std::time(nullptr);
        return seconds >= -62167219200LL && seconds <= 253402300799LL;
    }
    Asn1String local{};
    if (!CopyGuestInPreservingErrno(&local, value, sizeof(local)))
        NotImplemented_nid_no_patch("ASN1_TIME_diff");
    if (!local.data || (local.length != 13 && local.length != 15)) return false;
    unsigned char data[15]{};
    if (!CopyGuestInPreservingErrno(data, local.data, static_cast<std::size_t>(local.length)))
        NotImplemented_nid_no_patch("ASN1_TIME_diff");
    if (data[local.length - 1] != 'Z') return false;
    for (int i = 0; i < local.length - 1; ++i)
        if (data[i] < '0' || data[i] > '9') return false;
    const int yearDigits = local.length - 11;
    int year = 0;
    for (int i = 0; i < yearDigits; ++i) year = year * 10 + data[i] - '0';
    if (yearDigits == 2) year += year < 50 ? 2000 : 1900;
    unsigned fields[5];
    for (int i = 0; i < 5; ++i) {
        const int position = yearDigits + i * 2;
        fields[i] = (data[position] - '0') * 10 + data[position + 1] - '0';
    }
    const std::chrono::year_month_day date{std::chrono::year{year},
        std::chrono::month{fields[0]}, std::chrono::day{fields[1]}};
    if (!date.ok() || fields[2] > 23 || fields[3] > 59 || fields[4] > 59) return false;
    seconds = static_cast<std::int64_t>(std::chrono::sys_days{date}.time_since_epoch().count()) * 86400
        + fields[2] * 3600 + fields[3] * 60 + fields[4];
    return true;
}

}

extern "C" int APS5_VABI ASN1_TIME_diff_nid_postfix(int* days, int* seconds,
                                                 const Asn1String* from, const Asn1String* to) {
    std::int64_t start;
    std::int64_t end;
    if (!ReadTime(from, start) || !ReadTime(to, end)) return 0;
    if (!days || !seconds) throw std::invalid_argument("ASN1_TIME_diff: null output");
    const auto difference = end - start;
    const int outSeconds = static_cast<int>(difference % 86400);
    const int outDays = static_cast<int>(difference / 86400);
    int previousDays = 0;
    const bool hadDays = CopyGuestInPreservingErrno(&previousDays, days, sizeof(previousDays));
    if (!CopyGuestOutPreservingErrno(days, &outDays, sizeof(outDays)))
        NotImplemented_nid_no_patch(__func__);
    if (!CopyGuestOutPreservingErrno(seconds, &outSeconds, sizeof(outSeconds))) {
        if (hadDays)
            CopyGuestOutPreservingErrno(days, &previousDays, sizeof(previousDays));
        NotImplemented_nid_no_patch(__func__);
    }
    return 1;
}
