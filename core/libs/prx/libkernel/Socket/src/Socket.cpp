#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/File/include/DescriptorControl.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/Socket/include/SocketSelectDeadline.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#include <poll.h>
#endif

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int SelectDescriptorLimit = 1024;
constexpr int SelectPollSliceMilliseconds = 10;

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

std::atomic<KernelSocketPoll::Poller> g_socketPoller{nullptr};
std::atomic<KernelSocketPoll::Closer> g_socketCloser{nullptr};
std::atomic<KernelSocketPoll::OptionSetter> g_socketOptionSetter{nullptr};
std::atomic<KernelSocketPoll::OptionGetter> g_socketOptionGetter{nullptr};
std::atomic<KernelSocketPoll::StatusControl> g_socketStatusControl{nullptr};
std::atomic<KernelSocketPoll::NativeLookup> g_socketNativeLookup{nullptr};

bool DescriptorSet(const void* set, int descriptor) {
    return set != nullptr && ((static_cast<const std::uint64_t*>(set)[descriptor / 64] >> (descriptor % 64)) & 1u) != 0;
}

void ClearDescriptor(void* set, int descriptor) {
    if (set != nullptr) static_cast<std::uint64_t*>(set)[descriptor / 64] &= ~(std::uint64_t{1} << (descriptor % 64));
}

void ClearDescriptorsFrom(int nfds, void* readfds, void* writefds, void* exceptfds) {
    if (nfds % 64 == 0) return;
    const std::uint64_t kept = (std::uint64_t{1} << (nfds % 64)) - 1;
    for (void* set : {readfds, writefds, exceptfds}) {
        if (set != nullptr) static_cast<std::uint64_t*>(set)[nfds / 64] &= kept;
    }
}

struct PollGroup {
    KernelSocketPoll::Poller poll;
    std::vector<KernelSocketPoll::Entry> entries;
    std::vector<std::size_t> slots;
};

int KernelSockets(KernelSocketPoll::Entry* entries, int count, int timeoutMilliseconds) {
    return GuestSockets::Poll(entries, count, timeoutMilliseconds);
}

int Kqueues(KernelSocketPoll::Entry* entries, int count, int timeoutMilliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);
    for (;;) {
        int ready = 0;
        for (int index = 0; index < count; ++index) {
            auto& entry = entries[index];
            const auto readable = KqueueReadable(entry.descriptor);
            entry.revents = !readable ? KernelSocketPoll::Unknown : *readable ? static_cast<short>(entry.events & KernelSocketPoll::Readable) : short{0};
            if (entry.revents != 0) ++ready;
        }
        if (ready > 0 || std::chrono::steady_clock::now() >= deadline) return ready;
        PreciseSleepUntil(std::min(deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(1)));
    }
}

#ifndef _WIN32

short HostEventFlags(short events) {
    short flags = 0;
    if ((events & KernelSocketPoll::Readable) != 0) flags |= POLLIN;
    if ((events & KernelSocketPoll::Writable) != 0) flags |= POLLOUT;
    return flags;
}

short KernelEventFlags(short revents) {
    short flags = 0;
    if ((revents & POLLIN) != 0) flags |= KernelSocketPoll::Readable;
    if ((revents & POLLOUT) != 0) flags |= KernelSocketPoll::Writable;
    if ((revents & POLLERR) != 0) flags |= KernelSocketPoll::Error;
    if ((revents & POLLHUP) != 0) flags |= KernelSocketPoll::HangUp;
    return flags;
}

int HostPoll(KernelSocketPoll::Entry* descriptors, int count, int timeoutMilliseconds) {
    std::vector<struct pollfd> native(static_cast<std::size_t>(count));
    for (std::size_t index = 0; index < native.size(); ++index) {
        native[index].fd = descriptors[index].descriptor;
        native[index].events = HostEventFlags(descriptors[index].events);
        native[index].revents = 0;
    }
    const int result = ::poll(native.data(), static_cast<nfds_t>(native.size()), timeoutMilliseconds);
    if (result < 0) return -errno;
    for (std::size_t index = 0; index < native.size(); ++index) {
        if ((native[index].revents & POLLNVAL) != 0) return -EBADF;
        descriptors[index].revents = KernelEventFlags(native[index].revents);
    }
    return result;
}

