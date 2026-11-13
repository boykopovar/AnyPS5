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

bool IsTemporaryMountPoint(const AppContentMountPoint* mountPoint) {
    return std::strncmp(mountPoint->data, TEMPORARY_MOUNT_POINT, sizeof(mountPoint->data)) == 0;
}

std::atomic<std::uint64_t> download1QuotaBytes{0};
std::atomic<std::uint64_t> download2QuotaBytes{0};

bool IsDownloadMountPoint(const AppContentMountPoint* mountPoint) {
    if (!mountPoint) return false;
    for (const char* prefix : {"/download0", "/download1", "/download2"}) {
        if (std::strncmp(mountPoint->data, prefix, sizeof(mountPoint->data)) == 0) return true;
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

std::vector<AddonEntry>& AddonEntries() {
    static auto entries = [] {
        std::vector<AddonEntry> list;
        const auto rawEntries = AnyPS5::Entitlements::ParseEntries();
        std::size_t index = 0;
        for (const auto& raw : rawEntries) {
            AddonEntry entry{};
            std::memcpy(entry.info.entitlement_label.data, raw.label.data(), raw.label.size());
            entry.info.status = 4;
            if (!raw.target.empty()) {
                entry.mount_point = raw.target;
            } else {
                char defaultMount[16];
                std::snprintf(defaultMount, sizeof(defaultMount), "/addcont%zu", index);
                entry.mount_point = defaultMount;
            }
            std::error_code ec;
            const auto resolved = ResolveDiskPath_nid_no_patch(entry.mount_point.c_str());
            if (std::filesystem::is_directory(resolved, ec)) {
                entry.host_path = resolved;
                entry.has_host_path = true;
                BlockPathAlias_nid_no_patch(entry.mount_point.c_str());
            }
            list.push_back(entry);
            ++index;
        }
        return list;
    }();
    return entries;
}

static void ClearDirectory(const std::filesystem::path& directory) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) RemoveTree(entry.path());
    RecordWrittenPath_nid_no_patch(directory);
}

extern "C" {

int APS5_VABI sceAppContentCheckBundleLicenseOnDisc(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)serviceLabel;
    if (!entitlementLabel) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlementLabel, 0x03500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentDownloadDataGetBlockSize(const AppContentMountPoint* mountPoint, size_t* blockSize) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!mountPoint || !blockSize) return SCE_APP_CONTENT_ERROR_PARAMETER;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppContentGetGameTrialsFlag(uint32_t* gameTrialsFlag) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!gameTrialsFlag) return SCE_APP_CONTENT_ERROR_PARAMETER;
    const uint32_t flag = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(gameTrialsFlag, &flag, sizeof(flag));
    errno = savedErrno;
    if (!copied) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return 0;
}

int APS5_VABI sceAppContentGetPlayableStatus(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, uint32_t* playableStatus) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)serviceLabel;
    if (!entitlementLabel || !playableStatus) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlementLabel, 0x03500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}


int APS5_VABI sceAppContentAddcontMount(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, AppContentMountPoint* mount_point) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)service_label;
    if (!entitlement_label || !mount_point) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlement_label, 0x01500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::lock_guard lock(addcontMutex);
    for (auto& addon : AddonEntries()) {
        if (std::strncmp(addon.info.entitlement_label.data, entitlement_label->data, sizeof(entitlement_label->data)) == 0) {
            if (!addon.has_host_path) return SCE_APP_CONTENT_ERROR_NOT_FOUND;
            if (addon.mounted) return SCE_APP_CONTENT_ERROR_BUSY;
            addon.mounted = true;
            AddPathAlias_nid_no_patch(addon.mount_point.c_str(), addon.host_path.string().c_str());
            std::memset(mount_point->data, 0, sizeof(mount_point->data));
            std::memcpy(mount_point->data, addon.mount_point.data(), addon.mount_point.size());
            return 0;
        }
    }
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentAddcontUnmount(const AppContentMountPoint* mount_point) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!mount_point) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::lock_guard lock(addcontMutex);
    for (auto& addon : AddonEntries()) {
        if (addon.mounted && std::strncmp(addon.mount_point.c_str(), mount_point->data, sizeof(mount_point->data)) == 0) {
            RemovePathAlias_nid_no_patch(addon.mount_point.c_str());
            BlockPathAlias_nid_no_patch(addon.mount_point.c_str());
            addon.mounted = false;
            return 0;
        }
    }
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentGetAddcontInfo(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, AppContentAddcontInfo* info) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)service_label;
    if (!entitlement_label || !info) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlement_label, 0x01500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::lock_guard lock(addcontMutex);
    for (const auto& addon : AddonEntries()) {
        if (std::strncmp(addon.info.entitlement_label.data, entitlement_label->data, sizeof(entitlement_label->data)) == 0) {
            *info = addon.info;
            return 0;
        }
    }
    return SCE_APP_CONTENT_ERROR_DRM_NO_ENTITLEMENT;
}

