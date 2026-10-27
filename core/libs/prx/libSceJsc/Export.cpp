#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>



namespace {

std::unique_ptr<GuestJSString> Decode(const char* text) {
    auto result = std::make_unique<GuestJSString>();
    const auto* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor) {
        std::uint32_t codepoint = *cursor++;
        unsigned following = 0;
        std::uint32_t minimum = 0;
        if (codepoint >= 0xC2 && codepoint <= 0xDF) {
            codepoint &= 0x1F;
            following = 1;
            minimum = 0x80;
        } else if (codepoint >= 0xE0 && codepoint <= 0xEF) {
            codepoint &= 0x0F;
            following = 2;
            minimum = 0x800;
        } else if (codepoint >= 0xF0 && codepoint <= 0xF4) {
            codepoint &= 7;
            following = 3;
            minimum = 0x10000;
        } else if (codepoint >= 0x80) {
            result->nullString = true;
            break;
        }
        while (following > 0) {
            --following;
            if ((*cursor & 0xC0) != 0x80) {
                result->nullString = true;
                break;
            }
            codepoint = codepoint << 6 | (*cursor++ & 0x3F);
        }
        if (result->nullString || codepoint < minimum || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            result->nullString = true;
            break;
        }
        if (codepoint > 0xFFFF) {
            const auto supplementary = codepoint - 0x10000;
            result->characters.push_back(static_cast<std::uint16_t>(0xD800 + (supplementary >> 10)));
            result->characters.push_back(static_cast<std::uint16_t>(0xDC00 + (supplementary & 0x3FF)));
        } else result->characters.push_back(static_cast<std::uint16_t>(codepoint));
    }
    if (result->nullString) result->characters.clear();
    result->characters.push_back(0);
    return result;
}

bool Equal(const GuestJSString* first, const GuestJSString* second) {
    if (first == second) return true;
    if (!first || !second) return false;
    return first->nullString == second->nullString && first->characters == second->characters;
}

}

