#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Equeue/include/EdgeWatch.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/HostStreams.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include "prx/libc/include/WindowsDescriptor.hpp"
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

constexpr int GUEST_EPERM = 1;
constexpr int GUEST_ENOENT = 2;
constexpr int GUEST_EBADF = 9;
constexpr int GUEST_EFAULT = 14;
constexpr int GUEST_EINVAL = 22;
constexpr int GUEST_ENFILE = 23;
constexpr int GUEST_EMFILE = 24;
constexpr int GUEST_EPIPE = 32;

constexpr int KQ_NEVENTS = 8;
constexpr std::int16_t EVFILT_AIO = -3;
constexpr std::int16_t EVFILT_PROCDESC = -8;
constexpr std::int16_t EVFILT_LIO = -10;
constexpr std::int16_t EVFILT_SENDFILE = -12;
constexpr std::uint32_t NOTE_FFCTRLMASK = 0xc0000000;
constexpr std::uint32_t NOTE_FFLAGSMASK = 0x00ffffff;
constexpr std::uint32_t NOTE_FFAND = 0x40000000;
constexpr std::uint32_t NOTE_FFOR = 0x80000000;
constexpr std::uint32_t NOTE_FFCOPY = 0xc0000000;
constexpr std::uint32_t NOTE_TRIGGER = 0x01000000;
constexpr std::uint32_t NOTE_LOWAT = 0x0001;
constexpr std::uint32_t NOTE_SECONDS = 0x0001;
constexpr std::uint32_t NOTE_MSECONDS = 0x0002;
constexpr std::uint32_t NOTE_USECONDS = 0x0004;
constexpr std::uint32_t NOTE_NSECONDS = 0x0008;
constexpr std::uint32_t NOTE_TIMER_PRECMASK = NOTE_SECONDS | NOTE_MSECONDS | NOTE_USECONDS | NOTE_NSECONDS;
constexpr std::uint16_t KNOTE_CLEARED_FLAGS = EV_ADD | EV_DELETE | EV_ENABLE | EV_DISABLE | EV_FORCEONESHOT;

std::mutex g_kqueueMutex;
std::unordered_map<int, KernelEqueueRef> g_kqueues;
std::mutex g_kqueueWatchMutex;

