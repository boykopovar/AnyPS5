#include "Runtime.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <optional>

namespace Fios {
namespace {

constexpr std::size_t MaxStoredParameters = 512;
constexpr std::uint64_t IoChunk = 1ull << 30;

struct Op {
    SceFiosOp id = 0;
    SceFiosOpAttr attr{};
    std::string path;
    std::int64_t offset = 0;
    std::int64_t request = 0;
    void* buffer = nullptr;
    Work work;
    bool done = false;
    bool queued = false;
    bool cancelled = false;
    std::int32_t error = SCE_FIOS_OK;
    std::int64_t result = 0;
};

struct State {
    std::mutex mutex;
    std::condition_variable finished;
    std::map<SceFiosHandle, std::shared_ptr<Op>> ops;
    std::map<SceFiosHandle, std::shared_ptr<FileHandle>> files;
    std::map<SceFiosHandle, std::shared_ptr<DirectoryHandle>> directories;
    std::deque<SceFiosOp> pending;
    SceFiosHandle lastHandle = 0;
    bool initialized = false;
    std::vector<std::uint8_t> parameters;
    std::uint32_t suspendCount = 0;
    std::optional<SceFiosOpAttr> globalAttr;
};

State& Global() {
    static State state;
    return state;
}

thread_local std::optional<SceFiosOpAttr> g_threadAttr;

bool HandleInUse(const State& state, SceFiosHandle handle) {
    return state.ops.count(handle) != 0 || state.files.count(handle) != 0 || state.directories.count(handle) != 0;
}

SceFiosHandle NextHandle(State& state) {
    for (int attempts = 0; attempts < 0x7fffffff; ++attempts) {
        state.lastHandle = state.lastHandle >= 0x7ffffffe ? 1 : state.lastHandle + 1;
        if (!HandleInUse(state, state.lastHandle)) return state.lastHandle;
    }
    return 0;
}

void Finish(const std::shared_ptr<Op>& op, const Completion& completion) {
    SceFiosOpCallback callback = nullptr;
    void* context = nullptr;
    {
        const std::lock_guard lock(Global().mutex);
        op->error = completion.error;
        op->result = completion.result;
        op->done = true;
        op->queued = false;
        op->work = nullptr;
        callback = op->attr.pCallback;
        context = op->attr.pCallbackContext;
        Global().finished.notify_all();
    }
    if (callback != nullptr) callback(context, op->id, SCE_FIOS_OPEVENT_COMPLETE, completion.error);
}

void Run(const std::shared_ptr<Op>& op) {
    Work work;
    bool cancelled = false;
    {
        const std::lock_guard lock(Global().mutex);
        work = std::move(op->work);
        cancelled = op->cancelled;
    }
    Completion completion;
    if (cancelled || !work) {
        completion.error = SCE_FIOS_ERROR_CANCELLED;
        Finish(op, completion);
        return;
    }
    try {
        work(completion);
    } catch (...) {
        completion.error = SCE_FIOS_ERROR_UNKNOWN;
        completion.result = 0;
        Finish(op, completion);
        throw;
    }
    Finish(op, completion);
}

std::shared_ptr<Op> FindOp(State& state, SceFiosOp id) {
    const auto found = state.ops.find(id);
    return found == state.ops.end() ? nullptr : found->second;
}

void FireDelete(const std::shared_ptr<Op>& op) {
    if (op->attr.pCallback != nullptr) op->attr.pCallback(op->attr.pCallbackContext, op->id, SCE_FIOS_OPEVENT_DELETE, SCE_FIOS_OK);
}

bool CancelQueued(const std::shared_ptr<Op>& op) {
    {
        const std::lock_guard lock(Global().mutex);
        if (op->done || !op->queued) return false;
        auto& pending = Global().pending;
        pending.erase(std::remove(pending.begin(), pending.end(), op->id), pending.end());
        op->cancelled = true;
    }
    Completion completion;
    completion.error = SCE_FIOS_ERROR_CANCELLED;
    Finish(op, completion);
    return true;
}

}

std::int32_t FromKernelError(std::int64_t kernelResult) {
    if (kernelResult >= 0) return SCE_FIOS_OK;
    const auto code = static_cast<std::uint32_t>(kernelResult);
    if ((code & 0xffff0000u) != 0x80020000u) return SCE_FIOS_ERROR_UNKNOWN;
    switch (code & 0xffffu) {
    case 1: case 13: case 66: return SCE_FIOS_ERROR_ACCESS;
    case 2: case 20: return SCE_FIOS_ERROR_BAD_PATH;
    case 9: return SCE_FIOS_ERROR_BAD_FH;
    case 14: return SCE_FIOS_ERROR_BAD_PTR;
    case 17: return SCE_FIOS_ERROR_ALREADY_EXISTS;
    case 21: return SCE_FIOS_ERROR_NOT_A_FILE;
    case 22: return SCE_FIOS_ERROR_BAD_OFFSET;
    case 23: case 24: return SCE_FIOS_ERROR_CANT_ALLOCATE_FH;
    case 30: return SCE_FIOS_ERROR_READ_ONLY;
    case 63: return SCE_FIOS_ERROR_PATH_TOO_LONG;
    default: return SCE_FIOS_ERROR_UNKNOWN;
    }
}

std::int32_t CopyPath(const char* source, std::string& destination) {
    if (source == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    switch (CopyGuestString_nid_no_patch(source, SCE_FIOS_PATH_MAX, destination)) {
    case 0: break;
    case 14: return SCE_FIOS_ERROR_BAD_PTR;
    case 63: return SCE_FIOS_ERROR_PATH_TOO_LONG;
    default: return SCE_FIOS_ERROR_UNKNOWN;
    }
    return destination.empty() ? SCE_FIOS_ERROR_BAD_PATH : SCE_FIOS_OK;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    return GuestCopyOut_nid_no_patch(destination, source, bytes);
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    return GuestCopyIn_nid_no_patch(destination, source, bytes);
}

std::uint64_t TimespecToDate(std::int64_t seconds, std::int64_t nanoseconds) {
    if (seconds < 0) return 0;
    return static_cast<std::uint64_t>(seconds) * 1000000000ull + static_cast<std::uint64_t>(nanoseconds);
}

SceFiosOp Submit(const SceFiosOpAttr* attr, OpInfo info, Work work) {
    auto op = std::make_shared<Op>();
    bool runNow = false;
    SceFiosOpAttr given{};
    if (attr != nullptr && !CopyIn(&given, attr, sizeof(given))) return SCE_FIOS_ERROR_BAD_PTR;
    {
        const std::lock_guard lock(Global().mutex);
        State& state = Global();
        const SceFiosHandle id = NextHandle(state);
        if (id == 0) return SCE_FIOS_ERROR_CANT_ALLOCATE_OP;
        if (attr != nullptr) op->attr = given;
        else if (g_threadAttr) op->attr = *g_threadAttr;
        else if (state.globalAttr) op->attr = *state.globalAttr;
        op->id = id;
        op->path = std::move(info.path);
        op->offset = info.offset;
        op->request = info.request;
        op->buffer = info.buffer;
        op->work = std::move(work);
        state.ops[id] = op;
        if (state.suspendCount > 0) {
            op->queued = true;
            state.pending.push_back(id);
        } else {
            runNow = true;
        }
    }
    if (runNow) Run(op);
    return op->id;
}

SceFiosOp SubmitFailed(const SceFiosOpAttr* attr, OpInfo info, std::int32_t error) {
    return Submit(attr, std::move(info), [error](Completion& completion) { completion.error = error; });
}

std::int32_t Wait(SceFiosOp op) {
    std::unique_lock lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    if (!found) return SCE_FIOS_ERROR_BAD_OP;
    Global().finished.wait(lock, [&] { return found->done; });
    return found->error;
}

std::int32_t SyncStatus(SceFiosOp op) {
    if (op < 0) return op;
    Completion completion;
    return SyncWait(op, completion) ? completion.error : SCE_FIOS_ERROR_BAD_OP;
}

std::int64_t SyncCount(SceFiosOp op) {
    if (op < 0) return op;
    Completion completion;
    if (!SyncWait(op, completion)) return SCE_FIOS_ERROR_BAD_OP;
    return completion.error != SCE_FIOS_OK ? completion.error : completion.result;
}

bool SyncWait(SceFiosOp op, Completion& completion) {
    std::shared_ptr<Op> found;
    {
        std::unique_lock lock(Global().mutex);
        found = FindOp(Global(), op);
        if (!found) return false;
        Global().finished.wait(lock, [&] { return found->done; });
        completion.error = found->error;
        completion.result = found->result;
    }
    DeleteOp(op);
    return true;
}

bool OpDone(SceFiosOp op) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    return found && found->done;
}

bool DeleteOp(SceFiosOp op) {
    std::shared_ptr<Op> found;
    {
        const std::lock_guard lock(Global().mutex);
        found = FindOp(Global(), op);
    }
    if (!found) return false;
    CancelQueued(found);
    {
        std::unique_lock lock(Global().mutex);
        Global().finished.wait(lock, [&] { return found->done; });
        if (Global().ops.erase(op) == 0) return false;
    }
    FireDelete(found);
    return true;
}

bool CancelOp(SceFiosOp op) {
    std::shared_ptr<Op> found;
    {
        const std::lock_guard lock(Global().mutex);
        found = FindOp(Global(), op);
    }
    return found && CancelQueued(found);
}

bool OpIsCancelled(SceFiosOp op) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    return found && found->cancelled;
}

