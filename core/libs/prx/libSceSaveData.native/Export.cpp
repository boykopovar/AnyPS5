#include <algorithm>
#include <atomic>
#include <cstddef>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "SaveData.hpp"

static constexpr char SAVE_DIR[] = "_sd";

static std::atomic<std::int32_t> g_transaction_counter{1};
static bool g_initialized = false;

static std::string save_root() {
    return std::string(SAVE_DIR);
}

// Events the title polls with sceSaveDataGetEventResult (a backup completes at once here), and the
// "save data memory" blobs (small per-slot memories the title reads and writes without mounting),
// persisted as files under the save root so they survive restarts.
static std::mutex g_state_mutex;
static std::deque<SaveDataEvent> g_events;
static std::map<std::uint32_t, std::vector<std::uint8_t>> g_memories;
constexpr std::uint32_t SAVE_DATA_EVENT_TYPE_BACKUP = 2;

static std::string memory_path(std::uint32_t slot) {
    return save_root() + "/_memory_" + std::to_string(slot) + ".bin";
}

static void store_memory(std::uint32_t slot, const std::vector<std::uint8_t>& memory) {
    std::filesystem::create_directories(save_root());
    std::ofstream file(memory_path(slot), std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(memory.data()), static_cast<std::streamsize>(memory.size()));
}

static bool dir_name_match(const char* str, const char* pattern) {
    if (pattern == nullptr || pattern[0] == '\0') {
        return true;
    }
    while (*str != '\0' && *pattern != '\0') {
        if (*pattern == '%') {
            for (const char* s = str;; s++) {
                if (dir_name_match(s, pattern + 1)) {
                    return true;
                }
                if (*s == '\0') {
                    break;
                }
            }
            return false;
        }
        if (*pattern == '_') {
            str++;
            pattern++;
            continue;
        }
        if (*pattern != *str) {
            return false;
        }
        str++;
        pattern++;
    }
    return *str == '\0' && *pattern == '\0';
}