int APS5_VABI sceAppContentGetAddcontInfoList(uint32_t serviceLabel, AppContentAddcontInfo* list, uint32_t listNum, uint32_t* hitNum) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)serviceLabel;
    if (!hitNum) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::lock_guard lock(addcontMutex);
    const auto& entries = AddonEntries();
    std::uint32_t hits = static_cast<std::uint32_t>(entries.size());
    const int savedErrno = errno;
    if (list && listNum > 0) {
        const auto count = std::min<std::size_t>(listNum, entries.size());
        if (count > 0) {
            std::vector<AppContentAddcontInfo> infos;
            infos.reserve(count);
            for (std::size_t i = 0; i < count; ++i) infos.push_back(entries[i].info);
            if (!GuestCopyOut_nid_no_patch(list, infos.data(), infos.size() * sizeof(AppContentAddcontInfo))) {
                errno = savedErrno;
                return SCE_APP_CONTENT_ERROR_PARAMETER;
            }
        }
        hits = static_cast<std::uint32_t>(count);
    }
    const bool copied = GuestCopyOut_nid_no_patch(hitNum, &hits, sizeof(hits));
    errno = savedErrno;
    return copied ? 0 : SCE_APP_CONTENT_ERROR_PARAMETER;
}

int APS5_VABI sceAppContentAppParamGetInt(uint32_t param_id, int32_t* value) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!value) return SCE_APP_CONTENT_ERROR_PARAMETER;
    int32_t result;
    switch (param_id) {
    case APPPARAM_ID_SKU_FLAG:
        result = SKU_FLAG_FULL;
        break;
    case 1: case 2: case 3: case 4:
        result = GetAppUserDefinedParam_nid_postfix(param_id - 1);
        break;
    default:
        return SCE_APP_CONTENT_ERROR_PARAMETER;
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(value, &result, sizeof(result));
    errno = savedErrno;
    if (!copied) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return 0;
}

int APS5_VABI sceAppContentDownloadDataFormat(const AppContentMountPoint* mount_point) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!mount_point) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!IsDownloadMountPoint(mount_point)) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    const auto directory = ResolvePath_nid_no_patch(mount_point->data);
    if (std::filesystem::create_directories(directory)) RecordWrittenPath_nid_no_patch(directory);
    ClearDirectory(directory);
    return 0;
}