#else

int HostPoll(KernelSocketPoll::Entry* descriptors, int count, int timeoutMilliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);
    for (;;) {
        int ready = 0;
        for (int index = 0; index < count; ++index) {
            auto& entry = descriptors[index];
            if (!DescriptorIsOpen_nid_no_patch(entry.descriptor)) return -GuestEbadf;
            if (!GuestSockets::PipeReady(entry.descriptor, entry.events, entry.revents)) entry.revents = KernelSocketPoll::Unknown;
            if (entry.revents != 0) ++ready;
        }
        if (ready > 0 || std::chrono::steady_clock::now() >= deadline) return ready;
        PreciseSleepUntil(std::min(deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(1)));
    }
}

#endif

}

extern "C" {

const char* APS5_VABI inet_ntop_nid_postfix(int af, const void* src, char* dst, uint32_t size) {
    return __inet_ntop_nid_postfix(af, src, dst, size);
}

int APS5_VABI inet_pton_nid_postfix(int af, const char* src, void* dst) {
    return __inet_pton_nid_postfix(af, src, dst);
}

void KernelSetSocketPoller_nid_no_patch(KernelSocketPoll::Poller poller) {
    g_socketPoller.store(poller);
}

void KernelSetSocketCloser_nid_no_patch(KernelSocketPoll::Closer closer) {
    g_socketCloser.store(closer);
}

bool KernelCloseSocket_nid_no_patch(int descriptor, int* error) {
    const auto closer = g_socketCloser.load();
    return closer != nullptr && closer(descriptor, error);
}

void KernelSetSocketOptionSetter_nid_no_patch(KernelSocketPoll::OptionSetter setter) {
    g_socketOptionSetter.store(setter);
}

void KernelSetSocketOptionGetter_nid_no_patch(KernelSocketPoll::OptionGetter getter) {
    g_socketOptionGetter.store(getter);
}

bool KernelGetSocketOption_nid_no_patch(int descriptor, int level, int option, void* value, std::uint32_t* length, int* error) {
    const auto getter = g_socketOptionGetter.load();
    return getter != nullptr && getter(descriptor, level, option, value, length, error);
}

bool KernelSetSocketOption_nid_no_patch(int descriptor, int level, int option, const void* value, std::uint32_t length, int* error) {
    const auto setter = g_socketOptionSetter.load();
    return setter != nullptr && setter(descriptor, level, option, value, length, error);
}

void KernelSetSocketStatusControl_nid_no_patch(KernelSocketPoll::StatusControl control) {
    g_socketStatusControl.store(control);
}

bool KernelControlSocketStatus_nid_no_patch(int descriptor, int command, int flags, int* result, int* error) {
    const auto control = g_socketStatusControl.load();
    return control != nullptr && control(descriptor, command, flags, result, error);
}

void KernelSetSocketNativeLookup_nid_no_patch(KernelSocketPoll::NativeLookup lookup) {
    g_socketNativeLookup.store(lookup);
}

bool KernelFindSocketNative_nid_no_patch(int descriptor, std::shared_ptr<void>* owner, std::uintptr_t* native) {
    const auto lookup = g_socketNativeLookup.load();
    return lookup != nullptr && lookup(descriptor, owner, native);
}

int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
    const auto* limit = static_cast<const GuestTimeval*>(timeout);
    if (nfds < 0 || nfds > SelectDescriptorLimit || (limit != nullptr && (limit->seconds < 0 || limit->microseconds < 0 || limit->microseconds >= 1000000))) {
        *__error_nid_postfix() = GuestEinval;
        return -1;
    }
    PollGroup kernel{&KernelSockets, {}, {}};
    PollGroup kqueues{&Kqueues, {}, {}};
    PollGroup net{nullptr, {}, {}};
    std::vector<KernelSocketPoll::Entry> devices;
    std::vector<KernelSocketPoll::Entry> requested;
    for (int descriptor = 0; descriptor < nfds; ++descriptor) {
        short events = 0;
        if (DescriptorSet(readfds, descriptor)) events |= KernelSocketPoll::Readable;
        if (DescriptorSet(writefds, descriptor)) events |= KernelSocketPoll::Writable;
        if (DescriptorSet(exceptfds, descriptor)) events |= KernelSocketPoll::Urgent;
        if (events == 0) continue;
        if (GuestSockets::IsOpen(descriptor)) {
            kernel.entries.push_back({descriptor, events, 0});
            kernel.slots.push_back(requested.size());
            requested.push_back({descriptor, events, 0});
        } else if (IsKqueueDescriptor_nid_no_patch(descriptor)) {
            kqueues.entries.push_back({descriptor, events, 0});
            kqueues.slots.push_back(requested.size());
            requested.push_back({descriptor, events, 0});
        } else if (File::IsRandomDevice(descriptor)) devices.push_back({descriptor, events, static_cast<short>((events & KernelSocketPoll::Readable) != 0 ? KernelSocketPoll::Readable : events)});
        else if (File::IsVnode(descriptor)) devices.push_back({descriptor, events, static_cast<short>(events & (KernelSocketPoll::Readable | KernelSocketPoll::Writable))});
        else if (descriptor <= 2) devices.push_back({descriptor, events, static_cast<short>(descriptor == 0 ? 0 : events & KernelSocketPoll::Writable)});
        else {
            net.entries.push_back({descriptor, events, 0});
            net.slots.push_back(requested.size());
            requested.push_back({descriptor, events, 0});
        }
    }
    for (const auto& device : devices) requested.push_back(device);
    const bool devicesReady = std::any_of(devices.begin(), devices.end(), [](const auto& entry) { return entry.revents != 0; });
    std::vector<PollGroup> groups;
    if (!kernel.entries.empty()) groups.push_back(std::move(kernel));
    if (!kqueues.entries.empty()) groups.push_back(std::move(kqueues));
    if (!net.entries.empty()) {
        PollGroup hosts{&HostPoll, {}, {}};
        net.poll = g_socketPoller.load();
        if (net.poll != nullptr) {
            for (auto& socket : net.entries) socket.revents = 0;
            const int probe = net.poll(net.entries.data(), static_cast<int>(net.entries.size()), 0);
            if (probe < 0) {
                *__error_nid_postfix() = -probe;
                return -1;
            }
        }
        for (std::size_t index = 0; index < net.entries.size();) {
            if (net.poll != nullptr && (net.entries[index].revents & KernelSocketPoll::Unknown) == 0) {
                ++index;
                continue;
            }
            const int descriptor = net.entries[index].descriptor;
            hosts.entries.push_back({descriptor, net.entries[index].events, 0});
            hosts.slots.push_back(net.slots[index]);
            net.entries.erase(net.entries.begin() + static_cast<std::ptrdiff_t>(index));
            net.slots.erase(net.slots.begin() + static_cast<std::ptrdiff_t>(index));
        }
        if (!net.entries.empty()) groups.push_back(std::move(net));
        if (!hosts.entries.empty()) {
            const int probe = HostPoll(hosts.entries.data(), static_cast<int>(hosts.entries.size()), 0);
            if (probe < 0) {
                *__error_nid_postfix() = -probe;
                return -1;
            }
            for (const auto& entry : hosts.entries) {
                if ((entry.revents & KernelSocketPoll::Unknown) != 0)
                    throw std::runtime_error("select: descriptor " + std::to_string(entry.descriptor) + " is neither a socket, a pipe, a file, a directory nor a kqueue");
            }
            groups.push_back(std::move(hosts));
        }
    }
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = limit == nullptr ? std::chrono::steady_clock::time_point::max()
        : KernelSocketSelect::DeadlineAfter(start, limit->seconds, limit->microseconds);
    for (;;) {
        int waitMilliseconds = 100;
        if (limit != nullptr) {
            const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
            const auto roundedMilliseconds = remaining > 0 ? remaining / 1000 + (remaining % 1000 != 0) : 0;
            waitMilliseconds = static_cast<int>(std::clamp<std::int64_t>(roundedMilliseconds, 0, 100));
        }
        if (devicesReady) waitMilliseconds = 0;
        if (groups.empty()) {
            if (waitMilliseconds > 0) PreciseSleepUntil(std::min(deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMilliseconds)));
        } else {
            const int slice = groups.size() == 1 ? waitMilliseconds : std::min(waitMilliseconds, SelectPollSliceMilliseconds);
            const auto sliceEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMilliseconds);
            for (;;) {
                int groupReady = 0;
                for (auto& group : groups) {
                    for (auto& entry : group.entries) entry.revents = 0;
                    const int result = group.poll(group.entries.data(), static_cast<int>(group.entries.size()), slice);
                    if (result < 0) {
                        *__error_nid_postfix() = -result;
                        return -1;
                    }
                    for (std::size_t index = 0; index < group.entries.size(); ++index) requested[group.slots[index]].revents = group.entries[index].revents;
                    groupReady += result;
                }
                if (groups.size() == 1 || groupReady > 0 || std::chrono::steady_clock::now() >= sliceEnd) break;
            }
        }
        if (std::any_of(requested.begin(), requested.end(), [](const auto& entry) { return (entry.revents & KernelSocketPoll::Unknown) != 0; })) {
            for (const auto& entry : requested) {
                if ((entry.revents & KernelSocketPoll::Unknown) == 0 || DescriptorIsOpen_nid_no_patch(entry.descriptor)) continue;
                *__error_nid_postfix() = GuestEbadf;
                return -1;
            }
            if (limit == nullptr) return select_nid_postfix(nfds, readfds, writefds, exceptfds, nullptr);
            const auto remaining = std::max<std::int64_t>(0, std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count());
            const GuestTimeval rest{remaining / 1000000, remaining % 1000000};
            return select_nid_postfix(nfds, readfds, writefds, exceptfds, &rest);
        }
        int ready = 0;
        for (const auto& entry : requested) {
            const bool readable = (entry.events & KernelSocketPoll::Readable) != 0 && (entry.revents & (KernelSocketPoll::Readable | KernelSocketPoll::HangUp | KernelSocketPoll::Error)) != 0;
            const bool writable = (entry.events & KernelSocketPoll::Writable) != 0 && (entry.revents & (KernelSocketPoll::Writable | KernelSocketPoll::Error)) != 0;
            const bool urgent = (entry.events & KernelSocketPoll::Urgent) != 0 && (entry.revents & KernelSocketPoll::Urgent) != 0;
            ready += static_cast<int>(readable) + static_cast<int>(writable) + static_cast<int>(urgent);
        }
        if (ready > 0) {
            for (const auto& entry : requested) {
                if ((entry.revents & (KernelSocketPoll::Readable | KernelSocketPoll::HangUp | KernelSocketPoll::Error)) == 0) ClearDescriptor(readfds, entry.descriptor);
                if ((entry.revents & (KernelSocketPoll::Writable | KernelSocketPoll::Error)) == 0) ClearDescriptor(writefds, entry.descriptor);
                if ((entry.revents & KernelSocketPoll::Urgent) == 0) ClearDescriptor(exceptfds, entry.descriptor);
            }
            ClearDescriptorsFrom(nfds, readfds, writefds, exceptfds);
            return ready;
        }
        if (limit != nullptr && std::chrono::steady_clock::now() >= deadline) {
            for (const auto& entry : requested) {
                ClearDescriptor(readfds, entry.descriptor);
                ClearDescriptor(writefds, entry.descriptor);
                ClearDescriptor(exceptfds, entry.descriptor);
            }
            ClearDescriptorsFrom(nfds, readfds, writefds, exceptfds);
            return 0;
        }
    }
}

}
