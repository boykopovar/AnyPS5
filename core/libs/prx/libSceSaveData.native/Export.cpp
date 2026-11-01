#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
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
#include <string_view>
#include <system_error>
#include <utility>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/RemoveTree.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SaveData.hpp"
#include "prx/libSceSaveData.native/include/SaveDataHostSpace.hpp"
#include "prx/libSceSaveData/SaveDataFile.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static constexpr char SAVE_DIR[] = "_sd";

static std::atomic<std::int32_t> g_transaction_counter{1};
static std::atomic<int> g_initializations{0};
static std::atomic<float> g_convertProgress{0.0f};

static std::string save_root() {
    return std::string(SAVE_DIR);
}

// Events the title polls with sceSaveDataGetEventResult; a backup completes at once here.
static std::mutex g_state_mutex;
static std::deque<SaveDataEvent> g_events;
constexpr std::uint32_t SAVE_DATA_EVENT_TYPE_UMOUNT_BACKUP = 1;
constexpr std::uint32_t SAVE_DATA_EVENT_TYPE_BACKUP = 2;
constexpr std::uint32_t SAVE_DATA_EVENT_TYPE_CONVERT = 5;

namespace {

struct EventDelivery {
    SaveDataEvent event;
    SaveDataEventCallback callback;
    void* userdata;
};

struct EventWorker {
    Pthread thread = nullptr;
    bool stopping = false;
};

struct EventCallbacks {
    std::recursive_mutex calling;
    std::mutex mutex;
    std::condition_variable wake;
    SaveDataEventCallback callback = nullptr;
    void* userdata = nullptr;
    std::deque<EventDelivery> pending;
    EventWorker* worker = nullptr;
    Pthread delivering = nullptr;
};

EventCallbacks& Callbacks() {
    static auto* callbacks = new EventCallbacks;
    return *callbacks;
}

void* APS5_VABI DeliverEvents(void* argument) {
    auto& callbacks = Callbacks();
    auto* worker = static_cast<EventWorker*>(argument);
    for (;;) {
        {
            std::unique_lock lock(callbacks.mutex);
            callbacks.wake.wait(lock, [&] { return worker->stopping || !callbacks.pending.empty(); });
            if (worker->stopping) return nullptr;
        }
        std::lock_guard calling(callbacks.calling);
        EventDelivery delivery{};
        {
            std::lock_guard lock(callbacks.mutex);
            if (worker->stopping) return nullptr;
            if (callbacks.pending.empty()) continue;
            delivery = callbacks.pending.front();
            callbacks.pending.pop_front();
            callbacks.delivering = scePthreadSelf();
        }
        delivery.callback(&delivery.event, delivery.userdata);
        std::lock_guard lock(callbacks.mutex);
        callbacks.delivering = nullptr;
    }
}

void StartEventDelivery() {
    auto& callbacks = Callbacks();
    auto worker = std::make_unique<EventWorker>();
    std::lock_guard lock(callbacks.mutex);
    if (callbacks.worker != nullptr) return;
    if (scePthreadCreate(&worker->thread, nullptr, DeliverEvents, worker.get(), "SceSaveDataEventCallback") != 0) {
        throw std::runtime_error("sceSaveDataRegisterEventCallback: scePthreadCreate failed for the event callback thread");
    }
    callbacks.worker = worker.release();
}

EventWorker* DetachEventDelivery() {
    auto& callbacks = Callbacks();
    std::lock_guard lock(callbacks.mutex);
    callbacks.callback = nullptr;
    callbacks.userdata = nullptr;
    callbacks.pending.clear();
    auto* worker = std::exchange(callbacks.worker, nullptr);
    if (worker != nullptr) worker->stopping = true;
    return worker;
}

void JoinEventDelivery(EventWorker* worker) {
    std::unique_ptr<EventWorker> owned(worker);
    Callbacks().wake.notify_all();
    if (scePthreadJoin(owned->thread, nullptr) != 0) throw std::runtime_error("sceSaveDataTerminate: scePthreadJoin failed for the event callback thread");
}

bool InEventCallback() {
    auto& callbacks = Callbacks();
    std::lock_guard lock(callbacks.mutex);
    return callbacks.delivering != nullptr && scePthreadEqual(scePthreadSelf(), callbacks.delivering) != 0;
}

void DeliverEvent(const SaveDataEvent& event) {
    auto& callbacks = Callbacks();
    {
        std::lock_guard lock(callbacks.mutex);
        if (callbacks.callback == nullptr || callbacks.worker == nullptr) return;
        callbacks.pending.push_back({event, callbacks.callback, callbacks.userdata});
    }
    callbacks.wake.notify_all();
}

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
    while (*pattern == '%') {
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

constexpr char MEM_JOURNAL_MAGIC[] = {'A', 'P', 'S', '5', 'M', 'J', 'N', '1'};

struct MemoryWrite {
    std::uint64_t offset;
    std::uint64_t size;
    const char* data;
};

std::vector<char> encode_memory_journal(const std::vector<MemoryWrite>& writes) {
    std::vector<char> bytes(std::begin(MEM_JOURNAL_MAGIC), std::end(MEM_JOURNAL_MAGIC));
    for (const MemoryWrite& write : writes) {
        const std::size_t at = bytes.size();
        bytes.resize(at + sizeof(write.offset) + sizeof(write.size) + write.size);
        std::memcpy(bytes.data() + at, &write.offset, sizeof(write.offset));
        std::memcpy(bytes.data() + at + sizeof(write.offset), &write.size, sizeof(write.size));
        std::memcpy(bytes.data() + at + sizeof(write.offset) + sizeof(write.size), write.data, write.size);
    }
    return bytes;
}

std::vector<MemoryWrite> decode_memory_journal(const std::string& journal, const std::vector<char>& bytes, std::size_t memory_size) {
    const auto malformed = [&journal]() { return std::runtime_error("SaveData: memory journal " + journal + " is malformed"); };
    if (bytes.size() < sizeof(MEM_JOURNAL_MAGIC) || std::memcmp(bytes.data(), MEM_JOURNAL_MAGIC, sizeof(MEM_JOURNAL_MAGIC)) != 0) {
        throw malformed();
    }
    std::vector<MemoryWrite> writes;
    std::size_t at = sizeof(MEM_JOURNAL_MAGIC);
    while (at != bytes.size()) {
        MemoryWrite write{};
        if (bytes.size() - at < sizeof(write.offset) + sizeof(write.size)) {
            throw malformed();
        }
        std::memcpy(&write.offset, bytes.data() + at, sizeof(write.offset));
        std::memcpy(&write.size, bytes.data() + at + sizeof(write.offset), sizeof(write.size));
        at += sizeof(write.offset) + sizeof(write.size);
        if (write.size > bytes.size() - at || write.offset > memory_size || write.size > memory_size - write.offset) {
            throw malformed();
        }
        write.data = bytes.data() + at;
        at += write.size;
        writes.push_back(write);
    }
    return writes;
}

void apply_memory_journal(std::fstream& memory, const std::string& journal, const std::vector<MemoryWrite>& writes) {
    for (const MemoryWrite& write : writes) {
        memory.seekp(static_cast<std::streamoff>(write.offset));
        memory.write(write.data, static_cast<std::streamsize>(write.size));
    }
    memory.flush();
    if (!memory) {
        throw std::runtime_error("SaveData: cannot write memory journal " + journal + " into the save-data memory");
    }
    std::error_code ec;
    if (!std::filesystem::remove(journal, ec)) {
        throw std::runtime_error("SaveData: cannot remove memory journal " + journal);
    }
}

void replay_memory_journal(std::int32_t user_id, std::uint32_t slot) {
    const std::string journal = mem_path(user_id, slot, "journal");
    std::error_code ec;
    const bool pending = std::filesystem::exists(journal, ec);
    if (ec) {
        throw std::runtime_error("SaveData: cannot check memory journal " + journal);
    }
    if (!pending) {
        return;
    }
    const std::string path = mem_path(user_id, slot, "bin");
    std::vector<char> bytes;
    std::size_t size = 0;
    if (!read_file_all(journal, bytes) || !file_size_of(path, &size)) {
        throw std::runtime_error("SaveData: cannot replay memory journal " + journal);
    }
    const std::vector<MemoryWrite> writes = decode_memory_journal(journal, bytes, size);
    std::fstream memory(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!memory) {
        throw std::runtime_error("SaveData: cannot open " + path + " to replay memory journal " + journal);
    }
    apply_memory_journal(memory, journal, writes);
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
#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (GetFileAttributesExW(std::filesystem::path(real_path).c_str(), GetFileExInfoStandard, &data)) {
            const auto ticks = static_cast<std::int64_t>((static_cast<std::uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime);
            param.mtime = (ticks - 116444736000000000LL) / 10000000LL;
        }
#else
        std::error_code ec;
        const auto written = std::filesystem::last_write_time(real_path, ec);
        if (!ec) {
            const auto system = std::chrono::file_clock::to_sys(written);
            param.mtime = std::chrono::duration_cast<std::chrono::seconds>(system.time_since_epoch()).count();
        }
#endif
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
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (std::memchr(backup->dir_name->data, '\0', sizeof(backup->dir_name->data)) == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    SaveDataEvent event{};
    event.type = SAVE_DATA_EVENT_TYPE_BACKUP;
    event.error_code = SAVE_DATA_OK;
    event.user_id = backup->user_id;
    if (backup->title_id != nullptr) event.title_id = *backup->title_id;
    event.dir_name = *backup->dir_name;
    {
        std::lock_guard lock(g_state_mutex);
        g_events.push_back(event);
    }
    DeliverEvent(event);
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataCommit(const SaveDataCommitParam* param) {
    (void)param;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataCreateTransactionResource(uint32_t size) {
    (void)size;
    if (g_initializations == 0) return SAVE_DATA_ERROR_NOT_INITIALIZED;
    return g_transaction_counter.fetch_add(1);
}

static std::string blocks_path(const std::string& real_path) {
    return real_path + ".blocks";
}

static bool isMountedDirectory(const std::string& path, std::error_code& error) {
    for (const auto& slot : g_slots) {
        if (!slot.used) continue;
        if (slot.real_path == path) return true;
        const bool exists = std::filesystem::exists(path, error);
        if (error) return false;
        if (!exists) continue;
        const bool mountedExists = std::filesystem::exists(slot.real_path, error);
        if (error) return false;
        if (!mountedExists) continue;
        const bool sameDirectory = std::filesystem::equivalent(slot.real_path, path, error);
        if (error == std::errc::no_such_file_or_directory) error.clear();
        if (error) return false;
        if (sameDirectory) return true;
    }
    return false;
}

static constexpr char BACKUP_DIR[] = "_sd_backup";
static constexpr char BACKUP_STAGING_DIR[] = "_sd_backup_tmp";
static constexpr char BACKUP_OLD_DIR[] = "_sd_backup_old";
static constexpr char RESTORE_STAGING_DIR[] = "_sd_restore_tmp";
static constexpr char RESTORE_OLD_DIR[] = "_sd_restore_old";

static std::atomic<bool (*)(const char*, const char*)> g_file_step_hook{nullptr};

void SaveDataSetFileStepHook_nid_no_patch(bool (*hook)(const char* step, const char* path)) {
    g_file_step_hook.store(hook, std::memory_order_release);
}

static void file_step(const char* step, const std::string& path) {
    const auto hook = g_file_step_hook.load(std::memory_order_acquire);
    if (hook != nullptr && !hook(step, path.c_str())) {
        throw std::filesystem::filesystem_error(std::string("SaveData: ") + step + " failed", path, std::make_error_code(std::errc::io_error));
    }
}

static bool is_link(const std::filesystem::path& path, const std::filesystem::file_status& status) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return true;
    }
#endif
    return std::filesystem::is_symlink(status);
}

static std::filesystem::file_type plain_type(const std::string& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (status.type() == std::filesystem::file_type::not_found) {
        return std::filesystem::file_type::not_found;
    }
    if (error) {
        throw std::filesystem::filesystem_error("SaveData: cannot read the type of", path, error);
    }
    if (is_link(path, status) || (status.type() != std::filesystem::file_type::regular && status.type() != std::filesystem::file_type::directory)) {
        throw std::filesystem::filesystem_error("SaveData: not a plain file or directory", path, std::make_error_code(std::errc::operation_not_permitted));
    }
    return status.type();
}

static void require_plain_tree(const std::string& path) {
    if (plain_type(path) != std::filesystem::file_type::directory) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        require_plain_tree(entry.path().string());
    }
}

static void require_inside(const std::string& root, const std::string& path) {
    const auto base = std::filesystem::weakly_canonical(std::filesystem::absolute(root));
    const auto target = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
    const auto [inBase, inTarget] = std::mismatch(base.begin(), base.end(), target.begin(), target.end());
    if (inBase != base.end() || inTarget == target.end()) {
        throw std::filesystem::filesystem_error("SaveData: a save path leaves its root", path, root, std::make_error_code(std::errc::operation_not_permitted));
    }
}

static void copy_file_step(const char* step, const std::string& from, const std::string& to) {
    if (plain_type(from) != std::filesystem::file_type::regular) {
        throw std::filesystem::filesystem_error("SaveData: not a plain file", from, std::make_error_code(std::errc::operation_not_permitted));
    }
    file_step(step, to);
    std::filesystem::copy_file(from, to);
}

static void copy_tree(const char* step, const std::string& from, const std::string& to) {
    file_step(step, to);
    std::filesystem::create_directory(to);
    for (const auto& entry : std::filesystem::directory_iterator(from)) {
        const std::string source = entry.path().string();
        const std::string target = to + "/" + entry.path().filename().string();
        if (plain_type(source) == std::filesystem::file_type::directory) {
            copy_tree(step, source, target);
        } else {
            copy_file_step(step, source, target);
        }
    }
}

static std::map<std::string, std::uintmax_t> tree_sizes(const std::string& root) {
    std::map<std::string, std::uintmax_t> sizes;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        sizes[entry.path().lexically_relative(root).generic_string()] = entry.is_directory() ? std::numeric_limits<std::uintmax_t>::max() : entry.file_size();
    }
    return sizes;
}

static void flush_path(const std::string& path, bool directory) {
#ifdef _WIN32
    if (directory) {
        return;
    }
    const HANDLE handle = CreateFileW(std::filesystem::path(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        throw std::filesystem::filesystem_error("SaveData: cannot open to flush", path, std::error_code(static_cast<int>(GetLastError()), std::system_category()));
    }
    const bool flushed = FlushFileBuffers(handle) != 0;
    const DWORD error = GetLastError();
    CloseHandle(handle);
    if (!flushed) {
        throw std::filesystem::filesystem_error("SaveData: cannot flush", path, std::error_code(static_cast<int>(error), std::system_category()));
    }
#else
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | (directory ? O_DIRECTORY : 0));
    if (descriptor < 0) {
        throw std::filesystem::filesystem_error("SaveData: cannot open to flush", path, std::error_code(errno, std::generic_category()));
    }
    const int result = ::fsync(descriptor);
    const int error = errno;
    ::close(descriptor);
    if (result != 0 && error != EINVAL) {
        throw std::filesystem::filesystem_error("SaveData: cannot flush", path, std::error_code(error, std::generic_category()));
    }
#endif
}

static void flush_tree(const std::string& root) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        flush_path(entry.path().string(), entry.is_directory());
    }
    flush_path(root, true);
}

