#include "prx/libcurl/src/CurlSockets.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Socket/include/SocketBorrow.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

namespace CurlSockets {
struct Socket {
    curl_socket_t native = CURL_SOCKET_BAD;
    int descriptor = -1;
    std::shared_ptr<void> owner;
    Slot close;
    bool watched = false;
    bool borrowed = false;
    bool retire = true;
    ~Socket() {
        if (borrowed && retire) KernelReleaseBorrowedSocket_nid_no_patch(descriptor, &owner);
    }
};

namespace {
std::mutex socketsMutex;
std::unordered_map<curl_socket_t, std::shared_ptr<Socket>> connections;

int NativeClose(curl_socket_t socket) {
#ifdef _WIN32
    return ::closesocket(socket);
#else
    return ::close(socket);
#endif
}

struct NativeGuard {
    curl_socket_t socket;
    ~NativeGuard() { if (socket != CURL_SOCKET_BAD) NativeClose(socket); }
};

int GuestFamily(int family) {
    if (family == AF_INET) return 2;
    if (family == AF_INET6) return 28;
#ifdef AF_UNIX
    if (family == AF_UNIX) return 1;
#endif
    NotImplemented_nid_no_patch("libcurl: socket address family");
    return -1;
}

int HostFamily(int family) {
    if (family == 2) return AF_INET;
    if (family == 28) return AF_INET6;
#ifdef AF_UNIX
    if (family == 1) return AF_UNIX;
#endif
    NotImplemented_nid_no_patch("libcurl: guest socket address family");
    return -1;
}

struct GuestAddress {
    guest_curl_sockaddr value{};
    std::array<unsigned char, 128> extra{};
};
static_assert(offsetof(GuestAddress, extra) == sizeof(guest_curl_sockaddr));

void CopyAddressToGuest(const curl_sockaddr& native, GuestAddress& guest) {
    guest.value.family = GuestFamily(native.family);
    guest.value.socktype = native.socktype;
    guest.value.protocol = native.protocol;
    auto* bytes = reinterpret_cast<unsigned char*>(&guest) + offsetof(GuestAddress, value) + offsetof(guest_curl_sockaddr, addr);
    const auto* address = reinterpret_cast<const sockaddr*>(&native.addr);
    if (native.family == AF_INET) {
        guest.value.addrlen = 16;
        const auto* v4 = reinterpret_cast<const sockaddr_in*>(address);
        std::memcpy(bytes + 2, &v4->sin_port, 2);
        std::memcpy(bytes + 4, &v4->sin_addr, 4);
    } else if (native.family == AF_INET6) {
        guest.value.addrlen = 28;
        const auto* v6 = reinterpret_cast<const sockaddr_in6*>(address);
        std::memcpy(bytes + 2, &v6->sin6_port, 2);
        std::memcpy(bytes + 4, &v6->sin6_flowinfo, 4);
        std::memcpy(bytes + 8, &v6->sin6_addr, 16);
        std::memcpy(bytes + 24, &v6->sin6_scope_id, 4);
    } else {
        if (native.addrlen > sizeof(guest.value.addr) + guest.extra.size()) NotImplemented_nid_no_patch("libcurl: socket address size");
        guest.value.addrlen = native.addrlen;
        std::memcpy(bytes + 2, reinterpret_cast<const unsigned char*>(address) + 2, native.addrlen - 2);
    }
    bytes[0] = static_cast<unsigned char>(guest.value.addrlen);
    bytes[1] = static_cast<unsigned char>(guest.value.family);
}

void CopyAddressToHost(const GuestAddress& guest, curl_sockaddr& native) {
    const auto length = guest.value.addrlen;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&guest) + offsetof(GuestAddress, value) + offsetof(guest_curl_sockaddr, addr);
    if (length < 2 || length > 128) NotImplemented_nid_no_patch("libcurl: callback socket address size");
    if (bytes[1] != guest.value.family) NotImplemented_nid_no_patch("libcurl: inconsistent callback socket address");
    const int family = HostFamily(guest.value.family);
    if ((family == AF_INET && length != 16) || (family == AF_INET6 && length != 28))
        NotImplemented_nid_no_patch("libcurl: callback socket address length");
    native.family = family;
    native.socktype = guest.value.socktype;
    native.protocol = guest.value.protocol;
    auto* address = reinterpret_cast<sockaddr*>(&native.addr);
    std::memset(address, 0, length);
    address->sa_family = static_cast<decltype(address->sa_family)>(family);
#ifdef __APPLE__
    address->sa_len = static_cast<unsigned char>(length);
#endif
    if (family == AF_INET) {
        native.addrlen = sizeof(sockaddr_in);
        auto* v4 = reinterpret_cast<sockaddr_in*>(address);
        std::memcpy(&v4->sin_port, bytes + 2, 2);
        std::memcpy(&v4->sin_addr, bytes + 4, 4);
    } else if (family == AF_INET6) {
        native.addrlen = sizeof(sockaddr_in6);
        auto* v6 = reinterpret_cast<sockaddr_in6*>(address);
        std::memcpy(&v6->sin6_port, bytes + 2, 2);
        std::memcpy(&v6->sin6_flowinfo, bytes + 4, 4);
        std::memcpy(&v6->sin6_addr, bytes + 8, 16);
        std::memcpy(&v6->sin6_scope_id, bytes + 24, 4);
    } else {
        native.addrlen = length;
        std::memcpy(reinterpret_cast<unsigned char*>(address) + 2, bytes + 2, length - 2);
    }
}

std::shared_ptr<Socket> Borrow(curl_socket_t native, bool nonblocking) {
    int family = 0;
    int type = 0;
#ifdef _WIN32
    WSAPROTOCOL_INFOW information{};
    int length = sizeof(information);
    if (::getsockopt(native, SOL_SOCKET, SO_PROTOCOL_INFOW,
                     reinterpret_cast<char*>(&information), &length) != 0) return {};
    family = information.iAddressFamily;
    type = information.iSocketType;
#else
    sockaddr_storage address{};
    socklen_t length = sizeof(address);
    socklen_t typeLength = sizeof(int);
    if (::getsockname(native, reinterpret_cast<sockaddr*>(&address), &length) != 0 ||
        ::getsockopt(native, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&type), &typeLength) != 0) return {};
    family = address.ss_family;
#endif
    auto record = std::make_shared<Socket>();
    record->native = native;
    record->borrowed = true;
    record->descriptor = KernelBorrowSocketNative_nid_no_patch(static_cast<std::uintptr_t>(native),
                                                               GuestFamily(family), type, nonblocking, &record->owner);
    if (record->descriptor < 0) return {};
    return record;
}

