extern "C" {
#include "prx/libSceAppContent/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/RemoveTree.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libSceNpEntitlementAccess/EntitlementsIni.hpp"
#include <algorithm>
#include <cerrno>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

static constexpr int SCE_APP_CONTENT_ERROR_NOT_INITIALIZED = static_cast<int>(0x80D90001);
static constexpr int SCE_APP_CONTENT_ERROR_PARAMETER = static_cast<int>(0x80D90002);
static constexpr int SCE_APP_CONTENT_ERROR_BUSY = static_cast<int>(0x80D90003);
static constexpr int SCE_APP_CONTENT_ERROR_NOT_MOUNTED = static_cast<int>(0x80D90004);
static constexpr int SCE_APP_CONTENT_ERROR_NOT_FOUND = static_cast<int>(0x80D90005);
static constexpr int SCE_APP_CONTENT_ERROR_DRM_NO_ENTITLEMENT = static_cast<int>(0x80D90007);
static constexpr int SCE_APP_CONTENT_ERROR_GET_STORE_COUNTRY = static_cast<int>(0x80D91000);
static constexpr uint32_t APPPARAM_ID_SKU_FLAG = 0;
static constexpr int32_t SKU_FLAG_FULL = 3;

static constexpr char TEMPORARY_MOUNT_POINT[] = "/temp0";
static constexpr char DOWNLOAD_MOUNT_POINT[] = "/download0";
static constexpr uint32_t TEMPORARY_DATA_OPTION_FORMAT = 1;

namespace {

std::atomic_bool initialized{false};
std::mutex initializationMutex;

std::uint32_t compiledSdkVersion() {
    const auto* process = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t version;
    std::memcpy(&version, process + 16, sizeof(version));
    return version;
}

bool validEntitlementLabel(const NpUnifiedEntitlementLabel& label, std::uint32_t minimumSdkVersion) {
    return compiledSdkVersion() < minimumSdkVersion || std::all_of(std::begin(label.padding), std::end(label.padding), [](char value) { return value == 0; });
}

std::optional<NpUnifiedEntitlementLabel> CopyEntitlementLabel(const NpUnifiedEntitlementLabel* label) {
    if (label == nullptr) return std::nullopt;
    NpUnifiedEntitlementLabel copy{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&copy, label, sizeof(copy));
    errno = savedErrno;
    if (!copied) return std::nullopt;
    return copy;
}

std::optional<AppContentMountPoint> CopyMountPoint(const AppContentMountPoint* mountPoint) {
    if (mountPoint == nullptr) return std::nullopt;
    AppContentMountPoint copy{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&copy, mountPoint, sizeof(copy));
    errno = savedErrno;
    if (!copied) return std::nullopt;
    return copy;
}

struct TemporaryData {
    std::mutex mutex;
    const std::filesystem::path directory = ResolvePath_nid_no_patch(TEMPORARY_MOUNT_POINT);
    bool mounted = false;

    TemporaryData() { BlockPathAlias_nid_no_patch(TEMPORARY_MOUNT_POINT); }
};

TemporaryData& Temporary() {
    static TemporaryData state;
    return state;
}

bool IsTemporaryMountPoint(const AppContentMountPoint& mountPoint) {
    return std::strncmp(mountPoint.data, TEMPORARY_MOUNT_POINT, sizeof(mountPoint.data)) == 0;
}

std::atomic<std::uint64_t> download0ExpandedMiB{0};
std::atomic<std::uint64_t> download1QuotaBytes{0};
std::atomic<std::uint64_t> download2QuotaBytes{0};

bool IsDownloadMountPoint(const AppContentMountPoint& mountPoint) {
    for (const char* prefix : {"/download0", "/download1", "/download2"}) {
        if (std::strncmp(mountPoint.data, prefix, sizeof(mountPoint.data)) == 0) return true;
    }
    return false;
}

}

struct AddonEntry {
    AppContentAddcontInfo info;
    std::string mount_point;
    std::filesystem::path host_path;
    bool has_host_path = false;
    bool mounted = false;
};

std::mutex addcontMutex;

int APS5_VABI sceAppContentDownload1Shrink(size_t size) {
    (void)size;
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppContentDownload2Expand(size_t size) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (size == 0) return SCE_APP_CONTENT_ERROR_PARAMETER;
    const auto directory = ResolvePath_nid_no_patch("/download2");
    if (std::filesystem::create_directories(directory)) RecordWrittenPath_nid_no_patch(directory);
    std::uint64_t current = download2QuotaBytes.load();
    while (size > current && !download2QuotaBytes.compare_exchange_weak(current, size)) {}
    return 0;
}

int APS5_VABI sceAppContentDownload2Shrink(size_t size) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (size == 0) return SCE_APP_CONTENT_ERROR_PARAMETER;
    NotImplemented_nid_no_patch(__func__);
}

APS5_EXPORT("SWVxsi-ZBlw", sceAppContentUnknown_SWVxsi_ZBlw);
int APS5_VABI sceAppContentUnknown_SWVxsi_ZBlw(const void* mdid, bool* matches) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (mdid == nullptr || matches == nullptr) return SCE_APP_CONTENT_ERROR_PARAMETER;
    NotImplemented_nid_no_patch("sceAppContentUnknown_SWVxsi_ZBlw query to the SceAppContent service");
}

int APS5_VABI sceAppContentGetAddcontInfoByEntitlementId(const char* entitlementId, AppContentAddcontInfo* info) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!entitlementId || !info) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_DRM_NO_ENTITLEMENT;
}

int APS5_VABI sceAppContentGetAddcontInfoListByIroTag(uint32_t iroTag, AppContentAddcontInfo* list, uint32_t listNum, uint32_t* hitNum) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)iroTag;
    if ((!list || listNum == 0) && !hitNum) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (hitNum) *hitNum = 0;
    return 0;
}

int APS5_VABI sceAppContentGetDownloadedStoreCountry(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, void* storeCountry) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)serviceLabel;
    if (!storeCountry) return SCE_APP_CONTENT_ERROR_PARAMETER;
    const auto label = CopyEntitlementLabel(entitlementLabel);
    if (!label || !validEntitlementLabel(*label, 0x01500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_GET_STORE_COUNTRY;
}

int APS5_VABI sceAppContentGetEntitlementKey(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, AppContentEntitlementKey* key) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)serviceLabel;
    if (!key) return SCE_APP_CONTENT_ERROR_PARAMETER;
    const auto label = CopyEntitlementLabel(entitlementLabel);
    if (!label || !validEntitlementLabel(*label, 0x01500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_DRM_NO_ENTITLEMENT;
}

int APS5_VABI sceAppContentGetPftFlag(uint32_t* pftFlag) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!pftFlag) return SCE_APP_CONTENT_ERROR_PARAMETER;
    *pftFlag = 0;
    return 0;
}

int APS5_VABI sceAppContentGetRegion(void* region) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!region) return SCE_APP_CONTENT_ERROR_PARAMETER;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppContentRequestPatchInstall(const char* text) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!text) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::string copy;
    if (CopyGuestString_nid_no_patch(text, 0x100, copy) != 0) return SCE_APP_CONTENT_ERROR_PARAMETER;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppContentTemporaryDataMount(AppContentMountPoint* mountPoint) {
    return sceAppContentTemporaryDataMount2(TEMPORARY_DATA_OPTION_FORMAT, mountPoint);
}

}