struct FileSwap {
    std::vector<std::pair<std::string, std::string>> moved;

    void move(const char* step, const std::string& from, const std::string& to) {
        file_step(step, to);
        std::filesystem::rename(from, to);
        moved.emplace_back(from, to);
        flush_path(std::filesystem::path(from).parent_path().string(), true);
        flush_path(std::filesystem::path(to).parent_path().string(), true);
    }

    void undo(const char* step) {
        while (!moved.empty()) {
            const auto [from, to] = moved.back();
            file_step(step, from);
            std::filesystem::rename(to, from);
            moved.pop_back();
            flush_path(std::filesystem::path(from).parent_path().string(), true);
            flush_path(std::filesystem::path(to).parent_path().string(), true);
        }
    }
};

static std::string backup_entry(const char* root, const std::string& dirName) {
    return std::string(root) + "/" + dirName;
}

static std::string entry_save(const std::string& entry) {
    return entry + "/save";
}

static void require_plain_paths(const std::string& dirName, std::initializer_list<const char*> roots) {
    for (const char* root : roots) {
        if (plain_type(root) == std::filesystem::file_type::regular) {
            throw std::filesystem::filesystem_error("SaveData: a save root is not a directory", root, std::make_error_code(std::errc::not_a_directory));
        }
        const std::string entry = backup_entry(root, dirName);
        for (const std::string& path : {entry, param_path(entry), blocks_path(entry)}) {
            require_inside(root, path);
            require_plain_tree(path);
        }
    }
}

