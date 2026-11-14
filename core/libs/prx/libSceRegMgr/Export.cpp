#include "prx/libSceRegMgr/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_REG_MGR_ERROR_INVALID_PARAM = static_cast<int>(0x80820001);
constexpr int SCE_REG_MGR_ERROR_NOT_FOUND = static_cast<int>(0x80820002);
constexpr int SCE_REG_MGR_ERROR_BUFFER_TOO_SMALL = static_cast<int>(0x80820004);
constexpr int SCE_REG_MGR_ERROR_INVALID_TYPE = static_cast<int>(0x80820006);
constexpr int SCE_REG_MGR_NONSYS_ERROR_INVALID_PARAM = static_cast<int>(0x800D0202);
constexpr int SCE_REG_MGR_NONSYS_ERROR_BUFFER_TOO_BIG = static_cast<int>(0x800D0208);
constexpr std::size_t SCE_REG_MGR_NONSYS_MAX_SIZE = 0x800;

enum EntryType {
    TypeInt = 0,
    TypeStr = 1,
    TypeBin = 2,
};

struct RegEntry {
    EntryType type;
    int32_t intVal = 0;
    std::string strVal;
    std::vector<uint8_t> binVal;
};

constexpr std::size_t SCE_REG_MGR_MAX_BINARY_SIZE = 1024 * 1024;

class RegistryStore {
public:
    RegistryStore() = default;

    int getInt(int64_t key, int32_t* out) {
        if (out == nullptr) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) return SCE_REG_MGR_ERROR_NOT_FOUND;
        if (it->second.type != TypeInt) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        const int32_t val = it->second.intVal;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(out, &val, sizeof(val));
        errno = savedErrno;
        if (!copied) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        return 0;
    }

    int getStr(int64_t key, char* out, int len) {
        if (len == 0) NotImplemented_nid_no_patch(__func__);
        if (out == nullptr || len < 0) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) return SCE_REG_MGR_ERROR_NOT_FOUND;
        if (it->second.type != TypeStr) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        if (static_cast<size_t>(len) <= it->second.strVal.size()) return SCE_REG_MGR_ERROR_BUFFER_TOO_SMALL;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(out, it->second.strVal.c_str(), it->second.strVal.size() + 1);
        errno = savedErrno;
        if (!copied) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        return 0;
    }

    int getBin(int64_t key, void* out, int size) {
        if (size == 0) NotImplemented_nid_no_patch(__func__);
        if (out == nullptr || size < 0) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) return SCE_REG_MGR_ERROR_NOT_FOUND;
        if (it->second.type != TypeBin) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        if (static_cast<size_t>(size) < it->second.binVal.size()) return SCE_REG_MGR_ERROR_BUFFER_TOO_SMALL;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(out, it->second.binVal.data(), it->second.binVal.size());
        errno = savedErrno;
        if (!copied) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        return 0;
    }

    int setInt(int64_t key, int32_t value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it != entries_.end() && it->second.type != TypeInt) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        const bool changed = it == entries_.end() || it->second.intVal != value;
        RegEntry& entry = entries_[key];
        entry.type = TypeInt;
        entry.intVal = value;
        entry.strVal.clear();
        entry.binVal.clear();
        if (changed) changed_[key] = 0xff;
        return 0;
    }

    int setStr(int64_t key, const char* value, size_t length) {
        if (value == nullptr) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it != entries_.end() && it->second.type != TypeStr) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        size_t stringLength = 0;
        while (stringLength < length && value[stringLength] != '\0') ++stringLength;
        const bool changed = it == entries_.end() || it->second.strVal.size() != stringLength ||
                             it->second.strVal.compare(0, stringLength, value, stringLength) != 0;
        RegEntry& entry = entries_[key];
        entry.type = TypeStr;
        entry.intVal = 0;
        entry.strVal.assign(value, stringLength);
        entry.binVal.clear();
        if (changed) changed_[key] = 0xff;
        return 0;
    }

    int setBin(int64_t key, const void* value, size_t size) {
        if (value == nullptr) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it != entries_.end() && it->second.type != TypeBin) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        if (size > SCE_REG_MGR_MAX_BINARY_SIZE) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::vector<std::uint8_t> bytes(size);
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(bytes.data(), value, size);
        errno = savedErrno;
        if (!copied) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        const bool changed = it == entries_.end() || it->second.binVal.size() != size ||
                             (size != 0 && std::memcmp(it->second.binVal.data(), bytes.data(), size) != 0);
        RegEntry& entry = entries_[key];
        entry.type = TypeBin;
        entry.intVal = 0;
        entry.strVal.clear();
        entry.binVal = std::move(bytes);
        if (changed) changed_[key] = 0xff;
        return 0;
    }

    int resetVal(uint32_t key) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (entries_.erase(key) != 0) changed_[key] = 0xff;
        return 0;
    }

    int isChange(uint32_t key, uint32_t consumer) {
        const uint32_t index = consumer - 0x0d010001u;
        const bool known = (index < 32u && (((0x8000808bu >> index) & 1u) != 0u)) || consumer == 0x0d890080u;
        if (!known) return 0;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = changed_.find(key);
        if (it == changed_.end()) return 0;
        const auto mask = static_cast<uint8_t>(consumer);
        const bool changed = (it->second & mask) != 0;
        it->second &= static_cast<uint8_t>(~mask);
        return changed ? 1 : 0;
    }

private:
    std::mutex mutex_;
    std::unordered_map<int64_t, RegEntry> entries_;
    std::unordered_map<int64_t, uint8_t> changed_;
};

RegistryStore gStore;

} // namespace

