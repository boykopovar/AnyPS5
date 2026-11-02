#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int GuestEagain = 35;
constexpr int GuestEinprogress = 36;
constexpr int GuestEopnotsupp = 45;
constexpr int GuestEcanceled = 85;
constexpr int LioWrite = 1;
constexpr int LioRead = 2;
constexpr int LioSync = 3;
constexpr int AioCanceled = 1;
constexpr int AioNotCanceled = 2;
constexpr int AioAllDone = 3;
constexpr int SigevNone = 0;
constexpr int SigevSignal = 1;
constexpr int SigevKevent = 3;
constexpr int SigevThreadId = 4;
constexpr int SignalMax = 128;
constexpr int GuestOSync = 0x80;
constexpr int GetFlags = 3;
constexpr int AccessMode = 3;
constexpr int WriteOnly = 1;
constexpr int ReadWrite = 2;
constexpr std::uint16_t TypeMask = 0170000;
constexpr std::uint16_t TypeFifo = 0010000;
constexpr std::uint16_t TypeDirectory = 0040000;
constexpr std::uint16_t TypeRegular = 0100000;
constexpr std::uint16_t TypeSocket = 0140000;
constexpr std::size_t QueuePerProcess = 256;
constexpr std::uint64_t IoSizeMax = std::numeric_limits<int>::max();
constexpr std::int64_t NanosecondsPerSecond = 1000000000;
constexpr std::int64_t TickNanoseconds = 1000000;

static_assert(offsetof(GuestAiocb, lioOpcode) == 48 && offsetof(GuestAiocb, status) == 56 && offsetof(GuestAiocb, sigevent) == 80);
static_assert(sizeof(GuestAiocb) == 160);

enum class State { Queued, Running, Done };

struct Job {
    GuestAiocb* user;
    int descriptor;
    int opcode;
    std::int64_t offset;
    void* buffer;
    std::uint64_t bytes;
    State state = State::Queued;
    std::int64_t status = -1;
    int error = 0;
};

struct Aio {
    std::mutex lock;
    std::condition_variable changed;
    bool used = false;
    bool held = false;
    bool started = false;
    std::intptr_t nextId = 0;
    std::deque<std::shared_ptr<Job>> queued;
    std::list<std::shared_ptr<Job>> all;
    std::list<std::shared_ptr<Job>> done;
};

Aio& Instance() {
    static auto* aio = new Aio;
    return *aio;
}

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

void Run(Job& job) {
    std::int64_t result = 0;
    if (job.opcode == LioRead) result = job.bytes == 0 ? 0 : pread_nid_postfix(job.descriptor, job.buffer, job.bytes, job.offset);
    else if (job.opcode == LioWrite) result = pwrite_nid_disambig1_nid_postfix(job.descriptor, job.buffer, job.bytes, job.offset);
    else result = fsync_nid_postfix(job.descriptor);
    job.status = result < 0 ? -1 : result;
    job.error = result < 0 ? *__error_nid_postfix() : 0;
}

void Worker() {
    auto& aio = Instance();
    std::unique_lock lock(aio.lock);
    for (;;) {
        aio.changed.wait(lock, [&] { return !aio.held && !aio.queued.empty(); });
        auto job = aio.queued.front();
        aio.queued.pop_front();
        job->state = State::Running;
        lock.unlock();
        Run(*job);
        lock.lock();
        job->state = State::Done;
        aio.done.push_back(job);
        aio.changed.notify_all();
    }
}

void Complete(Aio& aio, const std::shared_ptr<Job>& job, std::int64_t status, int error) {
    job->status = status;
    job->error = error;
    job->state = State::Done;
    aio.done.push_back(job);
    aio.changed.notify_all();
}

std::optional<int> ReadyError(const std::shared_ptr<Job>& job) {
    if (job->state != State::Done) return std::nullopt;
    return job->error;
}

std::int64_t Consume(Aio& aio, std::list<std::shared_ptr<Job>>::iterator found) {
    const auto job = *found;
    aio.done.erase(found);
    aio.all.remove(job);
    job->user->error = job->error;
    job->user->status = job->status;
    return job->error != 0 ? Fail(job->error) : job->status;
}

