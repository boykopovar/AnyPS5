#include "Runtime.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <map>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

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
    bool syncRequested = false;
    bool executing = false;
    bool ioDone = false;
    bool cancelled = false;
    bool callbackExecuting = false;
    bool callbackFinished = false;
    bool deleteDeferred = false;
    bool deleteFired = false;
    std::thread::id callbackThread{};
    std::thread::id inlineOwner{};
    std::int32_t error = SCE_FIOS_OK;
    std::int64_t result = 0;
    ExecutionPolicy policy = Async;
    std::exception_ptr exception;
};

struct State {
    std::mutex mutex;
    std::condition_variable finished;
    std::condition_variable workAvailable;
    std::map<SceFiosHandle, std::shared_ptr<Op>> ops;
    std::map<SceFiosHandle, std::shared_ptr<Op>> inlineCompletions;
    std::map<SceFiosHandle, std::shared_ptr<FileHandle>> files;
    std::map<SceFiosHandle, std::shared_ptr<DirectoryHandle>> directories;
    std::deque<SceFiosOp> pending;
    std::map<std::thread::id, std::vector<SceFiosOp>> callbackStackByThread;
    SceFiosHandle lastHandle = 0;
    bool initialized = false;
    bool terminating = false;
    bool workerRunning = false;
    Pthread worker = nullptr;
    std::exception_ptr retainedException;
    std::vector<std::uint8_t> parameters;
    std::uint32_t suspendCount = 0;
    std::optional<SceFiosOpAttr> globalAttr;
};

State& Global() {
    static State state;
    return state;
}

thread_local std::optional<SceFiosOpAttr> g_threadAttr;

struct ThreadAttributes {
    ThreadAttributes() {
        if (scePthreadAttrInit(&value) != 0) throw std::runtime_error("libSceFios2: scePthreadAttrInit failed");
    }
    ~ThreadAttributes() { static_cast<void>(scePthreadAttrDestroy(&value)); }
    ThreadAttributes(const ThreadAttributes&) = delete;
    ThreadAttributes& operator=(const ThreadAttributes&) = delete;
    PthreadAttr value = nullptr;
};

void ProcessShutdown();
void* APS5_VABI FiosWorkerEntry_nid_no_patch(void*);

void StartWorkerLocked(State& state) {
    if (state.workerRunning) return;
    ThreadAttributes attributes;
    const KernelSchedParam schedule{260};
    if (scePthreadAttrSetinheritsched(&attributes.value, 0) != 0 ||
        scePthreadAttrSetschedparam(&attributes.value, &schedule) != 0 ||
        scePthreadAttrSetstacksize(&attributes.value, 0x14000) != 0) {
        throw std::runtime_error("libSceFios2: worker thread attribute rejected");
    }
    static const bool registered = (LibcRegisterShutdown_nid_postfix(ProcessShutdown), true);
    static_cast<void>(registered);
    state.workerRunning = true;
    const int created = scePthreadCreate(&state.worker, &attributes.value, FiosWorkerEntry_nid_no_patch, nullptr, "SceFiosWorker");
    if (created != 0) {
        state.workerRunning = false;
        state.worker = nullptr;
        throw std::runtime_error("libSceFios2: scePthreadCreate failed");
    }
}

bool HandleInUse(const State& state, SceFiosHandle handle) {
    return state.ops.count(handle) != 0 || state.files.count(handle) != 0 || state.directories.count(handle) != 0;
}

bool HandleAllocated(const State& state, SceFiosHandle handle) {
    return HandleInUse(state, handle) || state.inlineCompletions.count(handle) != 0;
}

SceFiosHandle NextHandle(State& state) {
    for (int attempts = 0; attempts < 0x7fffffff; ++attempts) {
        state.lastHandle = state.lastHandle >= 0x7ffffffe ? 1 : state.lastHandle + 1;
        if (!HandleAllocated(state, state.lastHandle)) return state.lastHandle;
    }
    return 0;
}

std::shared_ptr<Op> FindOp(State& state, SceFiosOp id) {
    const auto found = state.ops.find(id);
    return found == state.ops.end() ? nullptr : found->second;
}

std::deque<SceFiosOp>::iterator ReadyPending(State& state) {
    if (state.terminating || state.suspendCount == 0) return state.pending.begin();
    return std::find_if(state.pending.begin(), state.pending.end(), [&](SceFiosOp id) {
        const auto op = FindOp(state, id);
        return !op || op->syncRequested;
    });
}

