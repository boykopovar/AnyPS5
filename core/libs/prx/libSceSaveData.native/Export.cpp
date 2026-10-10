#include <algorithm>
#include <atomic>
#include <cstddef>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "SaveData.hpp"
#include "prx/libSceSaveData/SaveDataFile.hpp"

static constexpr char SAVE_DIR[] = "_sd";

static std::atomic<std::int32_t> g_transaction_counter{1};
static std::atomic<int> g_initializations{0};

static std::string save_root() {
    return std::string(SAVE_DIR);
}

// Events the title polls with sceSaveDataGetEventResult; a backup completes at once here.
static std::mutex g_state_mutex;
static std::deque<SaveDataEvent> g_events;
constexpr std::uint32_t SAVE_DATA_EVENT_TYPE_BACKUP = 2;

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

namespace {

// Save-data memory: one blob per user and slot, kept next to the save directory (_sd_mem/u<user>/slot<n>.bin),
// with a small .param sidecar holding the last SaveDataParam the title wrote. Keying by user as well as slot
// keeps two users' in-memory saves from colliding on the same console.
constexpr char MEM_DIR[] = "_sd_mem";
constexpr std::size_t MEM_MAX_SIZE = 32u * 1024u * 1024u;
std::mutex g_mem_mutex;

std::string mem_path(std::int32_t user_id, std::uint32_t slot, const char* ext) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s/u%d/slot%u.%s", MEM_DIR, static_cast<int>(user_id), static_cast<unsigned>(slot), ext);
    return buf;
}

bool file_size_of(const std::string& path, std::size_t* out) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return false;
    }
    const auto sz = std::filesystem::file_size(path, ec);
    if (ec) {
        return false;
    }
    *out = static_cast<std::size_t>(sz);
    return true;
}

// Atomic-ish write: write to a temp file then rename over the target, so a kill mid-write never
// leaves a torn save behind.
bool write_file_replace(const std::string& path, const std::vector<char>& data) {
    return savedata::replace_file(path, data.data(), data.size());
}

bool read_file_all(const std::string& path, std::vector<char>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(0, std::ios::end);
    const auto n = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    out.resize(n);
    if (n != 0) {
        f.read(out.data(), static_cast<std::streamsize>(n));
    }
    return static_cast<bool>(f);
}

std::string param_path(const std::string& real_path) {
    return real_path + ".param";
}

bool read_param_file(const std::string& path, SaveDataParam* param) {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    std::vector<char> bytes;
    if (!read_file_all(path, bytes)) {
        throw std::runtime_error("SaveData: cannot read param file " + path);
    }
    if (bytes.size() != sizeof(SaveDataParam)) {
        throw std::runtime_error("SaveData: param file " + path + " is " + std::to_string(bytes.size()) + " bytes, expected " + std::to_string(sizeof(SaveDataParam)) + " (outdated format)");
    }
    std::memcpy(param, bytes.data(), sizeof(SaveDataParam));
    return true;
}

SaveDataParam load_param(const std::string& real_path) {
    SaveDataParam param{};
    read_param_file(param_path(real_path), &param);
    if (param.mtime == 0) {
        std::error_code ec;
        const auto written = std::filesystem::last_write_time(real_path, ec);
        if (!ec) {
            const auto system = std::filesystem::file_time_type::clock::to_sys(written);
            param.mtime = std::chrono::duration_cast<std::chrono::seconds>(system.time_since_epoch()).count();
        }
    }
    return param;
}

bool store_param(const std::string& real_path, const SaveDataParam& param) {
    std::vector<char> bytes(sizeof(param));
    std::memcpy(bytes.data(), &param, sizeof(param));
    return write_file_replace(param_path(real_path), bytes);
}

struct ParamField {
    std::size_t offset;
    std::size_t size;
    bool text;
};

