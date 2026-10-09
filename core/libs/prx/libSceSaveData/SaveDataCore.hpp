#ifndef CORE_LIBS_PRX_LIBSCESAVEDATA_SAVEDATACORE_HPP
#define CORE_LIBS_PRX_LIBSCESAVEDATA_SAVEDATACORE_HPP

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceSaveData/SaveDataFile.hpp"

namespace savedata {

extern "C" {
int APS5_VABI sceKernelCreateEventFlag(KernelEventFlag* ef, const char* name, std::uint32_t attr,
                                       std::uint64_t init_pattern, const void* param);
int APS5_VABI sceKernelDeleteEventFlag(KernelEventFlag ef);
int APS5_VABI sceKernelSetEventFlag(KernelEventFlag ef, std::uint64_t bit_pattern);
int APS5_VABI sceKernelCancelEventFlag(KernelEventFlag ef, std::uint64_t set_pattern, int* num_wait_threads);
}

namespace {

constexpr std::uint32_t SD_FACILITY = 0x809F0000u;
constexpr int sd_error(std::uint32_t code) { return static_cast<int>(SD_FACILITY + code); }

constexpr int SD_OK = 0;
constexpr int SD_ERROR_PARAMETER = sd_error(0x00);
constexpr int SD_ERROR_NOT_INITIALIZED = sd_error(0x01);
constexpr int SD_ERROR_OUT_OF_MEMORY = sd_error(0x02);
constexpr int SD_ERROR_BUSY = sd_error(0x03);
constexpr int SD_ERROR_NOT_MOUNTED = sd_error(0x04);
constexpr int SD_ERROR_NO_PERMISSION = sd_error(0x05);
constexpr int SD_ERROR_FINGERPRINT_MISMATCH = sd_error(0x06);
constexpr int SD_ERROR_EXISTS = sd_error(0x07);
constexpr int SD_ERROR_NOT_FOUND = sd_error(0x08);
constexpr int SD_ERROR_NO_SPACE_FS = sd_error(0x0A);
constexpr int SD_ERROR_INTERNAL = sd_error(0x0B);
constexpr int SD_ERROR_MOUNT_FULL = sd_error(0x0C);
constexpr int SD_ERROR_BAD_MOUNTED = sd_error(0x0D);
constexpr int SD_ERROR_FILE_NOT_FOUND = sd_error(0x0E);
constexpr int SD_ERROR_BROKEN = sd_error(0x0F);
constexpr int SD_ERROR_INVALID_LOGIN_USER = sd_error(0x11);
constexpr int SD_ERROR_MEMORY_NOT_READY = sd_error(0x12);
constexpr int SD_ERROR_BACKUP_BUSY = sd_error(0x13);
constexpr int SD_ERROR_NOT_REGIST_CALLBACK = sd_error(0x15);
constexpr int SD_ERROR_BUSY_FOR_SAVING = sd_error(0x16);
constexpr int SD_ERROR_LIMITATION_OVER = sd_error(0x17);
constexpr int SD_ERROR_EVENT_BUSY = sd_error(0x18);

constexpr std::uint32_t SD_MOUNT_RDONLY = 1u;
constexpr std::uint32_t SD_MOUNT_RDWR = 2u;
constexpr std::uint32_t SD_MOUNT_CREATE = 4u;
constexpr std::uint32_t SD_MOUNT_COPY_ICON = 16u;
constexpr std::uint32_t SD_MOUNT_CREATE2 = 32u;

constexpr std::uint32_t SD_UMOUNT_BACKUP_ASYNC = 1u << 16u;
constexpr std::uint32_t SD_COMMIT_BACKUP_ASYNC = 1u;

constexpr std::uint32_t SD_EVENT_UMOUNT_BACKUP_END = 1u;
constexpr std::uint32_t SD_EVENT_BACKUP_END = 2u;
constexpr std::uint32_t SD_EVENT_MEMORY_SYNC_END = 3u;
constexpr std::uint32_t SD_EVENT_COMMIT_BACKUP_END = 4u;

constexpr std::uint64_t SD_BLOCKS_MIN = 1;
constexpr std::uint64_t SD_BLOCKS_MAX = 32768;

constexpr std::uint32_t SD_MEMORY_SLOTS = 4;
constexpr std::uint32_t SD_MEMORY_MAX_MOUNTED = 4;
constexpr std::size_t SD_MEMORY_MAX_SIZE = 32u * 1024u * 1024u;
constexpr std::uint32_t SD_MEMORY_SET_PARAM = 1u;
constexpr std::uint32_t SD_MEMORY_DOUBLE_BUFFER = 2u;
constexpr std::uint32_t SD_MEMORY_MAX_DATA_NUM = 5;

constexpr std::uint32_t SD_PARAM_WHOLE = 0;
constexpr std::uint32_t SD_PARAM_TITLE = 1;
constexpr std::uint32_t SD_PARAM_SUB_TITLE = 2;
constexpr std::uint32_t SD_PARAM_DETAIL = 3;
constexpr std::uint32_t SD_PARAM_USER_PARAM = 4;
constexpr std::uint32_t SD_PARAM_MTIME = 5;

constexpr std::uint32_t SD_APP_STATUS_NONE = 0;
constexpr std::uint32_t SD_APP_STATUS_DELETED = 1;
constexpr std::uint32_t SD_APP_STATUS_LOCAL = 2;
constexpr std::uint32_t SD_APP_STATUS_ONLINE = 4;
constexpr std::uint32_t SD_APP_STATUS_ANY = 0x7FFFFFFFu;

constexpr char SD_ROOT_GUEST[] = "/_sd";
constexpr char SD_SUBDIR[] = "sce_sys";
constexpr char SD_MEMORY_PREFIX[] = "sce_sdmemory";
constexpr std::size_t SD_MOUNT_PREFIX_LEN = sizeof(SD_ROOT_GUEST);
constexpr std::size_t SD_MOUNT_POINT_CAPACITY = sizeof(SaveDataMountPoint) - SD_MOUNT_PREFIX_LEN - 1;

struct MemoryData {
    void* buf;
    std::size_t buf_size;
    std::int64_t offset;
    std::uint8_t reserved[40];
};
static_assert(sizeof(MemoryData) == 64, "SceSaveDataMemoryData is 64 bytes");

struct MemorySync {
    std::int32_t user_id;
    std::uint32_t slot_id;
    std::uint32_t option;
    std::uint8_t reserved[28];
};
static_assert(sizeof(MemorySync) == 40, "SceSaveDataMemorySync is 40 bytes");

struct EventParam {
    std::size_t size;
    std::uint8_t reserved[24];
};

struct MountSlot {
    bool used = false;
    std::string mount_point;
    std::string dir_name;
    std::filesystem::path directory;
};

struct MemorySlot {
    std::vector<char> data;
    SaveDataParam param {};
    std::uint32_t option = 0;
    bool dirty = false;
};

struct PendingEvent {
    SaveDataEvent record {};
    KernelEventFlag flag = nullptr;
};

struct State {
    std::mutex mutex;
    std::uint32_t instances = 0;
    std::array<MountSlot, 16> slots {};
    std::deque<PendingEvent> events;
    std::unordered_map<std::string, MemorySlot> memory;
    std::map<std::string, std::string> aliases;
    std::int32_t next_transaction_resource = 1;
    std::set<std::int32_t> live_transaction_resources;
    void* event_callback = nullptr;
    void* event_callback_userdata = nullptr;
    bool aliases_loaded = false;
};

State& state() {
    static State instance;
    return instance;
}

bool is_ready() {
    return state().instances != 0;
}

bool valid_dir_name(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    std::size_t length = 0;
    while (name[length] != '\0') {
        length++;
        if (length >= sizeof(SceSaveDataDirName::data)) {
            return false;
        }
    }
    if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) {
        return false;
    }
    for (const char* p = name; *p != '\0'; p++) {
        const bool ok = std::isalnum(static_cast<unsigned char>(*p)) != 0 || *p == '_' || *p == '-' ||
                        *p == '.' || *p == '@';
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool bounded_string_ok(const char* data, std::size_t capacity) {
    return std::memchr(data, '\0', capacity) != nullptr;
}

const std::filesystem::path& save_root() {
    static const std::filesystem::path root = ResolvePath_nid_no_patch(SD_ROOT_GUEST);
    return root;
}

bool ensure_save_root(std::error_code& error) {
    if (std::filesystem::create_directories(save_root(), error)) RecordWrittenPath_nid_no_patch(save_root());
    if (error) {
        error.clear();
    }
    return std::filesystem::is_directory(save_root(), error);
}

void load_aliases_locked(State& st) {
    if (st.aliases_loaded) {
        return;
    }
    st.aliases_loaded = true;
    std::ifstream file(save_root() / "_aliases", std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t sep = line.find('\t');
        if (sep == std::string::npos || sep == 0) {
            continue;
        }
        st.aliases[line.substr(0, sep)] = line.substr(sep + 1);
    }
}

void store_aliases_locked(State& st) {
    std::error_code error;
    ensure_save_root(error);
    std::ofstream file(save_root() / "_aliases", std::ios::binary | std::ios::trunc);
    for (const auto& [alias, dir_name] : st.aliases) {
        file << alias << '\t' << dir_name << '\n';
    }
    RecordWrittenPath_nid_no_patch(save_root() / "_aliases");
}

std::string save_alias_locked(State& st, const std::string& dir_name) {
    if (dir_name.size() <= SD_MOUNT_POINT_CAPACITY) {
        return dir_name;
    }
    load_aliases_locked(st);
    const std::string base = dir_name.substr(0, 6);
    std::uint32_t hash = 2166136261u;
    for (const char c : dir_name) {
        hash = (hash ^ static_cast<std::uint8_t>(c)) * 16777619u;
    }
    for (std::uint32_t attempt = 0; attempt < 4096u; attempt++) {
        char tail[8];
        std::snprintf(tail, sizeof(tail), "~%03X", static_cast<unsigned>((hash + attempt) & 0xFFFu));
        const std::string candidate = base + tail;
        const auto found = st.aliases.find(candidate);
        if (found == st.aliases.end()) {
            st.aliases[candidate] = dir_name;
            store_aliases_locked(st);
            return candidate;
        }
        if (found->second == dir_name) {
            return candidate;
        }
    }
    return base;
}

std::string real_dir_name_locked(State& st, const std::string& alias) {
    load_aliases_locked(st);
    const auto found = st.aliases.find(alias);
    return found != st.aliases.end() ? found->second : alias;
}

std::string mount_point_for(const std::string& alias) {
    return std::string(SD_ROOT_GUEST) + "/" + alias;
}

std::filesystem::path save_directory(const std::string& alias) {
    return save_root() / alias;
}

std::filesystem::path save_metadata_directory(const std::filesystem::path& directory) {
    return directory / SD_SUBDIR;
}

std::filesystem::path save_param_path(const std::filesystem::path& directory) {
    return save_metadata_directory(directory) / "param.bin";
}

std::filesystem::path save_blocks_path(const std::filesystem::path& directory) {
    return save_metadata_directory(directory) / "blocks.bin";
}

bool write_blob(const std::filesystem::path& path, const void* data, std::size_t size) {
    if (!replace_file(path, data, size)) return false;
    RecordWrittenPath_nid_no_patch(path);
    return true;
}

bool read_blob(const std::filesystem::path& path, void* data, std::size_t size) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(file.gcount()) == size;
}

int write_u64(const std::filesystem::path& path, std::uint64_t value) {
    return write_blob(path, &value, sizeof(value)) ? SD_OK : SD_ERROR_INTERNAL;
}

int read_u64(const std::filesystem::path& path, std::uint64_t* value) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return SD_ERROR_BROKEN;
    }
    if (std::filesystem::file_size(path, error) != sizeof(std::uint64_t) || error) {
        return SD_ERROR_BROKEN;
    }
    return read_blob(path, value, sizeof(*value)) ? SD_OK : SD_ERROR_BROKEN;
}

