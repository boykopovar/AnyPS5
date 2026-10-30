#include "prx/libSceRegMgr/include/Api.hpp"
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
        if (len == 0) NotImplemented_nid_no_patch(__func__);
        if (out == nullptr || len < 0) return SCE_REG_MGR_ERROR_INVALID_PARAM;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) return SCE_REG_MGR_ERROR_NOT_FOUND;
        if (it->second.type != TypeStr) return SCE_REG_MGR_ERROR_INVALID_TYPE;
        if (static_cast<size_t>(len) <= it->second.strVal.size()) return SCE_REG_MGR_ERROR_BUFFER_TOO_SMALL;
        std::memcpy(out, it->second.strVal.c_str(), it->second.strVal.size() + 1);
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
        std::memcpy(out, it->second.binVal.data(), it->second.binVal.size());
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
int APS5_VABI sceRegMgrSetBin(std::uint32_t optionId, const void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceRegMgrSetInt(int32_t optionId, int32_t inInt) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceRegMgrSetStr(int32_t optionId, const char* inStirng, std::size_t stringLength) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI sceRegMgrResetVal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI sceRegMgrUnknown_001_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("HGxgXwGSAzQ", sceRegMgrUnknown_001_nid_no_patch);

}