std::shared_ptr<Socket> Connection(curl_socket_t native) {
    std::lock_guard lock(socketsMutex);
    const auto found = connections.find(native);
    return found == connections.end() ? nullptr : found->second;
}

bool Current(const Socket& socket) {
    std::shared_ptr<void> owner;
    std::uintptr_t native = 0;
    return KernelLookupSocketNative_nid_no_patch(socket.descriptor, &owner, &native) && owner.get() == socket.owner.get();
}

std::shared_ptr<Socket> View(Multi* multi, curl_socket_t native) {
    if (auto record = Connection(native)) return record;
    const auto found = multi->views.find(native);
    if (found != multi->views.end()) return found->second;
    auto record = Borrow(native, true);
    if (record) multi->views.emplace(native, record);
    return record;
}

void ReleaseGuest(const std::shared_ptr<Socket>& record) {
    if (!record || record->borrowed || !Current(*record)) return;
    if (record->close.function) {
        using Function = int (APS5_VABI*)(void*, int);
        reinterpret_cast<Function>(record->close.function)(record->close.data, record->descriptor);
    } else close_nid_postfix(record->descriptor);
}

curl_socket_t Open(void* opaque, curlsocktype purpose, curl_sockaddr* address) {
    std::shared_ptr<Socket> record;
    try {
        auto* context = static_cast<Easy*>(opaque);
        NativeGuard guard{CURL_SOCKET_BAD};
        if (context->open.function) {
            record = std::make_shared<Socket>();
            record->close = context->close;
            GuestAddress translated;
            CopyAddressToGuest(*address, translated);
            using Function = int (APS5_VABI*)(void*, int, guest_curl_sockaddr*);
            const int descriptor = reinterpret_cast<Function>(context->open.function)(context->open.data, purpose, &translated.value);
            if (descriptor == -1) return CURL_SOCKET_BAD;
            std::uintptr_t native = 0;
            if (!KernelLookupSocketNative_nid_no_patch(descriptor, &record->owner, &native)) return CURL_SOCKET_BAD;
            record->descriptor = descriptor;
            record->native = static_cast<curl_socket_t>(native);
            CopyAddressToHost(translated, *address);
        } else {
            guard.socket = ::socket(address->family, address->socktype, address->protocol);
            if (guard.socket == CURL_SOCKET_BAD) return CURL_SOCKET_BAD;
            record = Borrow(guard.socket, false);
            if (!record) return CURL_SOCKET_BAD;
        }
        record->close = context->close;
        {
            std::lock_guard lock(socketsMutex);
            connections.insert_or_assign(record->native, record);
        }
        guard.socket = CURL_SOCKET_BAD;
        return record->native;
    } catch (const std::bad_alloc&) {
        ReleaseGuest(record);
        return CURL_SOCKET_BAD;
    } catch (...) {
        ReleaseGuest(record);
        throw;
    }
}

