#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libSceSaveData/SaveDataCore.hpp"
#include "prx/libc/include/General.hpp"

// Savedata imports bind by NID either to libSceSaveData.native or (via the relinker alias table,
// by name) to this library. Every entry point used to return SoftFailNp (0x800242e2), which is the
// NP-family "online service not available" code - wrong dialect for a purely local save call and
// fatal-looking for callers. All entry points now delegate to SaveDataCore.hpp: a local,
// file-backed implementation that needs no PSN account and tolerates any userId
// (0 / 65535 / 268435456), mirroring the native library.

namespace {
// Logs the first three calls of each entry point, then stays silent, keeping guest logs readable.
#define SAVEDATA_TRACE(...)                                                                                            \
    do {                                                                                                               \
        static std::atomic<int> traceCount{0};                                                                         \
        if (traceCount.fetch_add(1, std::memory_order_relaxed) < 3) {                                                  \
            std::fprintf(stderr, "[SAVEDATA:core] " __VA_ARGS__);                                                      \
            std::fputc('\n', stderr);                                                                                  \
            std::fflush(stderr);                                                                                       \
        }                                                                                                              \
    } while (0)
} // namespace

extern "C" {

int APS5_VABI sceSaveDataInitialize3(const void* init) {
    const int rc = savedata::initialize_internal(init);
    SAVEDATA_TRACE("sceSaveDataInitialize3 -> 0x%08x", static_cast<unsigned>(rc));
    return rc;
}
int APS5_VABI sceSaveDataTerminate(void) {
    const int rc = savedata::terminate_internal();
    SAVEDATA_TRACE("sceSaveDataTerminate -> 0x%08x", static_cast<unsigned>(rc));
    return rc;
}
int APS5_VABI sceSaveDataBackup(const SaveDataBackup* backup) {
    const int rc = savedata::backup_internal(backup, nullptr);
    SAVEDATA_TRACE("sceSaveDataBackup -> 0x%08x", static_cast<unsigned>(rc));
    return rc;
}
int APS5_VABI sceSaveDataGetEventResult(const void* event_param, SaveDataEvent* event) {
    const int rc = savedata::get_event_result_internal(event_param, event);
    SAVEDATA_TRACE("sceSaveDataGetEventResult -> 0x%08x", static_cast<unsigned>(rc));
    return rc;
}
int APS5_VABI sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2* get_param) {
    if (get_param == nullptr) {
        return savedata::SD_ERROR_PARAMETER;
    }
    const int rc = savedata::get_memory_internal(get_param);
    SAVEDATA_TRACE("get2 user=%d slot=%u -> 0x%08x", static_cast<int>(get_param->user_id),
                   static_cast<unsigned>(get_param->slot_id), static_cast<unsigned>(rc));
    return rc;
}
int APS5_VABI sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2* set_param) {
    if (set_param == nullptr) {
        return savedata::SD_ERROR_PARAMETER;
    }
    const int rc = savedata::set_memory_internal(set_param);
    SAVEDATA_TRACE("set2 user=%d slot=%u num=%u -> 0x%08x", static_cast<int>(set_param->user_id),
                   static_cast<unsigned>(set_param->slot_id), static_cast<unsigned>(set_param->data_num),
                   static_cast<unsigned>(rc));
    return rc;
}
int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2* setup_param,
                                              SaveDataMemorySetupResult* result) {
    const int rc = savedata::setup_memory_internal(setup_param, result, nullptr);
    SAVEDATA_TRACE("setup2 -> 0x%08x existed=0x%zx", static_cast<unsigned>(rc),
                   result != nullptr ? result->existed_memory_size : static_cast<std::size_t>(0));
    return rc;
}
int APS5_VABI sceSaveDataSyncSaveDataMemory(const void* sync_param) {
    if (sync_param == nullptr) {
        return savedata::SD_ERROR_PARAMETER;
    }
    const auto* sync = static_cast<const savedata::MemorySync*>(sync_param);
    const int rc = savedata::sync_memory_internal(sync->user_id, sync->slot_id, sync->option, nullptr, true);
    SAVEDATA_TRACE("sync user=%d slot=%u option=%u -> 0x%08x", static_cast<int>(sync->user_id),
                   static_cast<unsigned>(sync->slot_id), static_cast<unsigned>(sync->option),
                   static_cast<unsigned>(rc));
    return rc;
}
int APS5_VABI sceSaveDataTransferringMount(const SaveDataTransferringMount* mount, SaveDataMountResult* mount_result) {
    const int rc = savedata::transferring_mount_internal(mount, mount_result);
    SAVEDATA_TRACE("sceSaveDataTransferringMount -> 0x%08x", static_cast<unsigned>(rc));
    return rc;
}
}
