#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct NamedGroup {
    const char* name;
    std::uint16_t id;
};

constexpr NamedGroup NamedGroups[] = {
    {"sect163k1", 1}, {"sect163r1", 2}, {"sect163r2", 3}, {"sect193r1", 4}, {"sect193r2", 5},
    {"sect233k1", 6}, {"sect233r1", 7}, {"sect239k1", 8}, {"sect283k1", 9}, {"sect283r1", 10},
    {"sect409k1", 11}, {"sect409r1", 12}, {"sect571k1", 13}, {"sect571r1", 14}, {"secp160k1", 15},
    {"secp160r1", 16}, {"secp160r2", 17}, {"secp192k1", 18}, {"prime192v1", 19}, {"secp224k1", 20},
    {"secp224r1", 21}, {"secp256k1", 22}, {"prime256v1", 23}, {"secp384r1", 24}, {"secp521r1", 25},
    {"brainpoolP256r1", 26}, {"brainpoolP384r1", 27}, {"brainpoolP512r1", 28}, {"X25519", 29},
};

struct NameAlias {
    const char* name;
    const char* canonical;
};

constexpr NameAlias NameAliases[] = {
    {"secp192r1", "prime192v1"}, {"secp256r1", "prime256v1"},
    {"P-192", "prime192v1"}, {"P-224", "secp224r1"}, {"P-256", "prime256v1"},
    {"P-384", "secp384r1"}, {"P-521", "secp521r1"},
};

constexpr std::size_t MaximumGroupsLength = 1 << 20;

std::uint16_t GroupId(const std::string& token) {
    const char* canonical = token.c_str();
    for (const NameAlias& alias : NameAliases) {
        if (token == alias.name) {
            canonical = alias.canonical;
            break;
        }
    }
    for (const NamedGroup& group : NamedGroups) {
        if (std::strcmp(canonical, group.name) == 0) return group.id;
    }
    return 0;
}

}

extern "C" {

int APS5_VABI SSL_CTX_set1_groups_list_nid_postfix(LibreSslCtx* ctx, const char* groups) {
    if (!ctx) APS5_INVALID_ARG_EX;
    if (!ctx->internal) throw std::invalid_argument("SSL_CTX_set1_groups_list: null context internals");
    if (!groups) throw std::invalid_argument("SSL_CTX_set1_groups_list: null group list");
    std::string text;
    if (CopyGuestString_nid_no_patch(groups, MaximumGroupsLength, text) != 0)
        NotImplemented_nid_no_patch("SSL_CTX_set1_groups_list: unreadable or unterminated group list");
    std::vector<std::uint16_t> ids;
    std::size_t offset = 0;
    while (offset <= text.size()) {
        const std::size_t end = text.find(':', offset);
        const std::string token = text.substr(offset, end == std::string::npos ? std::string::npos : end - offset);
        const std::uint16_t id = GroupId(token);
        if (id == 0) return 0;
        ids.push_back(id);
        if (end == std::string::npos) break;
        offset = end + 1;
    }
    auto* list = static_cast<std::uint16_t*>(malloc_nid_postfix(ids.size() * sizeof(std::uint16_t)));
    if (!list) return 0;
    std::memcpy(list, ids.data(), ids.size() * sizeof(std::uint16_t));
    LibreSslCtxInternal& internal = *ctx->internal;
    free_nid_postfix(internal.tlsextSupportedgroups);
    internal.tlsextSupportedgroups = list;
    internal.tlsextSupportedgroupsLength = ids.size();
    return 1;
}

}
