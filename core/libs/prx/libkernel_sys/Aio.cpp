#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int GuestEintr = 4;
constexpr int GuestEbadf = 9;
constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int GuestEagain = 35;
constexpr int GuestEinprogress = 36;
constexpr int GuestEpipe = 32;
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
constexpr int Sigpipe = 13;
constexpr std::int32_t SiAsyncio = 0x10004;
constexpr std::int32_t SiKernel = 0x10006;
constexpr std::uint16_t KeventFlagsAllowed = 0x10 | 0x20 | 0x80;
constexpr int Nonblocking = 0x4;
constexpr int MessageDontWait = 0x80;
constexpr int MessageNoSignal = 0x20000;
constexpr int SocketLevel = 0xffff;
constexpr int NoSigpipe = 0x800;
constexpr short PollIn = 0x1;
constexpr short PollOut = 0x4;
constexpr int SocketPollMilliseconds = 10;
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
    std::uint64_t token = 0;
    int notify = SigevNone;
    int signo = 0;
    std::int64_t value = 0;
    std::int64_t thread = 0;
    std::int64_t submitter = 0;
    std::uint64_t signalTicket = 0;
    bool socket = false;
    bool cancelled = false;
    std::uint64_t done = 0;
    State state = State::Queued;
    std::int64_t status = -1;
    int error = 0;
};

struct SocketQueue {
    int descriptor;
    bool write;
    std::deque<std::shared_ptr<Job>> jobs;
};

struct Aio {
    std::mutex lock;
    TimedWait::Condition changed;
    bool used = false;
    bool held = false;
    bool socketHeld = false;
    std::size_t socketWaiting = 0;
    bool started = false;
    std::intptr_t nextId = 0;
    std::deque<std::shared_ptr<Job>> queued;
    std::list<std::shared_ptr<Job>> all;
    std::list<std::shared_ptr<Job>> done;
    std::list<SocketQueue> sockets;
};

Aio& Instance() {
    static auto* aio = new Aio;
    return *aio;
}

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

template <typename Field>
std::uintptr_t Address(const GuestAiocb* user, Field GuestAiocb::*member) {
    static const GuestAiocb layout{};
    const auto offset = reinterpret_cast<std::uintptr_t>(&(layout.*member)) - reinterpret_cast<std::uintptr_t>(&layout);
    return reinterpret_cast<std::uintptr_t>(user) + offset;
}

template <typename Field>
bool Store(GuestAiocb* user, Field GuestAiocb::*member, Field value) {
    return GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(Address(user, member)), &value, sizeof(value));
}

std::int64_t Fetch(const GuestAiocb* user, std::int64_t GuestAiocb::*member) {
    std::int64_t value = 0;
    return GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(Address(user, member)), sizeof(value)) ? value : -1;
}

void Run(Job& job) {
    std::int64_t result = 0;
    if (job.opcode == LioRead) result = job.bytes == 0 ? 0 : pread_nid_postfix(job.descriptor, job.buffer, job.bytes, job.offset);
    else if (job.opcode == LioWrite) result = pwrite_nid_disambig1_nid_postfix(job.descriptor, job.buffer, job.bytes, job.offset);
    else result = fsync_nid_postfix(job.descriptor);
    job.status = result < 0 ? -1 : result;
    job.error = result < 0 ? *__error_nid_postfix() : 0;
}

std::uint64_t SendSignal(int guest, std::int32_t code, std::int64_t value, std::int64_t thread, bool directed) {
    std::uint64_t ticket = 0;
    if (SendValueSignal_nid_no_patch(guest, code, value, thread, directed, &ticket) == SignalSendUnblocked)
        NotImplemented_nid_no_patch("aio delivering a signal to a thread that does not block it");
    return ticket;
}

void Notify(Job& job) {
    if (job.notify == SigevSignal || job.notify == SigevThreadId) job.signalTicket = SendSignal(job.signo, SiAsyncio, job.value, job.thread, job.notify == SigevThreadId);
    if (job.notify == SigevKevent) KqueueAioDone_nid_no_patch(reinterpret_cast<std::uintptr_t>(job.user), job.token, job.error);
}

void Complete(Aio& aio, const std::shared_ptr<Job>& job, std::int64_t status, int error) {
    job->status = status;
    job->error = error;
    job->state = State::Done;
    aio.done.push_back(job);
    Notify(*job);
    aio.changed.NotifyAll();
}