bool IsInAnyCallback(const State& state, std::thread::id threadId) {
    const auto it = state.callbackStackByThread.find(threadId);
    return it != state.callbackStackByThread.end() && !it->second.empty();
}

bool IsSelfOrAncestorCallback(const State& state, std::thread::id threadId, SceFiosOp opId) {
    const auto it = state.callbackStackByThread.find(threadId);
    if (it == state.callbackStackByThread.end()) return false;
    const auto& stack = it->second;
    return std::find(stack.begin(), stack.end(), opId) != stack.end();
}

void CompleteOpLocked(std::unique_lock<std::mutex>& lock, const std::shared_ptr<Op>& op, const Completion& completion, std::exception_ptr caughtException) {
    op->error = completion.error;
    op->result = completion.result;
    if (caughtException && !op->exception) op->exception = caughtException;
    op->ioDone = true;
    op->executing = false;

    SceFiosOpCallback callback = op->attr.pCallback;
    void* context = op->attr.pCallbackContext;
    const SceFiosOp opId = op->id;
    const std::int32_t opError = completion.error;
    const std::thread::id threadId = std::this_thread::get_id();

    if (callback != nullptr) {
        op->callbackExecuting = true;
        op->callbackThread = threadId;
        Global().callbackStackByThread[threadId].push_back(opId);

        lock.unlock();
        try {
            callback(context, opId, SCE_FIOS_OPEVENT_COMPLETE, opError);
        } catch (...) {
            if (!caughtException) caughtException = std::current_exception();
        }
        lock.lock();

        if (caughtException && !op->exception) op->exception = caughtException;

        if (op->deleteDeferred && !op->deleteFired) {
            op->deleteFired = true;

            lock.unlock();
            try {
                callback(context, opId, SCE_FIOS_OPEVENT_DELETE, SCE_FIOS_OK);
            } catch (...) {
                if (!caughtException) caughtException = std::current_exception();
            }
            lock.lock();
            if (caughtException && !op->exception) op->exception = caughtException;
            Global().ops.erase(opId);
        }

        auto& stack = Global().callbackStackByThread[threadId];
        stack.erase(std::remove(stack.begin(), stack.end(), opId), stack.end());
        if (stack.empty()) {
            Global().callbackStackByThread.erase(threadId);
        }
        op->callbackExecuting = false;
        op->callbackThread = std::thread::id();
    } else {
        if (op->deleteDeferred && !op->deleteFired) {
            op->deleteFired = true;
            Global().ops.erase(opId);
        }
    }

    op->callbackFinished = true;
    op->done = true;
    if (op->exception && !Global().retainedException) Global().retainedException = op->exception;
    Global().finished.notify_all();
}

void ExecuteOpLocked(std::unique_lock<std::mutex>& lock, const std::shared_ptr<Op>& op) {
    Work work = std::move(op->work);
    const bool cancelled = op->cancelled;
    op->work = nullptr;

    lock.unlock();

    Completion completion;
    std::exception_ptr caughtException = nullptr;
    if (cancelled || !work) {
        completion.error = SCE_FIOS_ERROR_CANCELLED;
        completion.result = 0;
    } else {
        try {
            work(completion);
        } catch (...) {
            caughtException = std::current_exception();
            completion.error = SCE_FIOS_ERROR_UNKNOWN;
            completion.result = 0;
        }
    }

    lock.lock();
    CompleteOpLocked(lock, op, completion, caughtException);
}

bool CancelQueuedLocked(std::unique_lock<std::mutex>& lock, const std::shared_ptr<Op>& op) {
    if (op->done || !op->queued || op->executing) return false;
    auto& pending = Global().pending;
    pending.erase(std::remove(pending.begin(), pending.end(), op->id), pending.end());
    op->queued = false;
    op->cancelled = true;
    Completion completion{SCE_FIOS_ERROR_CANCELLED, 0};
    CompleteOpLocked(lock, op, completion, nullptr);
    return true;
}