int load_save_param(const std::filesystem::path& directory, SaveDataParam* param) {
    std::memset(param, 0, sizeof(*param));
    const int status = read_blob(save_param_path(directory), param, sizeof(*param)) ? SD_OK : SD_ERROR_NOT_FOUND;
    if (status == SD_OK) {
        std::int64_t newest = 0;
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator it(directory,
                 std::filesystem::directory_options::skip_permission_denied, error), end;
             it != end; it.increment(error)) {
            if (error) {
                error.clear();
                continue;
            }
            std::error_code file_error;
            if (it->is_regular_file(file_error) && !file_error) {
                const auto written = std::filesystem::last_write_time(it->path(), file_error);
                if (!file_error) {
                    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                        std::filesystem::file_time_type::clock::now() - written).count();
                    const auto now = static_cast<std::int64_t>(std::time(nullptr));
                    newest = std::max(newest, now - static_cast<std::int64_t>(seconds));
                }
            }
        }
        param->mtime = newest;
    }
    return status;
}

bool save_is_mounted_locked(const State& st, const std::filesystem::path& directory) {
    for (const auto& slot : st.slots) {
        if (slot.used && slot.directory == directory) {
            return true;
        }
    }
    return false;
}

int slot_of_mount_point_locked(const State& st, const char* mount_point) {
    for (std::size_t i = 0; i < st.slots.size(); i++) {
        if (st.slots[i].used && st.slots[i].mount_point == mount_point) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int free_slot_locked(const State& st) {
    for (std::size_t i = 0; i < st.slots.size(); i++) {
        if (!st.slots[i].used) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void queue_event_locked(State& st, std::uint32_t type, std::int32_t user_id,
                        const SceSaveDataTitleId* title_id, const SceSaveDataDirName* dir_name,
                        int error_code, KernelEventFlag flag) {
    PendingEvent pending;
    pending.record = {};
    pending.record.type = type;
    pending.record.error_code = error_code;
    pending.record.user_id = user_id;
    if (title_id != nullptr) {
        std::memcpy(&pending.record.title_id, title_id, sizeof(pending.record.title_id));
    }
    if (dir_name != nullptr) {
        std::memcpy(&pending.record.dir_name, dir_name, sizeof(pending.record.dir_name));
    }
    pending.flag = flag;
    st.events.push_back(pending);
    while (st.events.size() > 64) {
        if (st.events.front().flag != nullptr) {
            sceKernelDeleteEventFlag(st.events.front().flag);
        }
        st.events.pop_front();
    }
}

KernelEventFlag create_signalled_flag(const char* name) {
    KernelEventFlag flag = nullptr;
    if (sceKernelCreateEventFlag(&flag, name, 0x20u, 0, nullptr) != 0) {
        return nullptr;
    }
    sceKernelSetEventFlag(flag, 1u);
    return flag;
}

int initialize_internal(const void* init) {
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    st.instances++;
    return SD_OK;
}

int terminate_internal() {
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    if (st.instances == 0) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (std::any_of(st.slots.begin(), st.slots.end(),
            [](const MountSlot& slot) { return slot.used; }) || !st.memory.empty()) {
        return SD_ERROR_BUSY;
    }
    st.instances--;
    if (st.instances == 0) {
        for (auto& pending : st.events) {
            if (pending.flag != nullptr) {
                sceKernelDeleteEventFlag(pending.flag);
            }
        }
        st.events.clear();
        st.memory.clear();
        st.live_transaction_resources.clear();
    }
    return SD_OK;
}

int mount_internal(const SceSaveDataDirName* dir_name, std::uint32_t mount_mode,
                   std::uint64_t blocks, std::int32_t user_id, SaveDataMountResult* mount_result,
                   const char* api) {
    if (mount_result == nullptr) {
        return SD_ERROR_PARAMETER;
    }
    std::memset(mount_result, 0, sizeof(*mount_result));
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (dir_name == nullptr || !bounded_string_ok(dir_name->data, sizeof(dir_name->data)) ||
        !valid_dir_name(dir_name->data)) {
        return SD_ERROR_PARAMETER;
    }
    if (user_id < 0) {
        return SD_ERROR_INVALID_LOGIN_USER;
    }
    const bool create = (mount_mode & SD_MOUNT_CREATE) != 0;
    const bool create_or_open = (mount_mode & SD_MOUNT_CREATE2) != 0;
    const bool open = !create && !create_or_open && (mount_mode & (SD_MOUNT_RDONLY | SD_MOUNT_RDWR)) != 0;
    if (!create && !create_or_open && !open) {
        return SD_ERROR_PARAMETER;
    }

    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    const std::string requested = dir_name->data;
    const std::string alias = save_alias_locked(st, requested);
    const std::string mount_point = mount_point_for(alias);
    if (mount_point.size() + 1 > sizeof(mount_result->mount_point)) {
        return SD_ERROR_PARAMETER;
    }
    const std::filesystem::path directory = save_directory(alias);
    for (const auto& slot : st.slots) {
        if (slot.used && slot.directory == directory) {
            return SD_ERROR_BUSY;
        }
    }
    const int free_index = free_slot_locked(st);
    if (free_index == -1) {
        return SD_ERROR_MOUNT_FULL;
    }

    std::error_code error;
    const bool exists = std::filesystem::is_directory(directory, error) && !error;
    if (open && !exists) {
        return SD_ERROR_NOT_FOUND;
    }
    if (create && exists) {
        return SD_ERROR_EXISTS;
    }
    bool created = false;
    if ((create || create_or_open) && !exists) {
        if (blocks < SD_BLOCKS_MIN || blocks > SD_BLOCKS_MAX) {
            return SD_ERROR_PARAMETER;
        }
        std::error_code root_error;
        if (!ensure_save_root(root_error)) {
            return SD_ERROR_INTERNAL;
        }
        if (!std::filesystem::create_directories(directory, error) || error) {
            return SD_ERROR_INTERNAL;
        }
        RecordWrittenPath_nid_no_patch(directory);
        created = true;
        if (write_u64(save_blocks_path(directory), blocks) != SD_OK) {
            std::error_code cleanup;
            std::filesystem::remove_all(directory, cleanup);
            return SD_ERROR_INTERNAL;
        }
        SaveDataParam initial {};
        std::strncpy(initial.title, "Saved Data", sizeof(initial.title) - 1);
        if (!write_blob(save_param_path(directory), &initial, sizeof(initial))) {
            std::error_code cleanup;
            std::filesystem::remove_all(directory, cleanup);
            return SD_ERROR_INTERNAL;
        }
    }
    if (exists && !created) {
        std::uint64_t recorded = 0;
        if (read_u64(save_blocks_path(directory), &recorded) != SD_OK && blocks != 0) {
            if (blocks < SD_BLOCKS_MIN || blocks > SD_BLOCKS_MAX) {
                return SD_ERROR_PARAMETER;
            }
            write_u64(save_blocks_path(directory), blocks);
        }
    }

    MountSlot& slot = st.slots[static_cast<std::size_t>(free_index)];
    slot.used = true;
    slot.mount_point = mount_point;
    slot.dir_name = requested;
    slot.directory = directory;
    std::memcpy(mount_result->mount_point.data, mount_point.c_str(), mount_point.size() + 1);
    mount_result->required_blocks = 0;
    mount_result->mount_status = created ? 1u : 0u;
    return SD_OK;
}

int umount_internal(std::uint32_t mode, const SaveDataMountPoint* mount_point, const char* api) {
    State& st = state();
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (mount_point == nullptr || !bounded_string_ok(mount_point->data, sizeof(mount_point->data))) {
        return SD_ERROR_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const int index = slot_of_mount_point_locked(st, mount_point->data);
    if (index == -1) {
        return SD_ERROR_NOT_MOUNTED;
    }
    st.slots[static_cast<std::size_t>(index)] = MountSlot {};
    if ((mode & SD_UMOUNT_BACKUP_ASYNC) != 0) {
        queue_event_locked(st, SD_EVENT_UMOUNT_BACKUP_END, 0, nullptr, nullptr, SD_OK, nullptr);
    }
    return SD_OK;
}

int delete_internal(std::int32_t user_id, const SceSaveDataDirName* dir_name,
                    const SceSaveDataTitleId* title_id) {
    State& st = state();
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (dir_name == nullptr || !bounded_string_ok(dir_name->data, sizeof(dir_name->data)) ||
        !valid_dir_name(dir_name->data) ||
        (title_id != nullptr && !bounded_string_ok(title_id->data, sizeof(title_id->data)))) {
        return SD_ERROR_PARAMETER;
    }
    if (user_id < 0) {
        return SD_ERROR_INVALID_LOGIN_USER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const std::string alias = save_alias_locked(st, dir_name->data);
    const std::filesystem::path directory = save_directory(alias);
    if (save_is_mounted_locked(st, directory)) {
        return SD_ERROR_BUSY;
    }
    std::error_code error;
    if (!std::filesystem::exists(directory, error) || error) {
        return SD_ERROR_NOT_FOUND;
    }
    std::filesystem::remove_all(directory, error);
    RecordWrittenPath_nid_no_patch(directory);
    if (error) {
        return SD_ERROR_INTERNAL;
    }
    st.aliases.erase(alias);
    if (alias != dir_name->data) {
        store_aliases_locked(st);
    }
    return SD_OK;
}

bool dir_name_matches(const char* str, const char* pattern) {
    if (pattern == nullptr || pattern[0] == '\0') {
        return true;
    }
    while (*str != '\0' && *pattern != '\0') {
        if (*pattern == '%') {
            for (const char* s = str;; s++) {
                if (dir_name_matches(s, pattern + 1)) {
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
        if (std::tolower(static_cast<unsigned char>(*str)) !=
            std::tolower(static_cast<unsigned char>(*pattern))) {
            return false;
        }
        str++;
        pattern++;
    }
    return *str == '\0' && *pattern == '\0';
}

int dir_name_search_internal(const SaveDataDirNameSearchCond* cond,
                             SaveDataDirNameSearchResult* result) {
    State& st = state();
    if (result != nullptr) {
        std::memset(result, 0, sizeof(*result));
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (cond == nullptr || result == nullptr ||
        (result->dir_names_num != 0 && result->dir_names == nullptr) ||
        (cond->dir_name != nullptr && !bounded_string_ok(cond->dir_name->data, sizeof(cond->dir_name->data))) ||
        (cond->title_id != nullptr && !bounded_string_ok(cond->title_id->data, sizeof(cond->title_id->data)))) {
        return SD_ERROR_PARAMETER;
    }
    if (cond->user_id < 0) {
        return SD_ERROR_INVALID_LOGIN_USER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    load_aliases_locked(st);
    std::vector<std::string> found;
    std::error_code error;
    if (std::filesystem::is_directory(save_root(), error) && !error) {
        for (const auto& entry : std::filesystem::directory_iterator(save_root(), error)) {
            if (error) {
                break;
            }
            std::error_code kind_error;
            if (!entry.is_directory(kind_error) || kind_error) {
                continue;
            }
            const std::string alias = entry.path().filename().string();
            if (alias.empty() || alias[0] == '.' || alias[0] == '_' ||
                !valid_dir_name(alias.c_str())) {
                continue;
            }
            std::error_code meta_error;
            if (!std::filesystem::is_regular_file(save_param_path(entry.path()), meta_error) || meta_error) {
                continue;
            }
            const std::string dir_name = real_dir_name_locked(st, alias);
            if (cond->dir_name != nullptr && !dir_name_matches(dir_name.c_str(), cond->dir_name->data)) {
                continue;
            }
            found.push_back(dir_name);
        }
    }
    std::sort(found.begin(), found.end());
    if (cond->order == 1) {
        std::reverse(found.begin(), found.end());
    }
    result->hit_num = static_cast<std::uint32_t>(found.size());
    const std::uint32_t set_count = std::min<std::uint32_t>(result->dir_names_num, found.size());
    for (std::uint32_t i = 0; i < set_count; i++) {
        std::snprintf(result->dir_names[i].data, sizeof(result->dir_names[i].data), "%s",
                      found[i].c_str());
        if (result->params != nullptr) {
            load_save_param(save_directory(save_alias_locked(st, found[i])), &result->params[i]);
        }
        if (result->infos != nullptr) {
            std::memset(&result->infos[i], 0, sizeof(result->infos[i]));
            std::uint64_t blocks = 0;
            if (read_u64(save_blocks_path(save_directory(save_alias_locked(st, found[i]))), &blocks) == SD_OK) {
                std::memcpy(&result->infos[i], &blocks, sizeof(blocks));
            }
        }
    }
    result->set_num = set_count;
    return SD_OK;
}

int get_param_internal(const SaveDataMountPoint* mount_point, std::uint32_t param_type,
                       void* param_buf, std::size_t param_buf_size, std::size_t* got_size) {
    State& st = state();
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (mount_point == nullptr || param_buf == nullptr || param_type > SD_PARAM_MTIME ||
        !bounded_string_ok(mount_point->data, sizeof(mount_point->data))) {
        return SD_ERROR_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const int index = slot_of_mount_point_locked(st, mount_point->data);
    if (index == -1) {
        return SD_ERROR_NOT_MOUNTED;
    }
    SaveDataParam param {};
    const int status = load_save_param(st.slots[static_cast<std::size_t>(index)].directory, &param);
    if (status != SD_OK) {
        return status;
    }
    std::size_t written = 0;
    if (param_type == SD_PARAM_WHOLE) {
        if (param_buf_size < sizeof(param)) {
            return SD_ERROR_PARAMETER;
        }
        std::memcpy(param_buf, &param, sizeof(param));
        written = sizeof(param);
    } else if (param_type == SD_PARAM_USER_PARAM || param_type == SD_PARAM_MTIME) {
        const std::size_t size = param_type == SD_PARAM_USER_PARAM ? sizeof(param.user_param)
                                                                   : sizeof(param.mtime);
        if (param_buf_size < size) {
            return SD_ERROR_PARAMETER;
        }
        std::memcpy(param_buf,
                    param_type == SD_PARAM_USER_PARAM
                        ? static_cast<const void*>(&param.user_param)
                        : static_cast<const void*>(&param.mtime),
                    size);
        written = size;
    } else {
        char* field = param_type == SD_PARAM_TITLE ? param.title
                  : param_type == SD_PARAM_SUB_TITLE ? param.sub_title : param.detail;
        const std::size_t capacity = param_type == SD_PARAM_DETAIL ? sizeof(param.detail)
                                                                   : sizeof(param.title);
        if (param_buf_size == 0) {
            return SD_ERROR_PARAMETER;
        }
        const std::size_t length = std::min<std::size_t>(std::strlen(field), param_buf_size - 1);
        std::memcpy(param_buf, field, length);
        static_cast<char*>(param_buf)[length] = '\0';
        written = length + 1;
    }
    if (got_size != nullptr) {
        *got_size = written;
    }
    return SD_OK;
}

int set_param_internal(const SaveDataMountPoint* mount_point, std::uint32_t param_type,
                       const void* param_buf, std::size_t param_buf_size) {
    State& st = state();
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (mount_point == nullptr || param_buf == nullptr || param_type > SD_PARAM_MTIME ||
        !bounded_string_ok(mount_point->data, sizeof(mount_point->data))) {
        return SD_ERROR_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const int index = slot_of_mount_point_locked(st, mount_point->data);
    if (index == -1) {
        return SD_ERROR_NOT_MOUNTED;
    }
    const std::filesystem::path directory = st.slots[static_cast<std::size_t>(index)].directory;
    if (param_type == SD_PARAM_WHOLE) {
        if (param_buf_size < sizeof(SaveDataParam)) {
            return SD_ERROR_PARAMETER;
        }
        SaveDataParam param = *static_cast<const SaveDataParam*>(param_buf);
        param.mtime = static_cast<std::int64_t>(std::time(nullptr));
        return write_blob(save_param_path(directory), &param, sizeof(param)) ? SD_OK : SD_ERROR_INTERNAL;
    }
    SaveDataParam param {};
    const int status = load_save_param(directory, &param);
    if (status != SD_OK && status != SD_ERROR_NOT_FOUND) {
        return status;
    }
    if (param_type == SD_PARAM_USER_PARAM || param_type == SD_PARAM_MTIME) {
        const std::size_t size = param_type == SD_PARAM_USER_PARAM ? sizeof(param.user_param)
                                                                   : sizeof(param.mtime);
        if (param_buf_size < size) {
            return SD_ERROR_PARAMETER;
        }
        std::memcpy(param_type == SD_PARAM_USER_PARAM ? static_cast<void*>(&param.user_param)
                                                      : static_cast<void*>(&param.mtime),
                    param_buf, size);
    } else {
        char* field = param_type == SD_PARAM_TITLE ? param.title
                  : param_type == SD_PARAM_SUB_TITLE ? param.sub_title : param.detail;
        const std::size_t capacity = param_type == SD_PARAM_DETAIL ? sizeof(param.detail)
                                                                   : sizeof(param.title);
        if (param_buf_size == 0) {
            return SD_ERROR_PARAMETER;
        }
        const std::size_t length = std::min(param_buf_size, capacity - 1);
        const char* text = static_cast<const char*>(param_buf);
        const char* end = static_cast<const char*>(std::memchr(text, '\0', length));
        std::memset(field, 0, capacity);
        std::memcpy(field, text, end != nullptr ? static_cast<std::size_t>(end - text) : length);
    }
    return write_blob(save_param_path(directory), &param, sizeof(param)) ? SD_OK : SD_ERROR_INTERNAL;
}

int get_mount_info_internal(const SaveDataMountPoint* mount_point, SaveDataMountInfo* info) {
    State& st = state();
    if (info != nullptr) {
        std::memset(info, 0, sizeof(*info));
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (mount_point == nullptr || info == nullptr ||
        !bounded_string_ok(mount_point->data, sizeof(mount_point->data))) {
        return SD_ERROR_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const int index = slot_of_mount_point_locked(st, mount_point->data);
    if (index == -1) {
        return SD_ERROR_NOT_MOUNTED;
    }
    std::uint64_t blocks = 0;
    const int status = read_u64(save_blocks_path(st.slots[static_cast<std::size_t>(index)].directory),
                                &blocks);
    if (status != SD_OK) {
        return status;
    }
    info->blocks = blocks;
    info->free_blocks = blocks;
    return SD_OK;
}

int is_mounted_internal(const SaveDataMountPoint* mount_point, std::uint32_t* is_mounted) {
    if (mount_point == nullptr || is_mounted == nullptr ||
        !bounded_string_ok(mount_point->data, sizeof(mount_point->data))) {
        return SD_ERROR_PARAMETER;
    }
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    *is_mounted = slot_of_mount_point_locked(st, mount_point->data) != -1 ? 1u : 0u;
    return SD_OK;
}

std::int64_t count_mounted_internal() {
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    std::int64_t count = 0;
    for (const auto& slot : st.slots) {
        if (slot.used) {
            count++;
        }
    }
    return count;
}

std::string memory_key(std::int32_t user_id, std::uint32_t slot_id) {
    char key[32];
    std::snprintf(key, sizeof(key), "%d:%u", static_cast<int>(user_id), static_cast<unsigned>(slot_id));
    return key;
}

std::filesystem::path memory_directory(std::int32_t user_id, std::uint32_t slot_id) {
    char name[48];
    std::snprintf(name, sizeof(name), "%s%s", SD_MEMORY_PREFIX,
                  slot_id == 0 ? "" : std::to_string(slot_id).c_str());
    return save_root() / name / std::to_string(static_cast<int>(user_id));
}

bool valid_memory_range(const MemoryData& data, std::size_t size) {
    return data.buf != nullptr && data.offset >= 0 &&
           static_cast<std::size_t>(data.offset) <= size &&
           data.buf_size <= size - static_cast<std::size_t>(data.offset);
}

int setup_memory_internal(const SaveDataMemorySetup2* setup, SaveDataMemorySetupResult* result,
                          KernelEventFlag flag) {
    State& st = state();
    if (setup == nullptr || setup->slot_id >= SD_MEMORY_SLOTS || setup->memory_size == 0 ||
        setup->memory_size > SD_MEMORY_MAX_SIZE || (setup->option & ~(SD_MEMORY_SET_PARAM | SD_MEMORY_DOUBLE_BUFFER)) != 0 ||
        (setup->init_param != nullptr && setup->option != 0 && (setup->option & SD_MEMORY_SET_PARAM) == 0)) {
        return SD_ERROR_PARAMETER;
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (setup->user_id < 0) {
        return SD_ERROR_INVALID_LOGIN_USER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const std::string key = memory_key(setup->user_id, setup->slot_id);
    if (st.memory.count(key) != 0) {
        return SD_ERROR_BUSY;
    }
    if (st.memory.size() >= SD_MEMORY_MAX_MOUNTED) {
        return SD_ERROR_LIMITATION_OVER;
    }
    if (save_is_mounted_locked(st, memory_directory(setup->user_id, setup->slot_id))) {
        return SD_ERROR_BUSY;
    }
    MemorySlot slot;
    slot.option = setup->option;
    slot.data.resize(setup->memory_size, 0);
    std::strncpy(slot.param.title, "Saved Data", sizeof(slot.param.title) - 1);
    std::size_t existed = 0;
    std::error_code error;
    const std::filesystem::path blob = memory_directory(setup->user_id, setup->slot_id) / "memory.dat";
    if (std::filesystem::is_regular_file(blob, error) && !error) {
        const std::size_t stored = static_cast<std::size_t>(std::filesystem::file_size(blob, error));
        if (error || stored > SD_MEMORY_MAX_SIZE) {
            return SD_ERROR_BROKEN;
        }
        existed = stored;
        std::vector<char> loaded(stored, 0);
        std::ifstream file(blob, std::ios::binary);
        file.read(loaded.data(), static_cast<std::streamsize>(stored));
        if (!file) {
            return SD_ERROR_BROKEN;
        }
        std::copy_n(loaded.begin(), std::min(stored, slot.data.size()), slot.data.begin());
        load_save_param(memory_directory(setup->user_id, setup->slot_id), &slot.param);
    }
    if (existed == 0 && setup->init_param != nullptr && (setup->option & SD_MEMORY_SET_PARAM) != 0) {
        slot.param = *setup->init_param;
        slot.param.mtime = 0;
    }
    st.memory.emplace(key, std::move(slot));
    if (result != nullptr) {
        std::memset(result, 0, sizeof(*result));
        result->existed_memory_size = existed;
    }
    if (flag != nullptr) {
        queue_event_locked(st, SD_EVENT_MEMORY_SYNC_END, setup->user_id, nullptr, nullptr, SD_OK, flag);
    }
    return SD_OK;
}

int get_memory_internal(SaveDataMemoryGet2* get_param) {
    State& st = state();
    if (get_param == nullptr || get_param->slot_id >= SD_MEMORY_SLOTS) {
        return SD_ERROR_PARAMETER;
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (get_param->user_id < 0) {
        return SD_ERROR_INVALID_LOGIN_USER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const auto it = st.memory.find(memory_key(get_param->user_id, get_param->slot_id));
    if (it == st.memory.end()) {
        return SD_ERROR_MEMORY_NOT_READY;
    }
    const MemorySlot& slot = it->second;
    if (get_param->data != nullptr &&
        !valid_memory_range(*reinterpret_cast<const MemoryData*>(get_param->data), slot.data.size())) {
        return SD_ERROR_PARAMETER;
    }
    if (get_param->param != nullptr && (slot.option & SD_MEMORY_SET_PARAM) == 0) {
        return SD_ERROR_PARAMETER;
    }
    if (get_param->data != nullptr) {
        const MemoryData& data = *reinterpret_cast<const MemoryData*>(get_param->data);
        std::memcpy(data.buf, slot.data.data() + data.offset, data.buf_size);
    }
    if (get_param->param != nullptr) {
        *get_param->param = slot.param;
    }
    if (get_param->icon != nullptr) {
        get_param->icon->data_size = 0;
    }
    return SD_OK;
}

int set_memory_internal(const SaveDataMemorySet2* set_param) {
    State& st = state();
    if (set_param == nullptr || set_param->slot_id >= SD_MEMORY_SLOTS ||
        set_param->data_num > SD_MEMORY_MAX_DATA_NUM ||
        (set_param->data == nullptr && set_param->data_num != 0)) {
        return SD_ERROR_PARAMETER;
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (set_param->user_id < 0) {
        return SD_ERROR_INVALID_LOGIN_USER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const auto it = st.memory.find(memory_key(set_param->user_id, set_param->slot_id));
    if (it == st.memory.end()) {
        return SD_ERROR_MEMORY_NOT_READY;
    }
    if (set_param->param != nullptr && (it->second.option & SD_MEMORY_SET_PARAM) == 0) {
        return SD_ERROR_PARAMETER;
    }
    const std::uint32_t count =
        set_param->data != nullptr && set_param->data_num == 0 ? 1u : set_param->data_num;
    const MemoryData* descriptors = reinterpret_cast<const MemoryData*>(set_param->data);
    for (std::uint32_t i = 0; i < count; i++) {
        if (!valid_memory_range(descriptors[i], it->second.data.size())) {
            return SD_ERROR_PARAMETER;
        }
    }
    MemorySlot updated = it->second;
    for (std::uint32_t i = 0; i < count; i++) {
        const MemoryData& data = descriptors[i];
        std::memcpy(updated.data.data() + data.offset, data.buf, data.buf_size);
    }
    if (set_param->param != nullptr) {
        updated.param = *set_param->param;
    }
    updated.param.mtime = static_cast<std::int64_t>(std::time(nullptr));
    updated.dirty = true;
    it->second = std::move(updated);
    return SD_OK;
}

int sync_memory_internal(const std::int32_t user_id, const std::uint32_t slot_id,
                         const std::uint32_t option, KernelEventFlag flag, bool report_event) {
    State& st = state();
    if (slot_id >= SD_MEMORY_SLOTS || option > 1) {
        return SD_ERROR_PARAMETER;
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (user_id < 0) {
        return SD_ERROR_INVALID_LOGIN_USER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const auto it = st.memory.find(memory_key(user_id, slot_id));
    if (it == st.memory.end()) {
        return SD_ERROR_MEMORY_NOT_READY;
    }
    const std::filesystem::path directory = memory_directory(user_id, slot_id);
    int status = SD_OK;
    if (!write_blob(directory / "memory.dat", it->second.data.data(), it->second.data.size())) {
        status = SD_ERROR_INTERNAL;
    } else if (!write_blob(save_param_path(directory), &it->second.param, sizeof(it->second.param))) {
        status = SD_ERROR_INTERNAL;
    } else {
        it->second.dirty = false;
    }
    if (option == 1 || !report_event) {
        return status;
    }
    char title_text[sizeof(SceSaveDataTitleId)] = {};
    char dir_text[sizeof(SceSaveDataDirName)] = {};
    std::snprintf(dir_text, sizeof(dir_text), "%s%s", SD_MEMORY_PREFIX,
                  slot_id == 0 ? "" : std::to_string(slot_id).c_str());
    SceSaveDataTitleId title {};
    SceSaveDataDirName directory_name {};
    std::memcpy(title.data, title_text, sizeof(title.data));
    std::memcpy(directory_name.data, dir_text, sizeof(directory_name.data));
    queue_event_locked(st, SD_EVENT_MEMORY_SYNC_END, user_id, &title, &directory_name, status, flag);
    return SD_OK;
}

int commit_internal(const SaveDataCommitParam* param) {
    State& st = state();
    if (param == nullptr) {
        return SD_ERROR_PARAMETER;
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    for (auto& [key, slot] : st.memory) {
        if (!slot.dirty) {
            continue;
        }
        const std::size_t sep = key.find(':');
        const std::int32_t user_id = static_cast<std::int32_t>(std::strtol(key.c_str(), nullptr, 10));
        const std::uint32_t slot_id = sep == std::string::npos
                                          ? 0u
                                          : static_cast<std::uint32_t>(std::strtoul(key.c_str() + sep + 1, nullptr, 10));
        const std::filesystem::path directory = memory_directory(user_id, slot_id);
        if (!write_blob(directory / "memory.dat", slot.data.data(), slot.data.size()) ||
            !write_blob(save_param_path(directory), &slot.param, sizeof(slot.param))) {
            return SD_ERROR_INTERNAL;
        }
        slot.dirty = false;
    }
    if ((param->commit_mode & SD_COMMIT_BACKUP_ASYNC) != 0) {
        queue_event_locked(st, SD_EVENT_COMMIT_BACKUP_END, 0, nullptr, nullptr, SD_OK, nullptr);
    }
    return SD_OK;
}

int prepare_internal(const SaveDataMountPoint* mount_point, const SaveDataPrepareParam* param) {
    if (mount_point == nullptr || param == nullptr ||
        !bounded_string_ok(mount_point->data, sizeof(mount_point->data))) {
        return SD_ERROR_PARAMETER;
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    return SD_OK;
}

int backup_internal(const SaveDataBackup* backup, KernelEventFlag flag) {
    State& st = state();
    if (backup == nullptr || backup->dir_name == nullptr ||
        !bounded_string_ok(backup->dir_name->data, sizeof(backup->dir_name->data))) {
        return SD_ERROR_PARAMETER;
    }
    if (backup->title_id != nullptr && !bounded_string_ok(backup->title_id->data, sizeof(backup->title_id->data))) {
        return SD_ERROR_PARAMETER;
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (backup->user_id < 0) {
        return SD_ERROR_INVALID_LOGIN_USER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    queue_event_locked(st, SD_EVENT_BACKUP_END, backup->user_id, backup->title_id, backup->dir_name,
                       SD_OK, flag);
    return SD_OK;
}

int transferring_mount_internal(const SaveDataTransferringMount* mount,
                                SaveDataMountResult* mount_result) {
    if (mount_result != nullptr) {
        std::memset(mount_result, 0, sizeof(*mount_result));
    }
    State& st = state();
    if (mount == nullptr || mount_result == nullptr || mount->title_id == nullptr ||
        mount->dir_name == nullptr ||
        !bounded_string_ok(mount->title_id->data, sizeof(mount->title_id->data)) ||
        !bounded_string_ok(mount->dir_name->data, sizeof(mount->dir_name->data))) {
        return SD_ERROR_PARAMETER;
    }
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    if (mount->user_id < 0) {
        return SD_ERROR_INVALID_LOGIN_USER;
    }
    if (!valid_dir_name(mount->title_id->data) || !valid_dir_name(mount->dir_name->data)) {
        return SD_ERROR_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    const std::filesystem::path directory = save_root() / "transfer" / mount->title_id->data /
                                           mount->dir_name->data;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error) {
        return SD_ERROR_NOT_FOUND;
    }
    const int index = free_slot_locked(st);
    if (index == -1) {
        return SD_ERROR_MOUNT_FULL;
    }
    for (const auto& slot : st.slots) {
        if (slot.used && slot.directory == directory) {
            return SD_ERROR_BUSY;
        }
    }
    const std::string mount_point = mount_point_for(save_alias_locked(
        st, std::string(mount->title_id->data) + "-" + mount->dir_name->data));
    if (mount_point.size() + 1 > sizeof(mount_result->mount_point)) {
        return SD_ERROR_PARAMETER;
    }
    MountSlot& slot = st.slots[static_cast<std::size_t>(index)];
    slot.used = true;
    slot.mount_point = mount_point;
    slot.dir_name = mount->dir_name->data;
    slot.directory = directory;
    std::memcpy(mount_result->mount_point.data, mount_point.c_str(), mount_point.size() + 1);
    mount_result->required_blocks = 0;
    mount_result->mount_status = 0;
    return SD_OK;
}

int get_event_result_internal(const void* event_param, SaveDataEvent* event) {
    (void)event_param;
    State& st = state();
    if (event == nullptr) {
        return SD_ERROR_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    if (st.events.empty()) {
        return SD_ERROR_NOT_FOUND;
    }
    const PendingEvent pending = st.events.front();
    st.events.pop_front();
    if (pending.flag != nullptr) {
        sceKernelDeleteEventFlag(pending.flag);
    }
    *event = pending.record;
    return event->error_code;
}

int cancel_internal(KernelEventFlag event) {
    State& st = state();
    if (event == nullptr) {
        return SD_ERROR_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(st.mutex);
    for (auto it = st.events.begin(); it != st.events.end(); ++it) {
        if (it->flag == event) {
            sceKernelCancelEventFlag(event, 0, nullptr);
            const KernelEventFlag flag = it->flag;
            st.events.erase(it);
            sceKernelDeleteEventFlag(flag);
            return SD_OK;
        }
    }
    return SD_ERROR_NOT_FOUND;
}

int abort_internal() {
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    for (auto& pending : st.events) {
        if (pending.flag != nullptr) {
            sceKernelCancelEventFlag(pending.flag, 0, nullptr);
            sceKernelDeleteEventFlag(pending.flag);
            pending.flag = nullptr;
        }
    }
    st.events.clear();
    return SD_OK;
}

int create_transaction_resource_internal(std::uint32_t size) {
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    if (st.next_transaction_resource <= 0) {
        return SD_ERROR_OUT_OF_MEMORY;
    }
    const std::int32_t id = st.next_transaction_resource++;
    st.live_transaction_resources.insert(id);
    return id;
}

int delete_transaction_resource_internal(std::int32_t resource) {
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    return st.live_transaction_resources.erase(resource) != 0 ? SD_OK : SD_ERROR_PARAMETER;
}

std::filesystem::path app_status_path(const std::string& dir_name) {
    return save_root() / "_app_status" / dir_name;
}

int get_local_storage_app_status_internal(const SceSaveDataDirName* dir_name,
                                          const SceSaveDataTitleId* title_id,
                                          std::uint32_t* status) {
    if (status == nullptr || dir_name == nullptr ||
        !bounded_string_ok(dir_name->data, sizeof(dir_name->data)) ||
        (title_id != nullptr && !bounded_string_ok(title_id->data, sizeof(title_id->data)))) {
        return SD_ERROR_PARAMETER;
    }
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    *status = SD_APP_STATUS_NONE;
    std::error_code error;
    const std::filesystem::path path = app_status_path(dir_name->data);
    if (std::filesystem::is_regular_file(path, error) && !error) {
        std::uint32_t stored = 0;
        std::ifstream file(path, std::ios::binary);
        file.read(reinterpret_cast<char*>(&stored), sizeof(stored));
        if (!file) {
            return SD_ERROR_BROKEN;
        }
        *status = stored;
        return SD_OK;
    }
    const std::string alias = save_alias_locked(st, dir_name->data);
    if (std::filesystem::is_directory(save_directory(alias), error) && !error) {
        *status = SD_APP_STATUS_LOCAL;
        return SD_OK;
    }
    *status = SD_APP_STATUS_NONE;
    return SD_OK;
}

int set_local_storage_app_status_internal(const SceSaveDataDirName* dir_name,
                                          const SceSaveDataTitleId* title_id,
                                          std::uint32_t status) {
    if (dir_name == nullptr || !bounded_string_ok(dir_name->data, sizeof(dir_name->data)) ||
        (title_id != nullptr && !bounded_string_ok(title_id->data, sizeof(title_id->data))) ||
        (status & ~(SD_APP_STATUS_DELETED | SD_APP_STATUS_LOCAL | SD_APP_STATUS_ONLINE)) != 0) {
        return SD_ERROR_PARAMETER;
    }
    std::error_code error;
    if (std::filesystem::create_directories(app_status_path(dir_name->data).parent_path(), error))
        RecordWrittenPath_nid_no_patch(app_status_path(dir_name->data).parent_path());
    if (error) {
        return SD_ERROR_INTERNAL;
    }
    return write_blob(app_status_path(dir_name->data), &status, sizeof(status)) ? SD_OK
                                                                                : SD_ERROR_INTERNAL;
}

int delete_local_storage_app_status_internal(const SceSaveDataDirName* dir_name,
                                             const SceSaveDataTitleId* title_id) {
    if (dir_name == nullptr || !bounded_string_ok(dir_name->data, sizeof(dir_name->data)) ||
        (title_id != nullptr && !bounded_string_ok(title_id->data, sizeof(title_id->data)))) {
        return SD_ERROR_PARAMETER;
    }
    std::error_code error;
    if (std::filesystem::remove(app_status_path(dir_name->data), error))
        RecordWrittenPath_nid_no_patch(app_status_path(dir_name->data));
    return error ? SD_ERROR_INTERNAL : SD_OK;
}

int check_internal() {
    if (!is_ready()) {
        return SD_ERROR_NOT_INITIALIZED;
    }
    std::error_code error;
    return ensure_save_root(error) ? SD_OK : SD_ERROR_INTERNAL;
}

std::uint64_t get_all_size_internal(std::int32_t user_id) {
    State& st = state();
    std::lock_guard<std::mutex> lock(st.mutex);
    std::uint64_t total = 0;
    for (const auto& [key, slot] : st.memory) {
        total += slot.data.size() * ((slot.option & SD_MEMORY_DOUBLE_BUFFER) != 0 ? 2u : 1u);
    }
    return total;
}

}  // namespace
}  // namespace savedata

#endif