bool QueryOp(SceFiosOp op, std::int64_t& request, std::int64_t& actual, std::int64_t& offset) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    if (!found) return false;
    request = found->request;
    actual = found->done ? found->result : 0;
    offset = found->offset;
    return true;
}

bool OpError(SceFiosOp op, std::int32_t& error) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    if (!found) return false;
    error = found->done ? found->error : 1;
    return true;
}

const SceFiosOpAttr* OpAttr(SceFiosOp op) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    return found ? &found->attr : nullptr;
}

const char* OpPath(SceFiosOp op) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    return found ? found->path.c_str() : nullptr;
}

void* OpBuffer(SceFiosOp op) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    return found ? found->buffer : nullptr;
}

template<typename TMap>
std::uint32_t ListKeys(const TMap& map, SceFiosHandle* out, std::uint32_t capacity) {
    std::uint32_t index = 0;
    for (const auto& entry : map) {
        if (out != nullptr && index < capacity) out[index] = entry.first;
        ++index;
    }
    return index;
}

std::uint32_t ListOps(SceFiosOp* out, std::uint32_t capacity) {
    const std::lock_guard lock(Global().mutex);
    return ListKeys(Global().ops, out, capacity);
}

void CancelAll() {
    std::vector<std::shared_ptr<Op>> queued;
    {
        const std::lock_guard lock(Global().mutex);
        for (const SceFiosOp id : Global().pending) {
            if (const auto found = FindOp(Global(), id)) queued.push_back(found);
        }
    }
    for (const auto& op : queued) CancelQueued(op);
}