void* APS5_VABI FiosWorkerEntry_nid_no_patch(void*) {
    std::unique_lock lock(Global().mutex);
    while (Global().workerRunning) {
        if (!Global().terminating && ReadyPending(Global()) == Global().pending.end()) {
            Global().workAvailable.wait(lock, [&] {
                return !Global().workerRunning || Global().terminating || ReadyPending(Global()) != Global().pending.end();
            });
            if (!Global().workerRunning && (!Global().terminating || Global().pending.empty())) break;
        }
        if (Global().pending.empty()) {
            if (Global().terminating || !Global().workerRunning) break;
            continue;
        }

        const auto ready = ReadyPending(Global());
        if (ready == Global().pending.end()) continue;
        const SceFiosOp id = *ready;
        Global().pending.erase(ready);
        const auto op = FindOp(Global(), id);
        if (!op) continue;
        if (op->done) {
            op->queued = false;
            continue;
        }
        op->queued = false;
        op->executing = true;

        ExecuteOpLocked(lock, op);
    }
    Global().workerRunning = false;
    Global().finished.notify_all();
    return nullptr;
}

} // namespace

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

SceFiosOp Submit(const SceFiosOpAttr* attr, OpInfo info, Work work, ExecutionPolicy policy) {
    auto op = std::make_shared<Op>();
    bool runNow = false;
    SceFiosOpAttr given{};
    if (attr != nullptr && !CopyIn(&given, attr, sizeof(given))) return SCE_FIOS_ERROR_BAD_PTR;
    {
        std::unique_lock lock(Global().mutex);
        State& state = Global();
        if (state.terminating) {
            NotImplemented_nid_no_patch(__func__);
        }
        const SceFiosHandle id = NextHandle(state);
        if (id == 0) return SCE_FIOS_ERROR_CANT_ALLOCATE_OP;
        if (attr != nullptr) op->attr = given;
        else if (g_threadAttr) op->attr = *g_threadAttr;
        else if (state.globalAttr) op->attr = *state.globalAttr;
        op->id = id;
        op->policy = policy;
        op->path = std::move(info.path);
        op->offset = info.offset;
        op->request = info.request;
        op->buffer = info.buffer;
        op->work = std::move(work);
        state.ops[id] = op;
        if (policy == Inline) {
            op->inlineOwner = std::this_thread::get_id();
            try {
                state.inlineCompletions[id] = op;
            } catch (...) {
                state.ops.erase(id);
                throw;
            }
            op->executing = true;
            runNow = true;
        } else {
            try {
                StartWorkerLocked(state);
            } catch (...) {
                state.ops.erase(id);
                throw;
            }
            op->queued = true;
            state.pending.push_back(id);
            if (state.suspendCount == 0) {
                state.workAvailable.notify_one();
            }
        }
    }
    if (runNow) {
        try {
            std::unique_lock lock(Global().mutex);
            ExecuteOpLocked(lock, op);
        } catch (...) {
            std::lock_guard lock(Global().mutex);
            Global().inlineCompletions.erase(op->id);
            Global().ops.erase(op->id);
            Global().finished.notify_all();
            throw;
        }
    }
    return op->id;
}

SceFiosOp SubmitFailed(const SceFiosOpAttr* attr, OpInfo info, std::int32_t error, ExecutionPolicy policy) {
    return Submit(attr, std::move(info), [error](Completion& completion) { completion.error = error; }, policy);
}

std::int32_t Wait(SceFiosOp op) {
    std::unique_lock lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    if (!found) return SCE_FIOS_ERROR_BAD_OP;

    const std::thread::id threadId = std::this_thread::get_id();
    if (IsInAnyCallback(Global(), threadId)) {
        if (IsSelfOrAncestorCallback(Global(), threadId, op)) {
            if (found->exception) std::rethrow_exception(found->exception);
            return found->error;
        }
        if (!found->done && !found->ioDone) {
            NotImplemented_nid_no_patch(__func__);
        }
    }

    Global().finished.wait(lock, [&] { return found->callbackFinished && !found->callbackExecuting; });
    if (found->exception) std::rethrow_exception(found->exception);
    return found->error;
}