extern "C" {

int APS5_VABI sceRegMgrGetBin(std::int64_t optionId, void* data, int size) {
    return gStore.getBin(optionId, data, size);
}

int APS5_VABI sceRegMgrGetInt(std::int64_t optionId, int32_t* outInt) {
    return gStore.getInt(optionId, outInt);
}

int APS5_VABI sceRegMgrGetStr(std::int64_t optionId, char* outString, int stringLength) {
    return gStore.getStr(optionId, outString, stringLength);
}
int APS5_VABI sceRegMgrSetBin(std::uint32_t optionId, const void* data, std::size_t size) {
    if (size == 0) NotImplemented_nid_no_patch(__func__);
    return gStore.setBin(optionId, data, size);
}
int APS5_VABI sceRegMgrSetInt(int32_t optionId, int32_t inInt) {
    return gStore.setInt(static_cast<std::uint32_t>(optionId), inInt);
}
int APS5_VABI sceRegMgrSetStr(int32_t optionId, const char* inStirng, std::size_t stringLength) {
    if (stringLength == 0) NotImplemented_nid_no_patch(__func__);
    return gStore.setStr(static_cast<std::uint32_t>(optionId), inStirng, stringLength);
}
std::int32_t APS5_VABI sceRegMgrNonSysGetInt(std::int64_t key, std::int32_t type, std::int32_t* data) {
    (void)type;
    if (data == nullptr) return SCE_REG_MGR_NONSYS_ERROR_INVALID_PARAM;
    return gStore.getInt(key, data);
}

int APS5_VABI sceRegMgrCheckError(void) {
    return 0;
}

int APS5_VABI sceRegMgrCheckIntVal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrGetBinInitVal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrGetIntInitVal(std::uint32_t optionId, std::int32_t* outInt) {
    (void)optionId;
    (void)outInt;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceRegMgrGetStrInitVal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrGetVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrIsChange(std::uint32_t optionId, std::uint32_t consumer) {
    return gStore.isChange(optionId, consumer);
}

int APS5_VABI sceRegMgrEvtGetCnt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrEvtGetRegId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrResetVal(std::int32_t optionId, std::int32_t unk) {
    (void)unk;
    return gStore.resetVal(static_cast<std::uint32_t>(optionId));
}

int APS5_VABI sceRegMgrSetInitLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrSrvCnvRegionInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrSrvCnvRegionStr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrSrvGetMachineType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrSrvGetQAFforReg(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrSrvGetRealMachineType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrSrvGetRegion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrSrvGetRegionStr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrBackupNeedMem(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrBackupPullData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrBackupPushData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrBackupPullDataGkp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrBackupPushDataGkp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrCntlStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrCntlDeleteReg(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrCntlGetFilesCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrCntlGetFileName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrCntSetInitReinst(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrIsInitOK(void) {
    return 1;
}

int APS5_VABI sceRegMgrGetInitError(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrRecoverRegNvs(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrNonSysCheckError(void) {
    return 0;
}

int APS5_VABI sceRegMgrNonSysGetBin(std::int64_t key, std::int32_t type, void* data, std::size_t size) {
    (void)type;
    if (data == nullptr) return SCE_REG_MGR_NONSYS_ERROR_INVALID_PARAM;
    if (size > SCE_REG_MGR_NONSYS_MAX_SIZE) return SCE_REG_MGR_NONSYS_ERROR_BUFFER_TOO_BIG;
    if (size == 0) return SCE_REG_MGR_ERROR_BUFFER_TOO_SMALL;
    return gStore.getBin(key, data, static_cast<int>(size));
}

int APS5_VABI sceRegMgrNonSysGetStr(std::int64_t key, std::int32_t type, char* data, std::size_t size) {
    (void)type;
    if (data == nullptr) return SCE_REG_MGR_NONSYS_ERROR_INVALID_PARAM;
    if (size > SCE_REG_MGR_NONSYS_MAX_SIZE) return SCE_REG_MGR_NONSYS_ERROR_BUFFER_TOO_BIG;
    if (size == 0) return SCE_REG_MGR_ERROR_BUFFER_TOO_SMALL;
    return gStore.getStr(key, data, static_cast<int>(size));
}

int APS5_VABI sceRegMgrNonSysSetBin(std::int64_t key, std::int32_t type, const void* data, std::size_t size) {
    (void)type;
    if (data == nullptr) return SCE_REG_MGR_NONSYS_ERROR_INVALID_PARAM;
    if (size > SCE_REG_MGR_NONSYS_MAX_SIZE) return SCE_REG_MGR_NONSYS_ERROR_BUFFER_TOO_BIG;
    return gStore.setBin(key, data, size);
}

int APS5_VABI sceRegMgrNonSysSetInt(std::int64_t key, std::int32_t type, std::int32_t value) {
    (void)type;
    return gStore.setInt(key, value);
}

int APS5_VABI sceRegMgrNonSysSetStr(std::int64_t key, std::int32_t type, const char* data, std::size_t size) {
    (void)type;
    if (data == nullptr) return SCE_REG_MGR_NONSYS_ERROR_INVALID_PARAM;
    if (size > SCE_REG_MGR_NONSYS_MAX_SIZE) return SCE_REG_MGR_NONSYS_ERROR_BUFFER_TOO_BIG;
    return gStore.setStr(key, data, size);
}

int APS5_VABI sceRegMgrGetVersionForPS4(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrBackupNeedMemForPS4(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrBackupPullDataForPS4(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrBackupPushDataForPS4(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrEvtGetCntForPS4(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrEvtGetRegIdForPS4(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrStrncpy(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrMemset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrSetbin(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrToolGetUpdateCnt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrToolGetEntryCnt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrToolDataCheckGet(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrDrvGetUpdateCnt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrDrvGetEntCnt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrDrvDataOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrDrvDataCheckGet(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrDrvDataClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrToolGetInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrLogPull(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrCntlStopHdd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrPrintInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrLogStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