bool param_field(std::uint32_t param_type, ParamField* field) {
    switch (param_type) {
    case SAVE_DATA_PARAM_TYPE_ALL: *field = {0, sizeof(SaveDataParam), false}; return true;
    case SAVE_DATA_PARAM_TYPE_TITLE: *field = {offsetof(SaveDataParam, title), sizeof(SaveDataParam::title), true}; return true;
    case SAVE_DATA_PARAM_TYPE_SUB_TITLE: *field = {offsetof(SaveDataParam, sub_title), sizeof(SaveDataParam::sub_title), true}; return true;
    case SAVE_DATA_PARAM_TYPE_DETAIL: *field = {offsetof(SaveDataParam, detail), sizeof(SaveDataParam::detail), true}; return true;
    case SAVE_DATA_PARAM_TYPE_USER_PARAM: *field = {offsetof(SaveDataParam, user_param), sizeof(SaveDataParam::user_param), false}; return true;
    case SAVE_DATA_PARAM_TYPE_MTIME: *field = {offsetof(SaveDataParam, mtime), sizeof(SaveDataParam::mtime), false}; return true;
    default: return false;
    }
}

int traceLimit() {
    static const int limit = std::getenv("APS5_SAVEDATA_TRACE") != nullptr ? std::numeric_limits<int>::max() : 3;
    return limit;
}
#define SAVEDATA_TRACE(...) \
    do { \
        static std::atomic<int> traceCount{0}; \
        if (traceCount.fetch_add(1, std::memory_order_relaxed) < traceLimit()) { \
            std::fprintf(stderr, "[SAVEDATA:native] " __VA_ARGS__); \
            std::fputc('\n', stderr); \
            std::fflush(stderr); \
        } \
    } while (0)
}  // namespace

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

static int deleteSave(const SaveDataDelete* del) {
    if (del == nullptr || del->dir_name == nullptr) {
        throw std::runtime_error("sceSaveDataDelete: null argument");
    }
    const auto* nameEnd = static_cast<const char*>(std::memchr(del->dir_name->data, '\0', sizeof(del->dir_name->data)));
    if (nameEnd == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    const std::string dirName(del->dir_name->data, static_cast<std::size_t>(nameEnd - del->dir_name->data));
    if (dirName.empty() || dirName == "." || dirName == ".." || dirName.find_first_of("/\\:") != std::string::npos) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    const std::string path = save_root() + "/" + dirName;
    std::lock_guard lock(g_slots_mutex);
    for (const auto& slot : g_slots) {
        if (!slot.used) {
            continue;
        }
        if (slot.real_path == path) {
            return SAVE_DATA_ERROR_BUSY;
        }
        std::error_code ec;
        const bool same_directory = std::filesystem::equivalent(slot.real_path, path, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) {
            throw std::filesystem::filesystem_error("sceSaveDataDelete: failed to compare save paths", slot.real_path, path, ec);
        }
        if (same_directory) {
            return SAVE_DATA_ERROR_BUSY;
        }
    }
    if (std::filesystem::is_directory(path)) {
        std::filesystem::remove_all(path);
    }
    std::error_code ec;
    std::filesystem::remove(param_path(path), ec);
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataDelete(const SaveDataDelete* del) {
    const int rc = deleteSave(del);
    SAVEDATA_TRACE("delete dir=%s -> 0x%08x", del != nullptr && del->dir_name != nullptr ? del->dir_name->data : "(null)", static_cast<unsigned>(rc));
    return rc;
}

int APS5_VABI sceSaveDataDeleteTransactionResource(int32_t resource) {
    (void)resource;
    return SAVE_DATA_OK;
}

static int dirNameSearch(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
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
                result->params[set] = load_param(entry.path().string());
            }
            set++;
        }
    }
    result->hit_num = hit;
    result->set_num = set;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
    const int rc = dirNameSearch(cond, result);
    SAVEDATA_TRACE("dirNameSearch user=%d pattern=%s hit=%u set=%u -> 0x%08x", cond != nullptr ? cond->user_id : -1, cond != nullptr && cond->dir_name != nullptr ? cond->dir_name->data : "(all)", result != nullptr ? result->hit_num : 0u, result != nullptr ? result->set_num : 0u, static_cast<unsigned>(rc));
    return rc;
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