std::int32_t WaitUntil(SceFiosOp op, SceFiosTime deadline) {
    std::unique_lock lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    if (!found) return SCE_FIOS_ERROR_BAD_OP;

    const std::thread::id threadId = std::this_thread::get_id();
    if (IsInAnyCallback(Global(), threadId)) {
        if (IsSelfOrAncestorCallback(Global(), threadId, op)) {
            if (found->exception) std::rethrow_exception(found->exception);
            return found->error;
        }
        if (!found->done && !found->ioDone) {
            NotImplemented_nid_no_patch(__func__);
        }
    }

    if (!Global().finished.wait_until(lock, std::chrono::steady_clock::time_point(std::chrono::nanoseconds(deadline)),
                                      [&] { return found->callbackFinished && !found->callbackExecuting; })) {
        return SCE_FIOS_ERROR_TIMEOUT;
    }
    if (found->exception) std::rethrow_exception(found->exception);
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
    std::unique_lock lock(Global().mutex);
    auto found = FindOp(Global(), op);
    const std::thread::id threadId = std::this_thread::get_id();
    if (!found) {
        const auto it = Global().inlineCompletions.find(op);
        if (it != Global().inlineCompletions.end() && it->second->inlineOwner == threadId) {
            found = it->second;
        }
    }
    if (!found) return false;

    if (IsInAnyCallback(Global(), threadId)) {
        if (IsSelfOrAncestorCallback(Global(), threadId, op)) {
            completion.error = found->error;
            completion.result = found->result;
            const auto ex = found->exception;
            lock.unlock();
            if (ex) std::rethrow_exception(ex);
            DeleteOp(op);
            return true;
        }
        if (!found->done && !found->ioDone) {
            NotImplemented_nid_no_patch(__func__);
        }
    }

    if (found->queued) {
        found->syncRequested = true;
        Global().workAvailable.notify_one();
    }

    Global().finished.wait(lock, [&] { return found->callbackFinished && !found->callbackExecuting; });

    completion.error = found->error;
    completion.result = found->result;
    const auto exception = found->exception;
    const bool isOwnerConsumption = (found->inlineOwner == threadId);
    lock.unlock();

    auto releaseLease = [&]() {
        if (isOwnerConsumption) {
            const std::lock_guard leaseLock(Global().mutex);
            Global().inlineCompletions.erase(op);
            Global().finished.notify_all();
        }
    };

    struct LeaseGuard {
        decltype(releaseLease)& cleanup;
        ~LeaseGuard() { cleanup(); }
    } guard{releaseLease};

    DeleteOp(op);
    if (exception) std::rethrow_exception(exception);
    return true;
}

bool OpDone(SceFiosOp op) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    if (!found) return false;
    const std::thread::id threadId = std::this_thread::get_id();
    if (IsSelfOrAncestorCallback(Global(), threadId, op)) {
        return found->done || found->ioDone;
    }
    return found->done;
}

bool DeleteOp(SceFiosOp op) {
    std::shared_ptr<Op> found;
    SceFiosOpCallback callback = nullptr;
    void* context = nullptr;
    {
        std::unique_lock lock(Global().mutex);
        found = FindOp(Global(), op);
        if (!found) return false;

        const std::thread::id threadId = std::this_thread::get_id();
        if (IsInAnyCallback(Global(), threadId)) {
            if (IsSelfOrAncestorCallback(Global(), threadId, op)) {
                found->deleteDeferred = true;
                return true;
            }
            if (!found->done && !found->ioDone) {
                NotImplemented_nid_no_patch(__func__);
            }
        }

        if (found->deleteFired) {
            Global().finished.wait(lock, [&] { return found->callbackFinished && !found->callbackExecuting; });
            if (found->exception) std::rethrow_exception(found->exception);
            return false;
        }

        CancelQueuedLocked(lock, found);

        Global().finished.wait(lock, [&] {
            return found->callbackFinished && !found->callbackExecuting;
        });

        if (found->deleteFired) {
            if (found->exception) std::rethrow_exception(found->exception);
            return false;
        }
        if (Global().ops.count(op) == 0) {
            if (found->exception) std::rethrow_exception(found->exception);
            return false;
        }
        found->deleteFired = true;
        callback = found->attr.pCallback;
        context = found->attr.pCallbackContext;

        if (callback != nullptr) {
            found->callbackFinished = false;
            found->callbackExecuting = true;
            found->callbackThread = threadId;
            Global().callbackStackByThread[threadId].push_back(op);

            lock.unlock();
            std::exception_ptr callbackException;
            try {
                callback(context, op, SCE_FIOS_OPEVENT_DELETE, SCE_FIOS_OK);
            } catch (...) {
                callbackException = std::current_exception();
            }
            lock.lock();
            if (callbackException && !found->exception) found->exception = callbackException;

            auto& stack = Global().callbackStackByThread[threadId];
            stack.erase(std::remove(stack.begin(), stack.end(), op), stack.end());
            if (stack.empty()) Global().callbackStackByThread.erase(threadId);

            found->callbackExecuting = false;
            found->callbackFinished = true;
            found->callbackThread = std::thread::id();
            Global().finished.notify_all();
        }
        if (found->exception && !Global().retainedException) Global().retainedException = found->exception;
        Global().ops.erase(op);
    }

    if (found->exception) std::rethrow_exception(found->exception);
    return true;
}