int Close(void*, curl_socket_t native) {
    std::shared_ptr<Socket> record;
    {
        std::lock_guard lock(socketsMutex);
        const auto found = connections.find(native);
        if (found != connections.end()) {
            record = std::move(found->second);
            connections.erase(found);
        }
    }
    if (!record) return NativeClose(native);
    NativeGuard guard{record->borrowed ? native : CURL_SOCKET_BAD};
    if (record->close.function) {
        record->retire = false;
        using Function = int (APS5_VABI*)(void*, int);
        return reinterpret_cast<Function>(record->close.function)(record->close.data, record->descriptor);
    }
    if (!record->borrowed && Current(*record)) return close_nid_postfix(record->descriptor);
    return 0;
}

int Option(void* opaque, curl_socket_t native, curlsocktype purpose) {
    auto* context = static_cast<Easy*>(opaque);
    if (!context->option.function) return CURL_SOCKOPT_OK;
    auto record = Connection(native);
    if (!record || !Current(*record)) return CURL_SOCKOPT_ERROR;
    using Function = int (APS5_VABI*)(void*, int, int);
    return reinterpret_cast<Function>(context->option.function)(context->option.data, record->descriptor, purpose);
}

int SocketEvent(CURL* easy, curl_socket_t native, int action, void* opaque, void* socketData) {
    auto* context = static_cast<Multi*>(opaque);
    try {
        auto record = View(context, native);
        if (!record || !Current(*record)) return -1;
        record->watched = action != CURL_POLL_REMOVE;
        int result = 0;
        if (context->callback.function) {
            using Function = int (APS5_VABI*)(CURL*, int, int, void*, void*);
            result = reinterpret_cast<Function>(context->callback.function)(easy, record->descriptor, action, context->callback.data, socketData);
        }
        if (action == CURL_POLL_REMOVE) context->views.erase(native);
        return result;
    } catch (const std::bad_alloc&) {
        return -1;
    }
}