static bool parse_dir_name(const SceSaveDataDirName* name, std::string* dirName) {
    const auto* nameEnd = static_cast<const char*>(std::memchr(name->data, '\0', sizeof(name->data)));
    if (nameEnd == nullptr) {
        return false;
    }
    dirName->assign(name->data, static_cast<std::size_t>(nameEnd - name->data));
    return !dirName->empty() && *dirName != "." && *dirName != ".." && dirName->find_first_of("/\\:") == std::string::npos;
}

static bool running_title(const SceSaveDataTitleId* titleId) {
    return titleId == nullptr || std::strncmp(titleId->data, GetAppTitleId_nid_postfix().value, sizeof(titleId->data)) == 0;
}

static int read_backup(const std::string& dirName, SaveDataParam* param) {
    const std::string entry = backup_entry(BACKUP_DIR, dirName);
    std::error_code error;
    if (!std::filesystem::exists(entry, error)) {
        return error ? SAVE_DATA_ERROR_INTERNAL : SAVE_DATA_ERROR_NOT_FOUND;
    }
    const std::string backup = entry_save(entry);
    std::vector<char> bytes;
    if (!std::filesystem::is_directory(backup, error) || !std::filesystem::is_regular_file(param_path(backup), error) || !read_file_all(param_path(backup), bytes) || bytes.size() != sizeof(SaveDataParam)) {
        return SAVE_DATA_ERROR_INTERNAL;
    }
    const auto blocks = std::filesystem::status(blocks_path(backup), error).type();
    if (blocks != std::filesystem::file_type::not_found && blocks != std::filesystem::file_type::regular) {
        return SAVE_DATA_ERROR_INTERNAL;
    }
    if (param != nullptr) {
        std::memcpy(param, bytes.data(), sizeof(SaveDataParam));
    }
    return SAVE_DATA_OK;
}

static void backupSave(const std::string& dirName) {
    const std::string source = save_root() + "/" + dirName;
    const std::string entry = backup_entry(BACKUP_DIR, dirName);
    const std::string staged = backup_entry(BACKUP_STAGING_DIR, dirName);
    const std::string old = backup_entry(BACKUP_OLD_DIR, dirName);
    require_plain_paths(dirName, {SAVE_DIR, BACKUP_DIR, BACKUP_STAGING_DIR, BACKUP_OLD_DIR});
    RemoveTree(staged);
    RemoveTree(old);
    std::filesystem::create_directories(staged);
    FileSwap swap;
    try {
        copy_tree("backup.copy", source, entry_save(staged));
        file_step("backup.copy", param_path(entry_save(staged)));
        if (!store_param(entry_save(staged), load_param(source))) {
            throw std::filesystem::filesystem_error("SaveData: cannot write the backup's param file", param_path(entry_save(staged)), std::make_error_code(std::errc::io_error));
        }
        if (std::filesystem::exists(blocks_path(source))) {
            copy_file_step("backup.copy", blocks_path(source), blocks_path(entry_save(staged)));
        }
        flush_tree(staged);
        std::filesystem::create_directories(BACKUP_DIR);
        if (std::filesystem::exists(entry)) {
            std::filesystem::create_directories(BACKUP_OLD_DIR);
            swap.move("backup.aside", entry, old);
        }
        swap.move("backup.place", staged, entry);
    } catch (...) {
        swap.undo("backup.rollback");
        std::error_code ignored;
        RemoveTree(staged, ignored);
        throw;
    }
    std::error_code ignored;
    RemoveTree(old, ignored);
}

