#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <poll.h>
#endif

namespace {

constexpr int GuestEinval = 22;
constexpr int GuestEbadf = 9;
constexpr int SelectDescriptorLimit = 1024;

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

std::atomic<KernelSocketPoll::Poller> g_socketPoller{nullptr};

bool DescriptorSet(const void* set, int descriptor) {
    return set != nullptr && ((static_cast<const std::uint64_t*>(set)[descriptor / 64] >> (descriptor % 64)) & 1u) != 0;
}

void ClearDescriptor(void* set, int descriptor) {
    if (set != nullptr) static_cast<std::uint64_t*>(set)[descriptor / 64] &= ~(std::uint64_t{1} << (descriptor % 64));
}

struct RequestedDescriptor {
    int descriptor;
    short events;
    short revents;
};

struct HostPollDescriptor {
    int descriptor;
    short events;
    short revents;
};

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

int HostPoll(HostPollDescriptor* descriptors, std::size_t count, int timeoutMilliseconds) {
    std::vector<struct pollfd> native(count);
    for (std::size_t index = 0; index < count; ++index) {
        native[index].fd = descriptors[index].descriptor;
        native[index].events = descriptors[index].events;
        native[index].revents = 0;
    }
    const int result = ::poll(native.data(), static_cast<nfds_t>(count), timeoutMilliseconds);
    if (result < 0) return -1;
    for (std::size_t index = 0; index < count; ++index) {
        if ((native[index].revents & POLLNVAL) != 0) {
            errno = EBADF;
            return -1;
        }
        descriptors[index].revents = KernelEventFlags(native[index].revents);
    }
    return result;
}

#endif

}

extern "C" {

const char* APS5_VABI __inet_ntop_nid_postfix(int family, const void* source, char* destination, std::uint32_t capacity);
int APS5_VABI __inet_pton_nid_postfix(int family, const char* text, void* destination);

const char* APS5_VABI inet_ntop_nid_postfix(int af, const void* src, char* dst, uint32_t size) {
    return __inet_ntop_nid_postfix(af, src, dst, size);
}

int APS5_VABI inet_pton_nid_postfix(int af, const char* src, void* dst) {
    return __inet_pton_nid_postfix(af, src, dst);
}

int* APS5_VABI __error_nid_postfix();

void KernelSetSocketPoller_nid_no_patch(KernelSocketPoll::Poller poller) {
    g_socketPoller.store(poller);
}

int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
    const auto* limit = static_cast<const GuestTimeval*>(timeout);
    if (nfds < 0 || nfds > SelectDescriptorLimit || (limit != nullptr && (limit->seconds < 0 || limit->microseconds < 0 || limit->microseconds >= 1000000))) {
        *__error_nid_postfix() = GuestEinval;
        return -1;
    }
    std::vector<RequestedDescriptor> requested;
    std::vector<KernelSocketPoll::Entry> sockets;
    std::vector<std::size_t> socketSlots;
    std::vector<HostPollDescriptor> hosts;
    std::vector<std::size_t> hostSlots;
    for (int descriptor = 0; descriptor < nfds; ++descriptor) {
        short events = 0;
        if (DescriptorSet(readfds, descriptor)) events |= KernelSocketPoll::Readable;
        if (DescriptorSet(writefds, descriptor)) events |= KernelSocketPoll::Writable;
        if (DescriptorSet(exceptfds, descriptor)) events |= KernelSocketPoll::Urgent;
        if (events == 0) continue;
        if (descriptor < GuestSockets::FirstDescriptor) {
#ifdef _WIN32
            NotImplemented_nid_no_patch("select on descriptors that are not sockets");
#else
            hosts.push_back({descriptor, HostEventFlags(events), 0});
            hostSlots.push_back(requested.size());
#endif
        } else {
            sockets.push_back({descriptor, events, 0});
            socketSlots.push_back(requested.size());
        }
        requested.push_back({descriptor, events, 0});
    }
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = limit == nullptr ? std::chrono::steady_clock::time_point::max()
        : start + std::chrono::seconds(limit->seconds) + std::chrono::microseconds(limit->microseconds);
    for (;;) {
        int waitMilliseconds = 100;
        if (limit != nullptr) {
            const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
            waitMilliseconds = static_cast<int>(std::clamp<std::int64_t>((remaining + 999) / 1000, 0, 100));
        }
        if (requested.empty()) {
            if (waitMilliseconds > 0) std::this_thread::sleep_for(std::chrono::milliseconds(waitMilliseconds));
        } else {
#ifndef _WIN32
            if (!hosts.empty()) {
                for (auto& host : hosts) host.revents = 0;
                if (HostPoll(hosts.data(), hosts.size(), waitMilliseconds) < 0) {
                    *__error_nid_postfix() = errno;
                    return -1;
                }
                for (std::size_t index = 0; index < hosts.size(); ++index) requested[hostSlots[index]].revents = hosts[index].revents;
            }
#endif
            if (!sockets.empty()) {
                const auto poller = g_socketPoller.load();
                if (poller == nullptr) {
                    *__error_nid_postfix() = GuestEbadf;
                    return -1;
                }
                for (auto& socket : sockets) socket.revents = 0;
                const int wait = hosts.empty() ? waitMilliseconds : 0;
                const int result = poller(sockets.data(), static_cast<int>(sockets.size()), wait);
                if (result < 0) {
                    *__error_nid_postfix() = -result;
                    return -1;
                }
                for (std::size_t index = 0; index < sockets.size(); ++index) requested[socketSlots[index]].revents = sockets[index].revents;
            }
            int ready = 0;
            for (const auto& entry : requested) {
                if ((entry.revents & KernelSocketPoll::Unknown) != 0) {
                    *__error_nid_postfix() = GuestEbadf;
                    return -1;
                }
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
                return ready;
            }
        }
        if (limit != nullptr && std::chrono::steady_clock::now() >= deadline) {
            for (const auto& entry : requested) {
                ClearDescriptor(readfds, entry.descriptor);
                ClearDescriptor(writefds, entry.descriptor);
                ClearDescriptor(exceptfds, entry.descriptor);
            }
            return 0;
        }
    }
}

}
