#include "prx/libSceNpTrophy2/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNpTrophy2/include/InventoryExports.hpp"
#include "prx/libSceNpTrophy2/include/Api.hpp"
#include "prx/libSceNpTrophy2/include/NpTrophy2Constants.hpp"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int kErrInvalidArgument = static_cast<int>(0x80553904u);
constexpr int kErrBufferTooSmall = static_cast<int>(0x80553905u);
constexpr int kErrInvalidHandle = static_cast<int>(0x80553908u);
constexpr int kErrInvalidContext = static_cast<int>(0x80553909u);
constexpr int kErrInvalidTrophyId = static_cast<int>(0x8055390au);
constexpr int kErrInvalidGroupId = static_cast<int>(0x8055390bu);
constexpr int kErrInvalidUserId = static_cast<int>(0x8055391du);

struct SystemNpCommId {
    char data[9];
    char term;
    std::uint8_t num;
    char dummy;
};
static_assert(sizeof(SystemNpCommId) == 12);

struct SystemContextEntry {
    int userId;
    SystemNpCommId commId;
};

struct SystemCallbackSlot {
    void* fn = nullptr;
    void* userdata = nullptr;
};

struct SystemState {
    std::mutex mutex;
    std::set<int> handles;
    int nextHandle = 1;
    std::map<int, SystemContextEntry> contexts;
    int nextContext = 1;
    SystemCallbackSlot titleUpdateCb;
    SystemCallbackSlot titleSyncedCb;
};

SystemState& systemState() {
    static SystemState state;
    return state;
}

bool isEmptyNpCommId(const SystemNpCommId& id) {
    for (std::size_t i = 0; i < sizeof(id.data); ++i) {
        if (id.data[i] != '\0') {
            return false;
        }
    }
    return id.num == 0;
}

void formatNpCommId(const SystemNpCommId& id, char (&out)[13]) {
    std::memset(out, 0, sizeof(out));
    if (id.num > 99) {
        return;
    }
    std::memcpy(out, id.data, sizeof(id.data));
    out[9] = '_';
    out[10] = static_cast<char>('0' + (id.num / 10));
    out[11] = static_cast<char>('0' + (id.num % 10));
    out[12] = '\0';
}

int readAndValidateCommId(const void* npCommId, char* uriBuf, char (&commStr)[13]) {
    if (npCommId == nullptr) {
        return kErrInvalidArgument;
    }
    if (uriBuf == nullptr) {
        return kErrBufferTooSmall;
    }
    SystemNpCommId id{};
    if (!GuestCopyIn_nid_no_patch(&id, npCommId, sizeof(id))) {
        return kErrInvalidArgument;
    }
    if (isEmptyNpCommId(id)) {
        return kErrInvalidArgument;
    }
    formatNpCommId(id, commStr);
    return SCE_NP_TROPHY2_OK;
}

int writeUriResult(const char* uri, char* uriBuf, std::size_t bufSize) {
    const std::size_t len = std::strlen(uri);
    if (bufSize <= len) {
        return kErrBufferTooSmall;
    }
    if (bufSize <= 512) {
        char padded[512]{};
        std::memcpy(padded, uri, len);
        if (!GuestCopyOut_nid_no_patch(uriBuf, padded, bufSize)) {
            return kErrInvalidArgument;
        }
        return SCE_NP_TROPHY2_OK;
    }
    if (!GuestCopyOut_nid_no_patch(uriBuf, uri, len + 1)) {
        return kErrInvalidArgument;
    }
    return SCE_NP_TROPHY2_OK;
}

}