SceFiosFH AddFile(std::shared_ptr<FileHandle> file) {
    const std::lock_guard lock(Global().mutex);
    const SceFiosHandle id = NextHandle(Global());
    if (id == 0) return SCE_FIOS_ERROR_CANT_ALLOCATE_FH;
    Global().files[id] = std::move(file);
    return id;
}

std::shared_ptr<FileHandle> FindFile(SceFiosFH handle) {
    const std::lock_guard lock(Global().mutex);
    const auto found = Global().files.find(handle);
    return found == Global().files.end() ? nullptr : found->second;
}

std::shared_ptr<FileHandle> RemoveFile(SceFiosFH handle) {
    const std::lock_guard lock(Global().mutex);
    const auto found = Global().files.find(handle);
    if (found == Global().files.end()) return nullptr;
    auto file = std::move(found->second);
    Global().files.erase(found);
    return file;
}

std::uint32_t ListFiles(SceFiosFH* out, std::uint32_t capacity) {
    const std::lock_guard lock(Global().mutex);
    return ListKeys(Global().files, out, capacity);
}

void CloseAllFiles() {
    std::map<SceFiosHandle, std::shared_ptr<FileHandle>> files;
    {
        const std::lock_guard lock(Global().mutex);
        files.swap(Global().files);
    }
    for (auto& entry : files) {
        const std::lock_guard lock(entry.second->mutex);
        if (entry.second->descriptor >= 0) sceKernelClose(entry.second->descriptor);
        entry.second->descriptor = -1;
    }
}