extern "C" {

GuestJSString* APS5_VABI JSStringCreateWithCharacters(const std::uint16_t* characters, std::size_t count) {
    if (!characters && count) NotImplemented_nid_no_patch(__func__);
    if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    auto string = std::make_unique<GuestJSString>();
    string->nullString = !characters;
    if (count) string->characters.assign(characters, characters + count);
    string->characters.push_back(0);
    return string.release();
}

GuestJSString* APS5_VABI JSStringCreateWithUTF8CString(const char* text) {
    if (!text) NotImplemented_nid_no_patch(__func__);
    return Decode(text).release();
}

GuestJSString* APS5_VABI JSStringRetain(GuestJSString* string) {
    if (!string) NotImplemented_nid_no_patch(__func__);
    auto references = string->references.load(std::memory_order_relaxed);
    for (;;) {
        if (references == std::numeric_limits<std::size_t>::max()) NotImplemented_nid_no_patch(__func__);
        if (string->references.compare_exchange_weak(references, references + 1, std::memory_order_relaxed)) return string;
    }
}

void APS5_VABI JSStringRelease(GuestJSString* string) {
    if (!string) NotImplemented_nid_no_patch(__func__);
    if (string->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete string;
}

std::size_t APS5_VABI JSStringGetLength(const GuestJSString* string) {
    return string ? string->characters.size() - 1 : 0;
}

const std::uint16_t* APS5_VABI JSStringGetCharactersPtr(const GuestJSString* string) {
    return string && !string->nullString ? string->characters.data() : nullptr;
}

std::size_t APS5_VABI JSStringGetMaximumUTF8CStringSize(const GuestJSString* string) {
    if (!string) NotImplemented_nid_no_patch(__func__);
    const auto length = string->characters.size() - 1;
    if (length > (std::numeric_limits<std::size_t>::max() - 1) / 3) NotImplemented_nid_no_patch(__func__);
    return length * 3 + 1;
}

std::size_t APS5_VABI JSStringGetUTF8CString(const GuestJSString* string, char* buffer, std::size_t bufferSize) {
    if (!string || !buffer || !bufferSize) return 0;
    std::size_t written = 0;
    const auto count = string->characters.size() - 1;
    for (std::size_t index = 0; index < count; ++index) {
        std::uint32_t codepoint = string->characters[index];
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            if (index + 1 == count) break;
            const auto low = string->characters[index + 1];
            if (low < 0xDC00 || low > 0xDFFF) break;
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + low - 0xDC00;
            ++index;
        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) break;
        const std::size_t bytes = codepoint < 0x80 ? 1 : codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
        if (bytes > bufferSize - 1 - written) break;
        if (bytes == 1) buffer[written++] = static_cast<char>(codepoint);
        else {
            const unsigned prefix = bytes == 2 ? 0xC0 : bytes == 3 ? 0xE0 : 0xF0;
            buffer[written++] = static_cast<char>(prefix | (codepoint >> (6 * (bytes - 1))));
            for (auto remaining = bytes - 1; remaining > 0; --remaining) {
                buffer[written++] = static_cast<char>(0x80 | ((codepoint >> (6 * (remaining - 1))) & 0x3F));
            }
        }
    }
    buffer[written] = '\0';
    return written + 1;
}

bool APS5_VABI JSStringIsEqual(const GuestJSString* first, const GuestJSString* second) {
    return Equal(first, second);
}

bool APS5_VABI JSStringIsEqualToUTF8CString(const GuestJSString* string, const char* text) {
    if (!text) NotImplemented_nid_no_patch(__func__);
    const auto other = Decode(text);
    return Equal(string, other.get());
}

int APS5_VABI sceJscUnknown__0qGCrVTCcs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+0qGCrVTCcs", sceJscUnknown__0qGCrVTCcs_nid_no_patch);

int APS5_VABI sceJscUnknown__14hrhyqXkw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+14hrhyqXkw", sceJscUnknown__14hrhyqXkw_nid_no_patch);

int APS5_VABI sceJscUnknown__3jAVQxPMOc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+3jAVQxPMOc", sceJscUnknown__3jAVQxPMOc_nid_no_patch);

int APS5_VABI sceJscUnknown__i_vXbqYQMo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+i+vXbqYQMo", sceJscUnknown__i_vXbqYQMo_nid_no_patch);

int APS5_VABI sceJscUnknown__jBdpELLGNA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+jBdpELLGNA", sceJscUnknown__jBdpELLGNA_nid_no_patch);

int APS5_VABI sceJscUnknown__tn6ODC5FvQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+tn6ODC5FvQ", sceJscUnknown__tn6ODC5FvQ_nid_no_patch);

int APS5_VABI sceJscUnknown__tuS_migTvI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+tuS+migTvI", sceJscUnknown__tuS_migTvI_nid_no_patch);

int APS5_VABI sceJscUnknown__uKktfh_zD0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+uKktfh+zD0", sceJscUnknown__uKktfh_zD0_nid_no_patch);

int APS5_VABI sceJscUnknown__uKqBYnAudU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+uKqBYnAudU", sceJscUnknown__uKqBYnAudU_nid_no_patch);

int APS5_VABI sceJscUnknown__ugzaqpDezk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+ugzaqpDezk", sceJscUnknown__ugzaqpDezk_nid_no_patch);

int APS5_VABI sceJscUnknown__wd7ITlDmEM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+wd7ITlDmEM", sceJscUnknown__wd7ITlDmEM_nid_no_patch);

int APS5_VABI sceJscUnknown__ww_pcpOFNc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+ww+pcpOFNc", sceJscUnknown__ww_pcpOFNc_nid_no_patch);

int APS5_VABI sceJscUnknown__x_ywEbOrJo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+x-ywEbOrJo", sceJscUnknown__x_ywEbOrJo_nid_no_patch);

int APS5_VABI sceJscUnknown__yRvWsESIkM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+yRvWsESIkM", sceJscUnknown__yRvWsESIkM_nid_no_patch);

int APS5_VABI sceJscUnknown__ypzWWOJMwI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+ypzWWOJMwI", sceJscUnknown__ypzWWOJMwI_nid_no_patch);

int APS5_VABI sceJscUnknown__z3kfOo91Pg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+z3kfOo91Pg", sceJscUnknown__z3kfOo91Pg_nid_no_patch);

int APS5_VABI sceJscUnknown__zL3nHNv5GU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+zL3nHNv5GU", sceJscUnknown__zL3nHNv5GU_nid_no_patch);

int APS5_VABI sceJscUnknown__zPRztSZLTg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+zPRztSZLTg", sceJscUnknown__zPRztSZLTg_nid_no_patch);

int APS5_VABI sceJscUnknown__09a6Ceb6Y0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-09a6Ceb6Y0", sceJscUnknown__09a6Ceb6Y0_nid_no_patch);

int APS5_VABI sceJscUnknown__1Qgg3W9FVM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-1Qgg3W9FVM", sceJscUnknown__1Qgg3W9FVM_nid_no_patch);

int APS5_VABI sceJscUnknown__3x7_wyZpEU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-3x7+wyZpEU", sceJscUnknown__3x7_wyZpEU_nid_no_patch);

int APS5_VABI sceJscUnknown__4M0K8nGdfk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-4M0K8nGdfk", sceJscUnknown__4M0K8nGdfk_nid_no_patch);

int APS5_VABI sceJscUnknown__4jsoWj40_g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-4jsoWj40-g", sceJscUnknown__4jsoWj40_g_nid_no_patch);

int APS5_VABI sceJscUnknown__4sVkp5_H_M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-4sVkp5-H-M", sceJscUnknown__4sVkp5_H_M_nid_no_patch);

int APS5_VABI sceJscUnknown__5budpPgP8o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-5budpPgP8o", sceJscUnknown__5budpPgP8o_nid_no_patch);

int APS5_VABI sceJscUnknown__69D4t3LLHE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-69D4t3LLHE", sceJscUnknown__69D4t3LLHE_nid_no_patch);

int APS5_VABI sceJscUnknown__7g_5LVkStM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-7g-5LVkStM", sceJscUnknown__7g_5LVkStM_nid_no_patch);

int APS5_VABI sceJscUnknown__7u_WG3kX3U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-7u-WG3kX3U", sceJscUnknown__7u_WG3kX3U_nid_no_patch);

int APS5_VABI sceJscUnknown__9SPyZbLwdc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-9SPyZbLwdc", sceJscUnknown__9SPyZbLwdc_nid_no_patch);

int APS5_VABI sceJscUnknown__F3z0DF32Fw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-F3z0DF32Fw", sceJscUnknown__F3z0DF32Fw_nid_no_patch);

int APS5_VABI sceJscUnknown__G1ivTvs2mU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-G1ivTvs2mU", sceJscUnknown__G1ivTvs2mU_nid_no_patch);

int APS5_VABI sceJscUnknown__HYD6ysDUts_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-HYD6ysDUts", sceJscUnknown__HYD6ysDUts_nid_no_patch);

int APS5_VABI sceJscUnknown__KKlTIIqZrk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-KKlTIIqZrk", sceJscUnknown__KKlTIIqZrk_nid_no_patch);

int APS5_VABI sceJscUnknown__LSSffiQqPg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-LSSffiQqPg", sceJscUnknown__LSSffiQqPg_nid_no_patch);

int APS5_VABI sceJscUnknown__MSv8GTlzww_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-MSv8GTlzww", sceJscUnknown__MSv8GTlzww_nid_no_patch);

int APS5_VABI sceJscUnknown__MicizMIiag_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-MicizMIiag", sceJscUnknown__MicizMIiag_nid_no_patch);

int APS5_VABI sceJscUnknown__MnzCChBb5c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-MnzCChBb5c", sceJscUnknown__MnzCChBb5c_nid_no_patch);

int APS5_VABI sceJscUnknown__N79h7XJG8A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-N79h7XJG8A", sceJscUnknown__N79h7XJG8A_nid_no_patch);

int APS5_VABI sceJscUnknown__NAYLFFi9n8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-NAYLFFi9n8", sceJscUnknown__NAYLFFi9n8_nid_no_patch);

int APS5_VABI sceJscUnknown__NOpB57jCtM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-NOpB57jCtM", sceJscUnknown__NOpB57jCtM_nid_no_patch);

int APS5_VABI sceJscUnknown__Ru0nzM2cJ4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-Ru0nzM2cJ4", sceJscUnknown__Ru0nzM2cJ4_nid_no_patch);

int APS5_VABI sceJscUnknown__SS6hQt_3dQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-SS6hQt+3dQ", sceJscUnknown__SS6hQt_3dQ_nid_no_patch);

int APS5_VABI sceJscUnknown__T_6YbvQ3W4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-T+6YbvQ3W4", sceJscUnknown__T_6YbvQ3W4_nid_no_patch);

int APS5_VABI sceJscUnknown__UWXejaXycI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-UWXejaXycI", sceJscUnknown__UWXejaXycI_nid_no_patch);

int APS5_VABI sceJscUnknown__W0cyz1wnug_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-W0cyz1wnug", sceJscUnknown__W0cyz1wnug_nid_no_patch);

int APS5_VABI sceJscUnknown__W6XjwwK8Bo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-W6XjwwK8Bo", sceJscUnknown__W6XjwwK8Bo_nid_no_patch);

int APS5_VABI sceJscUnknown__W9fbd6kB_8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-W9fbd6kB-8", sceJscUnknown__W9fbd6kB_8_nid_no_patch);

int APS5_VABI sceJscUnknown__WAFtiljCdo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-WAFtiljCdo", sceJscUnknown__WAFtiljCdo_nid_no_patch);

int APS5_VABI sceJscUnknown__WzxDM_yTwY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-WzxDM-yTwY", sceJscUnknown__WzxDM_yTwY_nid_no_patch);

int APS5_VABI sceJscUnknown__XCn1Pwn7NU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-XCn1Pwn7NU", sceJscUnknown__XCn1Pwn7NU_nid_no_patch);

int APS5_VABI sceJscUnknown__XHaGqZr_PE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-XHaGqZr+PE", sceJscUnknown__XHaGqZr_PE_nid_no_patch);

int APS5_VABI sceJscUnknown__Xia6SzTPnQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-Xia6SzTPnQ", sceJscUnknown__Xia6SzTPnQ_nid_no_patch);

int APS5_VABI sceJscUnknown__Y_WZBvAu3M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-Y-WZBvAu3M", sceJscUnknown__Y_WZBvAu3M_nid_no_patch);

int APS5_VABI sceJscUnknown__YDX6TPLnro_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-YDX6TPLnro", sceJscUnknown__YDX6TPLnro_nid_no_patch);

int APS5_VABI sceJscUnknown__am9z4xyweE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-am9z4xyweE", sceJscUnknown__am9z4xyweE_nid_no_patch);

int APS5_VABI sceJscUnknown__e_i2gtmhdc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-e-i2gtmhdc", sceJscUnknown__e_i2gtmhdc_nid_no_patch);

int APS5_VABI sceJscUnknown__eUZ8xel7b0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-eUZ8xel7b0", sceJscUnknown__eUZ8xel7b0_nid_no_patch);

int APS5_VABI sceJscUnknown__fAFmad5Tnk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-fAFmad5Tnk", sceJscUnknown__fAFmad5Tnk_nid_no_patch);

int APS5_VABI sceJscUnknown__fKQRtsYUSc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-fKQRtsYUSc", sceJscUnknown__fKQRtsYUSc_nid_no_patch);

int APS5_VABI sceJscUnknown__fwHkWlm_wc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-fwHkWlm-wc", sceJscUnknown__fwHkWlm_wc_nid_no_patch);

int APS5_VABI sceJscUnknown__g6RLhTGOeg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-g6RLhTGOeg", sceJscUnknown__g6RLhTGOeg_nid_no_patch);

int APS5_VABI sceJscUnknown__gE3O_NCHS0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-gE3O+NCHS0", sceJscUnknown__gE3O_NCHS0_nid_no_patch);

int APS5_VABI sceJscUnknown__hdGlIfGY5g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-hdGlIfGY5g", sceJscUnknown__hdGlIfGY5g_nid_no_patch);

int APS5_VABI sceJscUnknown__hk9wmuusRU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-hk9wmuusRU", sceJscUnknown__hk9wmuusRU_nid_no_patch);

int APS5_VABI sceJscUnknown__md9AgyWZkw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-md9AgyWZkw", sceJscUnknown__md9AgyWZkw_nid_no_patch);

int APS5_VABI sceJscUnknown__n_GMwKBaD0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-n+GMwKBaD0", sceJscUnknown__n_GMwKBaD0_nid_no_patch);

int APS5_VABI sceJscUnknown__n_x6ZBExPE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-n+x6ZBExPE", sceJscUnknown__n_x6ZBExPE_nid_no_patch);

int APS5_VABI sceJscUnknown__oW_aAeCPs4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-oW-aAeCPs4", sceJscUnknown__oW_aAeCPs4_nid_no_patch);

int APS5_VABI sceJscUnknown__qMXl9DNcac_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-qMXl9DNcac", sceJscUnknown__qMXl9DNcac_nid_no_patch);

int APS5_VABI sceJscUnknown__qYqo3TC_8I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-qYqo3TC+8I", sceJscUnknown__qYqo3TC_8I_nid_no_patch);

int APS5_VABI sceJscUnknown__rEvUxwc4xY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-rEvUxwc4xY", sceJscUnknown__rEvUxwc4xY_nid_no_patch);

int APS5_VABI sceJscUnknown__rtzyTjX8AM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-rtzyTjX8AM", sceJscUnknown__rtzyTjX8AM_nid_no_patch);

int APS5_VABI sceJscUnknown__s4y23rdI0Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-s4y23rdI0Y", sceJscUnknown__s4y23rdI0Y_nid_no_patch);

int APS5_VABI sceJscUnknown__sQ3aB_rrVw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-sQ3aB-rrVw", sceJscUnknown__sQ3aB_rrVw_nid_no_patch);

int APS5_VABI sceJscUnknown__sjd__qKqaU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-sjd++qKqaU", sceJscUnknown__sjd__qKqaU_nid_no_patch);

int APS5_VABI sceJscUnknown__tPqziuAbt0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-tPqziuAbt0", sceJscUnknown__tPqziuAbt0_nid_no_patch);

int APS5_VABI sceJscUnknown__uLGa2Nmdg8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-uLGa2Nmdg8", sceJscUnknown__uLGa2Nmdg8_nid_no_patch);

int APS5_VABI sceJscUnknown__xFvxRPfTng_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-xFvxRPfTng", sceJscUnknown__xFvxRPfTng_nid_no_patch);

int APS5_VABI sceJscUnknown__zEvjr6MBaE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-zEvjr6MBaE", sceJscUnknown__zEvjr6MBaE_nid_no_patch);

int APS5_VABI sceJscUnknown__zr4F1xDgGA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-zr4F1xDgGA", sceJscUnknown__zr4F1xDgGA_nid_no_patch);

int APS5_VABI sceJscUnknown_0_mWELhv81k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0-mWELhv81k", sceJscUnknown_0_mWELhv81k_nid_no_patch);

int APS5_VABI sceJscUnknown_01IKkgXrFxs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("01IKkgXrFxs", sceJscUnknown_01IKkgXrFxs_nid_no_patch);

int APS5_VABI sceJscUnknown_01nXitlGn8k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("01nXitlGn8k", sceJscUnknown_01nXitlGn8k_nid_no_patch);

int APS5_VABI sceJscUnknown_04XvCW_DZys_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("04XvCW-DZys", sceJscUnknown_04XvCW_DZys_nid_no_patch);

int APS5_VABI sceJscUnknown_0AEWR2mf0dA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0AEWR2mf0dA", sceJscUnknown_0AEWR2mf0dA_nid_no_patch);

int APS5_VABI sceJscUnknown_0BNiqtvNqTA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0BNiqtvNqTA", sceJscUnknown_0BNiqtvNqTA_nid_no_patch);

int APS5_VABI sceJscUnknown_0BU_NMgvLhU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0BU+NMgvLhU", sceJscUnknown_0BU_NMgvLhU_nid_no_patch);

int APS5_VABI sceJscUnknown_0BtcoCh5nbg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0BtcoCh5nbg", sceJscUnknown_0BtcoCh5nbg_nid_no_patch);

int APS5_VABI sceJscUnknown_0CB7Dz8pEuA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0CB7Dz8pEuA", sceJscUnknown_0CB7Dz8pEuA_nid_no_patch);

int APS5_VABI sceJscUnknown_0CGMc99sJQc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0CGMc99sJQc", sceJscUnknown_0CGMc99sJQc_nid_no_patch);

int APS5_VABI sceJscUnknown_0CnwqNph95c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0CnwqNph95c", sceJscUnknown_0CnwqNph95c_nid_no_patch);

int APS5_VABI sceJscUnknown_0Cu5IQZsCWA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Cu5IQZsCWA", sceJscUnknown_0Cu5IQZsCWA_nid_no_patch);

int APS5_VABI sceJscUnknown_0DY9Snf59Wk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0DY9Snf59Wk", sceJscUnknown_0DY9Snf59Wk_nid_no_patch);

int APS5_VABI sceJscUnknown_0DkchTKnu8Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0DkchTKnu8Y", sceJscUnknown_0DkchTKnu8Y_nid_no_patch);

int APS5_VABI sceJscUnknown_0Eqn40ngE5M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Eqn40ngE5M", sceJscUnknown_0Eqn40ngE5M_nid_no_patch);

int APS5_VABI sceJscUnknown_0FlXpxjRjOA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0FlXpxjRjOA", sceJscUnknown_0FlXpxjRjOA_nid_no_patch);

int APS5_VABI sceJscUnknown_0FmMcDDxnAY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0FmMcDDxnAY", sceJscUnknown_0FmMcDDxnAY_nid_no_patch);

int APS5_VABI sceJscUnknown_0GxlvzlC15A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0GxlvzlC15A", sceJscUnknown_0GxlvzlC15A_nid_no_patch);

int APS5_VABI sceJscUnknown_0HkylFfb_Ig_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0HkylFfb+Ig", sceJscUnknown_0HkylFfb_Ig_nid_no_patch);

int APS5_VABI sceJscUnknown_0I6apGOYhtM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0I6apGOYhtM", sceJscUnknown_0I6apGOYhtM_nid_no_patch);

int APS5_VABI sceJscUnknown_0ICMatq6_6g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0ICMatq6+6g", sceJscUnknown_0ICMatq6_6g_nid_no_patch);

int APS5_VABI sceJscUnknown_0KClQgT2ckA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0KClQgT2ckA", sceJscUnknown_0KClQgT2ckA_nid_no_patch);

int APS5_VABI sceJscUnknown_0KqxIyH6DiE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0KqxIyH6DiE", sceJscUnknown_0KqxIyH6DiE_nid_no_patch);

int APS5_VABI sceJscUnknown_0KsRLd9dTVU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0KsRLd9dTVU", sceJscUnknown_0KsRLd9dTVU_nid_no_patch);

int APS5_VABI sceJscUnknown_0KynCQXu9XQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0KynCQXu9XQ", sceJscUnknown_0KynCQXu9XQ_nid_no_patch);

int APS5_VABI sceJscUnknown_0LSRs8rB_A8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0LSRs8rB-A8", sceJscUnknown_0LSRs8rB_A8_nid_no_patch);

int APS5_VABI sceJscUnknown_0MHM5pZ54E4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0MHM5pZ54E4", sceJscUnknown_0MHM5pZ54E4_nid_no_patch);

int APS5_VABI sceJscUnknown_0Mo4EsA5wVo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Mo4EsA5wVo", sceJscUnknown_0Mo4EsA5wVo_nid_no_patch);

int APS5_VABI sceJscUnknown_0MwjtlWeGGg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0MwjtlWeGGg", sceJscUnknown_0MwjtlWeGGg_nid_no_patch);

int APS5_VABI sceJscUnknown_0OI3DXKmR9w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0OI3DXKmR9w", sceJscUnknown_0OI3DXKmR9w_nid_no_patch);

int APS5_VABI sceJscUnknown_0OLbLK3fZlE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0OLbLK3fZlE", sceJscUnknown_0OLbLK3fZlE_nid_no_patch);

int APS5_VABI sceJscUnknown_0QVx_6Ffzjc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0QVx-6Ffzjc", sceJscUnknown_0QVx_6Ffzjc_nid_no_patch);

int APS5_VABI sceJscUnknown_0QbSxsg9i1M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0QbSxsg9i1M", sceJscUnknown_0QbSxsg9i1M_nid_no_patch);

int APS5_VABI sceJscUnknown_0QjiF9xEFQA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0QjiF9xEFQA", sceJscUnknown_0QjiF9xEFQA_nid_no_patch);

int APS5_VABI sceJscUnknown_0R0tdvM7fFs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0R0tdvM7fFs", sceJscUnknown_0R0tdvM7fFs_nid_no_patch);

int APS5_VABI sceJscUnknown_0RqgTKckJBs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0RqgTKckJBs", sceJscUnknown_0RqgTKckJBs_nid_no_patch);

int APS5_VABI sceJscUnknown_0SoDdOYaQE4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0SoDdOYaQE4", sceJscUnknown_0SoDdOYaQE4_nid_no_patch);

int APS5_VABI sceJscUnknown_0Uk_3H5HdEo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Uk-3H5HdEo", sceJscUnknown_0Uk_3H5HdEo_nid_no_patch);

int APS5_VABI sceJscUnknown_0Ukd6tvlVhs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Ukd6tvlVhs", sceJscUnknown_0Ukd6tvlVhs_nid_no_patch);

int APS5_VABI sceJscUnknown_0W_mECnGicE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0W-mECnGicE", sceJscUnknown_0W_mECnGicE_nid_no_patch);

int APS5_VABI sceJscUnknown_0XH9Pu6dPdk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0XH9Pu6dPdk", sceJscUnknown_0XH9Pu6dPdk_nid_no_patch);

int APS5_VABI sceJscUnknown_0Y_ebXGv50I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Y-ebXGv50I", sceJscUnknown_0Y_ebXGv50I_nid_no_patch);

int APS5_VABI sceJscUnknown_0YfwqGopI4c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0YfwqGopI4c", sceJscUnknown_0YfwqGopI4c_nid_no_patch);

int APS5_VABI sceJscUnknown_0Za34L_PPDg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Za34L-PPDg", sceJscUnknown_0Za34L_PPDg_nid_no_patch);

int APS5_VABI sceJscUnknown_0Zix0_Zcx2k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Zix0+Zcx2k", sceJscUnknown_0Zix0_Zcx2k_nid_no_patch);

int APS5_VABI sceJscUnknown_0bI_VPZXmzg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0bI-VPZXmzg", sceJscUnknown_0bI_VPZXmzg_nid_no_patch);

int APS5_VABI sceJscUnknown_0bq1LixdHAY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0bq1LixdHAY", sceJscUnknown_0bq1LixdHAY_nid_no_patch);

int APS5_VABI sceJscUnknown_0c_07mr9xP8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0c-07mr9xP8", sceJscUnknown_0c_07mr9xP8_nid_no_patch);

int APS5_VABI sceJscUnknown_0cIPqIrPhvg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0cIPqIrPhvg", sceJscUnknown_0cIPqIrPhvg_nid_no_patch);

int APS5_VABI sceJscUnknown_0dTfcPHH3dg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0dTfcPHH3dg", sceJscUnknown_0dTfcPHH3dg_nid_no_patch);

int APS5_VABI sceJscUnknown_0earDv9xPN8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0earDv9xPN8", sceJscUnknown_0earDv9xPN8_nid_no_patch);

int APS5_VABI sceJscUnknown_0f06o3_XgV0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0f06o3-XgV0", sceJscUnknown_0f06o3_XgV0_nid_no_patch);

int APS5_VABI sceJscUnknown_0g_jtYCdtQw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0g+jtYCdtQw", sceJscUnknown_0g_jtYCdtQw_nid_no_patch);

int APS5_VABI sceJscUnknown_0g66of16pvY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0g66of16pvY", sceJscUnknown_0g66of16pvY_nid_no_patch);

int APS5_VABI sceJscUnknown_0gQImk3C0sA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0gQImk3C0sA", sceJscUnknown_0gQImk3C0sA_nid_no_patch);

int APS5_VABI sceJscUnknown_0ggQR1NjDCY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0ggQR1NjDCY", sceJscUnknown_0ggQR1NjDCY_nid_no_patch);

int APS5_VABI sceJscUnknown_0ixkCjQDH2I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0ixkCjQDH2I", sceJscUnknown_0ixkCjQDH2I_nid_no_patch);

int APS5_VABI sceJscUnknown_0lR2YBKJMIE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0lR2YBKJMIE", sceJscUnknown_0lR2YBKJMIE_nid_no_patch);

int APS5_VABI sceJscUnknown_0nI81nsrl_A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0nI81nsrl+A", sceJscUnknown_0nI81nsrl_A_nid_no_patch);

int APS5_VABI sceJscUnknown_0pp8T8yX1BU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0pp8T8yX1BU", sceJscUnknown_0pp8T8yX1BU_nid_no_patch);

int APS5_VABI sceJscUnknown_0q3Pf6miAts_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0q3Pf6miAts", sceJscUnknown_0q3Pf6miAts_nid_no_patch);

int APS5_VABI sceJscUnknown_0qmAOS00O2g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0qmAOS00O2g", sceJscUnknown_0qmAOS00O2g_nid_no_patch);

int APS5_VABI sceJscUnknown_0qr5jdWMC3E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0qr5jdWMC3E", sceJscUnknown_0qr5jdWMC3E_nid_no_patch);

int APS5_VABI sceJscUnknown_0rj2akkxaO0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0rj2akkxaO0", sceJscUnknown_0rj2akkxaO0_nid_no_patch);

int APS5_VABI sceJscUnknown_0ro1gFmJat8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0ro1gFmJat8", sceJscUnknown_0ro1gFmJat8_nid_no_patch);

int APS5_VABI sceJscUnknown_0v8_cEQL93A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0v8-cEQL93A", sceJscUnknown_0v8_cEQL93A_nid_no_patch);

int APS5_VABI sceJscUnknown_0vAcIjIOiEE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0vAcIjIOiEE", sceJscUnknown_0vAcIjIOiEE_nid_no_patch);

int APS5_VABI sceJscUnknown_0wxHtqo7dew_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0wxHtqo7dew", sceJscUnknown_0wxHtqo7dew_nid_no_patch);

int APS5_VABI sceJscUnknown_0wzoLEfYeJo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0wzoLEfYeJo", sceJscUnknown_0wzoLEfYeJo_nid_no_patch);

int APS5_VABI sceJscUnknown_0xH4JecjF2Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0xH4JecjF2Q", sceJscUnknown_0xH4JecjF2Q_nid_no_patch);

}