extern "C" {

int APS5_VABI sceNpTrophy2SystemAbortHandle(int handle) {
    if (handle == -1) {
        return kErrInvalidHandle;
    }
    auto& state = systemState();
    std::lock_guard lock(state.mutex);
    if (state.handles.find(handle) == state.handles.end()) {
        return kErrInvalidHandle;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemBuildGroupIconUri(const void* npCommId, int groupId, char* uriBuf, size_t bufSize) {
    char commStr[13]{};
    const int rc = readAndValidateCommId(npCommId, uriBuf, commStr);
    if (rc != SCE_NP_TROPHY2_OK) {
        return rc;
    }
    if (groupId != -1 && (groupId < 1 || groupId > 50)) {
        return kErrInvalidGroupId;
    }
    char uri[96]{};
    std::snprintf(uri, sizeof(uri), "trp2://localfile/%s/trophyGroups/%d/icon", commStr, groupId);
    return writeUriResult(uri, uriBuf, bufSize);
}

int APS5_VABI sceNpTrophy2SystemBuildRewardIconUri(const void* npCommId, int rewardId, char* uriBuf, size_t bufSize) {
    char commStr[13]{};
    const int rc = readAndValidateCommId(npCommId, uriBuf, commStr);
    if (rc != SCE_NP_TROPHY2_OK) {
        return rc;
    }
    if (static_cast<std::uint32_t>(rewardId) > 0x3e7u) {
        return kErrInvalidTrophyId;
    }
    char uri[96]{};
    std::snprintf(uri, sizeof(uri), "trp2://localfile/%s/rewards/%d/icon", commStr, rewardId);
    return writeUriResult(uri, uriBuf, bufSize);
}

int APS5_VABI sceNpTrophy2SystemBuildTrophyIconUri(const void* npCommId, int trophyId, char* uriBuf, size_t bufSize) {
    char commStr[13]{};
    const int rc = readAndValidateCommId(npCommId, uriBuf, commStr);
    if (rc != SCE_NP_TROPHY2_OK) {
        return rc;
    }
    if (static_cast<std::uint32_t>(trophyId) > 0x3e7u) {
        return kErrInvalidTrophyId;
    }
    char uri[96]{};
    std::snprintf(uri, sizeof(uri), "trp2://localfile/%s/trophies/%d/icon", commStr, trophyId);
    return writeUriResult(uri, uriBuf, bufSize);
}

int APS5_VABI sceNpTrophy2SystemCheckCallback(void) {
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemCheckNetSyncTitles(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemCreateContext(int* context, int userId, const void* npCommId, uint64_t options) {
    if (userId == -1) {
        return kErrInvalidUserId;
    }
    if (context == nullptr || npCommId == nullptr || options != 0) {
        return kErrInvalidArgument;
    }
    const int invalid = -1;
    if (!GuestCopyOut_nid_no_patch(context, &invalid, sizeof(invalid))) {
        return kErrInvalidArgument;
    }
    SystemNpCommId id{};
    if (!GuestCopyIn_nid_no_patch(&id, npCommId, sizeof(id))) {
        return kErrInvalidArgument;
    }
    auto& state = systemState();
    std::lock_guard lock(state.mutex);
    const int created = state.nextContext;
    if (!GuestCopyOut_nid_no_patch(context, &created, sizeof(created))) {
        return kErrInvalidArgument;
    }
    ++state.nextContext;
    state.contexts[created] = {userId, id};
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemCreateHandle(int* handle) {
    if (handle == nullptr) {
        return kErrInvalidArgument;
    }
    const int invalid = -1;
    if (!GuestCopyOut_nid_no_patch(handle, &invalid, sizeof(invalid))) {
        return kErrInvalidArgument;
    }
    auto& state = systemState();
    std::lock_guard lock(state.mutex);
    const int created = state.nextHandle;
    if (!GuestCopyOut_nid_no_patch(handle, &created, sizeof(created))) {
        return kErrInvalidArgument;
    }
    ++state.nextHandle;
    state.handles.insert(created);
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemDebugLockTrophy(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemDebugUnlockTrophy(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemDestroyContext(int context) {
    if (context == -1) {
        return kErrInvalidContext;
    }
    auto& state = systemState();
    std::lock_guard lock(state.mutex);
    if (state.contexts.erase(context) == 0) {
        return kErrInvalidContext;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemGetTrophyDetails(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemGetTrophyGroupConf(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemGetTrophySetArray(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemGetTrophyTitleConf(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemGetTrophyTitleDetails(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemGetTrpIconByUri(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemIsServerAvailable(bool* available) {
    if (available == nullptr) {
        return kErrInvalidArgument;
    }
    const std::uint8_t value = 0;
    if (!GuestCopyOut_nid_no_patch(available, &value, sizeof(value))) {
        return kErrInvalidArgument;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemRegisterTitleSyncedCallback(void* callback, void* userdata) {
    if (callback == nullptr) {
        return kErrInvalidArgument;
    }
    auto& state = systemState();
    std::lock_guard lock(state.mutex);
    state.titleSyncedCb = {callback, userdata};
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemRegisterTitleUpdateCallback(void* callback, void* userdata) {
    if (callback == nullptr) {
        return kErrInvalidArgument;
    }
    auto& state = systemState();
    std::lock_guard lock(state.mutex);
    state.titleUpdateCb = {callback, userdata};
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemRemoveAll(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemRemoveTitleData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2SystemRemoveUserData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceNpTrophy2SystemBuildBgImageIconUri(const void* npCommId, int bgImageId, char* uriBuf, size_t bufSize) {
    char commStr[13]{};
    const int rc = readAndValidateCommId(npCommId, uriBuf, commStr);
    if (rc != SCE_NP_TROPHY2_OK) {
        return rc;
    }
    if (static_cast<std::uint32_t>(bgImageId) > 0x3e7u) {
        return kErrInvalidTrophyId;
    }
    char uri[96]{};
    std::snprintf(uri, sizeof(uri), "trp2://localfile/%s/bgImages/%d/icon", commStr, bgImageId);
    return writeUriResult(uri, uriBuf, bufSize);
}

int APS5_VABI sceNpTrophy2SystemBuildTitleIconUri(const void* npCommId, char* uriBuf, size_t bufSize) {
    char commStr[13]{};
    const int rc = readAndValidateCommId(npCommId, uriBuf, commStr);
    if (rc != SCE_NP_TROPHY2_OK) {
        return rc;
    }
    char uri[96]{};
    std::snprintf(uri, sizeof(uri), "trp2://localfile/%s/icon", commStr);
    return writeUriResult(uri, uriBuf, bufSize);
}

int APS5_VABI sceNpTrophy2SystemDestroyHandle(int handle) {
    if (handle == -1) {
        return kErrInvalidHandle;
    }
    auto& state = systemState();
    std::lock_guard lock(state.mutex);
    if (state.handles.erase(handle) == 0) {
        return kErrInvalidHandle;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemGetTrophyConf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemGetTrophyData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemGetTrophyGroupData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemGetTrophyGroupDetails() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemGetTrophyTitleData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemListAllTitles() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemListPlayedTitles() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemListTitlesByNpTitleId() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemNetSyncTitles() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemRemoveUserDataWithServerData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemUnregisterTitleSyncedCallback(void) {
    auto& state = systemState();
    std::lock_guard lock(state.mutex);
    state.titleSyncedCb = {};
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2SystemUnregisterTitleUpdateCallback(void) {
    auto& state = systemState();
    std::lock_guard lock(state.mutex);
    state.titleUpdateCb = {};
    return SCE_NP_TROPHY2_OK;
}

}

extern "C" {

int APS5_VABI npTrophy2LegacyUnknown_1nbnAo_minus_VuLw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1nbnAo-VuLw", npTrophy2LegacyUnknown_1nbnAo_minus_VuLw_nid_no_patch);

int APS5_VABI sceNpTrophy2SystemDbgCtl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTrophy2SystemGetDbgParam() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI npTrophy2LegacyUnknown_cKAQ579AuCc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cKAQ579AuCc", npTrophy2LegacyUnknown_cKAQ579AuCc_nid_no_patch);

int APS5_VABI sceNpTrophy2SystemSetDbgParam() {
    NotImplemented_nid_no_patch(__func__);
}

}
