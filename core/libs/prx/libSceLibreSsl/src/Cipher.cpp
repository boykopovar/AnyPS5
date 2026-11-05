#include <cstddef>
#include <cstdint>
#include <cstring>
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"

namespace {

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
    if (algorithmBits) {
        std::int32_t bits;
        std::memcpy(&bits, bytes + offsetof(Cipher, algorithmBits), sizeof(bits));
        *algorithmBits = bits;
    }
    std::int32_t strength;
    std::memcpy(&strength, bytes + offsetof(Cipher, strengthBits), sizeof(strength));
    return strength;
}

const char* APS5_VABI SSL_CIPHER_get_name_nid_postfix(const void* cipher) {
    if (!cipher) return "(NONE)";
    const char* name;
    std::memcpy(&name, static_cast<const std::byte*>(cipher) + offsetof(Cipher, name), sizeof(name));
    return name;
}

char* APS5_VABI SSL_CIPHER_get_version_nid_postfix(const void* cipher) {
    if (!cipher) return const_cast<char*>("(NONE)");
    std::uint64_t id;
    std::memcpy(&id, static_cast<const std::byte*>(cipher) + offsetof(Cipher, id), sizeof(id));
    return const_cast<char*>((id >> 24) == 3 ? "TLSv1/SSLv3" : "unknown");
}

}
