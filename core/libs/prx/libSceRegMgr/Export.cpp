#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_REG_MGR_ERROR_INVALID_PARAM = static_cast<int>(0x80820001);
constexpr int SCE_REG_MGR_ERROR_NOT_FOUND = static_cast<int>(0x80820002);
constexpr int SCE_REG_MGR_ERROR_BUFFER_TOO_SMALL = static_cast<int>(0x80820004);
constexpr int SCE_REG_MGR_ERROR_INVALID_TYPE = static_cast<int>(0x80820006);

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

class RegistryStore {
public:
    RegistryStore() = default;

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    int getInt(int64_t key, int32_t* out) {
        if (out == nullptr) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) return SCE_REG_MGR_ERROR_NOT_FOUND;
        if (it->second.type != TypeInt) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        *out = it->second.intVal;
        return 0;
    }

    int getStr(int64_t key, char* out, int len) {
        if (out == nullptr || len <= 0) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) return SCE_REG_MGR_ERROR_NOT_FOUND;
        if (it->second.type != TypeStr) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        if (static_cast<size_t>(len) <= it->second.strVal.size()) return SCE_REG_MGR_ERROR_BUFFER_TOO_SMALL;
        std::memcpy(out, it->second.strVal.c_str(), it->second.strVal.size() + 1);
        return 0;
    }

    int getBin(int64_t key, void* out, int size) {
        if (out == nullptr || size <= 0) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) return SCE_REG_MGR_ERROR_NOT_FOUND;
        if (it->second.type != TypeBin) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        if (static_cast<size_t>(size) < it->second.binVal.size()) return SCE_REG_MGR_ERROR_BUFFER_TOO_SMALL;
        std::memcpy(out, it->second.binVal.data(), it->second.binVal.size());
        return 0;
    }

    int setInt(int64_t key, int32_t val) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& entry = entries_[key];
        entry.type = TypeInt;
        entry.intVal = val;
        entry.strVal.clear();
        entry.binVal.clear();
        return 0;
    }

    int setStr(int64_t key, const char* str, int len) {
        if (str == nullptr || len < 0) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto& entry = entries_[key];
        entry.type = TypeStr;
        size_t slen = 0;
        if (len > 0) {
            auto ulen = static_cast<size_t>(len);
            while (slen < ulen && str[slen] != '\0') ++slen;
        } else {
            slen = std::strlen(str);
        }
        entry.strVal.assign(str, slen);
        entry.intVal = 0;
        entry.binVal.clear();
        return 0;
    }

    int setBin(int64_t key, const void* data, int size) {
        if (data == nullptr || size <= 0) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto& entry = entries_[key];
        entry.type = TypeBin;
        const auto* ptr = static_cast<const uint8_t*>(data);
        entry.binVal.assign(ptr, ptr + static_cast<size_t>(size));
        entry.intVal = 0;
        entry.strVal.clear();
        return 0;
    }

    int resetVal(int64_t key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) return SCE_REG_MGR_ERROR_NOT_FOUND;
        entries_.erase(it);
        return 0;
    }

private:
    std::mutex mutex_;
    std::unordered_map<int64_t, RegEntry> entries_;
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

int APS5_VABI sceRegMgrSetBin(std::int64_t optionId, const void* data, int size) {
    return gStore.setBin(optionId, data, size);
}

int APS5_VABI sceRegMgrSetInt(std::int64_t optionId, int32_t inInt) {
    return gStore.setInt(optionId, inInt);
}

int APS5_VABI sceRegMgrSetStr(std::int64_t optionId, const char* inString, int stringLength) {
    return gStore.setStr(optionId, inString, stringLength);
}

std::int32_t APS5_VABI sceRegMgrNonSysGetInt(std::int64_t key, std::int32_t unk, std::int32_t* data) {
    (void)key;
    (void)unk;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrCheckError(void) {
    NotImplemented_nid_no_patch(__func__);
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

int APS5_VABI sceRegMgrGetIntInitVal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrGetStrInitVal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrGetVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrIsChange(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrEvtGetCnt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrEvtGetRegId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrResetVal(std::int64_t optionId) {
    return gStore.resetVal(optionId);
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrNonSysGetBin(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrNonSysGetStr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrNonSysSetBin(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrNonSysSetInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRegMgrNonSysSetStr(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

void* APS5_VABI sceRegMgrMemset(void* dst, int val, std::size_t len) {
    if (dst == nullptr) {
        return nullptr;
    }
    return std::memset(dst, val, len);
}

int APS5_VABI sceRegMgrSetbin(std::int64_t optionId, const void* data, int size) {
    return gStore.setBin(optionId, data, size);
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

}
