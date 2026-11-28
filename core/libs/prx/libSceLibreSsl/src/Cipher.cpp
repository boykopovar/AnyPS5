#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"

namespace {

template <typename T>
void CopyGuestInPreservingErrno(const void* guestAddress, T& hostValue, const char* caller) {
    const bool ok = GuestCopyIn_nid_no_patch(&hostValue, guestAddress, sizeof(T));
    if (!ok) NotImplemented_nid_no_patch(caller);
}

template <typename T>
void CopyGuestOutPreservingErrno(void* guestAddress, const T& hostValue, const char* caller) {
    const bool ok = GuestCopyOut_nid_no_patch(guestAddress, &hostValue, sizeof(T));
    if (!ok) NotImplemented_nid_no_patch(caller);
}

struct Cipher {
    std::int32_t valid;
    const char* name;
    std::uint64_t id;
    std::uint64_t keyExchange;
    std::uint64_t authentication;
    std::uint64_t encryption;
    std::uint64_t mac;
    std::uint64_t protocol;
    std::uint64_t strengthFlags;
    std::uint64_t extraFlags;
    std::int32_t strengthBits;
    std::int32_t algorithmBits;
};

static_assert(sizeof(Cipher) == 88);
static_assert(offsetof(Cipher, name) == 8);
static_assert(offsetof(Cipher, id) == 16);
static_assert(offsetof(Cipher, strengthBits) == 80);
static_assert(offsetof(Cipher, algorithmBits) == 84);

} // namespace

extern "C" {

int APS5_VABI SSL_CIPHER_get_bits_nid_postfix(const void* cipher, std::int32_t* algorithmBits) {
    if (!cipher) return 0;
    const auto* bytes = static_cast<const std::byte*>(cipher);
    std::int32_t strength = 0;
    CopyGuestInPreservingErrno(bytes + offsetof(Cipher, strengthBits), strength, __func__);
    if (algorithmBits) {
        std::int32_t bits = 0;
        CopyGuestInPreservingErrno(bytes + offsetof(Cipher, algorithmBits), bits, __func__);
        CopyGuestOutPreservingErrno(algorithmBits, bits, __func__);
        CopyGuestInPreservingErrno(bytes + offsetof(Cipher, strengthBits), strength, __func__);
    }
    return strength;
}

const char* APS5_VABI SSL_CIPHER_get_name_nid_postfix(const void* cipher) {
    if (!cipher) return "(NONE)";
    const char* name = nullptr;
    CopyGuestInPreservingErrno(static_cast<const std::byte*>(cipher) + offsetof(Cipher, name), name, __func__);
    return name;
}

char* APS5_VABI SSL_CIPHER_get_version_nid_postfix(const void* cipher) {
    if (!cipher) return const_cast<char*>("(NONE)");
    std::uint64_t id = 0;
    CopyGuestInPreservingErrno(static_cast<const std::byte*>(cipher) + offsetof(Cipher, id), id, __func__);
    return const_cast<char*>((id >> 24) == 3 ? "TLSv1/SSLv3" : "unknown");
}

}