int APS5_VABI sceAppContentDownloadDataGetAvailableSpaceKb(const AppContentMountPoint* mountPoint, size_t* availableSpaceKb) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!mountPoint || !availableSpaceKb) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!IsDownloadMountPoint(mountPoint)) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    std::uint64_t quotaKb = 0;
    if (std::strncmp(mountPoint->data, DOWNLOAD_MOUNT_POINT, sizeof(mountPoint->data)) == 0) {
        const std::uint64_t quotaMiB = GetAppDownloadDataSizeMiB_nid_postfix();
        if (quotaMiB > std::numeric_limits<std::uint64_t>::max() / 1024u) throw std::overflow_error("AppContent download quota exceeds KiB range");
        quotaKb = quotaMiB * 1024u;
    } else if (std::strncmp(mountPoint->data, "/download1", sizeof(mountPoint->data)) == 0) {
        quotaKb = download1QuotaBytes.load() / 1024u;
    } else if (std::strncmp(mountPoint->data, "/download2", sizeof(mountPoint->data)) == 0) {
        quotaKb = download2QuotaBytes.load() / 1024u;
    }
    if (quotaKb == 0) {
        *availableSpaceKb = 0;
        return 0;
    }
    const auto directory = ResolvePath_nid_no_patch(mountPoint->data);
    if (std::filesystem::create_directories(directory)) RecordWrittenPath_nid_no_patch(directory);
    std::uint64_t usedKb = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) usedKb += (entry.file_size() + 1023u) / 1024u;
    }
    const std::uint64_t hostKb = std::filesystem::space(directory).available / 1024u;
    *availableSpaceKb = static_cast<size_t>(std::min(quotaKb - std::min(quotaKb, usedKb), hostKb));
    return 0;
}

int APS5_VABI sceAppContentInitialize(const AppContentInitParam* initParam, AppContentBootParam* bootParam) {
    std::lock_guard lock(initializationMutex);
    const auto sdkVersion = compiledSdkVersion();
    if (sdkVersion >= 0x01500000) {
        if (initialized.load()) return SCE_APP_CONTENT_ERROR_BUSY;
        if (initParam == nullptr || bootParam == nullptr) return SCE_APP_CONTENT_ERROR_PARAMETER;
        if (!std::all_of(std::begin(initParam->reserved), std::end(initParam->reserved), [](char value) { return value == 0; }) || !std::all_of(std::begin(bootParam->reserved1), std::end(bootParam->reserved1), [](char value) { return value == 0; }) || !std::all_of(std::begin(bootParam->reserved2), std::end(bootParam->reserved2), [](char value) { return value == 0; })) return SCE_APP_CONTENT_ERROR_PARAMETER;
    }
    if (bootParam == nullptr) return SCE_APP_CONTENT_ERROR_PARAMETER;
    Temporary();
    std::memset(bootParam, 0, sizeof(*bootParam));
    initialized = true;
    return 0;
}

int APS5_VABI sceAppContentTemporaryDataFormat(const AppContentMountPoint* mountPoint) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!mountPoint) return SCE_APP_CONTENT_ERROR_PARAMETER;
    auto& state = Temporary();
    std::lock_guard lock(state.mutex);
    if (!state.mounted || !IsTemporaryMountPoint(mountPoint)) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    ClearDirectory(state.directory);
    return 0;
}

int APS5_VABI sceAppContentTemporaryDataGetAvailableSpaceKb(const AppContentMountPoint* mountPoint, size_t* availableSpaceKb) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!mountPoint || !availableSpaceKb) return SCE_APP_CONTENT_ERROR_PARAMETER;
    auto& state = Temporary();
    std::lock_guard lock(state.mutex);
    if (!state.mounted || !IsTemporaryMountPoint(mountPoint)) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    const size_t localSpace = static_cast<size_t>(std::filesystem::space(state.directory).available / 1024);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(availableSpaceKb, &localSpace, sizeof(localSpace));
    errno = savedErrno;
    if (!copied) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return 0;
}

int APS5_VABI sceAppContentTemporaryDataMount2(uint32_t option, AppContentMountPoint* mountPoint) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!mountPoint || option > TEMPORARY_DATA_OPTION_FORMAT) return SCE_APP_CONTENT_ERROR_PARAMETER;
    auto& state = Temporary();
    std::lock_guard lock(state.mutex);
    if (state.mounted) return SCE_APP_CONTENT_ERROR_BUSY;
    AppContentMountPoint output{};
    std::memcpy(output.data, TEMPORARY_MOUNT_POINT, sizeof(TEMPORARY_MOUNT_POINT));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(mountPoint, &output, sizeof(output));
    errno = savedErrno;
    if (!copied) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (std::filesystem::create_directories(state.directory)) RecordWrittenPath_nid_no_patch(state.directory);
    if (option == TEMPORARY_DATA_OPTION_FORMAT) ClearDirectory(state.directory);
    AddPathAlias_nid_no_patch(TEMPORARY_MOUNT_POINT, state.directory.string().c_str());
    state.mounted = true;
    return 0;
}

