#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace {

constexpr int Allocated = 1;
constexpr int StaticData = 2;
constexpr int ConstTime = 4;

bool Validate_nid_no_patch(LibreSsl3Bignum* number, LibreSsl3Bignum& guestNumber, std::size_t& dataBytes, const char* caller) {
    if (!number) {
        guestNumber = {};
        dataBytes = 0;
        return false;
    }
    if (!GuestCopyIn_nid_no_patch(&guestNumber, number, sizeof(guestNumber))) NotImplemented_nid_no_patch(caller);
    if (guestNumber.data && !(guestNumber.flags & StaticData) && guestNumber.capacity < 0)
        throw std::invalid_argument("LibreSSL3 BIGNUM has a negative limb capacity");
    if (!GuestWritable_nid_no_patch(number, sizeof(guestNumber))) NotImplemented_nid_no_patch(caller);
    dataBytes = 0;
    if (guestNumber.data && !(guestNumber.flags & StaticData)) {
        const auto count = static_cast<std::size_t>(guestNumber.capacity);
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(*guestNumber.data)) NotImplemented_nid_no_patch(caller);
        dataBytes = count * sizeof(*guestNumber.data);
        if (dataBytes != 0 && !GuestWritable_nid_no_patch(guestNumber.data, dataBytes)) NotImplemented_nid_no_patch(caller);
    }
    return true;
}

void ZeroGuest_nid_no_patch(void* pointer, std::size_t bytes, const char* caller) {
    constexpr std::array<unsigned char, 256> zeros{};
    auto* cursor = static_cast<unsigned char*>(pointer);
    std::size_t remaining = bytes;
    while (remaining != 0) {
        const std::size_t chunk = remaining < zeros.size() ? remaining : zeros.size();
        if (!GuestCopyOut_nid_no_patch(cursor, zeros.data(), chunk)) NotImplemented_nid_no_patch(caller);
        cursor += chunk;
        remaining -= chunk;
    }
}

void ValidateReplacement_nid_no_patch(void* previous, const LibreSsl3Bignum* replacement, const char* caller) {
    if (!replacement) return;
    LibreSsl3Bignum guestNumber{};
    std::size_t dataBytes = 0;
    Validate_nid_no_patch(static_cast<LibreSsl3Bignum*>(previous), guestNumber, dataBytes, caller);
}

void ValidatePrivateReplacement_nid_no_patch(void* previous, LibreSsl3Bignum* replacement, const char* caller) {
    ValidateReplacement_nid_no_patch(previous, replacement, caller);
    if (!replacement) return;
    LibreSsl3Bignum guestNumber{};
    if (!GuestCopyIn_nid_no_patch(&guestNumber, replacement, sizeof(guestNumber)) ||
        !GuestWritable_nid_no_patch(replacement, sizeof(guestNumber)))
        NotImplemented_nid_no_patch(caller);
}

void Replace_nid_no_patch(void*& previous, LibreSsl3Bignum* replacement) {
    if (!replacement) return;
    BN_clear_free_nid_postfix(static_cast<LibreSsl3Bignum*>(previous));
    previous = replacement;
}