std::optional<std::chrono::nanoseconds> WaitTime(const KernelTimespec& time) {
    if (time.tv_sec < 0 || time.tv_nsec < 0 || time.tv_nsec >= NanosecondsPerSecond) return std::nullopt;
    const auto microseconds = time.tv_nsec / 1000;
    const std::int64_t whole = microseconds * 1000;
    const std::int64_t rounded = time.tv_sec == 0 && whole < TickNanoseconds ? TickNanoseconds : whole;
    const auto seconds = std::min<std::int64_t>(time.tv_sec, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::hours(24 * 365)).count());
    return std::chrono::seconds(seconds) + std::chrono::nanoseconds(rounded);
}

int Queue(GuestAiocb* user, int opcode) {
    if (user == nullptr) return Fail(GuestEfault);
    auto& aio = Instance();
    std::unique_lock lock(aio.lock);
    aio.used = true;
    user->status = -1;
    user->error = 0;
    user->kernelinfo = reinterpret_cast<void*>(std::intptr_t{-1});
    const auto fail = [&](int error) {
        user->error = error;
        return Fail(error);
    };
    if (aio.all.size() >= QueuePerProcess) return fail(GuestEagain);
    const GuestAiocb request = *user;
    if (request.nbytes > IoSizeMax) return Fail(GuestEinval);
    const int notify = request.sigevent.notify;
    if (notify != SigevKevent && notify != SigevSignal && notify != SigevThreadId && notify != SigevNone) return fail(GuestEinval);
    if ((notify == SigevSignal || notify == SigevThreadId) && (request.sigevent.signo <= 0 || request.sigevent.signo > SignalMax))
        return Fail(GuestEinval);
    const int flags = fcntl_nid_postfix(request.fildes, GetFlags);
    if (flags < 0) return fail(GuestEbadf);
    const int access = flags & AccessMode;
    if (opcode == LioRead && access != 0 && access != ReadWrite) return fail(GuestEbadf);
    if (opcode == LioWrite && access != WriteOnly && access != ReadWrite) return fail(GuestEbadf);
    FileStat status{};
    if (_fstat_nid_postfix(request.fildes, &status) != 0) return fail(GuestEbadf);
    const auto type = static_cast<std::uint16_t>(status.st_mode & TypeMask);
    if (type == TypeSocket) NotImplemented_nid_no_patch("aio on a socket");
    const bool vnode = type != 0 && type != TypeFifo;
    if (opcode == LioSync && !vnode) return fail(GuestEinval);
    if (opcode != LioSync && request.offset < 0) return fail(GuestEinval);
    user->kernelinfo = reinterpret_cast<void*>(aio.nextId++);
    if (notify != SigevNone) NotImplemented_nid_no_patch("aio completion delivering a signal or a kevent");
    user->error = GuestEinprogress;
    if (type != TypeRegular && type != TypeDirectory) return fail(GuestEopnotsupp);
    auto job = std::make_shared<Job>(Job{user, request.fildes, opcode, request.offset, request.buf, request.nbytes});
    aio.all.push_back(job);
    aio.queued.push_back(job);
    if (!aio.started) {
        aio.started = true;
        std::thread(Worker).detach();
    }
    aio.changed.notify_all();
    return 0;
}

}