SceFiosDH AddDirectory(std::shared_ptr<DirectoryHandle> directory) {
    const std::lock_guard lock(Global().mutex);
    const SceFiosHandle id = NextHandle(Global());
    if (id == 0) return SCE_FIOS_ERROR_CANT_ALLOCATE_DH;
    Global().directories[id] = std::move(directory);
    return id;
}

std::shared_ptr<DirectoryHandle> FindDirectory(SceFiosDH handle) {
    const std::lock_guard lock(Global().mutex);
    const auto found = Global().directories.find(handle);
    return found == Global().directories.end() ? nullptr : found->second;
}

std::shared_ptr<DirectoryHandle> RemoveDirectory(SceFiosDH handle) {
    const std::lock_guard lock(Global().mutex);
    const auto found = Global().directories.find(handle);
    if (found == Global().directories.end()) return nullptr;
    auto directory = std::move(found->second);
    Global().directories.erase(found);
    return directory;
}

std::uint32_t ListDirectories(SceFiosDH* out, std::uint32_t capacity) {
    const std::lock_guard lock(Global().mutex);
    return ListKeys(Global().directories, out, capacity);
}

bool IsValidHandle(SceFiosHandle handle) {
    const std::lock_guard lock(Global().mutex);
    return HandleInUse(Global(), handle);
}

bool StoreParameters(State& state, const void* params) {
    if (params == nullptr) {
        state.parameters.clear();
        return true;
    }
    std::uint32_t first = 0;
    if (!CopyIn(&first, params, sizeof(first))) return false;
    const std::size_t size = std::min<std::size_t>((first >> 1) & 0x7fff, MaxStoredParameters);
    std::vector<std::uint8_t> copy(size);
    if (size != 0 && !CopyIn(copy.data(), params, size)) return false;
    state.parameters = std::move(copy);
    return true;
}

bool Initialize(const void* params) {
    const std::lock_guard lock(Global().mutex);
    if (!StoreParameters(Global(), params)) return false;
    Global().initialized = true;
    return true;
}

bool UpdateParameters(const void* params) {
    const std::lock_guard lock(Global().mutex);
    return Global().initialized && StoreParameters(Global(), params);
}

void Terminate() {
    CancelAll();
    CloseAllFiles();
    const std::lock_guard lock(Global().mutex);
    State& state = Global();
    state.directories.clear();
    for (auto found = state.ops.begin(); found != state.ops.end();) {
        found = found->second->done ? state.ops.erase(found) : std::next(found);
    }
    state.parameters.clear();
    state.suspendCount = 0;
    state.initialized = false;
}

bool IsInitialized(void* out) {
    std::vector<std::uint8_t> copy;
    {
        const std::lock_guard lock(Global().mutex);
        if (!Global().initialized) return false;
        copy = Global().parameters;
    }
    if (out != nullptr && !copy.empty()) CopyOut(out, copy.data(), copy.size());
    return true;
}

void Suspend() {
    const std::lock_guard lock(Global().mutex);
    ++Global().suspendCount;
}

void Resume() {
    std::vector<std::shared_ptr<Op>> ready;
    {
        const std::lock_guard lock(Global().mutex);
        State& state = Global();
        if (state.suspendCount == 0) return;
        if (--state.suspendCount != 0) return;
        for (const SceFiosOp id : state.pending) {
            if (const auto found = FindOp(state, id)) {
                found->queued = false;
                ready.push_back(found);
            }
        }
        state.pending.clear();
    }
    for (const auto& op : ready) Run(op);
}

std::uint32_t SuspendCount() {
    const std::lock_guard lock(Global().mutex);
    return Global().suspendCount;
}

bool IsIdle() {
    const std::lock_guard lock(Global().mutex);
    return Global().pending.empty();
}