curl_socket_t Native(Multi* context, int descriptor, std::shared_ptr<void>& held) {
    std::uintptr_t native = 0;
    if (!KernelLookupSocketNative_nid_no_patch(descriptor, &held, &native)) return static_cast<curl_socket_t>(-2);
    for (const auto& [value, record] : context->views)
        if (record->descriptor == descriptor && record->owner.get() == held.get()) return value;
    {
        std::lock_guard lock(socketsMutex);
        for (const auto& [value, record] : connections)
            if (record->descriptor == descriptor && record->owner.get() == held.get()) return value;
        if (connections.contains(static_cast<curl_socket_t>(native))) return static_cast<curl_socket_t>(-2);
    }
    return static_cast<curl_socket_t>(native);
}

template<class TVisit>
bool ForEach(const fd_set& set, TVisit visit) {
#ifdef _WIN32
    for (u_int index = 0; index < set.fd_count; ++index)
        if (!visit(set.fd_array[index])) return false;
#else
    for (int descriptor = 0; descriptor < FD_SETSIZE; ++descriptor)
        if (FD_ISSET(descriptor, &set) && !visit(descriptor)) return false;
#endif
    return true;
}

bool Contains(const fd_set& set, curl_socket_t descriptor) {
#ifdef _WIN32
    return FD_ISSET(descriptor, &set) != 0;
#else
    return descriptor >= 0 && descriptor < FD_SETSIZE && FD_ISSET(descriptor, &set);
#endif
}

bool Add(guest_fd_set* set, int descriptor) {
    if (descriptor < 0 || descriptor >= 1024) return false;
    if (set) set->fds_bits[descriptor / 64] |= std::uint64_t{1} << (descriptor % 64);
    return true;
}
}

bool Add_nid_no_patch(guest_fd_set* set, int descriptor) {
    return Add(set, descriptor);
}

int ObserveMode_nid_no_patch(curl_socket_t socket, int nonblocking) {
    const auto result = CurlHostNonblock_nid_no_patch(socket, nonblocking);
    const auto record = Connection(socket);
    if (record && Current(*record)) {
        KernelNoteSocketConnect_nid_no_patch(record->descriptor, &record->owner);
        if (result == 0) fcntl_nid_postfix(record->descriptor, 4, nonblocking ? 6 : 2);
    }
    return result;
}

void BindEasy_nid_no_patch(CURL* easy, Easy* context) {
    ::curl_easy_setopt(easy, CURLOPT_OPENSOCKETFUNCTION, Open);
    ::curl_easy_setopt(easy, CURLOPT_OPENSOCKETDATA, context);
    ::curl_easy_setopt(easy, CURLOPT_CLOSESOCKETFUNCTION, Close);
    ::curl_easy_setopt(easy, CURLOPT_CLOSESOCKETDATA, nullptr);
    ::curl_easy_setopt(easy, CURLOPT_SOCKOPTFUNCTION, Option);
    ::curl_easy_setopt(easy, CURLOPT_SOCKOPTDATA, context);
}

CURLcode SetEasy_nid_no_patch(CURL* easy, Easy* context, CURLoption option, void* value) {
    switch (option) {
    case CURLOPT_OPENSOCKETFUNCTION: context->open.function = value; break;
    case CURLOPT_OPENSOCKETDATA: context->open.data = value; break;
    case CURLOPT_CLOSESOCKETFUNCTION: context->close.function = value; break;
    case CURLOPT_CLOSESOCKETDATA: context->close.data = value; break;
    case CURLOPT_SOCKOPTFUNCTION: context->option.function = value; break;
    case CURLOPT_SOCKOPTDATA: context->option.data = value; break;
    default: return CURLE_UNKNOWN_OPTION;
    }
    BindEasy_nid_no_patch(easy, context);
    return CURLE_OK;
}

void BindMulti_nid_no_patch(CURLM* multi, Multi* context) {
    ::curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, SocketEvent);
    ::curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, context);
}

CURLMcode SetMulti_nid_no_patch(CURLM* multi, Multi* context, CURLMoption option, void* value) {
    if (option == CURLMOPT_SOCKETFUNCTION) context->callback.function = value;
    else if (option == CURLMOPT_SOCKETDATA) context->callback.data = value;
    else return CURLM_UNKNOWN_OPTION;
    BindMulti_nid_no_patch(multi, context);
    return CURLM_OK;
}