extern "C" {

void AioHoldWorkerForTest_nid_no_patch(bool hold) {
    auto& aio = Instance();
    std::lock_guard lock(aio.lock);
    aio.held = hold;
    aio.changed.notify_all();
}

int APS5_VABI aio_read_nid_postfix(GuestAiocb* aiocb) {
    return Queue(aiocb, LioRead);
}

int APS5_VABI aio_write_nid_postfix(GuestAiocb* aiocb) {
    return Queue(aiocb, LioWrite);
}

int APS5_VABI aio_fsync_nid_postfix(int operation, GuestAiocb* aiocb) {
    if (operation != GuestOSync) return Fail(GuestEinval);
    return Queue(aiocb, LioSync);
}

int APS5_VABI aio_error_nid_postfix(const GuestAiocb* aiocb) {
    auto& aio = Instance();
    std::lock_guard lock(aio.lock);
    if (!aio.used || aiocb == nullptr) return GuestEinval;
    for (const auto& job : aio.all) {
        if (job->user == aiocb) return ReadyError(job).value_or(GuestEinprogress);
    }
    if (aiocb->status == -1) return static_cast<int>(aiocb->error);
    return GuestEinval;
}

std::int64_t APS5_VABI aio_return_nid_postfix(GuestAiocb* aiocb) {
    auto& aio = Instance();
    std::lock_guard lock(aio.lock);
    if (!aio.used) return Fail(GuestEinval);
    for (auto job = aio.done.begin(); job != aio.done.end(); ++job) {
        if ((*job)->user == aiocb) return Consume(aio, job);
    }
    return Fail(GuestEinval);
}

int APS5_VABI aio_suspend_nid_postfix(const GuestAiocb* const* list, int count, const KernelTimespec* timeout) {
    if (count < 0 || count > static_cast<int>(QueuePerProcess)) return Fail(GuestEinval);
    std::optional<std::chrono::nanoseconds> wait;
    if (timeout != nullptr) {
        wait = WaitTime(*timeout);
        if (!wait) return Fail(GuestEinval);
    }
    auto& aio = Instance();
    std::unique_lock lock(aio.lock);
    if (!aio.used) return Fail(GuestEagain);
    if (count == 0) return 0;
    for (;;) {
        bool listed = false;
        for (const auto& job : aio.all) {
            for (int index = 0; index < count; ++index) {
                if (job->user != list[index]) continue;
                listed = true;
                if (job->state == State::Done) return 0;
            }
        }
        if (!listed) return 0;
        if (!wait) aio.changed.wait(lock);
        else if (aio.changed.wait_for(lock, *wait) == std::cv_status::timeout) return Fail(GuestEagain);
    }
}

std::int64_t APS5_VABI aio_waitcomplete_nid_postfix(GuestAiocb** result, const KernelTimespec* timeout) {
    if (result != nullptr) *result = nullptr;
    std::optional<std::chrono::nanoseconds> wait;
    bool poll = false;
    if (timeout != nullptr) {
        poll = timeout->tv_sec == 0 && timeout->tv_nsec == 0;
        if (!poll) {
            wait = WaitTime(*timeout);
            if (!wait) return Fail(GuestEinval);
        }
    }
    auto& aio = Instance();
    std::unique_lock lock(aio.lock);
    aio.used = true;
    while (aio.done.empty()) {
        if (poll) return Fail(GuestEagain);
        if (!wait) aio.changed.wait(lock);
        else if (aio.changed.wait_for(lock, *wait) == std::cv_status::timeout && aio.done.empty()) return Fail(GuestEagain);
    }
    if (result != nullptr) *result = aio.done.front()->user;
    return Consume(aio, aio.done.begin());
}

int APS5_VABI aio_cancel_nid_postfix(int descriptor, GuestAiocb* aiocb) {
    if (fcntl_nid_postfix(descriptor, GetFlags) < 0) return Fail(GuestEbadf);
    auto& aio = Instance();
    std::lock_guard lock(aio.lock);
    int cancelled = 0;
    int notCancelled = 0;
    for (const auto& job : std::list<std::shared_ptr<Job>>(aio.all)) {
        if (job->state == State::Done || job->descriptor != descriptor || (aiocb != nullptr && job->user != aiocb)) continue;
        if (job->state == State::Queued) {
            std::erase(aio.queued, job);
            Complete(aio, job, -1, GuestEcanceled);
            ++cancelled;
        } else {
            ++notCancelled;
        }
        if (aiocb != nullptr) break;
    }
    if (aiocb != nullptr && cancelled != 0) return AioCanceled;
    if (notCancelled != 0) return AioNotCanceled;
    if (cancelled != 0) return AioCanceled;
    return AioAllDone;
}

}