bool SetGlobalAttr(const SceFiosOpAttr* attr) {
    SceFiosOpAttr copy{};
    if (attr != nullptr && !CopyIn(&copy, attr, sizeof(copy))) return false;
    const std::lock_guard lock(Global().mutex);
    if (attr == nullptr) Global().globalAttr.reset();
    else Global().globalAttr = copy;
    return true;
}

bool GlobalAttr(SceFiosOpAttr& attr) {
    const std::lock_guard lock(Global().mutex);
    if (!Global().globalAttr) return false;
    attr = *Global().globalAttr;
    return true;
}

bool SetThreadAttr(const SceFiosOpAttr* attr) {
    SceFiosOpAttr copy{};
    if (attr != nullptr && !CopyIn(&copy, attr, sizeof(copy))) return false;
    if (attr == nullptr) g_threadAttr.reset();
    else g_threadAttr = copy;
    return true;
}

bool ThreadAttr(SceFiosOpAttr& attr) {
    if (!g_threadAttr) return false;
    attr = *g_threadAttr;
    return true;
}

std::int64_t ReadFully(int descriptor, void* buffer, std::uint64_t length, std::int64_t offset, bool positioned) {
    std::uint64_t total = 0;
    while (total < length) {
        const std::uint64_t chunk = std::min(length - total, IoChunk);
        auto* destination = static_cast<char*>(buffer) + total;
        const std::int64_t count = positioned ? sceKernelPread(descriptor, destination, chunk, offset + static_cast<std::int64_t>(total))
                                              : sceKernelRead(descriptor, destination, chunk);
        if (count < 0) return total == 0 ? FromKernelError(count) : static_cast<std::int64_t>(total);
        if (count == 0) break;
        total += static_cast<std::uint64_t>(count);
    }
    return static_cast<std::int64_t>(total);
}

std::int64_t WriteFully(int descriptor, const void* buffer, std::uint64_t length, std::int64_t offset, bool positioned) {
    std::uint64_t total = 0;
    while (total < length) {
        const std::uint64_t chunk = std::min(length - total, IoChunk);
        const auto* source = static_cast<const char*>(buffer) + total;
        const std::int64_t count = positioned ? sceKernelPwrite(descriptor, source, chunk, offset + static_cast<std::int64_t>(total))
                                              : sceKernelWrite(descriptor, source, chunk);
        if (count < 0) return total == 0 ? FromKernelError(count) : static_cast<std::int64_t>(total);
        if (count == 0) break;
        total += static_cast<std::uint64_t>(count);
    }
    return static_cast<std::int64_t>(total);
}

namespace {

constexpr std::uint16_t FileTypeMask = 0170000;
constexpr std::uint16_t DirectoryType = 0040000;

void FillStat(const FileStat& native, SceFiosStat& stat) {
    stat = SceFiosStat{};
    stat.fileSize = native.st_size;
    stat.accessDate = TimespecToDate(native.st_atim.tv_sec, native.st_atim.tv_nsec);
    stat.modificationDate = TimespecToDate(native.st_mtim.tv_sec, native.st_mtim.tv_nsec);
    stat.creationDate = TimespecToDate(native.st_birthtim.tv_sec, native.st_birthtim.tv_nsec);
    stat.statFlags = (native.st_mode & FileTypeMask) == DirectoryType ? SCE_FIOS_STAT_DIRECTORY : 0;
    stat.uid = native.st_uid;
    stat.gid = native.st_gid;
    stat.dev = native.st_dev;
    stat.ino = native.st_ino;
    stat.mode = native.st_mode;
}

}

std::int32_t StatPath(const std::string& path, SceFiosStat& stat) {
    FileStat native{};
    if (const int result = sceKernelStat(path.c_str(), &native); result < 0) return FromKernelError(result);
    FillStat(native, stat);
    return SCE_FIOS_OK;
}

std::int32_t StatDescriptor(int descriptor, SceFiosStat& stat) {
    FileStat native{};
    if (const int result = sceKernelFstat(descriptor, &native); result < 0) return FromKernelError(result);
    FillStat(native, stat);
    return SCE_FIOS_OK;
}

}