CURLMcode Action_nid_no_patch(CURLM* multi, Multi* context, int descriptor, int events, int* running, bool legacy) {
    GuestArena::HostWrite write(running, running ? sizeof(*running) : 0);
    std::shared_ptr<void> held;
    const auto native = descriptor == -1 ? CURL_SOCKET_TIMEOUT : Native(context, descriptor, held);
    const auto result = legacy ? ::curl_multi_socket(multi, native, running) : ::curl_multi_socket_action(multi, native, events, running);
    Refresh_nid_no_patch(multi, context);
    return result;
}

CURLMcode Assign_nid_no_patch(CURLM* multi, Multi* context, int descriptor, void* value) {
    std::shared_ptr<void> held;
    return ::curl_multi_assign(multi, Native(context, descriptor, held), value);
}

CURLMcode Fdset_nid_no_patch(CURLM* multi, Multi* context, guest_fd_set* read, guest_fd_set* write, guest_fd_set*, int* maximum) {
    GuestArena::HostWrite readAccess(read, read ? sizeof(*read) : 0);
    GuestArena::HostWrite writeAccess(write, write ? sizeof(*write) : 0);
    GuestArena::HostWrite maximumAccess(maximum, sizeof(*maximum));
    try {
        fd_set readers, writers, exceptions;
        FD_ZERO(&readers);
        FD_ZERO(&writers);
        FD_ZERO(&exceptions);
        int ignored = -1;
        const auto result = ::curl_multi_fdset(multi, &readers, &writers, &exceptions, &ignored);
        if (result != CURLM_OK) return result;
        int highest = -1;
        const auto project = [&](const fd_set& native, guest_fd_set* target) {
            return ForEach(native, [&](curl_socket_t descriptor) {
                const auto record = View(context, descriptor);
                if (!record) return false;
                if (Add(target, record->descriptor)) highest = std::max(highest, record->descriptor);
                return true;
            });
        };
        if (!project(readers, read) || !project(writers, write)) return CURLM_OUT_OF_MEMORY;
        *maximum = highest;
        Refresh_nid_no_patch(multi, context);
        return CURLM_OK;
    } catch (const std::bad_alloc&) {
        return CURLM_OUT_OF_MEMORY;
    }
}