void ReplacePrivate_nid_no_patch(void*& previous, LibreSsl3Bignum* replacement, const char* caller) {
    if (!replacement) return;
    Replace_nid_no_patch(previous, replacement);
    LibreSsl3Bignum guestNumber{};
    if (!GuestCopyIn_nid_no_patch(&guestNumber, replacement, sizeof(guestNumber)))
        NotImplemented_nid_no_patch(caller);
    guestNumber.flags |= ConstTime;
    if (!GuestCopyOut_nid_no_patch(replacement, &guestNumber, sizeof(guestNumber)))
        NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

void APS5_VABI BN_clear_free_nid_postfix(LibreSsl3Bignum* number) {
    LibreSsl3Bignum guestNumber{};
    std::size_t dataBytes = 0;
    if (!Validate_nid_no_patch(number, guestNumber, dataBytes, __func__)) return;
    const bool allocated = (guestNumber.flags & Allocated) != 0;
    if (guestNumber.data && !(guestNumber.flags & StaticData)) {
        ZeroGuest_nid_no_patch(guestNumber.data, dataBytes, __func__);
        free_nid_postfix(guestNumber.data);
    }
    const LibreSsl3Bignum cleared{};
    if (!GuestCopyOut_nid_no_patch(number, &cleared, sizeof(cleared))) NotImplemented_nid_no_patch(__func__);
    if (allocated) free_nid_postfix(number);
}

int APS5_VABI RSA_set0_key_nid_postfix(LibreSsl3Rsa* rsa, LibreSsl3Bignum* n, LibreSsl3Bignum* e, LibreSsl3Bignum* d) {
    if (!rsa) throw std::invalid_argument("RSA_set0_key requires a key");
    LibreSsl3Rsa guestRsa{};
    if (!GuestCopyIn_nid_no_patch(&guestRsa, rsa, sizeof(guestRsa))) NotImplemented_nid_no_patch(__func__);
    if ((!guestRsa.n && !n) || (!guestRsa.e && !e)) return 0;
    if (!GuestWritable_nid_no_patch(rsa, sizeof(guestRsa))) NotImplemented_nid_no_patch(__func__);
    ValidateReplacement_nid_no_patch(guestRsa.n, n, __func__);
    ValidateReplacement_nid_no_patch(guestRsa.e, e, __func__);
    ValidatePrivateReplacement_nid_no_patch(guestRsa.d, d, __func__);
    Replace_nid_no_patch(guestRsa.n, n);
    Replace_nid_no_patch(guestRsa.e, e);
    ReplacePrivate_nid_no_patch(guestRsa.d, d, __func__);
    if (!GuestCopyOut_nid_no_patch(rsa, &guestRsa, sizeof(guestRsa))) NotImplemented_nid_no_patch(__func__);
    return 1;
}

int APS5_VABI RSA_set0_factors_nid_postfix(LibreSsl3Rsa* rsa, LibreSsl3Bignum* p, LibreSsl3Bignum* q) {
    if (!rsa) throw std::invalid_argument("RSA_set0_factors requires a key");
    LibreSsl3Rsa guestRsa{};
    if (!GuestCopyIn_nid_no_patch(&guestRsa, rsa, sizeof(guestRsa))) NotImplemented_nid_no_patch(__func__);
    if ((!guestRsa.p && !p) || (!guestRsa.q && !q)) return 0;
    if (!GuestWritable_nid_no_patch(rsa, sizeof(guestRsa))) NotImplemented_nid_no_patch(__func__);
    ValidatePrivateReplacement_nid_no_patch(guestRsa.p, p, __func__);
    ValidatePrivateReplacement_nid_no_patch(guestRsa.q, q, __func__);
    ReplacePrivate_nid_no_patch(guestRsa.p, p, __func__);
    ReplacePrivate_nid_no_patch(guestRsa.q, q, __func__);
    if (!GuestCopyOut_nid_no_patch(rsa, &guestRsa, sizeof(guestRsa))) NotImplemented_nid_no_patch(__func__);
    return 1;
}

int APS5_VABI RSA_set0_crt_params_nid_postfix(LibreSsl3Rsa* rsa, LibreSsl3Bignum* dmp1, LibreSsl3Bignum* dmq1, LibreSsl3Bignum* iqmp) {
    if (!rsa) throw std::invalid_argument("RSA_set0_crt_params requires a key");
    LibreSsl3Rsa guestRsa{};
    if (!GuestCopyIn_nid_no_patch(&guestRsa, rsa, sizeof(guestRsa))) NotImplemented_nid_no_patch(__func__);
    if ((!guestRsa.dmp1 && !dmp1) || (!guestRsa.dmq1 && !dmq1) || (!guestRsa.iqmp && !iqmp)) return 0;
    if (!GuestWritable_nid_no_patch(rsa, sizeof(guestRsa))) NotImplemented_nid_no_patch(__func__);
    ValidatePrivateReplacement_nid_no_patch(guestRsa.dmp1, dmp1, __func__);
    ValidatePrivateReplacement_nid_no_patch(guestRsa.dmq1, dmq1, __func__);
    ValidatePrivateReplacement_nid_no_patch(guestRsa.iqmp, iqmp, __func__);
    ReplacePrivate_nid_no_patch(guestRsa.dmp1, dmp1, __func__);
    ReplacePrivate_nid_no_patch(guestRsa.dmq1, dmq1, __func__);
    ReplacePrivate_nid_no_patch(guestRsa.iqmp, iqmp, __func__);
    if (!GuestCopyOut_nid_no_patch(rsa, &guestRsa, sizeof(guestRsa))) NotImplemented_nid_no_patch(__func__);
    return 1;
}

}