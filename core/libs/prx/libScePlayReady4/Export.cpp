#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libScePlayReady4/include/InventoryExports.hpp"
#include "prx/libScePlayReady4/include/Api.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::size_t ProfileEntryBytes = 24;
constexpr std::size_t ProfileMaxDepth = 64;
constexpr std::size_t ProfileStackSize = 64;

struct ProfileState {
    std::mutex mutex;
    void* buffer = nullptr;
    std::uint64_t capacity = 0;
    std::uint64_t used = 0;
    bool paused = false;
    std::uint64_t depth = 0;
    std::array<std::uint64_t, ProfileStackSize> stack {};
};

ProfileState& Profile() {
    static ProfileState state;
    return state;
}

std::uint64_t NowMicroseconds() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

void WriteProfileBytes(void* destination, const void* source, const std::size_t bytes, const char* error) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(error);
}

void ReadProfileBytes(void* destination, const void* source, const std::size_t bytes, const char* error) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(error);
}

std::byte* ProfileEntry(void* buffer, const std::uint64_t index) {
    return static_cast<std::byte*>(buffer) + index * ProfileEntryBytes;
}

} // namespace

extern "C" {

int APS5_VABI scePlayReadyApiPlatform() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiClose() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiCreateMediaKeySession() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiCreateMediaKeySystemAccess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiCreateMediaKeys() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiDecryptMultipleOpaque() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiDecryptOpaque() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiDecryptOpaque2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiDestroyMediaKeySession() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiDestroyMediaKeySystemAccess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiDestroyMediaKeys() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiFreeOpaqueDecryptedContent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiGenerateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiGetConfiguration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiGetExpiration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiGetKeySystem() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiGetMediaKeyCount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiGetMediaKeyStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiGetSessionAppContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiGetSessionId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiIsLicenseAckRequired() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiLoad() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiRemove() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiRetrieveOpaqueDecryptedContent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiSetServerCertificate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiSetSessionCallbacks() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCdmiUpdate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseCount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI scePlayReadyProfileClear() {
    auto& profile = Profile();
    std::lock_guard lock(profile.mutex);
    if (profile.buffer == nullptr) return;
    const std::size_t bytes = static_cast<std::size_t>(profile.used * ProfileEntryBytes);
    const std::vector<std::byte> zeros(bytes);
    if (bytes != 0) WriteProfileBytes(profile.buffer, zeros.data(), bytes, "scePlayReadyProfileClear: unwritable profile buffer");
    profile.used = 0;
}

void APS5_VABI scePlayReadyProfileEnter(int first, int second, int third) {
    auto& profile = Profile();
    std::lock_guard lock(profile.mutex);
    if (profile.paused || profile.buffer == nullptr || profile.used >= profile.capacity) return;
    if (profile.depth > ProfileMaxDepth - 1) {
        ++profile.depth;
        return;
    }
    auto* entry = ProfileEntry(profile.buffer, profile.used);
    const std::array<std::byte, 4> header {static_cast<std::byte>(profile.depth + 1), static_cast<std::byte>(first), static_cast<std::byte>(second), static_cast<std::byte>(third)};
    WriteProfileBytes(entry, header.data(), header.size(), "scePlayReadyProfileEnter: unwritable entry header");
    const std::uint64_t timestamp = NowMicroseconds();
    WriteProfileBytes(entry + 8, &timestamp, sizeof(timestamp), "scePlayReadyProfileEnter: unwritable entry timestamp");
    profile.stack[profile.depth] = profile.used;
    ++profile.used;
    ++profile.depth;
}

void APS5_VABI scePlayReadyProfileLeave() {
    auto& profile = Profile();
    std::lock_guard lock(profile.mutex);
    if (profile.paused || profile.buffer == nullptr || profile.depth == 0) return;
    const std::uint64_t previousDepth = profile.depth;
    --profile.depth;
    if (previousDepth > ProfileMaxDepth) return;
    auto* entry = ProfileEntry(profile.buffer, profile.stack[profile.depth]);
    std::uint64_t timestamp = 0;
    ReadProfileBytes(&timestamp, entry + 8, sizeof(timestamp), "scePlayReadyProfileLeave: unreadable entry timestamp");
    const std::uint64_t duration = NowMicroseconds() - timestamp;
    WriteProfileBytes(entry + 16, &duration, sizeof(duration), "scePlayReadyProfileLeave: unwritable entry duration");
}

void APS5_VABI scePlayReadyProfilePause() {
    auto& profile = Profile();
    std::lock_guard lock(profile.mutex);
    profile.paused = true;
}

void APS5_VABI scePlayReadyProfileResume() {
    auto& profile = Profile();
    std::lock_guard lock(profile.mutex);
    profile.paused = false;
}

void APS5_VABI scePlayReadyProfileStart(void* buffer, std::uint64_t entries) {
    auto& profile = Profile();
    std::lock_guard lock(profile.mutex);
    if (buffer == nullptr) return;
    profile.buffer = buffer;
    profile.capacity = entries;
    profile.used = 0;
    profile.paused = false;
}

void APS5_VABI scePlayReadyProfileStop() {
    auto& profile = Profile();
    std::lock_guard lock(profile.mutex);
    profile.buffer = nullptr;
    profile.capacity = 0;
    profile.paused = true;
}


int APS5_VABI scePlayReadyCdmiProcessLicenseAckResponse() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePlayReadyCdmiGetSessionId2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePlayReadyCdmiGenerateLicenseAckRequest() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePlayReadyCdmiGenerateRequest2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePlayReadyDomainJoinProcessResponse2() {
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libScePlayReady4Unknown_2OZeMccOeho_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2OZeMccOeho", libScePlayReady4Unknown_2OZeMccOeho_nid_no_patch);

int APS5_VABI libScePlayReady4Unknown_3fo8fAvHn9A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3fo8fAvHn9A", libScePlayReady4Unknown_3fo8fAvHn9A_nid_no_patch);

int APS5_VABI libScePlayReady4Unknown_HNB_minus_eWVwwng_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HNB-eWVwwng", libScePlayReady4Unknown_HNB_minus_eWVwwng_nid_no_patch);

int APS5_VABI libScePlayReady4Unknown_LL9r9r4cCdQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LL9r9r4cCdQ", libScePlayReady4Unknown_LL9r9r4cCdQ_nid_no_patch);

int APS5_VABI libScePlayReady4Unknown_OqSN3eUfHMY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("OqSN3eUfHMY", libScePlayReady4Unknown_OqSN3eUfHMY_nid_no_patch);

int APS5_VABI libScePlayReady4Unknown_XawoudE54JE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("XawoudE54JE", libScePlayReady4Unknown_XawoudE54JE_nid_no_patch);

int APS5_VABI libScePlayReady4Unknown_asAQ9li6_plus_mo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("asAQ9li6+mo", libScePlayReady4Unknown_asAQ9li6_plus_mo_nid_no_patch);

int APS5_VABI libScePlayReady4Unknown_o2H2P7xoLec_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("o2H2P7xoLec", libScePlayReady4Unknown_o2H2P7xoLec_nid_no_patch);

int APS5_VABI libScePlayReady4Unknown_sK0jAAnbbiI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("sK0jAAnbbiI", libScePlayReady4Unknown_sK0jAAnbbiI_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown__plus_YmG2s3DvCk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+YmG2s3DvCk", libScePlayReady4LegacyUnknown__plus_YmG2s3DvCk_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown__minus_gF0zOwGz9E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-gF0zOwGz9E", libScePlayReady4LegacyUnknown__minus_gF0zOwGz9E_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_04KqHETjgSM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("04KqHETjgSM", libScePlayReady4LegacyUnknown_04KqHETjgSM_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_0VUjhKU8MuM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0VUjhKU8MuM", libScePlayReady4LegacyUnknown_0VUjhKU8MuM_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_0y6EouMfHdc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0y6EouMfHdc", libScePlayReady4LegacyUnknown_0y6EouMfHdc_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_1exu5Xgmk_minus_M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1exu5Xgmk-M", libScePlayReady4LegacyUnknown_1exu5Xgmk_minus_M_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_4DC6ObdGHnM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4DC6ObdGHnM", libScePlayReady4LegacyUnknown_4DC6ObdGHnM_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_4Y2U8T8qLK8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4Y2U8T8qLK8", libScePlayReady4LegacyUnknown_4Y2U8T8qLK8_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_5QciIg5fiEU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5QciIg5fiEU", libScePlayReady4LegacyUnknown_5QciIg5fiEU_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_5sj70SoNZk4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5sj70SoNZk4", libScePlayReady4LegacyUnknown_5sj70SoNZk4_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_7xpxheSYUGc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7xpxheSYUGc", libScePlayReady4LegacyUnknown_7xpxheSYUGc_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_9VmAjxwffnw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9VmAjxwffnw", libScePlayReady4LegacyUnknown_9VmAjxwffnw_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_C7YeAC1DT_minus_8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("C7YeAC1DT-8", libScePlayReady4LegacyUnknown_C7YeAC1DT_minus_8_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_C7ukQ505fng_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("C7ukQ505fng", libScePlayReady4LegacyUnknown_C7ukQ505fng_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_E_plus_PJjXu_plus_ldo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("E+PJjXu+ldo", libScePlayReady4LegacyUnknown_E_plus_PJjXu_plus_ldo_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_FN34vyYimhw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("FN34vyYimhw", libScePlayReady4LegacyUnknown_FN34vyYimhw_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_Fjp5yC5nra4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Fjp5yC5nra4", libScePlayReady4LegacyUnknown_Fjp5yC5nra4_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_HQFs7qCzusw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HQFs7qCzusw", libScePlayReady4LegacyUnknown_HQFs7qCzusw_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_JitxfgFD8fY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JitxfgFD8fY", libScePlayReady4LegacyUnknown_JitxfgFD8fY_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_SDoU52cLUdM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SDoU52cLUdM", libScePlayReady4LegacyUnknown_SDoU52cLUdM_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_SO8bKPwYC9w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SO8bKPwYC9w", libScePlayReady4LegacyUnknown_SO8bKPwYC9w_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_SRJvxiIfRgc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SRJvxiIfRgc", libScePlayReady4LegacyUnknown_SRJvxiIfRgc_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_VIZHbSOKjCY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VIZHbSOKjCY", libScePlayReady4LegacyUnknown_VIZHbSOKjCY_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_X1XN0nxTrB0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("X1XN0nxTrB0", libScePlayReady4LegacyUnknown_X1XN0nxTrB0_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_Z7uUgRiNaUA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Z7uUgRiNaUA", libScePlayReady4LegacyUnknown_Z7uUgRiNaUA_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_aCSc_plus_gFPhTk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aCSc+gFPhTk", libScePlayReady4LegacyUnknown_aCSc_plus_gFPhTk_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_aMlr7G9NwEc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aMlr7G9NwEc", libScePlayReady4LegacyUnknown_aMlr7G9NwEc_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_hGDkBn_plus_wW24_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hGDkBn+wW24", libScePlayReady4LegacyUnknown_hGDkBn_plus_wW24_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_i1ZfbPUs1hY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("i1ZfbPUs1hY", libScePlayReady4LegacyUnknown_i1ZfbPUs1hY_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_mo83jp_plus_TDf4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mo83jp+TDf4", libScePlayReady4LegacyUnknown_mo83jp_plus_TDf4_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_tIqisAWtdsc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tIqisAWtdsc", libScePlayReady4LegacyUnknown_tIqisAWtdsc_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_tdqGjUK6t4A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tdqGjUK6t4A", libScePlayReady4LegacyUnknown_tdqGjUK6t4A_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_uyPYr3M0e88_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uyPYr3M0e88", libScePlayReady4LegacyUnknown_uyPYr3M0e88_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_vItDz4CEogs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("vItDz4CEogs", libScePlayReady4LegacyUnknown_vItDz4CEogs_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_wvdUsNr4oio_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wvdUsNr4oio", libScePlayReady4LegacyUnknown_wvdUsNr4oio_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_y8e4ucs2eZ0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("y8e4ucs2eZ0", libScePlayReady4LegacyUnknown_y8e4ucs2eZ0_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_yaXM3FQJV1E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yaXM3FQJV1E", libScePlayReady4LegacyUnknown_yaXM3FQJV1E_nid_no_patch);

int APS5_VABI libScePlayReady4LegacyUnknown_zGW9LWmm1vI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zGW9LWmm1vI", libScePlayReady4LegacyUnknown_zGW9LWmm1vI_nid_no_patch);

}