void Worker() {
    auto& aio = Instance();
    std::unique_lock lock(aio.lock);
    for (;;) {
        aio.changed.Wait(lock, [&] { return !aio.held && !aio.queued.empty(); });
        auto job = aio.queued.front();
        aio.queued.pop_front();
        job->state = State::Running;
        lock.unlock();
        Run(*job);
        lock.lock();
        Complete(aio, job, job->status, job->error);
    }
}

bool SocketReady(int descriptor, bool write) {
    GuestPollDescriptor entry{descriptor, write ? PollOut : PollIn, 0};
    return poll_nid_postfix(&entry, 1, 0) > 0;
}

bool Blocking(int descriptor) {
    const int flags = fcntl_nid_postfix(descriptor, GetFlags);
    return flags >= 0 && (flags & Nonblocking) == 0;
}

bool NoSigpipeSet(int descriptor) {
    int value = 0;
    std::uint32_t length = sizeof(value);
    return getsockopt_nid_postfix(descriptor, SocketLevel, NoSigpipe, &value, &length) == 0 && value != 0;
}

void RunSocketJob(Aio& aio, std::unique_lock<std::mutex>& lock, SocketQueue& queue) {
    const auto job = queue.jobs.front();
    queue.jobs.pop_front();
    job->state = State::Running;
    if (aio.socketHeld) {
        ++aio.socketWaiting;
        aio.changed.NotifyAll();
        aio.changed.Wait(lock, [&] { return !aio.socketHeld; });
        --aio.socketWaiting;
    }
    lock.unlock();
    int error = 0;
    bool wouldBlock = false;
    for (;;) {
        auto* buffer = static_cast<char*>(job->buffer) + job->done;
        const std::uint64_t left = job->bytes - job->done;
        const std::int64_t moved = queue.write ? send_nid_postfix(job->descriptor, buffer, left, MessageDontWait | MessageNoSignal)
                                               : recv_nid_postfix(job->descriptor, buffer, left, MessageDontWait);
        error = moved < 0 ? *__error_nid_postfix() : 0;
        if (moved > 0) job->done += static_cast<std::uint64_t>(moved);
        if (queue.write && error == GuestEpipe && !NoSigpipeSet(job->descriptor)) SendSignal(Sigpipe, SiKernel, 0, job->submitter, false);
        wouldBlock = error == GuestEagain || (queue.write && error == 0 && job->done != job->bytes);
        if (!wouldBlock) break;
        if ((job->done != 0 && !Blocking(job->descriptor)) || !SocketReady(job->descriptor, queue.write)) break;
    }
    lock.lock();
    if (wouldBlock && (job->done == 0 || Blocking(job->descriptor)) && job->cancelled) {
        if (job->done != 0) Complete(aio, job, static_cast<std::int64_t>(job->done), 0);
        else Complete(aio, job, -1, GuestEcanceled);
        return;
    }
    if (wouldBlock && (job->done == 0 || Blocking(job->descriptor))) {
        job->state = State::Queued;
        queue.jobs.push_front(job);
        return;
    }
    if (job->done != 0 && (error == GuestEintr || error == GuestEagain)) error = 0;
    if (error != 0) Complete(aio, job, -1, error);
    else Complete(aio, job, static_cast<std::int64_t>(job->done), 0);
}

void SocketWorker(std::list<SocketQueue>::iterator queue) {
    auto& aio = Instance();
    std::unique_lock lock(aio.lock);
    while (!queue->jobs.empty()) {
        GuestPollDescriptor entry{queue->descriptor, queue->write ? PollOut : PollIn, 0};
        lock.unlock();
        const int ready = poll_nid_postfix(&entry, 1, SocketPollMilliseconds);
        lock.lock();
        if (ready > 0 && !queue->jobs.empty()) RunSocketJob(aio, lock, *queue);
    }
    aio.sockets.erase(queue);
}

void QueueSocket(Aio& aio, const std::shared_ptr<Job>& job) {
    const bool write = job->opcode == LioWrite;
    auto found = std::find_if(aio.sockets.begin(), aio.sockets.end(), [&](const SocketQueue& queue) {
        return queue.descriptor == job->descriptor && queue.write == write;
    });
    const bool fresh = found == aio.sockets.end();
    if (fresh) found = aio.sockets.insert(aio.sockets.end(), SocketQueue{job->descriptor, write, {}});
    found->jobs.push_back(job);
    if (fresh) std::thread(SocketWorker, found).detach();
}