CURLMcode Wait_nid_no_patch(CURLM* multi, guest_curl_waitfd* extra, unsigned count, int timeout, int* ready, bool poll) {
    if (timeout < 0) return CURLM_BAD_FUNCTION_ARGUMENT;
    if (count && !extra) NotImplemented_nid_no_patch("libcurl: null extra socket array");
    GuestArena::HostWrite extraAccess(extra, static_cast<std::size_t>(count) * sizeof(*extra));
    GuestArena::HostWrite readyAccess(ready, ready ? sizeof(*ready) : 0);
    try {
        std::vector<curl_waitfd> native;
        std::vector<unsigned> indices;
        std::vector<std::shared_ptr<void>> held;
        std::vector<std::unique_ptr<NativeGuard>> files;
        unsigned invalid = 0;
        native.reserve(count);
        indices.reserve(count);
        for (unsigned index = 0; index < count; ++index) {
            std::shared_ptr<void> owner;
            std::uintptr_t socket = 0;
            curl_socket_t descriptor = CURL_SOCKET_BAD;
            if (extra[index].fd < 0) descriptor = static_cast<curl_socket_t>(extra[index].fd);
            else if (KernelLookupSocketNative_nid_no_patch(extra[index].fd, &owner, &socket)) {
                descriptor = static_cast<curl_socket_t>(socket);
                held.push_back(std::move(owner));
            } else {
                GuestPollDescriptor probe{extra[index].fd, static_cast<short>(extra[index].events & 7), 0};
                if (poll_nid_postfix(&probe, 1, 0) < 0) NotImplemented_nid_no_patch("libcurl: extra descriptor probe failure");
                if (probe.revents & 0x20) {
                    ++invalid;
                    continue;
                }
#ifdef _WIN32
                NotImplemented_nid_no_patch("libcurl: non-socket extra descriptors on Windows");
#else
                struct stat status{};
                if (::fstat(extra[index].fd, &status) != 0) NotImplemented_nid_no_patch("libcurl: extra descriptor identity failure");
                if (!S_ISFIFO(status.st_mode) && !S_ISREG(status.st_mode) && !S_ISCHR(status.st_mode))
                    NotImplemented_nid_no_patch("libcurl: virtual extra descriptor");
                auto lease = std::make_unique<NativeGuard>(CURL_SOCKET_BAD);
                lease->socket = ::fcntl(extra[index].fd, F_DUPFD_CLOEXEC, 0);
                if (lease->socket == CURL_SOCKET_BAD) NotImplemented_nid_no_patch("libcurl: extra descriptor lease failure");
                descriptor = lease->socket;
                files.push_back(std::move(lease));
#endif
            }
            native.push_back({descriptor, extra[index].events, extra[index].revents});
            indices.push_back(index);
        }
        if (invalid)
            for (auto& descriptor : native) descriptor.revents = 0;
        int nativeReady = 0;
        const auto result = poll ? ::curl_multi_poll(multi, native.data(), static_cast<unsigned>(native.size()), invalid ? 0 : timeout, &nativeReady)
                                 : ::curl_multi_wait(multi, native.data(), static_cast<unsigned>(native.size()), invalid ? 0 : timeout, &nativeReady);
        if (result == CURLM_OK) {
            if (invalid || nativeReady > 0)
                for (unsigned index = 0; index < count; ++index) extra[index].revents = 0;
            for (std::size_t index = 0; index < native.size(); ++index) extra[indices[index]].revents = native[index].revents;
            if (ready) *ready = nativeReady + static_cast<int>(invalid);
        }
        return result;
    } catch (const std::bad_alloc&) {
        return CURLM_OUT_OF_MEMORY;
    }
}

CURLcode Active_nid_no_patch(CURL* easy, CURLINFO info, void* output) {
    if (!output) return CURLE_UNKNOWN_OPTION;
    GuestArena::HostWrite write(output, info == CURLINFO_LASTSOCKET ? sizeof(std::int64_t) : sizeof(int));
    curl_socket_t native = CURL_SOCKET_BAD;
    const auto result = ::curl_easy_getinfo(easy, CURLINFO_ACTIVESOCKET, &native);
    if (result != CURLE_OK) return result;
    int descriptor = -1;
    if (native != CURL_SOCKET_BAD) {
        const auto record = Connection(native);
        if (!record || !Current(*record)) NotImplemented_nid_no_patch("libcurl: unregistered active socket");
        descriptor = record->descriptor;
    }
    if (info == CURLINFO_LASTSOCKET) *static_cast<std::int64_t*>(output) = descriptor;
    else *static_cast<int*>(output) = descriptor;
    return CURLE_OK;
}

void Refresh_nid_no_patch(CURLM* multi, Multi* context) {
    if (context->views.empty()) return;
    fd_set readers, writers, exceptions;
    FD_ZERO(&readers);
    FD_ZERO(&writers);
    FD_ZERO(&exceptions);
    int maximum = -1;
    if (::curl_multi_fdset(multi, &readers, &writers, &exceptions, &maximum) != CURLM_OK) return;
    for (auto it = context->views.begin(); it != context->views.end();) {
        if (!it->second->watched && !Contains(readers, it->first) && !Contains(writers, it->first)) it = context->views.erase(it);
        else ++it;
    }
}
}

extern "C" int CurlSocketNonblock_nid_no_patch(curl_socket_t socket, int nonblocking) {
    return CurlSockets::ObserveMode_nid_no_patch(socket, nonblocking);
}