bool CancelOp(SceFiosOp op) {
    std::unique_lock lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    return found && CancelQueuedLocked(lock, found);
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
    actual = (found->done || found->ioDone) ? found->result : 0;
    offset = found->offset;
    return true;
}

bool OpError(SceFiosOp op, std::int32_t& error) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    if (!found) return false;
    error = (found->done || found->ioDone) ? found->error : 1;
    return true;
}

std::int32_t SetOpBuffer(SceFiosOp op, void* buffer) {
    const std::lock_guard lock(Global().mutex);
    const auto found = FindOp(Global(), op);
    if (!found) return SCE_FIOS_ERROR_BAD_OP;
    if (found->queued || found->executing) return SCE_FIOS_ERROR_BUSY;
    found->buffer = buffer;
    return SCE_FIOS_OK;
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
    std::unique_lock lock(Global().mutex);
    std::vector<std::shared_ptr<Op>> queued;
    for (const SceFiosOp id : Global().pending) {
        if (const auto found = FindOp(Global(), id)) queued.push_back(found);
    }
    for (const auto& op : queued) CancelQueuedLocked(lock, op);
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
    if (Global().terminating) NotImplemented_nid_no_patch(__func__);
    if (!StoreParameters(Global(), params)) return false;
    Global().initialized = true;
    return true;
}

bool UpdateParameters(const void* params) {
    const std::lock_guard lock(Global().mutex);
    if (Global().terminating) NotImplemented_nid_no_patch(__func__);
    if (!Global().initialized) return false;
    return StoreParameters(Global(), params);
}

void Terminate() {
    Pthread workerToJoin = nullptr;
    std::exception_ptr retainedException = nullptr;
    {
        std::unique_lock lock(Global().mutex);
        State& state = Global();
        const std::thread::id threadId = std::this_thread::get_id();
        if (IsInAnyCallback(state, threadId) || (state.worker != nullptr && scePthreadSelf() == state.worker)) {
            NotImplemented_nid_no_patch(__func__);
        }
        if (state.terminating) {
            state.finished.wait(lock, [&] { return !state.terminating; });
            return;
        }
        state.terminating = true;

        for (const SceFiosOp id : state.pending) {
            if (const auto op = FindOp(state, id)) {
                op->cancelled = true;
            }
        }
        state.workAvailable.notify_all();

        if (state.workerRunning) {
            state.finished.wait(lock, [&] {
                return !state.workerRunning;
            });
            workerToJoin = state.worker;
            state.worker = nullptr;
        }

        state.finished.wait(lock, [&] {
            if (!state.callbackStackByThread.empty()) return false;
            for (const auto& entry : state.ops) {
                const auto& op = entry.second;
                if (op->executing || op->callbackExecuting) return false;
            }
            return state.inlineCompletions.empty();
        });

        retainedException = state.retainedException;
        for (const auto& entry : state.ops) {
            if (entry.second->exception && !retainedException) {
                retainedException = entry.second->exception;
            }
        }
    }

    if (workerToJoin != nullptr) {
        if (scePthreadJoin(workerToJoin, nullptr) != 0) {
            throw std::runtime_error("libSceFios2: scePthreadJoin failed");
        }
    }

    CloseAllFiles();

    {
        const std::lock_guard lock(Global().mutex);
        State& state = Global();
        state.directories.clear();
        state.ops.clear();
        state.inlineCompletions.clear();
        state.pending.clear();
        state.parameters.clear();
        state.suspendCount = 0;
        state.initialized = false;
        state.retainedException = nullptr;
        state.terminating = false;
        state.finished.notify_all();
    }

    if (retainedException) std::rethrow_exception(retainedException);
}

namespace {

void ProcessShutdown() {
    Terminate();
}

} // namespace

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
    const std::lock_guard lock(Global().mutex);
    State& state = Global();
    if (state.suspendCount == 0) return;
    if (--state.suspendCount == 0) {
        state.workAvailable.notify_one();
    }
}

std::uint32_t SuspendCount() {
    const std::lock_guard lock(Global().mutex);
    return Global().suspendCount;
}

bool IsIdle() {
    const std::lock_guard lock(Global().mutex);
    if (!Global().pending.empty()) return false;
    for (const auto& entry : Global().ops) {
        const auto& op = entry.second;
        if (op->queued || op->executing || op->callbackExecuting || !op->callbackFinished) {
            return false;
        }
    }
    return true;
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

} // namespace

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

} // namespace Fios