int APS5_VABI sceAppContentTemporaryDataUnmount(const AppContentMountPoint* mountPoint) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!mountPoint) return SCE_APP_CONTENT_ERROR_PARAMETER;
    auto& state = Temporary();
    std::lock_guard lock(state.mutex);
    if (!state.mounted || !IsTemporaryMountPoint(mountPoint)) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    BlockPathAlias_nid_no_patch(TEMPORARY_MOUNT_POINT);
    state.mounted = false;
    return 0;
}

int APS5_VABI sceAppContentAddcontEnqueueDownload(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)service_label;
    if (!entitlement_label) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlement_label, 0x01500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentAddcontDelete(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)service_label;
    if (!entitlement_label) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlement_label, 0x03500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentGetAddcontDownloadProgress(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, void* progress) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)service_label;
    if (!entitlement_label || !progress) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlement_label, 0x03500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}
int APS5_VABI sceAppContentAddcontEnqueueDownloadByEntitlemetId(const char* entitlementId) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!entitlementId) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentAddcontEnqueueDownloadSp(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)serviceLabel;
    if (!entitlementLabel) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlementLabel, 0x03500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlementLabel, 0)) throw std::runtime_error("sceAppContentAddcontEnqueueDownloadSp: padded entitlement label before SDK 0x03500000");
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentAddcontMountByEntitlemetId(const char* entitlementId, AppContentMountPoint* mountPoint) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!entitlementId || !mountPoint) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentAddcontShrink(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)serviceLabel;
    if (!entitlementLabel) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlementLabel, 0x03500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlementLabel, 0)) throw std::runtime_error("sceAppContentAddcontShrink: padded entitlement label before SDK 0x03500000");
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentAppParamGetString(uint32_t paramId, char* value, size_t valueSize) {
    (void)paramId;
    (void)valueSize;
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (!value) return SCE_APP_CONTENT_ERROR_PARAMETER;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppContentDownload0Expand(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppContentDownload0Shrink(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppContentDownload1Expand(size_t size) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    if (size == 0) return SCE_APP_CONTENT_ERROR_PARAMETER;
    const auto directory = ResolvePath_nid_no_patch("/download1");
    if (std::filesystem::create_directories(directory)) RecordWrittenPath_nid_no_patch(directory);
    std::uint64_t current = download1QuotaBytes.load();
    while (size > current && !download1QuotaBytes.compare_exchange_weak(current, size)) {}
    return 0;
}

int APS5_VABI sceAppContentDownload1Shrink(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI sceAppContentDownload2Shrink(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("SWVxsi-ZBlw", sceAppContentUnknown_SWVxsi_ZBlw);
int APS5_VABI sceAppContentUnknown_SWVxsi_ZBlw(const void* mdid, bool* matches) {
    (void)mdid;
    (void)matches;
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    if (!entitlementLabel || !storeCountry) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlementLabel, 0x01500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_GET_STORE_COUNTRY;
}

int APS5_VABI sceAppContentGetEntitlementKey(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, AppContentEntitlementKey* key) {
    if (!initialized.load()) return SCE_APP_CONTENT_ERROR_NOT_INITIALIZED;
    (void)serviceLabel;
    if (!entitlementLabel || !key) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!validEntitlementLabel(*entitlementLabel, 0x01500000)) return SCE_APP_CONTENT_ERROR_PARAMETER;
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
    if (!text || std::strlen(text) > 0xff) return SCE_APP_CONTENT_ERROR_PARAMETER;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAppContentTemporaryDataMount(AppContentMountPoint* mountPoint) {
    return sceAppContentTemporaryDataMount2(TEMPORARY_DATA_OPTION_FORMAT, mountPoint);
}

}
