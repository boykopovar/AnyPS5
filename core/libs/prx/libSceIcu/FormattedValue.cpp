#include "prx/libSceIcu/FormattedValueBackend.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>

namespace {

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    const int savedErrno = errno;
    std::int32_t value = 0;
    const bool ok = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing == incoming) return;
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

const char16_t* APS5_VABI ufmtval_getString_nid_postfix(const UFormattedValue* ufmtval, std::int32_t* pLength,
                                                      std::int32_t* ec) {
    const auto incoming = ReadStatus(ec, __func__);
    if (incoming > 0) return nullptr;
    if (!ufmtval) {
        WriteStatus(ec, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    if (!Icu::FormattedValueBackend::IsRegistered(ufmtval)) NotImplemented_nid_no_patch(__func__);
    auto local = incoming;
    std::int32_t hostLength = 0;
    const char16_t* text = Icu::FormattedValueBackend::GetString(ufmtval, &hostLength, &local);
    WriteStatus(ec, incoming, local, __func__);
    if (local <= 0 && pLength) {
        const int savedErrno = errno;
        const bool ok = GuestCopyOut_nid_no_patch(pLength, &hostLength, sizeof(hostLength));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(__func__);
    }
    return text;
}

std::int8_t APS5_VABI ufmtval_nextPosition_nid_postfix(const UFormattedValue* ufmtval,
                                                       UConstrainedFieldPosition* ucfpos, std::int32_t* ec) {
    const auto incoming = ReadStatus(ec, __func__);
    if (incoming > 0) return 0;
    if (!ufmtval || !ucfpos) {
        WriteStatus(ec, incoming, Icu::IllegalArgument, __func__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