extern "C" {

int APS5_VABI sceSaveDataBackup(const SaveDataBackup* backup) {
    if (backup == nullptr || backup->dir_name == nullptr) {
        throw std::runtime_error("sceSaveDataBackup: null argument");
    }
    // The system copies the save directory to its backup area asynchronously; the copy itself is
    // not observable by the title, only its completion event.
    SaveDataEvent event{};
    event.type = SAVE_DATA_EVENT_TYPE_BACKUP;
    event.error_code = SAVE_DATA_OK;
    event.user_id = backup->user_id;
    if (backup->title_id != nullptr) event.title_id = *backup->title_id;
    event.dir_name = *backup->dir_name;
    std::lock_guard lock(g_state_mutex);
    g_events.push_back(event);
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataCommit(const SaveDataCommitParam* param) {
    (void)param;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataCreateTransactionResource(uint32_t size) {
    (void)size;
    return g_transaction_counter.fetch_add(1);
}

int APS5_VABI sceSaveDataDelete(const SaveDataDelete* del) {
    if (del == nullptr || del->dir_name == nullptr) {
        throw std::runtime_error("sceSaveDataDelete: null argument");
    }
    const std::string path = save_root() + "/" + std::string(del->dir_name->data);
    if (std::filesystem::is_directory(path)) {
        std::filesystem::remove_all(path);
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataDeleteTransactionResource(int32_t resource) {
    (void)resource;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
    if (cond == nullptr || result == nullptr) {
        throw std::runtime_error("sceSaveDataDirNameSearch: null argument");
    }
    result->hit_num = 0;
    result->set_num = 0;
    const char* pattern = (cond->dir_name != nullptr) ? cond->dir_name->data : nullptr;
    const std::string root = save_root();
    if (!std::filesystem::is_directory(root)) {
        return SAVE_DATA_OK;
    }
    std::uint32_t hit = 0;
    std::uint32_t set = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (!dir_name_match(name.c_str(), pattern)) {
            continue;
        }
        hit++;
        if (result->dir_names != nullptr && set < result->dir_names_num) {
            std::snprintf(result->dir_names[set].data, sizeof(result->dir_names[set].data), "%s", name.c_str());
            if (result->params != nullptr) {
                std::memset(&result->params[set], 0, sizeof(SaveDataParam));
            }
            set++;
        }
    }
    result->hit_num = hit;
    result->set_num = set;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataGetEventResult(const void* event_param, SaveDataEvent* event) {
    (void)event_param;
    if (event == nullptr) {
        throw std::runtime_error("sceSaveDataGetEventResult: null event");
    }
    std::lock_guard lock(g_state_mutex);
    if (g_events.empty()) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    *event = g_events.front();
    g_events.pop_front();
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataGetMountInfo(const SaveDataMountPoint* mount_point, SaveDataMountInfo* info) {
    if (mount_point == nullptr || info == nullptr) {
        throw std::runtime_error("sceSaveDataGetMountInfo: null argument");
    }
    if (find_slot_by_mount_point(mount_point->data) == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    std::memset(info, 0, sizeof(*info));
    info->blocks = SAVE_DATA_BLOCKS_MAX;
    info->free_blocks = SAVE_DATA_BLOCKS_MAX;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataGetParam(const SaveDataMountPoint* mount_point, uint32_t param_type, void* param_buf, size_t param_buf_size, size_t* got_size) {
    (void)param_type;
    if (mount_point == nullptr || param_buf == nullptr) {
        throw std::runtime_error("sceSaveDataGetParam: null argument");
    }
    if (find_slot_by_mount_point(mount_point->data) == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    std::memset(param_buf, 0, param_buf_size);
    if (got_size != nullptr) {
        *got_size = param_buf_size;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2* get_param) {
    if (get_param == nullptr) {
        throw std::runtime_error("sceSaveDataGetSaveDataMemory2: null argument");
    }
    std::lock_guard lock(g_state_mutex);
    const auto it = g_memories.find(get_param->slot_id);
    if (it == g_memories.end()) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    if (get_param->data != nullptr && get_param->data->buf != nullptr) {
        const auto& memory = it->second;
        if (get_param->data->offset > memory.size()) {
            return SAVE_DATA_ERROR_PARAMETER;
        }
        const auto bytes = std::min(get_param->data->buf_size, memory.size() - get_param->data->offset);
        std::memcpy(get_param->data->buf, memory.data() + get_param->data->offset, bytes);
    }
    if (get_param->param != nullptr) std::memset(get_param->param, 0, sizeof(SaveDataParam));
    if (get_param->icon != nullptr) get_param->icon->data_size = 0;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataInitialize3(const void* init) {
    (void)init;
    // NotImplemented_nid_no_patch(__func__);
    if (g_initialized) {
        return SAVE_DATA_ERROR_ALREADY_INITIALIZED;
    }
    g_initialized = true;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataLoadIcon(const SaveDataMountPoint* mount_point, SaveDataIcon* icon) {
    (void)icon;
    if (mount_point == nullptr) {
        throw std::runtime_error("sceSaveDataLoadIcon: null mount_point");
    }
    if (find_slot_by_mount_point(mount_point->data) == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    if (icon != nullptr) {
        icon->data_size = 0;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataMount3(const SaveDataMount3* mount, SaveDataMountResult* mount_result) {
    if (mount == nullptr || mount_result == nullptr || mount->dir_name == nullptr) {
        throw std::runtime_error("sceSaveDataMount3: null argument");
    }
    std::memset(mount_result, 0, sizeof(*mount_result));
    const bool create = (mount->mount_mode & SAVE_DATA_MOUNT_MODE_CREATE) != 0;
    const bool create2 = (mount->mount_mode & SAVE_DATA_MOUNT_MODE_CREATE2) != 0;
    const bool rdonly = (mount->mount_mode & SAVE_DATA_MOUNT_MODE_RDONLY) != 0;
    const bool rdwr = (mount->mount_mode & SAVE_DATA_MOUNT_MODE_RDWR) != 0;
    const bool open = !create && !create2 && (rdonly || rdwr);
    if (!create && !create2 && !open) {
        throw std::runtime_error("sceSaveDataMount3: unknown mount_mode");
    }
    const auto* nameEnd = static_cast<const char*>(std::memchr(mount->dir_name->data, '\0', sizeof(mount->dir_name->data)));
    if (nameEnd == nullptr) {
        throw std::runtime_error("sceSaveDataMount3: unterminated directory name");
    }
    const std::string dirName(mount->dir_name->data, static_cast<std::size_t>(nameEnd - mount->dir_name->data));
    if (dirName.empty() || dirName == "." || dirName == ".." || dirName.find_first_of("/\\:") != std::string::npos) {
        throw std::runtime_error("sceSaveDataMount3: invalid directory name");
    }
    const std::string real_path = save_root() + "/" + dirName;
    for (const auto& used : g_slots) {
        if (used.used && used.real_path == real_path) {
            return SAVE_DATA_ERROR_BUSY;
        }
    }
    const bool exists = std::filesystem::is_directory(real_path);
    if (create && exists) {
        return SAVE_DATA_ERROR_EXISTS;
    }
    if (open && !exists) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    int slot = find_free_slot();
    if (slot == -1) {
        return SAVE_DATA_ERROR_MOUNT_FULL;
    }
    if (create || create2) {
        std::filesystem::create_directories(real_path);
    }
    // The title gets a short mount point (16 bytes on the PS5) and opens files under it; the
    // path resolver maps it to the save directory, whose name may be far longer.
    const std::string mountPoint = "/_sm/" + std::to_string(slot);
    AddPathAlias_nid_no_patch(mountPoint.c_str(), std::filesystem::absolute(real_path).string().c_str());
    g_slots[slot].used = true;
    g_slots[slot].mount_point = mountPoint;
    g_slots[slot].real_path = real_path;
    std::memcpy(mount_result->mount_point.data, mountPoint.c_str(), mountPoint.size() + 1);
    mount_result->required_blocks = 0;
    mount_result->mount_status = (create || create2) ? 1u : 0u;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataPrepare(const SaveDataMountPoint* mount_point, const SaveDataPrepareParam* param) {
    (void)mount_point;
    (void)param;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataSaveIcon(const SaveDataMountPoint* mount_point, const SaveDataIcon* icon) {
    (void)icon;
    if (mount_point == nullptr) {
        throw std::runtime_error("sceSaveDataSaveIcon: null mount_point");
    }
    if (find_slot_by_mount_point(mount_point->data) == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataSaveIconByPath(const SaveDataMountPoint* mount_point, const char* path) {
    (void)mount_point;
    (void)path;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataSetParam(const SaveDataMountPoint* mount_point, uint32_t param_type, const void* param_buf, size_t param_buf_size) {
    (void)param_type;
    (void)param_buf;
    (void)param_buf_size;
    if (mount_point == nullptr) {
        throw std::runtime_error("sceSaveDataSetParam: null mount_point");
    }
    if (find_slot_by_mount_point(mount_point->data) == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2* set_param) {
    if (set_param == nullptr) {
        throw std::runtime_error("sceSaveDataSetSaveDataMemory2: null argument");
    }
    std::lock_guard lock(g_state_mutex);
    const auto it = g_memories.find(set_param->slot_id);
    if (it == g_memories.end()) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    auto& memory = it->second;
    if (set_param->data != nullptr) {
        const auto count = set_param->data_num == 0 ? 1u : set_param->data_num;
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& data = set_param->data[i];
            if (data.buf == nullptr) continue;
            if (data.offset > memory.size() || data.buf_size > memory.size() - data.offset) {
                return SAVE_DATA_ERROR_PARAMETER;
            }
            std::memcpy(memory.data() + data.offset, data.buf, data.buf_size);
        }
    }
    store_memory(set_param->slot_id, memory);
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2* setup_param, SaveDataMemorySetupResult* result) {
    if (setup_param == nullptr) {
        throw std::runtime_error("sceSaveDataSetupSaveDataMemory2: null argument");
    }
    std::lock_guard lock(g_state_mutex);
    auto& memory = g_memories[setup_param->slot_id];
    std::size_t existed = 0;
    if (memory.empty()) {
        std::ifstream file(memory_path(setup_param->slot_id), std::ios::binary);
        if (file) {
            memory.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            existed = memory.size();
        }
    } else {
        existed = memory.size();
    }
    if (memory.size() < setup_param->memory_size) {
        memory.resize(setup_param->memory_size, 0);
    }
    if (existed == 0) store_memory(setup_param->slot_id, memory);
    if (result != nullptr) {
        std::memset(result, 0, sizeof(*result));
        result->existed_memory_size = existed;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataSyncSaveDataMemory(const void* sync_param) {
    (void)sync_param;
    // Every Set already reached the file; nothing is buffered.
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataTerminate(void) {
    if (!g_initialized) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    if (any_slot_used()) {
        return SAVE_DATA_ERROR_BUSY;
    }
    g_initialized = false;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataTransferringMount(const SaveDataTransferringMount* mount, SaveDataMountResult* mount_result) {
    (void)mount;
    (void)mount_result;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataUmount2(uint32_t mode, const SaveDataMountPoint* mount_point) {
    (void)mode;
    if (mount_point == nullptr) {
        throw std::runtime_error("sceSaveDataUmount2: null mount_point");
    }
    int slot = find_slot_by_mount_point(mount_point->data);
    if (slot == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    RemovePathAlias_nid_no_patch(g_slots[slot].mount_point.c_str());
    g_slots[slot] = MountSlot{};
    return SAVE_DATA_OK;
}

}