bool CancelSocketJob(Aio& aio, const std::shared_ptr<Job>& job) {
    for (auto& queue : aio.sockets) std::erase(queue.jobs, job);
    if (job->done != 0) {
        Complete(aio, job, static_cast<std::int64_t>(job->done), 0);
        return false;
    }
    Complete(aio, job, -1, GuestEcanceled);
    return true;
}

std::optional<int> ReadyError(const std::shared_ptr<Job>& job) {
    if (job->state != State::Done) return std::nullopt;
    return job->error;
}

std::int64_t Consume(Aio& aio, std::list<std::shared_ptr<Job>>::iterator found) {
    const auto job = *found;
    aio.done.erase(found);
    aio.all.remove(job);
    if (job->notify == SigevKevent) KqueueAioFree_nid_no_patch(reinterpret_cast<std::uintptr_t>(job->user), job->token);
    if (job->notify == SigevSignal || job->notify == SigevThreadId) TakeValueSignal_nid_no_patch(job->signo, job->signalTicket);
    Store(job->user, &GuestAiocb::error, std::int64_t{job->error});
    Store(job->user, &GuestAiocb::status, job->status);
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
    auto& aio = Instance();
    std::unique_lock lock(aio.lock);
    aio.used = true;
    Store(user, &GuestAiocb::status, std::int64_t{-1});
    Store(user, &GuestAiocb::error, std::int64_t{0});
    Store(user, &GuestAiocb::kernelinfo, reinterpret_cast<void*>(std::intptr_t{-1}));
    const auto fail = [&](int error) {
        Store(user, &GuestAiocb::error, std::int64_t{error});
        return Fail(error);
    };
    if (aio.all.size() >= QueuePerProcess) return fail(GuestEagain);
    GuestAiocb request{};
    if (!GuestCopyIn_nid_no_patch(&request, user, sizeof(request))) return fail(GuestEfault);
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
    const bool vnode = type != 0 && type != TypeFifo && type != TypeSocket;
    if (opcode == LioSync && !vnode) return fail(GuestEinval);
    if (opcode != LioSync && request.offset < 0) return fail(GuestEinval);
    const auto token = static_cast<std::uint64_t>(aio.nextId);
    if (!Store(user, &GuestAiocb::kernelinfo, reinterpret_cast<void*>(aio.nextId++))) return fail(GuestEinval);
    const auto ident = reinterpret_cast<std::uintptr_t>(user);
    if (notify == SigevKevent) {
        const auto keventFlags = static_cast<std::uint16_t>(request.sigevent.extra[0]);
        if ((keventFlags & ~KeventFlagsAllowed) != 0) return fail(GuestEinval);
        if (const int error = KqueueAddAio_nid_no_patch(request.sigevent.signo, ident, keventFlags, reinterpret_cast<void*>(request.sigevent.value), token))
            return fail(error);
    }
    Store(user, &GuestAiocb::error, std::int64_t{GuestEinprogress});
    if (type != TypeRegular && type != TypeDirectory && type != TypeSocket) {
        if (notify == SigevKevent) KqueueAioFree_nid_no_patch(ident, token);
        return fail(GuestEopnotsupp);
    }
    auto job = std::make_shared<Job>(Job{user, request.fildes, opcode, request.offset, request.buf, request.nbytes});
    job->token = token;
    job->notify = notify;
    job->signo = request.sigevent.signo;
    job->value = request.sigevent.value;
    job->submitter = scePthreadGetthreadid();
    job->thread = notify == SigevThreadId ? static_cast<std::int32_t>(request.sigevent.extra[0]) : job->submitter;
    job->socket = type == TypeSocket;
    aio.all.push_back(job);
    if (job->socket) {
        QueueSocket(aio, job);
    } else {
        aio.queued.push_back(job);
        if (!aio.started) {
            aio.started = true;
            std::thread(Worker).detach();
        }
    }
    aio.changed.NotifyAll();
    return 0;
}

}