static int getMountInfo(const SaveDataMountPoint* mount_point, SaveDataMountInfo* info) {
    if (mount_point == nullptr || info == nullptr) {
        throw std::runtime_error("sceSaveDataGetMountInfo: null argument");
    }
    std::lock_guard lock(g_slots_mutex);
    if (find_slot_by_mount_point(mount_point->data) == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    std::memset(info, 0, sizeof(*info));
    info->blocks = SAVE_DATA_BLOCKS_MAX;
    info->free_blocks = SAVE_DATA_BLOCKS_MAX;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataGetMountInfo(const SaveDataMountPoint* mount_point, SaveDataMountInfo* info) {
    const int rc = getMountInfo(mount_point, info);
    SAVEDATA_TRACE("getMountInfo point=%s -> 0x%08x", mount_point != nullptr ? mount_point->data : "(null)", static_cast<unsigned>(rc));
    return rc;
}

static int getParam(const SaveDataMountPoint* mount_point, uint32_t param_type, void* param_buf, size_t param_buf_size, size_t* got_size) {
    if (mount_point == nullptr || param_buf == nullptr) {
        throw std::runtime_error("sceSaveDataGetParam: null argument");
    }
    std::lock_guard lock(g_slots_mutex);
    const int slot = find_slot_by_mount_point(mount_point->data);
    if (slot == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    ParamField field{};
    if (!param_field(param_type, &field) || param_buf_size < field.size) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    const SaveDataParam param = load_param(g_slots[slot].real_path);
    std::memcpy(param_buf, reinterpret_cast<const char*>(&param) + field.offset, field.size);
    if (got_size != nullptr) {
        *got_size = field.size;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataGetParam(const SaveDataMountPoint* mount_point, uint32_t param_type, void* param_buf, size_t param_buf_size, size_t* got_size) {
    const int rc = getParam(mount_point, param_type, param_buf, param_buf_size, got_size);
    SAVEDATA_TRACE("getParam point=%s type=%u size=%zu -> 0x%08x got=%zu", mount_point != nullptr ? mount_point->data : "(null)", param_type, param_buf_size, static_cast<unsigned>(rc), got_size != nullptr ? *got_size : static_cast<size_t>(0));
    return rc;
}

static int getSaveDataMemory2(SaveDataMemoryGet2* get_param) {
    if (get_param == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    std::lock_guard<std::mutex> lk(g_mem_mutex);
    const std::string path = mem_path(get_param->user_id, get_param->slot_id, "bin");
    std::size_t size = 0;
    if (!file_size_of(path, &size)) {
        return SAVE_DATA_ERROR_MEMORY_NOT_READY;
    }
    const SaveDataMemoryData* d = get_param->data;
    if (d != nullptr && d->buf_size != 0) {
        if (d->buf == nullptr || d->offset > size || d->buf_size > size - d->offset) {
            return SAVE_DATA_ERROR_PARAMETER;
        }
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
        f.seekg(static_cast<std::streamoff>(d->offset));
        f.read(static_cast<char*>(d->buf), static_cast<std::streamsize>(d->buf_size));
        if (!f) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
    }
    if (get_param->param != nullptr) {
        std::memset(get_param->param, 0, sizeof(SaveDataParam));
        read_param_file(mem_path(get_param->user_id, get_param->slot_id, "param"), get_param->param);
    }
    if (get_param->icon != nullptr) {
        get_param->icon->data_size = 0;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2* get_param) {
    const int rc = getSaveDataMemory2(get_param);
    SAVEDATA_TRACE("getMemory2 user=%d slot=%u -> 0x%08x", get_param != nullptr ? get_param->user_id : -1, get_param != nullptr ? get_param->slot_id : 0u, static_cast<unsigned>(rc));
    return rc;
}

int APS5_VABI sceSaveDataInitialize3(const void* init) {
    (void)init;
    ++g_initializations;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataLoadIcon(const SaveDataMountPoint* mount_point, SaveDataIcon* icon) {
    (void)icon;
    if (mount_point == nullptr) {
        throw std::runtime_error("sceSaveDataLoadIcon: null mount_point");
    }
    std::lock_guard lock(g_slots_mutex);
    if (find_slot_by_mount_point(mount_point->data) == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    if (icon != nullptr) {
        icon->data_size = 0;
    }
    return SAVE_DATA_OK;
}

static int mount3(const SaveDataMount3* mount, SaveDataMountResult* mount_result) {
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
    std::lock_guard lock(g_slots_mutex);
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
    const std::string mountPoint = "/savedata" + std::to_string(slot);
    AddPathAlias_nid_no_patch(mountPoint.c_str(), std::filesystem::absolute(real_path).string().c_str());
    g_slots[slot].used = true;
    g_slots[slot].mount_point = mountPoint;
    g_slots[slot].real_path = real_path;
    std::memcpy(mount_result->mount_point.data, mountPoint.c_str(), mountPoint.size() + 1);
    mount_result->required_blocks = 0;
    mount_result->mount_status = (create || (create2 && !exists)) ? 1u : 0u;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataMount3(const SaveDataMount3* mount, SaveDataMountResult* mount_result) {
    const int rc = mount3(mount, mount_result);
    SAVEDATA_TRACE("mount3 user=%d dir=%s mode=0x%x blocks=%llu -> 0x%08x point=%s status=%u", mount != nullptr ? mount->user_id : -1, mount != nullptr && mount->dir_name != nullptr ? mount->dir_name->data : "(null)", mount != nullptr ? mount->mount_mode : 0u, mount != nullptr ? static_cast<unsigned long long>(mount->blocks) : 0ull, static_cast<unsigned>(rc), mount_result != nullptr ? mount_result->mount_point.data : "", mount_result != nullptr ? mount_result->mount_status : 0u);
    return rc;
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
    std::lock_guard lock(g_slots_mutex);
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

static int setParam(const SaveDataMountPoint* mount_point, uint32_t param_type, const void* param_buf, size_t param_buf_size) {
    if (mount_point == nullptr || param_buf == nullptr) {
        throw std::runtime_error("sceSaveDataSetParam: null argument");
    }
    std::lock_guard lock(g_slots_mutex);
    const int slot = find_slot_by_mount_point(mount_point->data);
    if (slot == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    ParamField field{};
    if (!param_field(param_type, &field) || param_buf_size == 0 || (!field.text && param_buf_size < field.size)) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    SaveDataParam param = load_param(g_slots[slot].real_path);
    char* destination = reinterpret_cast<char*>(&param) + field.offset;
    if (field.text) {
        const std::size_t length = std::min(param_buf_size, field.size - 1);
        const char* text = static_cast<const char*>(param_buf);
        const char* end = static_cast<const char*>(std::memchr(text, '\0', length));
        std::memset(destination, 0, field.size);
        std::memcpy(destination, text, end != nullptr ? static_cast<std::size_t>(end - text) : length);
    } else {
        std::memcpy(destination, param_buf, field.size);
    }
    if (param_type != SAVE_DATA_PARAM_TYPE_MTIME) {
        param.mtime = static_cast<std::int64_t>(std::time(nullptr));
    }
    return store_param(g_slots[slot].real_path, param) ? SAVE_DATA_OK : SAVE_DATA_ERROR_INTERNAL;
}

int APS5_VABI sceSaveDataSetParam(const SaveDataMountPoint* mount_point, uint32_t param_type, const void* param_buf, size_t param_buf_size) {
    const int rc = setParam(mount_point, param_type, param_buf, param_buf_size);
    SAVEDATA_TRACE("setParam point=%s type=%u size=%zu -> 0x%08x", mount_point != nullptr ? mount_point->data : "(null)", param_type, param_buf_size, static_cast<unsigned>(rc));
    return rc;
}

static int setSaveDataMemory2(const SaveDataMemorySet2* set_param) {
    if (set_param == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    std::lock_guard<std::mutex> lk(g_mem_mutex);
    const std::string path = mem_path(set_param->user_id, set_param->slot_id, "bin");
    std::size_t size = 0;
    if (!file_size_of(path, &size)) {
        return SAVE_DATA_ERROR_MEMORY_NOT_READY;
    }
    // Validate every range first so a bad entry never leaves a partial write.
    const std::uint32_t n = set_param->data != nullptr ? (set_param->data_num != 0 ? set_param->data_num : 1u) : 0u;
    for (std::uint32_t i = 0; i < n; i++) {
        const SaveDataMemoryData& d = set_param->data[i];
        if (d.buf_size == 0) {
            continue;
        }
        if (d.buf == nullptr || d.offset > size || d.buf_size > size - d.offset) {
            return SAVE_DATA_ERROR_PARAMETER;
        }
    }
    if (n != 0) {
        std::vector<char> memory;
        if (!read_file_all(path, memory) || memory.size() != size) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
        for (std::uint32_t i = 0; i < n; i++) {
            const SaveDataMemoryData& d = set_param->data[i];
            if (d.buf_size == 0) {
                continue;
            }
            std::memcpy(memory.data() + d.offset, d.buf, d.buf_size);
        }
        if (!write_file_replace(path, memory)) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
    }
    if (set_param->param != nullptr) {
        std::vector<char> pd(sizeof(SaveDataParam));
        std::memcpy(pd.data(), set_param->param, sizeof(SaveDataParam));
        if (!write_file_replace(mem_path(set_param->user_id, set_param->slot_id, "param"), pd)) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2* set_param) {
    const int rc = setSaveDataMemory2(set_param);
    SAVEDATA_TRACE("setMemory2 user=%d slot=%u num=%u -> 0x%08x", set_param != nullptr ? set_param->user_id : -1, set_param != nullptr ? set_param->slot_id : 0u, set_param != nullptr ? set_param->data_num : 0u, static_cast<unsigned>(rc));
    return rc;
}

static int setupSaveDataMemory2(const SaveDataMemorySetup2* setup_param, SaveDataMemorySetupResult* result) {
    if (setup_param == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    if (setup_param->memory_size == 0 || setup_param->memory_size > MEM_MAX_SIZE) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    std::lock_guard<std::mutex> lk(g_mem_mutex);
    const std::string path = mem_path(setup_param->user_id, setup_param->slot_id, "bin");
    std::size_t existed = 0;
    const bool have = file_size_of(path, &existed);
    if (!have) {
        existed = 0;
    }
    // First run: create a zero-filled blob and report existed size 0 so the title treats it as a new save.
    if (!have || existed < setup_param->memory_size) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
        std::vector<char> data;
        if (have && !read_file_all(path, data)) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
        data.resize(setup_param->memory_size, 0);
        if (!write_file_replace(path, data)) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
        if (setup_param->init_param != nullptr && (setup_param->option & 1u) != 0) {
            std::vector<char> pd(sizeof(SaveDataParam));
            std::memcpy(pd.data(), setup_param->init_param, sizeof(SaveDataParam));
            write_file_replace(mem_path(setup_param->user_id, setup_param->slot_id, "param"), pd);
        }
    }
    if (result != nullptr) {
        std::memset(result, 0, sizeof(*result));
        result->existed_memory_size = have ? existed : 0;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2* setup_param, SaveDataMemorySetupResult* result) {
    const int rc = setupSaveDataMemory2(setup_param, result);
    SAVEDATA_TRACE("setupMemory2 user=%d slot=%u size=%zu -> 0x%08x existed=%zu", setup_param != nullptr ? setup_param->user_id : -1, setup_param != nullptr ? setup_param->slot_id : 0u, setup_param != nullptr ? setup_param->memory_size : static_cast<size_t>(0), static_cast<unsigned>(rc), result != nullptr ? result->existed_memory_size : static_cast<size_t>(0));
    return rc;
}

int APS5_VABI sceSaveDataSyncSaveDataMemory(const void* sync_param) {
    if (sync_param == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    // Every Set already writes straight through to disk; sync only has to confirm the memory exists.
    const std::int32_t user_id = *static_cast<const std::int32_t*>(sync_param);
    const std::uint32_t slot_id = reinterpret_cast<const std::uint32_t*>(sync_param)[1];
    std::lock_guard<std::mutex> lk(g_mem_mutex);
    std::size_t size = 0;
    if (!file_size_of(mem_path(user_id, slot_id, "bin"), &size)) {
        return SAVE_DATA_ERROR_MEMORY_NOT_READY;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataTerminate(void) {
    std::lock_guard lock(g_slots_mutex);
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    if (g_initializations == 1 && any_slot_used()) {
        return SAVE_DATA_ERROR_BUSY;
    }
    --g_initializations;
    return SAVE_DATA_OK;
}

static int transferringMount(const SaveDataTransferringMount* mount, SaveDataMountResult* mount_result) {
    (void)mount;
    if (mount_result != nullptr) {
        std::memset(mount_result, 0, sizeof(*mount_result));
    }
    // No PS4-to-PS5 transfer data exists on this console.
    return SAVE_DATA_ERROR_NOT_FOUND;
}

int APS5_VABI sceSaveDataTransferringMount(const SaveDataTransferringMount* mount, SaveDataMountResult* mount_result) {
    const int rc = transferringMount(mount, mount_result);
    SAVEDATA_TRACE("transferringMount -> 0x%08x", static_cast<unsigned>(rc));
    return rc;
}

static int umount2(uint32_t mode, const SaveDataMountPoint* mount_point) {
    (void)mode;
    if (mount_point == nullptr) {
        throw std::runtime_error("sceSaveDataUmount2: null mount_point");
    }
    std::lock_guard lock(g_slots_mutex);
    int slot = find_slot_by_mount_point(mount_point->data);
    if (slot == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    RemovePathAlias_nid_no_patch(g_slots[slot].mount_point.c_str());
    g_slots[slot] = MountSlot{};
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataUmount2(uint32_t mode, const SaveDataMountPoint* mount_point) {
    const int rc = umount2(mode, mount_point);
    SAVEDATA_TRACE("umount2 mode=0x%x point=%s -> 0x%08x", mode, mount_point != nullptr ? mount_point->data : "(null)", static_cast<unsigned>(rc));
    return rc;
}

int APS5_VABI sceSaveDataTransferringMountPs4(const SaveDataTransferringMount* mount, SaveDataMountResult* mount_result) {
    const int rc = transferringMount(mount, mount_result);
    SAVEDATA_TRACE("transferringMountPs4 -> 0x%08x", static_cast<unsigned>(rc));
    return rc;
}

static int dirNameSearchPs4(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
    if (cond == nullptr || result == nullptr) {
        throw std::runtime_error("sceSaveDataDirNameSearchPs4: null argument");
    }
    result->hit_num = 0;
    result->set_num = 0;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataDirNameSearchPs4(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
    const int rc = dirNameSearchPs4(cond, result);
    SAVEDATA_TRACE("dirNameSearchPs4 user=%d -> 0x%08x", cond->user_id, static_cast<unsigned>(rc));
    return rc;
}

int APS5_VABI sceSaveDataConvert() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetConvertProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataCancel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
