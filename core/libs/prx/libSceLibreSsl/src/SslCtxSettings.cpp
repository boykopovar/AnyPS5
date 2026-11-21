#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "prx/libc/include/General.hpp"

static_assert(offsetof(LibreSslCtxInternal, alpnClientProtoList) == 416);
static_assert(offsetof(LibreSslCtxInternal, alpnClientProtoListLen) == 424);
static_assert(offsetof(LibreSslCtxInternal, tlsextSupportedgroupsLength) == 448);
static_assert(offsetof(LibreSslCtxInternal, tlsextSupportedgroups) == 456);

namespace {

constexpr std::array<int, 29> CurveNids = {
    721, 722, 723, 724, 725, 726, 727, 728, 729, 730, 731, 732, 733, 734, 708, 709,
    710, 711, 409, 712, 713, 714, 415, 715, 716, 928, 932, 934, 950,
};

struct NistCurve {
    std::string_view name;
    int nid;
};

constexpr std::array<NistCurve, 15> NistCurves = {{
    {"B-163", 723}, {"B-233", 727}, {"B-283", 730}, {"B-409", 732}, {"B-571", 734},
    {"K-163", 721}, {"K-233", 726}, {"K-283", 729}, {"K-409", 731}, {"K-571", 733},
    {"P-192", 409}, {"P-224", 713}, {"P-256", 415}, {"P-384", 715}, {"P-521", 716},
}};

LibreSslCtxInternal& Internal(LibreSslCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    if (!ctx->internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *ctx->internal;
}

int NistCurveNid(std::string_view name) {
    for (const NistCurve& curve : NistCurves)
        if (curve.name == name) return curve.nid;
    return 0;
}

std::uint16_t CurveId(int nid) {
    for (std::size_t i = 0; i < CurveNids.size(); ++i)
        if (CurveNids[i] == nid) return static_cast<std::uint16_t>(i + 1);
    return 0;
}

}

extern "C" {

int APS5_VABI SSL_CTX_set_alpn_protos_nid_postfix(LibreSslCtx* ctx, const unsigned char* protos, unsigned int length) {
    LibreSslCtxInternal& internal = Internal(ctx, __func__);
    if (!protos && length) throw std::invalid_argument(std::string(__func__) + ": null protocol list");
    free_nid_postfix(internal.alpnClientProtoList);
    internal.alpnClientProtoList = static_cast<unsigned char*>(malloc_nid_postfix(length));
    if (!internal.alpnClientProtoList) return 1;
    if (length) std::memcpy(internal.alpnClientProtoList, protos, length);
    internal.alpnClientProtoListLen = length;
    return 0;
}

int APS5_VABI SSL_CTX_set1_groups_list_nid_postfix(LibreSslCtx* ctx, const char* list) {
    LibreSslCtxInternal& internal = Internal(ctx, __func__);
    if (!list) throw std::invalid_argument(std::string(__func__) + ": null group list");
    std::vector<std::uint16_t> groups;
    const std::string_view names = list;
    std::size_t start = 0;
    try {
        while (true) {
            const std::size_t end = names.find(':', start);
            const std::string name(names.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
            int nid = OBJ_sn2nid_nid_postfix(name.c_str());
            if (!nid) {
                if (LibreSslHasAddedObjectName_nid_no_patch(name.c_str(), true)) NotImplemented_nid_no_patch(__func__);
                nid = NistCurveNid(name);
            }
            if (!nid) return 0;
            const std::uint16_t group = CurveId(nid);
            if (!group) return 0;
            groups.push_back(group);
            if (end == std::string_view::npos) break;
            start = end + 1;
        }
    } catch (const std::bad_alloc&) {
        return 0;
    }
    auto* result = static_cast<std::uint16_t*>(malloc_nid_postfix(groups.size() * sizeof(std::uint16_t)));
    if (!result) return 0;
    std::memcpy(result, groups.data(), groups.size() * sizeof(std::uint16_t));
    free_nid_postfix(internal.tlsextSupportedgroups);
    internal.tlsextSupportedgroups = result;
    internal.tlsextSupportedgroupsLength = groups.size();
    return 1;
}

}