extern "C" {

void AioHoldWorkerForTest_nid_no_patch(bool hold) {
    auto& aio = Instance();
    std::lock_guard lock(aio.lock);
    aio.held = hold;
    aio.changed.NotifyAll();
}

void AioHoldSocketForTest_nid_no_patch(bool hold) {
    auto& aio = Instance();
    std::lock_guard lock(aio.lock);
    aio.socketHeld = hold;
    aio.changed.NotifyAll();
}

std::size_t AioSocketsHeldForTest_nid_no_patch() {
    auto& aio = Instance();
    std::lock_guard lock(aio.lock);
    return aio.socketWaiting;
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
    if (!aio.used) return GuestEinval;
    for (const auto& job : aio.all) {
        if (job->user == aiocb) return ReadyError(job).value_or(GuestEinprogress);
    }
    if (Fetch(aiocb, &GuestAiocb::status) == -1) return static_cast<int>(Fetch(aiocb, &GuestAiocb::error));
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
    if (count > 0 && list == nullptr) return Fail(GuestEfault);
    const GuestAiocb* jobs[QueuePerProcess] = {};
    if (count > 0 && !GuestCopyIn_nid_no_patch(jobs, list, static_cast<std::size_t>(count) * sizeof(jobs[0]))) return Fail(GuestEfault);
    std::optional<std::uint64_t> deadline;
    if (timeout != nullptr) {
        KernelTimespec localTimeout{};
        if (!GuestCopyIn_nid_no_patch(&localTimeout, timeout, sizeof(localTimeout))) return Fail(GuestEfault);
        const auto wait = WaitTime(localTimeout);
        if (!wait) return Fail(GuestEinval);
        deadline = TimedWait::NowNanos() + static_cast<std::uint64_t>(wait->count());
    }
    auto& aio = Instance();
    std::unique_lock lock(aio.lock);
    if (!aio.used) return Fail(GuestEagain);
    if (count == 0) return 0;
    for (;;) {
        bool listed = false;
        for (const auto& job : aio.all) {
            for (int index = 0; index < count; ++index) {
                if (job->user != jobs[index]) continue;
                listed = true;
                if (job->state == State::Done) return 0;
            }
        }
        if (!listed) return 0;
        if (!deadline) aio.changed.Wait(lock);
        else if (!aio.changed.WaitUntil(lock, *deadline)) return Fail(GuestEagain);
    }
}

std::int64_t APS5_VABI aio_waitcomplete_nid_postfix(GuestAiocb** result, const KernelTimespec* timeout) {
    KernelTimespec localTimeout{};
    if (timeout != nullptr) {
        if (!GuestCopyIn_nid_no_patch(&localTimeout, timeout, sizeof(localTimeout))) {
            return Fail(GuestEfault);
        }
    }
    if (result != nullptr) {
        GuestAiocb* nullCb = nullptr;
        if (!GuestCopyOut_nid_no_patch(result, &nullCb, sizeof(nullCb))) {
            return Fail(GuestEfault);
        }
    }
    std::optional<std::uint64_t> deadline;
    bool poll = false;
    if (timeout != nullptr) {
        poll = localTimeout.tv_sec == 0 && localTimeout.tv_nsec == 0;
        if (!poll) {
            const auto wait = WaitTime(localTimeout);
            if (!wait) return Fail(GuestEinval);
            deadline = TimedWait::NowNanos() + static_cast<std::uint64_t>(wait->count());
        }
    }
    auto& aio = Instance();
    std::unique_lock lock(aio.lock);
    aio.used = true;
    while (aio.done.empty()) {
        if (poll) return Fail(GuestEagain);
        if (!deadline) aio.changed.Wait(lock);
        else if (!aio.changed.WaitUntil(lock, *deadline) && aio.done.empty()) return Fail(GuestEagain);
    }
    if (result != nullptr) {
        GuestAiocb* user = aio.done.front()->user;
        if (!GuestCopyOut_nid_no_patch(result, &user, sizeof(user))) {
            return Fail(GuestEfault);
        }
    }
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
        if (job->state == State::Queued && job->socket) {
            if (CancelSocketJob(aio, job)) ++cancelled;
            else ++notCancelled;
        } else if (job->state == State::Queued) {
            std::erase(aio.queued, job);
            Complete(aio, job, -1, GuestEcanceled);
            ++cancelled;
        } else {
            job->cancelled = true;
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