static void restoreSave(const std::string& dirName) {
    const std::string live = save_root() + "/" + dirName;
    const std::string backupEntry = backup_entry(BACKUP_DIR, dirName);
    const std::string backup = entry_save(backupEntry);
    const std::string stagedEntry = backup_entry(RESTORE_STAGING_DIR, dirName);
    const std::string oldEntry = backup_entry(RESTORE_OLD_DIR, dirName);
    const std::string staged = entry_save(stagedEntry);
    const std::string old = entry_save(oldEntry);
    require_plain_paths(dirName, {SAVE_DIR, BACKUP_DIR, RESTORE_STAGING_DIR, RESTORE_OLD_DIR});
    RemoveTree(stagedEntry);
    RemoveTree(oldEntry);
    std::filesystem::create_directories(stagedEntry);
    std::filesystem::create_directories(oldEntry);
    FileSwap swap;
    try {
        copy_tree("restore.copy", backup, staged);
        copy_file_step("restore.copy", param_path(backup), param_path(staged));
        if (std::filesystem::exists(blocks_path(backup))) {
            copy_file_step("restore.copy", blocks_path(backup), blocks_path(staged));
        }
        if (tree_sizes(stagedEntry) != tree_sizes(backupEntry)) {
            throw std::filesystem::filesystem_error("SaveData: the staged copy of the backup is incomplete", stagedEntry, std::make_error_code(std::errc::io_error));
        }
        flush_tree(stagedEntry);
        swap.move("restore.aside", live, old);
        for (const auto& [from, to] : {std::pair{param_path(live), param_path(old)}, std::pair{blocks_path(live), blocks_path(old)}}) {
            if (std::filesystem::exists(from)) {
                swap.move("restore.aside", from, to);
            }
        }
        swap.move("restore.place", staged, live);
        swap.move("restore.place", param_path(staged), param_path(live));
        if (std::filesystem::exists(blocks_path(staged))) {
            swap.move("restore.place", blocks_path(staged), blocks_path(live));
        }
    } catch (...) {
        swap.undo("restore.rollback");
        std::error_code ignored;
        RemoveTree(stagedEntry, ignored);
        RemoveTree(oldEntry, ignored);
        throw;
    }
    std::error_code ignored;
    RemoveTree(oldEntry, ignored);
    RemoveTree(stagedEntry, ignored);
}

static int deleteSave(const SaveDataDelete* del) {
    if (del == nullptr || del->dir_name == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    std::string dirName;
    if (!parse_dir_name(del->dir_name, &dirName)) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    const std::string path = save_root() + "/" + dirName;
    std::lock_guard lock(g_slots_mutex);
    std::error_code error;
    if (isMountedDirectory(path, error)) return SAVE_DATA_ERROR_BUSY;
    if (error) return SAVE_DATA_ERROR_INTERNAL;
    if (std::filesystem::is_directory(path)) {
        RemoveTree(path);
    }
    std::error_code ec;
    std::filesystem::remove(param_path(path), ec);
    std::filesystem::remove(blocks_path(path), ec);
    RemoveTree(backup_entry(BACKUP_DIR, dirName));
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataDelete(const SaveDataDelete* del) {
    const int rc = deleteSave(del);
    SAVEDATA_TRACE("delete dir=%s -> 0x%08x", del != nullptr && del->dir_name != nullptr ? del->dir_name->data : "(null)", static_cast<unsigned>(rc));
    return rc;
}

int APS5_VABI sceSaveDataDeleteTransactionResource(int32_t resource) {
    (void)resource;
    if (g_initializations == 0) return SAVE_DATA_ERROR_NOT_INITIALIZED;
    return SAVE_DATA_OK;
}

static constexpr std::uint64_t SAVE_DATA_BLOCK_BYTES = 65536;

enum class FreeSpaceMode : std::uint8_t { Fixed, Host };

static FreeSpaceMode free_space_mode() {
    const char* configured = std::getenv("ANYPS5_FREE_SPACE");
    if (configured == nullptr || std::strcmp(configured, "fixed") == 0) return FreeSpaceMode::Fixed;
    if (std::strcmp(configured, "host") == 0) return FreeSpaceMode::Host;
    throw std::runtime_error("ANYPS5_FREE_SPACE must be fixed or host, not \"" + std::string(configured) + "\"");
}

static std::uint64_t default_free_space(const std::string& path) {
    std::filesystem::path probe = std::filesystem::absolute(path);
    std::error_code error;
    while (!std::filesystem::exists(probe, error) && probe.has_relative_path()) probe = probe.parent_path();
    return static_cast<std::uint64_t>(std::filesystem::space(probe).available);
}

static SaveDataFreeSpaceSource g_free_space_source = default_free_space;

extern "C" void SetSaveDataFreeSpaceSourceForTest_nid_no_patch(SaveDataFreeSpaceSource source) {
    g_free_space_source = source != nullptr ? source : default_free_space;
}

static bool host_free_bytes(const std::string& path, std::uint64_t& bytes) {
    try {
        bytes = g_free_space_source(path);
        return true;
    } catch (const std::exception& failure) {
        static std::atomic<bool> reported{false};
        if (!reported.exchange(true, std::memory_order_relaxed)) {
            std::fprintf(stderr, "[SAVEDATA:native] host free space unavailable, saves keep their own size: %s\n", failure.what());
        }
        return false;
    }
}

static std::uint64_t save_blocks(const std::string& real_path) {
    std::ifstream file(blocks_path(real_path));
    std::uint64_t blocks = 0;
    if (!(file >> blocks) || blocks == 0) {
        return SAVE_DATA_BLOCKS_MAX;
    }
    return std::min(blocks, SAVE_DATA_BLOCKS_MAX);
}

static bool keep_save_blocks(const std::string& real_path, std::uint64_t blocks) {
    const auto content = std::to_string(std::min(blocks, SAVE_DATA_BLOCKS_MAX)) + '\n';
    return savedata::replace_file(blocks_path(real_path), content.data(), content.size());
}

static std::uint64_t used_blocks(const std::string& real_path) {
    std::uint64_t blocks = 0;
    std::error_code error;
    for (auto it = std::filesystem::recursive_directory_iterator(real_path, error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
        if (!it->is_regular_file(error)) {
            continue;
        }
        const auto bytes = it->file_size(error);
        if (!error) {
            blocks += (bytes + SAVE_DATA_BLOCK_BYTES - 1) / SAVE_DATA_BLOCK_BYTES;
        }
    }
    return blocks;
}

static SaveDataSearchInfo search_info(const std::string& real_path) {
    const auto blocks = save_blocks(real_path);
    auto freeBlocks = blocks - std::min(blocks, used_blocks(real_path));
    if (free_space_mode() == FreeSpaceMode::Host) {
        std::uint64_t available = 0;
        if (host_free_bytes(real_path, available)) freeBlocks = std::min(freeBlocks, available / SAVE_DATA_BLOCK_BYTES);
    }
    return SaveDataSearchInfo{blocks, freeBlocks, {}};
}

static int dirNameSearch(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
    if (cond == nullptr || result == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    result->hit_num = 0;
    result->set_num = 0;
    const std::uint32_t key = cond->key;
    if ((key > SAVE_DATA_SORT_KEY_MTIME && key != SAVE_DATA_SORT_KEY_FREE_BLOCKS) || cond->order > SAVE_DATA_SORT_ORDER_DESCENT) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (cond->title_id != nullptr && std::strncmp(cond->title_id->data, GetAppTitleId_nid_postfix().value, sizeof(cond->title_id->data)) != 0) {
        return SAVE_DATA_OK;
    }
    const char* pattern = (cond->dir_name != nullptr) ? cond->dir_name->data : nullptr;
    const std::string root = save_root();
    if (!std::filesystem::is_directory(root)) {
        return SAVE_DATA_OK;
    }
    struct Match {
        std::string name;
        std::string path;
        SaveDataParam param;
        SaveDataSearchInfo info;
    };
    const bool sortByParam = key == SAVE_DATA_SORT_KEY_USER_PARAM || key == SAVE_DATA_SORT_KEY_MTIME;
    std::vector<Match> matches;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (!dir_name_match(name.c_str(), pattern)) {
            continue;
        }
        Match match{name, entry.path().string(), {}, search_info(entry.path().string())};
        if (sortByParam) {
            match.param = load_param(match.path);
        }
        matches.push_back(std::move(match));
    }
    std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.name < b.name; });
    if (key == SAVE_DATA_SORT_KEY_USER_PARAM) {
        std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.param.user_param < b.param.user_param; });
    } else if (key == SAVE_DATA_SORT_KEY_MTIME) {
        std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.param.mtime < b.param.mtime; });
    } else if (key == SAVE_DATA_SORT_KEY_BLOCKS) {
        std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.info.blocks < b.info.blocks; });
    } else if (key == SAVE_DATA_SORT_KEY_FREE_BLOCKS) {
        std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.info.free_blocks < b.info.free_blocks; });
    }
    if (cond->order == SAVE_DATA_SORT_ORDER_DESCENT) {
        std::reverse(matches.begin(), matches.end());
    }
    const std::uint32_t set = result->dir_names == nullptr ? 0 : static_cast<std::uint32_t>(std::min<std::size_t>(result->dir_names_num, matches.size()));
    for (std::uint32_t i = 0; i < set; i++) {
        std::snprintf(result->dir_names[i].data, sizeof(result->dir_names[i].data), "%s", matches[i].name.c_str());
        if (result->params != nullptr) {
            result->params[i] = sortByParam ? matches[i].param : load_param(matches[i].path);
        }
        if (result->infos != nullptr) {
            result->infos[i] = matches[i].info;
        }
    }
    result->hit_num = static_cast<std::uint32_t>(matches.size());
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
        return SAVE_DATA_ERROR_PARAMETER;
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
        return SAVE_DATA_ERROR_PARAMETER;
    }
    std::lock_guard lock(g_slots_mutex);
    const int slot = find_slot_by_mount_point(mount_point->data);
    if (slot == -1) {
        return SAVE_DATA_ERROR_NOT_MOUNTED;
    }
    std::memset(info, 0, sizeof(*info));
    const auto search = search_info(g_slots[slot].real_path);
    info->blocks = search.blocks;
    info->free_blocks = search.free_blocks;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataGetMountInfo(const SaveDataMountPoint* mount_point, SaveDataMountInfo* info) {
    const int rc = getMountInfo(mount_point, info);
    SAVEDATA_TRACE("getMountInfo point=%s -> 0x%08x", mount_point != nullptr ? mount_point->data : "(null)", static_cast<unsigned>(rc));
    return rc;
}