int PosixFailure(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

int GuestErrno(int sceError) {
    return sceError & 0xffff;
}

KernelEqueueRef FindKqueue(int descriptor) {
    std::lock_guard lock(g_kqueueMutex);
    const auto found = g_kqueues.find(descriptor);
    return found != g_kqueues.end() ? found->second : KernelEqueueRef{};
}

int ReserveDescriptor() {
#ifdef _WIN32
    return ::_open("NUL", _O_RDONLY | _O_BINARY);
#else
    return ::open("/dev/null", O_RDONLY | O_CLOEXEC);
#endif
}

void ReleaseDescriptor(int descriptor) {
#ifdef _WIN32
    ::_close(descriptor);
#else
    ::close(descriptor);
#endif
}

std::optional<std::uint64_t> TimerPeriodNanos(std::int64_t data, std::uint32_t fflags) {
    std::uint64_t unit = 0;
    switch (fflags & NOTE_TIMER_PRECMASK) {
    case NOTE_SECONDS:
        unit = 1000000000ULL;
        break;
    case NOTE_MSECONDS:
    case 0:
        unit = 1000000ULL;
        break;
    case NOTE_USECONDS:
        unit = 1000ULL;
        break;
    case NOTE_NSECONDS:
        unit = 1;
        break;
    default:
        return std::nullopt;
    }
    const auto value = static_cast<std::uint64_t>(data);
    if (value > std::numeric_limits<std::uint64_t>::max() / unit) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return value * unit;
}

int DescriptorFilterError(int kqueueDescriptor, std::uintptr_t ident) {
    if (ident > static_cast<std::uintptr_t>(INT_MAX)) {
        return GUEST_EBADF;
    }
    const int descriptor = static_cast<int>(ident);
    if (GuestSockets::IsOpen(descriptor)) {
        return 0;
    }
    if (!DescriptorIsOpen_nid_no_patch(descriptor)) {
        return GUEST_EBADF;
    }
    return descriptor == kqueueDescriptor ? GUEST_EINVAL : 0;
}

bool MeasuredProcdescDescriptor(int descriptor) {
    std::shared_ptr<void> owner;
    std::uintptr_t native = 0;
    if (GuestSockets::IsOpen(descriptor) || KernelLookupSocketNative_nid_no_patch(descriptor, &owner, &native) || FindKqueue(descriptor)) {
        return true;
    }
    if (descriptor < 3 || File::IsRandomDevice(descriptor)) {
        return false;
    }
    if (File::IsArchivedFile(descriptor) || File::DirectoryDescriptorPath(descriptor)) {
        return true;
    }
#ifdef _WIN32
    const DWORD type = ::GetFileType(WindowsDescriptorHandle_nid_no_patch(descriptor));
    return type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE;
#else
    struct stat status{};
    return ::fstat(descriptor, &status) == 0 && (S_ISREG(status.st_mode) || S_ISDIR(status.st_mode) || S_ISFIFO(status.st_mode) || S_ISSOCK(status.st_mode));
#endif
}

std::int64_t DescriptorLowWater(const KernelEvent& change) {
    if ((change.fflags & NOTE_LOWAT) == 0 || !GuestSockets::IsOpen(static_cast<int>(change.ident))) {
        return 0;
    }
    if (change.data < 1) {
        NotImplemented_nid_no_patch("kevent: NOTE_LOWAT below 1 on a socket");
    }
    if (change.data > 1 && GuestSockets::Type(static_cast<int>(change.ident)) != 1) {
        NotImplemented_nid_no_patch("kevent: NOTE_LOWAT above 1 on a socket that is not a stream");
    }
    return change.data;
}

void RejectEdgeWriteMark(const KernelEvent& change, std::int64_t lowWater) {
    if (change.filter != EVFILT_WRITE || lowWater <= 1) {
        return;
    }
    const int family = GuestSockets::Family(static_cast<int>(change.ident));
    if ((family == 2 || family == 28) && GuestSockets::Type(static_cast<int>(change.ident)) == 1) {
        NotImplemented_nid_no_patch("kevent: EV_CLEAR EVFILT_WRITE with NOTE_LOWAT above 1 on a TCP socket");
    }
}

int AddStandardInputKnote(const KernelEqueueRef& queue, KernelEvent& change, std::uint16_t flags) {
    KernelEqueueEvent event{};
    event.event = change;
    event.event.flags = flags;
    event.event.fflags = 0;
    event.event.data = 0;
    change.fflags = 0;
    change.data = 0;
    return GuestErrno(queue->AddEvent(event));
}

bool PollEdge(KernelEqueueEvent* e) {
    auto& watch = *static_cast<EdgeWatch::Watch*>(e->filter.data);
    if (watch.TakeEdge()) {
        watch.armed = true;
    }
    if (!watch.armed) {
        return false;
    }
    std::int64_t data = 0;
    bool eof = false;
    if (!GuestSockets::Ready(static_cast<int>(e->event.ident), e->event.filter == EVFILT_WRITE, &data, &eof)) {
        watch.armed = false;
        return false;
    }
    if (!eof && data < e->lowWater && !GuestSockets::Listening(static_cast<int>(e->event.ident))) {
        watch.armed = false;
        return false;
    }
    e->event.data = static_cast<intptr_t>(data);
    e->event.flags = static_cast<std::uint16_t>(eof ? (e->event.flags | EV_EOF) : (e->event.flags & ~EV_EOF));
    return true;
}

void ResetEdge(KernelEqueueEvent* e) {
    static_cast<EdgeWatch::Watch*>(e->filter.data)->armed = false;
    e->triggered = false;
}

void WatchEdges(KernelEqueueEvent& event, bool socket) {
    const int descriptor = static_cast<int>(event.event.ident);
    auto watch = EdgeWatch::Watch::Open(socket ? GuestSockets::NativeDescriptor(descriptor) : descriptor, event.event.filter == EVFILT_WRITE);
    event.filter.data = watch.get();
    event.filter.waitDescriptor = watch->Descriptor();
    event.filter.owner = std::move(watch);
    event.filter.pollFunc = PollEdge;
    event.filter.resetFunc = ResetEdge;
}

bool PollKqueue(KernelEqueueEvent* e) {
    const auto pending = e->filter.watched->CountTriggeredEvents();
    e->event.data = static_cast<std::intptr_t>(pending);
    return pending != 0;
}

bool KqueueReaches(const KernelEqueueRef& from, const KernelEqueuePrivate* target, std::vector<const KernelEqueuePrivate*>& visited) {
    if (from.get() == target) {
        return true;
    }
    if (std::find(visited.begin(), visited.end(), from.get()) != visited.end()) {
        return false;
    }
    visited.push_back(from.get());
    for (const auto& next : from->WatchedQueues()) {
        if (KqueueReaches(next, target, visited)) {
            return true;
        }
    }
    return false;
}

int AddKqueueKnote(const KernelEqueueRef& queue, const KernelEqueueRef& watched, KernelEvent& change, std::uint16_t flags) {
    if ((flags & EV_CLEAR) != 0) {
        NotImplemented_nid_no_patch("kevent: EV_CLEAR on a kqueue descriptor");
    }
    std::lock_guard lock(g_kqueueWatchMutex);
    std::vector<const KernelEqueuePrivate*> visited;
    if (KqueueReaches(watched, queue.get(), visited)) {
        NotImplemented_nid_no_patch("kevent: EVFILT_READ on a kqueue that watches the kqueue being registered on");
    }
    KernelEqueueEvent event{};
    event.event = change;
    event.event.flags = flags;
    event.event.fflags = 0;
    event.event.data = 0;
    event.filter.pollFunc = PollKqueue;
    event.filter.watched = watched;
    change.fflags = 0;
    change.data = 0;
    return GuestErrno(queue->AddEvent(event));
}

int AddDescriptorKnote(const KernelEqueueRef& queue, KernelEvent& change, std::uint16_t flags) {
    const int descriptor = static_cast<int>(change.ident);
    if (const auto watched = FindKqueue(descriptor)) {
        if (change.filter != EVFILT_READ) {
            return GUEST_EINVAL;
        }
        return AddKqueueKnote(queue, watched, change, flags);
    }
    if (descriptor == 0 && change.filter == EVFILT_READ && File::IsHostStream(descriptor)) {
        return AddStandardInputKnote(queue, change, flags);
    }
    const bool socket = GuestSockets::IsOpen(descriptor);
    std::int64_t data = 0;
    bool eof = false;
    bool writable = false;
    const bool pipe = !socket && (change.filter == EVFILT_READ ? GuestSockets::PipeReadState(descriptor, &data, &eof) : GuestSockets::PipeWriteState(descriptor, &data, &eof, &writable));
    if (!socket && !pipe) {
        NotImplemented_nid_no_patch(change.filter == EVFILT_READ ? "kevent: EVFILT_READ on a descriptor that is neither a socket nor the read end of a pipe" : "kevent: EVFILT_WRITE on a descriptor that is neither a socket nor the write end of a pipe");
    }
    if (pipe && change.filter == EVFILT_WRITE && eof) {
        return GUEST_EPIPE;
    }
    const auto lowWater = DescriptorLowWater(change);
    if ((flags & EV_CLEAR) != 0) {
        RejectEdgeWriteMark(change, lowWater);
    }
    auto event = EqueueDescriptorEvent(descriptor, change.filter, flags, change.udata);
    event.lowWater = lowWater;
    if ((flags & EV_CLEAR) != 0) {
        WatchEdges(event, socket);
    }
    change.fflags = 0;
    change.data = 0;
    return GuestErrno(queue->AddEvent(event));
}

int AddTimerKnote(const KernelEqueueRef& queue, KernelEvent& change, std::uint16_t flags) {
    std::int64_t period = change.data;
    if (period < 0) {
        return GUEST_EINVAL;
    }
    if (period == 0 && (flags & EV_ONESHOT) == 0) {
        period = 1;
    }
    if ((change.fflags & ~NOTE_TIMER_PRECMASK) != 0) {
        return GUEST_EINVAL;
    }
    const auto periodNs = TimerPeriodNanos(period, change.fflags);
    if (!periodNs) {
        return GUEST_EINVAL;
    }
    const std::uint64_t now = KernelEqueuePrivate::MonotonicNs();
    KernelEqueueEvent event{};
    event.deadlineNs = *periodNs <= std::numeric_limits<std::uint64_t>::max() - now ? now + *periodNs : std::numeric_limits<std::uint64_t>::max();
    event.intervalNs = (flags & EV_ONESHOT) != 0 ? 0 : *periodNs;
    event.event = change;
    event.event.flags = static_cast<std::uint16_t>(flags | EV_CLEAR);
    event.event.fflags = 0;
    event.event.data = 0;
    change.fflags = 0;
    change.data = 0;
    return GuestErrno(queue->AddEvent(event));
}

int AddUserKnote(const KernelEqueueRef& queue, KernelEvent& change, std::uint16_t flags) {
    KernelEqueueEvent event{};
    event.event = change;
    event.event.flags = flags;
    event.filter.resetFunc = [](KernelEqueueEvent* e) {
        if ((e->event.flags & EV_CLEAR) != 0) {
            e->triggered = false;
        }
    };
    change.fflags = 0;
    change.data = 0;
    return GuestErrno(queue->AddEvent(event));
}

void TouchUserKnote(KernelEqueueEvent& event, KernelEvent& change) {
    if ((change.fflags & NOTE_TRIGGER) != 0) {
        event.triggered = true;
    }
    const std::uint32_t control = change.fflags & NOTE_FFCTRLMASK;
    change.fflags &= NOTE_FFLAGSMASK;
    if (control == NOTE_FFAND) {
        event.event.fflags &= change.fflags;
    } else if (control == NOTE_FFOR) {
        event.event.fflags |= change.fflags;
    } else if (control == NOTE_FFCOPY) {
        event.event.fflags = change.fflags;
    }
    event.event.data = change.data;
    if ((change.flags & EV_CLEAR) != 0) {
        event.triggered = false;
    }
}

int ModifyTimerKnote(const KernelEqueueRef& queue, KernelEvent& change) {
    if ((change.flags & ~(EV_ADD | EV_ONESHOT)) != 0) {
        NotImplemented_nid_no_patch("kevent: re-adding an EVFILT_TIMER with flags other than EV_ONESHOT");
    }
    if (change.data <= 0 || change.fflags != 0) {
        NotImplemented_nid_no_patch("kevent: re-adding an EVFILT_TIMER with nonzero fflags or a period of 0 or less");
    }
    const auto periodNs = TimerPeriodNanos(change.data, change.fflags);
    bool oneShot = false;
    bool disabled = false;
    bool overflow = false;
    const int error = GuestErrno(queue->EditEvent(change.ident, change.filter, [&change, &oneShot, &disabled, &overflow, periodNs](KernelEqueueEvent& event) {
        if (event.intervalNs == 0) {
            oneShot = true;
            return;
        }
        if (!event.enabled) {
            disabled = true;
            return;
        }
        if (*periodNs > std::numeric_limits<std::uint64_t>::max() - event.deadlineNs) {
            overflow = true;
            return;
        }
        event.intervalNs = *periodNs;
        event.event.udata = change.udata;
    }));
    if (oneShot) {
        NotImplemented_nid_no_patch("kevent: re-adding a one-shot EVFILT_TIMER");
    }
    if (disabled) {
        NotImplemented_nid_no_patch("kevent: re-adding a disabled EVFILT_TIMER");
    }
    if (overflow) {
        NotImplemented_nid_no_patch("kevent: re-adding an EVFILT_TIMER with a period that overflows the armed expiry");
    }
    change.fflags = 0;
    change.data = 0;
    return error;
}

int ModifyKnote(const KernelEqueueRef& queue, KernelEvent& change) {
    if (change.filter == EVFILT_TIMER) {
        NotImplemented_nid_no_patch("kevent: changing an existing EVFILT_TIMER");
    }
    const bool descriptor = change.filter == EVFILT_READ || change.filter == EVFILT_WRITE;
    const auto lowWater = descriptor ? DescriptorLowWater(change) : 0;
    if (change.filter == EVFILT_VNODE) {
        RequireVnodeNotes(change.fflags);
    }
    return GuestErrno(queue->EditEvent(change.ident, change.filter, [&change, descriptor, lowWater](KernelEqueueEvent& event) {
        if (event.filter.pollFunc == PollEdge) {
            RejectEdgeWriteMark(change, lowWater);
        }
        event.event.udata = change.udata;
        if (descriptor) {
            event.lowWater = lowWater;
        }
        if ((change.flags & EV_ENABLE) != 0) {
            event.enabled = true;
        }
        if ((change.flags & EV_DISABLE) != 0) {
            event.enabled = false;
        }
        if ((change.flags & EV_FORCEONESHOT) != 0) {
            event.event.flags = static_cast<std::uint16_t>(event.event.flags | EV_ONESHOT);
            event.triggered = true;
        }
        if (event.enabled && event.filter.pollFunc == PollEdge) {
            static_cast<EdgeWatch::Watch*>(event.filter.data)->armed = true;
        }
        if (change.filter == EVFILT_USER) {
            TouchUserKnote(event, change);
        } else if (change.filter == EVFILT_VNODE) {
            SetVnodeNotes(event, change.fflags);
        }
    }));
}

int Register(const KernelEqueueRef& queue, int kqueueDescriptor, KernelEvent& change) {
    if ((change.flags & (EV_ENABLE | EV_DISABLE)) == (EV_ENABLE | EV_DISABLE)) {
        return GUEST_EINVAL;
    }
    if (change.filter > 0) {
        return GUEST_EINVAL;
    }
    if (change.filter == EVFILT_AIO || change.filter == EVFILT_LIO || change.filter == EVFILT_SENDFILE) {
        if ((change.flags & EV_ADD) == 0) {
            return GUEST_ENOENT;
        }
        change.fflags = 0;
        change.data = 0;
        return GUEST_EPERM;
    }
    if (change.filter == EVFILT_PROCDESC) {
        if (const int error = DescriptorFilterError(kqueueDescriptor, change.ident)) {
            return error;
        }
        if ((change.flags & EV_ADD) == 0) {
            return GUEST_ENOENT;
        }
        if (!MeasuredProcdescDescriptor(static_cast<int>(change.ident))) {
            NotImplemented_nid_no_patch("kevent: EVFILT_PROCDESC on a kind of descriptor that was not measured");
        }
        change.fflags = 0;
        change.data = 0;
        return GUEST_EINVAL;
    }
    if (change.filter != EVFILT_READ && change.filter != EVFILT_WRITE && change.filter != EVFILT_VNODE && change.filter != EVFILT_TIMER && change.filter != EVFILT_USER) {
        NotImplemented_nid_no_patch(("kevent: filter " + std::to_string(change.filter)).c_str());
    }
    if (change.filter == EVFILT_READ || change.filter == EVFILT_WRITE || change.filter == EVFILT_VNODE) {
        if (const int error = DescriptorFilterError(kqueueDescriptor, change.ident)) {
            return error;
        }
    }
    if (!queue->HasEvent(change.ident, change.filter)) {
        if ((change.flags & EV_ADD) == 0) {
            return GUEST_ENOENT;
        }
        const auto flags = static_cast<std::uint16_t>(change.flags & ~KNOTE_CLEARED_FLAGS);
        int error;
        if (change.filter == EVFILT_TIMER) {
            error = AddTimerKnote(queue, change, flags);
        } else if (change.filter == EVFILT_USER) {
            error = AddUserKnote(queue, change, flags);
        } else if (change.filter == EVFILT_VNODE) {
            const std::uint32_t notes = change.fflags;
            change.fflags = 0;
            change.data = 0;
            error = AddVnodeEvent(queue, static_cast<int>(change.ident), notes, flags, change.udata);
        } else {
            error = AddDescriptorKnote(queue, change, flags);
        }
        if (error == 0 && (change.flags & EV_DISABLE) != 0) {
            queue->EditEvent(change.ident, change.filter, [](KernelEqueueEvent& event) { event.enabled = false; });
        }
        return error;
    }
    if ((change.flags & EV_DELETE) != 0) {
        return GuestErrno(queue->DeleteEvent(change.ident, change.filter));
    }
    if (change.filter == EVFILT_TIMER && (change.flags & EV_ADD) != 0) {
        return ModifyTimerKnote(queue, change);
    }
    return ModifyKnote(queue, change);
}

std::optional<std::uint64_t> ScanDeadline(const KernelTimespec& timeout) {
    if (timeout.tv_sec > INT32_MAX) {
        return std::nullopt;
    }
    const std::uint64_t span = static_cast<std::uint64_t>(timeout.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(timeout.tv_nsec);
    return TimedWait::NowNanos() + span;
}

}

bool IsKqueueDescriptor(int descriptor) {
    return static_cast<bool>(FindKqueue(descriptor));
}

std::optional<bool> KqueueReadable(int descriptor) {
    const auto queue = FindKqueue(descriptor);
    if (!queue) return std::nullopt;
    return queue->HasTriggeredEvents();
}

void KqueueDescriptorClosed(int descriptor) {
    KernelEqueueRef closed;
    std::vector<KernelEqueueRef> queues;
    {
        std::lock_guard lock(g_kqueueMutex);
        const auto found = g_kqueues.find(descriptor);
        if (found != g_kqueues.end()) {
            closed = std::move(found->second);
            g_kqueues.erase(found);
        }
        for (const auto& [kqueueDescriptor, queue] : g_kqueues) {
            queues.push_back(queue);
        }
    }
    if (closed) {
        closed->Close();
    }
    for (const auto& queue : queues) {
        queue->RemoveDescriptorEvents(static_cast<std::uintptr_t>(descriptor));
    }
}

extern "C" {

int APS5_VABI kqueue_nid_postfix() {
    const int descriptor = ReserveDescriptor();
    if (descriptor < 0) {
        if (errno == EMFILE) return PosixFailure(GUEST_EMFILE);
        if (errno == ENFILE) return PosixFailure(GUEST_ENFILE);
        throw std::runtime_error(std::string(__func__) + ": reserving a descriptor failed, errno=" + std::to_string(errno));
    }
    auto queue = std::make_shared<KernelEqueuePrivate>(static_cast<KernelEqueue>(descriptor), true);
    queue->SetName("kqueue");
    std::lock_guard lock(g_kqueueMutex);
    g_kqueues[descriptor] = std::move(queue);
    return descriptor;
}

int APS5_VABI kevent_nid_postfix(int descriptor, const KernelEvent* changelist, int nchanges, KernelEvent* eventlist, int nevents, const KernelTimespec* timeout) {
    const auto queue = FindKqueue(descriptor);
    if (!queue) {
        return PosixFailure(GUEST_EBADF);
    }
    if (nevents < 0) {
        return PosixFailure(GUEST_EINVAL);
    }
    if (nchanges > 0 && changelist == nullptr) {
        return PosixFailure(GUEST_EFAULT);
    }
    int errors = 0;
    for (int i = 0; i < nchanges; ++i) {
        KernelEvent change = changelist[i];
        if (change.filter == 0) {
            continue;
        }
        change.flags = static_cast<std::uint16_t>(change.flags & ~EV_SYSFLAGS);
        const int error = Register(queue, descriptor, change);
        if (error == 0 && (change.flags & EV_RECEIPT) == 0) {
            continue;
        }
        if (nevents == 0) {
            return error == 0 ? 0 : PosixFailure(error);
        }
        change.flags = EV_ERROR;
        change.data = error;
        if (eventlist != nullptr) {
            *eventlist++ = change;
        }
        --nevents;
        ++errors;
    }
    if (errors != 0) {
        return errors;
    }
    if (nevents == 0) {
        return 0;
    }
    std::optional<std::uint64_t> deadline;
    bool poll = false;
    if (timeout != nullptr) {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000LL) {
            return PosixFailure(GUEST_EINVAL);
        }
        poll = timeout->tv_sec == 0 && timeout->tv_nsec == 0;
        if (!poll) {
            deadline = ScanDeadline(*timeout);
        }
    }
    KernelEvent scratch[KQ_NEVENTS];
    KernelEvent* target = eventlist != nullptr ? eventlist : scratch;
    const int capacity = eventlist != nullptr ? nevents : std::min(nevents, KQ_NEVENTS);
    const int count = poll ? queue->GetTriggeredEvents(target, capacity) : queue->WaitForEventsUntil(target, capacity, deadline.value_or(0));
    if (count < 0) {
        return PosixFailure(GuestErrno(count));
    }
    if (count > 0 && eventlist == nullptr) {
        return PosixFailure(GUEST_EFAULT);
    }
    return count;
}

}