static int getParam(const SaveDataMountPoint* mount_point, uint32_t param_type, void* param_buf, size_t param_buf_size, size_t* got_size) {
    if (mount_point == nullptr || param_buf == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
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
    replay_memory_journal(get_param->user_id, get_param->slot_id);
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
        const GuestArena::HostWrite destination(d->buf, d->buf_size);
        if (!destination.Open()) {
            throw std::runtime_error("SaveData: the memory buffer is not writable");
        }
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
    std::lock_guard lock(g_slots_mutex);
    ++g_initializations;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataLoadIcon(const SaveDataMountPoint* mount_point, SaveDataIcon* icon) {
    (void)icon;
    if (mount_point == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
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
        return SAVE_DATA_ERROR_PARAMETER;
    }
    const FreeSpaceMode freeSpace = free_space_mode();
    const bool create = (mount->mount_mode & SAVE_DATA_MOUNT_MODE_CREATE) != 0;
    const bool create2 = (mount->mount_mode & SAVE_DATA_MOUNT_MODE_CREATE2) != 0;
    const bool rdonly = (mount->mount_mode & SAVE_DATA_MOUNT_MODE_RDONLY) != 0;
    const bool rdwr = (mount->mount_mode & SAVE_DATA_MOUNT_MODE_RDWR) != 0;
    const bool open = !create && !create2 && (rdonly || rdwr);
    if (!create && !create2 && !open) {
        throw std::runtime_error("sceSaveDataMount3: unknown mount_mode");
    }
    std::string dirName;
    if (!parse_dir_name(mount->dir_name, &dirName)) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    const std::string real_path = save_root() + "/" + dirName;
    std::lock_guard lock(g_slots_mutex);
    std::error_code error;
    if (isMountedDirectory(real_path, error)) return SAVE_DATA_ERROR_BUSY;
    if (error) return SAVE_DATA_ERROR_INTERNAL;
    const bool exists = std::filesystem::is_directory(real_path, error);
    if (error == std::errc::no_such_file_or_directory) error.clear();
    if (error) return SAVE_DATA_ERROR_INTERNAL;
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
    if (!exists && (create || create2) && freeSpace == FreeSpaceMode::Host) {
        const std::uint64_t blocks = mount->blocks == 0 ? SAVE_DATA_BLOCKS_MAX : std::min(mount->blocks, SAVE_DATA_BLOCKS_MAX);
        const std::uint64_t required = blocks * SAVE_DATA_BLOCK_BYTES;
        std::uint64_t available = 0;
        if (host_free_bytes(real_path, available) && available < required) {
            std::memset(mount_result, 0, sizeof(*mount_result));
            mount_result->required_blocks = (required - available + SAVE_DATA_BLOCK_BYTES - 1) / SAVE_DATA_BLOCK_BYTES;
            return SAVE_DATA_ERROR_NO_SPACE_FS;
        }
    }
    bool keepSize = mount->blocks != 0 && (create || (create2 && !exists));
    if (mount->blocks != 0 && !keepSize) {
        keepSize = !std::filesystem::exists(blocks_path(real_path), error);
        if (error) return SAVE_DATA_ERROR_INTERNAL;
    }
    const auto absolutePath = std::filesystem::absolute(real_path, error).string();
    if (error) return SAVE_DATA_ERROR_INTERNAL;
    const bool created = (create || create2) && std::filesystem::create_directories(real_path, error);
    if (error) return SAVE_DATA_ERROR_INTERNAL;
    if (keepSize && !keep_save_blocks(real_path, mount->blocks)) {
        if (created) {
            std::error_code ignored;
            std::filesystem::remove(real_path, ignored);
        }
        return SAVE_DATA_ERROR_INTERNAL;
    }
    // The title gets a short mount point (16 bytes on the PS5) and opens files under it; the
    // path resolver maps it to the save directory, whose name may be far longer.
    const std::string mountPoint = "/savedata" + std::to_string(slot);
    AddPathAlias_nid_no_patch(mountPoint.c_str(), absolutePath.c_str());
    std::memset(mount_result, 0, sizeof(*mount_result));
    g_slots[slot].used = true;
    g_slots[slot].user_id = mount->user_id;
    g_slots[slot].mount_point = mountPoint;
    g_slots[slot].real_path = real_path;
    std::memcpy(mount_result->mount_point.data, mountPoint.c_str(), mountPoint.size() + 1);
    mount_result->required_blocks = 0;
    mount_result->mount_status = (create || (create2 && !exists)) ? 1u : 0u;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataMount3(const SaveDataMount3* mount, SaveDataMountResult* mount_result) {
    const int rc = mount3(mount, mount_result);
    SAVEDATA_TRACE("mount3 user=%d dir=%.*s mode=0x%x blocks=%llu -> 0x%08x point=%.*s status=%u", mount != nullptr ? mount->user_id : -1, static_cast<int>(sizeof(SceSaveDataDirName::data)), mount != nullptr && mount->dir_name != nullptr ? mount->dir_name->data : "(null)", mount != nullptr ? mount->mount_mode : 0u, mount != nullptr ? static_cast<unsigned long long>(mount->blocks) : 0ull, static_cast<unsigned>(rc), static_cast<int>(sizeof(SaveDataMountPoint::data)), rc == SAVE_DATA_OK && mount_result != nullptr ? mount_result->mount_point.data : "", rc == SAVE_DATA_OK && mount_result != nullptr ? mount_result->mount_status : 0u);
    return rc;
}

static int mount2(const SaveDataMount2* mount, SaveDataMountResult* mount_result) {
    if (mount == nullptr || mount_result == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    if (mount->user_id == -1 || mount->user_id == 0) {
        return SAVE_DATA_ERROR_INVALID_LOGIN_USER;
    }
    const bool creates = (mount->mount_mode & SAVE_DATA_MOUNT_MODE_CREATE) != 0;
    if (mount->mount_mode == 0 || (creates && mount->blocks < SAVE_DATA_BLOCKS_MIN2)) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (mount->dir_name != nullptr) {
        const std::string_view name(mount->dir_name->data, sizeof(mount->dir_name->data));
        if (name.substr(0, name.find('\0')).find('_') != std::string_view::npos) {
            return SAVE_DATA_ERROR_PARAMETER;
        }
    }
    SaveDataMount3 converted{};
    converted.user_id = mount->user_id;
    converted.dir_name = mount->dir_name;
    converted.blocks = mount->blocks;
    converted.mount_mode = mount->mount_mode;
    return mount3(&converted, mount_result);
}

int APS5_VABI sceSaveDataMount2(const SaveDataMount2* mount, SaveDataMountResult* mount_result) {
    const int rc = mount2(mount, mount_result);
    SAVEDATA_TRACE("mount2 user=%d dir=%.*s mode=0x%x blocks=%llu -> 0x%08x point=%.*s status=%u", mount != nullptr ? mount->user_id : -1, static_cast<int>(sizeof(SceSaveDataDirName::data)), mount != nullptr && mount->dir_name != nullptr ? mount->dir_name->data : "(null)", mount != nullptr ? mount->mount_mode : 0u, mount != nullptr ? static_cast<unsigned long long>(mount->blocks) : 0ull, static_cast<unsigned>(rc), static_cast<int>(sizeof(SaveDataMountPoint::data)), mount_result != nullptr ? mount_result->mount_point.data : "", mount_result != nullptr ? mount_result->mount_status : 0u);
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
        return SAVE_DATA_ERROR_PARAMETER;
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
        return SAVE_DATA_ERROR_PARAMETER;
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
    replay_memory_journal(set_param->user_id, set_param->slot_id);
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
    std::vector<MemoryWrite> writes;
    for (std::uint32_t i = 0; i < n; i++) {
        const SaveDataMemoryData& d = set_param->data[i];
        if (d.buf_size != 0) {
            writes.push_back({d.offset, d.buf_size, static_cast<const char*>(d.buf)});
        }
    }
    if (!writes.empty()) {
        std::fstream memory(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!memory) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
        const std::string journal = mem_path(set_param->user_id, set_param->slot_id, "journal");
        const std::vector<char> bytes = encode_memory_journal(writes);
        const bool staged = savedata::replace_file_with(path, bytes.data(), bytes.size(), [&journal](const auto& temporary, const auto&) {
            return savedata::move_replacing(temporary, journal);
        });
        if (!staged) {
            return SAVE_DATA_ERROR_INTERNAL;
        }
        apply_memory_journal(memory, journal, writes);
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
    replay_memory_journal(setup_param->user_id, setup_param->slot_id);
    const std::string path = mem_path(setup_param->user_id, setup_param->slot_id, "bin");
    std::size_t existed = 0;
    const bool have = file_size_of(path, &existed);
    if (!have) {
        existed = 0;
    }
    const bool needParam = (setup_param->init_param != nullptr && (setup_param->option & 1u) != 0);
    const std::string paramPath = mem_path(setup_param->user_id, setup_param->slot_id, "param");
    std::size_t paramSize = 0;
    const bool haveParam = file_size_of(paramPath, &paramSize);
    // First run: create a zero-filled blob and report existed size 0 so the title treats it as a new save.
    if (!have || existed < setup_param->memory_size || (needParam && !haveParam)) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
        if (needParam) {
            std::vector<char> pd(sizeof(SaveDataParam));
            std::memcpy(pd.data(), setup_param->init_param, sizeof(SaveDataParam));
            if (!write_file_replace(paramPath, pd)) {
                return SAVE_DATA_ERROR_INTERNAL;
            }
        }
        if (!have || existed < setup_param->memory_size) {
            std::vector<char> data;
            if (have && !read_file_all(path, data)) {
                return SAVE_DATA_ERROR_INTERNAL;
            }
            data.resize(setup_param->memory_size, 0);
            if (!write_file_replace(path, data)) {
                return SAVE_DATA_ERROR_INTERNAL;
            }
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
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    // Every Set already writes straight through to disk; sync only has to confirm the memory exists.
    const std::int32_t user_id = *static_cast<const std::int32_t*>(sync_param);
    const std::uint32_t slot_id = reinterpret_cast<const std::uint32_t*>(sync_param)[1];
    std::lock_guard<std::mutex> lk(g_mem_mutex);
    replay_memory_journal(user_id, slot_id);
    std::size_t size = 0;
    if (!file_size_of(mem_path(user_id, slot_id, "bin"), &size)) {
        return SAVE_DATA_ERROR_MEMORY_NOT_READY;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataTerminate(void) {
    if (InEventCallback()) {
        return SAVE_DATA_ERROR_BUSY;
    }
    EventWorker* stopped = nullptr;
    {
        std::lock_guard lock(g_slots_mutex);
        if (g_initializations == 0) {
            return SAVE_DATA_ERROR_NOT_INITIALIZED;
        }
        if (g_initializations == 1 && any_slot_used()) {
            return SAVE_DATA_ERROR_BUSY;
        }
        --g_initializations;
        if (g_initializations == 0) {
            g_convertProgress = 0.0f;
            stopped = DetachEventDelivery();
        }
    }
    if (stopped != nullptr) JoinEventDelivery(stopped);
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
        return SAVE_DATA_ERROR_PARAMETER;
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
        return SAVE_DATA_ERROR_PARAMETER;
    }
    result->hit_num = 0;
    result->set_num = 0;
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataDirNameSearchPs4(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
    const int rc = dirNameSearchPs4(cond, result);
    SAVEDATA_TRACE("dirNameSearchPs4 user=%d -> 0x%08x", cond != nullptr ? cond->user_id : -1, static_cast<unsigned>(rc));
    return rc;
}

static int convert(const SaveDataConvert* param) {
    if (param == nullptr || param->dir_name == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    g_convertProgress = 0.0f;
    std::string dirName;
    if (!parse_dir_name(param->dir_name, &dirName)) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (!std::filesystem::is_directory(save_root() + "/" + dirName)) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    g_convertProgress = 1.0f;
    SaveDataEvent event{};
    event.type = SAVE_DATA_EVENT_TYPE_CONVERT;
    event.error_code = SAVE_DATA_OK;
    event.user_id = param->user_id;
    event.dir_name = *param->dir_name;
    std::lock_guard lock(g_state_mutex);
    g_events.push_back(event);
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataConvert(const SaveDataConvert* param) {
    const int rc = convert(param);
    SAVEDATA_TRACE("convert user=%d dir=%s -> 0x%08x", param != nullptr ? param->user_id : -1, param != nullptr && param->dir_name != nullptr ? param->dir_name->data : "(null)", static_cast<unsigned>(rc));
    return rc;
}

static int getConvertProgress(float* progress) {
    if (progress == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    *progress = g_convertProgress.load();
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataGetConvertProgress(float* progress) {
    const int rc = getConvertProgress(progress);
    SAVEDATA_TRACE("getConvertProgress -> 0x%08x progress=%.2f", static_cast<unsigned>(rc), progress != nullptr && rc == SAVE_DATA_OK ? *progress : 0.0f);
    return rc;
}

int APS5_VABI sceSaveDataCancel(std::uint32_t cancelArgument) {
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDirNameSearch2(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
    const int rc = dirNameSearch(cond, result);
    SAVEDATA_TRACE("dirNameSearch2 user=%d pattern=%s hit=%u set=%u -> 0x%08x", cond != nullptr ? cond->user_id : -1, cond != nullptr && cond->dir_name != nullptr ? cond->dir_name->data : "(all)", result != nullptr ? result->hit_num : 0u, result != nullptr ? result->set_num : 0u, static_cast<unsigned>(rc));
    return rc;
}

int APS5_VABI sceSaveDataPrepareForTransferring(const SaveDataMountPoint* mountPoint, const SaveDataPrepareParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataCheckIpmiIfSize() {
    return 0;
}

int APS5_VABI sceSaveDataDebug() {
    return 0;
}

int APS5_VABI sceSaveDataDebugCheckBackupData() {
    return 0;
}

int APS5_VABI sceSaveDataDebugCleanMount() {
    return 0;
}

int APS5_VABI sceSaveDataDebugCompiledSdkVersion() {
    return 0;
}

int APS5_VABI sceSaveDataDebugCreateSaveDataRoot() {
    return 0;
}

int APS5_VABI sceSaveDataDebugProspero() {
    return 0;
}

int APS5_VABI sceSaveDataDebugRemoveSaveDataRoot() {
    return 0;
}

int APS5_VABI sceSaveDataDebugTarget() {
    return 0;
}

int APS5_VABI sceSaveDataAbort() {
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataBindPsnAccount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataBindPsnAccountForSystemBackup(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataChangeDatabase(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataChangeInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

static int checkBackupData(const SaveDataCheckBackupData* check) {
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    if (check == nullptr || check->dir_name == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    std::string dirName;
    if (!parse_dir_name(check->dir_name, &dirName)) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (!running_title(check->title_id)) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    std::lock_guard lock(g_slots_mutex);
    std::error_code error;
    if (isMountedDirectory(save_root() + "/" + dirName, error)) {
        return SAVE_DATA_ERROR_BUSY;
    }
    if (error) return SAVE_DATA_ERROR_INTERNAL;
    try {
        require_plain_paths(dirName, {BACKUP_DIR});
    } catch (const std::filesystem::filesystem_error&) {
        return SAVE_DATA_ERROR_INTERNAL;
    }
    return read_backup(dirName, check->param);
}

int APS5_VABI sceSaveDataCheckBackupData(const SaveDataCheckBackupData* check) {
    const int rc = checkBackupData(check);
    SAVEDATA_TRACE("checkBackupData dir=%.*s -> 0x%08x", static_cast<int>(sizeof(SceSaveDataDirName::data)), check != nullptr && check->dir_name != nullptr ? check->dir_name->data : "(null)", static_cast<unsigned>(rc));
    return rc;
}

int APS5_VABI sceSaveDataCheckBackupDataForCdlg(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataCheckBackupDataInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataCheckBackupDataLight(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataCheckCloudData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataCheckSaveDataBroken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataCheckSaveDataVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataCheckSaveDataVersionLatest(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataClearProgress() {
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataCreateUploadData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDebugGetThreadId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDeleteAllUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDeleteBackupData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDeleteCloudData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDeleteProspero(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDeleteTitle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDeleteUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDirNameSearchInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataDownload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetAllSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetAppLaunchedUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetAutoUploadConditions(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetAutoUploadRequestInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetAutoUploadSetting(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetBoundPsnAccountCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetClientThreadPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetCloudQuotaInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetEventInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetMountedSaveDataCount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetSaveDataCount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetSaveDataRootDir(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetSaveDataRootPath(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetSaveDataRootUsbPath(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetSavePoint(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataGetUpdatedDataCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataInitialize2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataInitializeForCdlg(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataIsDeletingUsbDb(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataIsMounted() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataManualDelete(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataManualDownload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataManualUpload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataMount5() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataMountInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataMountSys(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataRebuildDatabase(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataRegisterEventCallback(SaveDataEventCallback callback, void* userdata) {
    auto& callbacks = Callbacks();
    std::lock_guard calling(callbacks.calling);
    std::lock_guard slots(g_slots_mutex);
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    if (callback == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    StartEventDelivery();
    std::lock_guard lock(callbacks.mutex);
    callbacks.callback = callback;
    callbacks.userdata = userdata;
    return SAVE_DATA_OK;
}

static int restoreBackupData(const SaveDataRestoreBackupData* restore) {
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    if (restore == nullptr || restore->dir_name == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    std::string dirName;
    if (!parse_dir_name(restore->dir_name, &dirName)) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    if (!running_title(restore->title_id)) {
        return SAVE_DATA_ERROR_NOT_FOUND;
    }
    const std::string target = save_root() + "/" + dirName;
    std::lock_guard lock(g_slots_mutex);
    std::error_code error;
    if (isMountedDirectory(target, error)) {
        return SAVE_DATA_ERROR_BUSY;
    }
    if (error) return SAVE_DATA_ERROR_INTERNAL;
    const int backup = read_backup(dirName, nullptr);
    if (backup != SAVE_DATA_OK) {
        return backup;
    }
    if (!std::filesystem::is_directory(target)) {
        return SAVE_DATA_ERROR_INTERNAL;
    }
    try {
        restoreSave(dirName);
    } catch (const std::filesystem::filesystem_error&) {
        return SAVE_DATA_ERROR_INTERNAL;
    }
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataRestoreBackupData(const SaveDataRestoreBackupData* restore) {
    const int rc = restoreBackupData(restore);
    SAVEDATA_TRACE("restoreBackupData dir=%.*s -> 0x%08x", static_cast<int>(sizeof(SceSaveDataDirName::data)), restore != nullptr && restore->dir_name != nullptr ? restore->dir_name->data : "(null)", static_cast<unsigned>(rc));
    return rc;
}

int APS5_VABI sceSaveDataRestoreBackupDataForCdlg(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataRestoreLoadSaveDataMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataSetAutoUploadSetting(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataSetEventInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataSetSaveDataLibraryUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataShutdownStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataSupportedFakeBrokenStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataSyncCloudList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataUmountSys(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

static int umountWithBackup(const SaveDataMountPoint* mount_point) {
    if (g_initializations == 0) {
        return SAVE_DATA_ERROR_NOT_INITIALIZED;
    }
    if (mount_point == nullptr) {
        return SAVE_DATA_ERROR_PARAMETER;
    }
    SaveDataEvent event{};
    event.type = SAVE_DATA_EVENT_TYPE_UMOUNT_BACKUP;
    event.error_code = SAVE_DATA_OK;
    {
        std::lock_guard lock(g_slots_mutex);
        const int slot = find_slot_by_mount_point(mount_point->data);
        if (slot == -1) {
            return SAVE_DATA_ERROR_NOT_MOUNTED;
        }
        const AppTitleId title = GetAppTitleId_nid_postfix();
        const MountSlot mounted = g_slots[slot];
        RemovePathAlias_nid_no_patch(mounted.mount_point.c_str());
        g_slots[slot] = MountSlot{};
        const std::string dirName = std::filesystem::path(mounted.real_path).filename().string();
        event.user_id = mounted.user_id;
        std::memcpy(event.title_id.data, title.value, strnlen(title.value, sizeof(event.title_id.data) - 1));
        std::memcpy(event.dir_name.data, dirName.data(), std::min(dirName.size(), sizeof(event.dir_name.data) - 1));
        try {
            backupSave(dirName);
        } catch (const std::exception&) {
            event.error_code = SAVE_DATA_ERROR_INTERNAL;
        }
    }
    {
        std::lock_guard lock(g_state_mutex);
        g_events.push_back(event);
    }
    DeliverEvent(event);
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataUmountWithBackup(const SaveDataMountPoint* mount_point) {
    const int rc = umountWithBackup(mount_point);
    SAVEDATA_TRACE("umountWithBackup point=%s -> 0x%08x", mount_point != nullptr ? mount_point->data : "(null)", static_cast<unsigned>(rc));
    return rc;
}

int APS5_VABI sceSaveDataUnregisterEventCallback(void) {
    auto& callbacks = Callbacks();
    std::lock_guard calling(callbacks.calling);
    std::lock_guard lock(callbacks.mutex);
    callbacks.callback = nullptr;
    callbacks.userdata = nullptr;
    callbacks.pending.clear();
    return SAVE_DATA_OK;
}

int APS5_VABI sceSaveDataUpload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSaveDataCheckPremiumMember() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSaveDataCheckSyncStatus() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSaveDataGetAllSizeProspero() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSaveDataGetAutoSyncInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSaveDataGetCloudInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSaveDataNotifySaveDataDownload() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSaveDataSetAutoSyncSettings() {
    NotImplemented_nid_no_patch(__func__);
}

}

namespace {

}

extern "C" {

int APS5_VABI sceSaveDataInitialize(const void* init) {
    return sceSaveDataInitialize3(init);
}

int APS5_VABI sceSaveDataMount(const SaveDataMountPs4* mount, SaveDataMountResult* mount_result) {
    if (mount == nullptr || mount_result == nullptr || mount->dir_name == nullptr) return SAVE_DATA_ERROR_PARAMETER;
    if (mount->title_id != nullptr) NotImplemented_nid_no_patch("sceSaveDataMount of another title's save data");
    SaveDataMount3 converted{};
    converted.user_id = mount->user_id;
    converted.dir_name = mount->dir_name;
    converted.blocks = mount->blocks;
    converted.mount_mode = mount->mount_mode;
    return sceSaveDataMount3(&converted, mount_result);
}

int APS5_VABI sceSaveDataUmount(const SaveDataMountPoint* mount_point) {
    return sceSaveDataUmount2(0, mount_point);
}

int APS5_VABI sceSaveDataSetupSaveDataMemory(std::int32_t user_id, std::size_t memory_size, const SaveDataParam* param) {
    SaveDataMemorySetup2 setup{};
    setup.user_id = user_id;
    setup.memory_size = memory_size;
    SaveDataMemorySetupResult result{};
    if (const int rc = sceSaveDataSetupSaveDataMemory2(&setup, &result); rc != SAVE_DATA_OK) return rc;
    if (param == nullptr) return SAVE_DATA_OK;
    SaveDataMemorySet2 set{};
    set.user_id = user_id;
    set.param = param;
    return sceSaveDataSetSaveDataMemory2(&set);
}

int APS5_VABI sceSaveDataGetSaveDataMemory(std::int32_t user_id, void* buf, std::size_t buf_size, std::int64_t offset) {
    SaveDataMemoryData data{};
    data.buf = buf;
    data.buf_size = buf_size;
    data.offset = static_cast<std::size_t>(offset);
    SaveDataMemoryGet2 get{};
    get.user_id = user_id;
    get.data = &data;
    return sceSaveDataGetSaveDataMemory2(&get);
}

int APS5_VABI sceSaveDataSetSaveDataMemory(std::int32_t user_id, void* buf, std::size_t buf_size, std::int64_t offset) {
    SaveDataMemoryData data{};
    data.buf = buf;
    data.buf_size = buf_size;
    data.offset = static_cast<std::size_t>(offset);
    SaveDataMemorySet2 set{};
    set.user_id = user_id;
    set.data = &data;
    set.data_num = 1;
    return sceSaveDataSetSaveDataMemory2(&set);
}

}
