#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <cstdint>
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <new>
#include <string>
#include <type_traits>
#include <vector>
#include "GuestResolver.hpp"

extern "C" int* APS5_VABI sceNetErrnoLoc();

namespace {
struct HostLookup {
    GuestResolver::HostEntry entry{};
    std::string name;
    std::vector<std::string> aliasNames;
    std::vector<char*> aliases;
    std::vector<std::string> addressBytes;
    std::vector<char*> addresses;
};

struct ServiceRecord {
    const char* name;
    std::uint16_t port;
    const char* protocol;
    std::array<const char*, 2> aliases;
};

constexpr ServiceRecord Services[]{
    {"ftp-data", 20, "tcp", {}}, {"ftp-data", 20, "udp", {}}, {"ftp-data", 20, "sctp", {}},
    {"ftp", 21, "tcp", {}}, {"ftp", 21, "udp", {}}, {"ftp", 21, "sctp", {}},
    {"ssh", 22, "tcp", {}}, {"ssh", 22, "udp", {}}, {"ssh", 22, "sctp", {}},
    {"telnet", 23, "tcp", {}}, {"telnet", 23, "udp", {}},
    {"smtp", 25, "tcp", {"mail"}}, {"smtp", 25, "udp", {"mail"}},
    {"domain", 53, "tcp", {}}, {"domain", 53, "udp", {}},
    {"tftp", 69, "tcp", {}}, {"tftp", 69, "udp", {}},
    {"http", 80, "tcp", {"www", "www-http"}}, {"http", 80, "udp", {"www", "www-http"}},
    {"http", 80, "sctp", {"www", "www-http"}},
    {"pop3", 110, "tcp", {}}, {"pop3", 110, "udp", {}},
    {"ntp", 123, "tcp", {}}, {"ntp", 123, "udp", {}},
    {"imap", 143, "tcp", {"imap2", "imap4"}}, {"imap", 143, "udp", {"imap2", "imap4"}},
    {"https", 443, "tcp", {}}, {"https", 443, "udp", {}}, {"https", 443, "sctp", {}},
    {"rtsp", 554, "tcp", {}}, {"rtsp", 554, "udp", {}},
    {"submission", 587, "tcp", {}}, {"submission", 587, "udp", {}},
    {"imaps", 993, "tcp", {}}, {"imaps", 993, "udp", {}},
    {"pop3s", 995, "tcp", {"spop3"}}, {"pop3s", 995, "udp", {"spop3"}},
};

constexpr std::size_t ServiceNameSize = 11;
constexpr std::size_t ServiceAliasSize = 9;
constexpr std::size_t ServiceProtocolSize = 5;
constexpr std::size_t ServiceAliasCount = std::tuple_size_v<decltype(ServiceRecord::aliases)>;

template<typename TSelect>
constexpr std::size_t LongestServiceText(TSelect select) {
    std::size_t longest = 0;
    for (const auto& service : Services)
        for (const char* text : select(service))
            if (text) longest = std::max(longest, std::char_traits<char>::length(text));
    return longest;
}

static_assert(ServiceNameSize == LongestServiceText([](const ServiceRecord& record) { return std::array{record.name}; }) + 1);
static_assert(ServiceAliasSize == LongestServiceText([](const ServiceRecord& record) { return record.aliases; }) + 1);
static_assert(ServiceProtocolSize == LongestServiceText([](const ServiceRecord& record) { return std::array{record.protocol}; }) + 1);

struct ServiceLookup {
    GuestResolver::ServiceEntry entry;
    std::array<char, ServiceNameSize> name;
    std::array<char, ServiceProtocolSize> protocol;
    std::array<std::array<char, ServiceAliasSize>, ServiceAliasCount> aliasNames;
    std::array<char*, ServiceAliasCount + 1> aliases;
};
static_assert(std::is_trivially_destructible_v<ServiceLookup>);

template<std::size_t Size>
char* CopyServiceText(std::array<char, Size>& target, const char* text) {
    std::memcpy(target.data(), text, std::strlen(text) + 1);
    return target.data();
}

bool NumericAddress(const char* name, std::array<unsigned char, 4>& address) {
    std::array<unsigned char, 3> parts{};
    std::size_t count = 0;
    std::uint64_t value;
    bool digit;
    for (;;) {
        if (*name < '0' || *name > '9') return false;
        value = 0;
        int base = 10;
        digit = false;
        if (*name == '0') {
            ++name;
            if (*name == 'x' || *name == 'X') { base = 16; ++name; }
            else { base = 8; digit = true; }
        }
        for (;;) {
            const int next = *name >= '0' && *name <= '9' ? *name - '0' :
                *name >= 'a' && *name <= 'f' ? *name - 'a' + 10 :
                *name >= 'A' && *name <= 'F' ? *name - 'A' + 10 : -1;
            if (next < 0 || next >= base) break;
            value = value * base + next;
            digit = true;
            ++name;
        }
        if (*name != '.') break;
        if (count == parts.size() || value > 255) return false;
        parts[count++] = static_cast<unsigned char>(value);
        ++name;
    }
    if (!digit) return false;
    if (*name != '\0' && *name != ' ' && *name != '\t' && *name != '\n' &&
        *name != '\r' && *name != '\f' && *name != '\v') return false;
    if (count && value >= (std::uint64_t{1} << (8 * (4 - count)))) return false;
    auto result = static_cast<std::uint32_t>(value);
    for (std::size_t i = 0; i < count; ++i)
        result |= static_cast<std::uint32_t>(parts[i]) << (24 - 8 * i);
    for (std::size_t i = 0; i < address.size(); ++i)
        address[i] = static_cast<unsigned char>(result >> (24 - 8 * i));
    return true;
}

bool Ready() {
#ifdef _WIN32
    static const int status = [] { WSADATA data{}; return WSAStartup(MAKEWORD(2, 2), &data); }();
    return status == 0;
#else
    return true;
#endif
}
int GuestError(int error) {
    if (error == 0) return 0;
    if (error == EAI_AGAIN) return 2;
    if (error == EAI_BADFLAGS) return 3;
    if (error == EAI_FAMILY) return 5;
    if (error == EAI_MEMORY) return 6;
    if (error == EAI_NONAME) return 8;
    if (error == EAI_SERVICE) return 9;
    if (error == EAI_SOCKTYPE) return 10;
#ifdef _WIN32
    if (error == WSAEFAULT) return 14;
#else
    if (error == EAI_OVERFLOW) return 14;
#endif
    return 4;
}
}

extern "C" {
GuestResolver::HostEntry* APS5_VABI gethostbyname_nid_postfix(const char* name) {
    if (!name || !Ready()) { *sceNetErrnoLoc() = 3; return nullptr; }
    try {
        std::array<unsigned char, 4> numericAddress{};
        char* numericAliases[]{nullptr};
        char* numericAddresses[]{reinterpret_cast<char*>(numericAddress.data()), nullptr};
        hostent nativeEntry{const_cast<char*>(name), numericAliases, AF_INET, 4, numericAddresses};
        hostent* native = &nativeEntry;
        const bool numeric = NumericAddress(name, numericAddress);
#ifdef _WIN32
        if (!numeric) native = ::gethostbyname(name);
        if (!native) {
            const auto error = WSAGetLastError();
            *sceNetErrnoLoc() = error == WSAHOST_NOT_FOUND ? 1 : error == WSATRY_AGAIN ? 2 : error == WSANO_DATA ? 4 : 3;
            return nullptr;
        }
#else
        std::vector<char> buffer;
        int hostError = 0;
        int error = 0;
        if (!numeric) {
#ifdef __APPLE__
            native = ::gethostbyname(name);
            if (native == nullptr) {
                hostError = h_errno;
                error = hostError == TRY_AGAIN ? EAGAIN : 0;
            }
#else
            buffer.resize(1024);
            while ((error = ::gethostbyname_r(name, &nativeEntry, buffer.data(), buffer.size(), &native, &hostError)) == ERANGE)
                buffer.resize(buffer.size() * 2);
#endif
        }
        if (error != 0 || !native) {
            *sceNetErrnoLoc() = hostError != 0 ? hostError : 3;
            if (hostError == -1) errno = error == EAGAIN ? 35 : error;
            return nullptr;
        }
#endif
        struct LookupTag {};
        auto& lookup = HostThreadLocal<HostLookup, LookupTag>();
        lookup.name = native->h_name;
        lookup.aliasNames.clear();
        lookup.aliases.clear();
        lookup.addressBytes.clear();
        lookup.addresses.clear();
        for (auto** alias = native->h_aliases; *alias; ++alias) lookup.aliasNames.emplace_back(*alias);
        for (auto& alias : lookup.aliasNames) lookup.aliases.push_back(alias.data());
        lookup.aliases.push_back(nullptr);
        for (auto** address = native->h_addr_list; *address; ++address)
            lookup.addressBytes.emplace_back(*address, native->h_length);
        for (auto& address : lookup.addressBytes) lookup.addresses.push_back(address.data());
        lookup.addresses.push_back(nullptr);
        lookup.entry = {lookup.name.data(), lookup.aliases.data(), native->h_addrtype == AF_INET6 ? 28 : 2,
            native->h_length, lookup.addresses.data()};
        if (numeric) *sceNetErrnoLoc() = 0;
        return &lookup.entry;
    } catch (const std::bad_alloc&) {
        *sceNetErrnoLoc() = -1;
        errno = ENOMEM;
        return nullptr;
    }
}

GuestResolver::ServiceEntry* APS5_VABI getservbyname_nid_postfix(const char* name, const char* protocol) {
    for (const auto& service : Services) {
        bool named = std::strcmp(name, service.name) == 0;
        for (const char* alias : service.aliases)
            named = named || (alias && std::strcmp(name, alias) == 0);
        if (!named || (protocol && std::strcmp(protocol, service.protocol) != 0)) continue;
        static thread_local ServiceLookup lookup;
        std::size_t count = 0;
        for (const char* alias : service.aliases) {
            if (!alias) continue;
            lookup.aliases[count] = CopyServiceText(lookup.aliasNames[count], alias);
            ++count;
        }
        lookup.aliases[count] = nullptr;
        lookup.entry = {CopyServiceText(lookup.name, service.name), lookup.aliases.data(), htons(service.port),
            CopyServiceText(lookup.protocol, service.protocol)};
        return &lookup.entry;
    }
    return nullptr;
}

void APS5_VABI freeaddrinfo_nid_postfix(GuestResolver::AddressInfo* first) {
    while (first) {
        auto* next = first->next;
        std::free(first->canonicalName);
        std::free(first->address);
        std::free(first);
        first = next;
    }
}

int APS5_VABI getaddrinfo_nid_postfix(const char* node, const char* service,
    const GuestResolver::AddressInfo* hints, GuestResolver::AddressInfo** output) {
    if (!output) return 4;
    *output = nullptr;
    addrinfo nativeHints{};
    if (hints) {
        if (hints->addressLength || hints->address || hints->canonicalName || hints->next) return 12;
        if ((hints->flags & ~0xd0f) != 0) return 3;
        if (hints->family != 0 && hints->family != 2 && hints->family != 28) return 5;
        if (hints->socketType != 0 && hints->socketType != 1 && hints->socketType != 2) return 10;
        nativeHints.ai_family = hints->family == 28 ? AF_INET6 : hints->family == 2 ? AF_INET : AF_UNSPEC;
        nativeHints.ai_socktype = hints->socketType == 1 ? SOCK_STREAM : hints->socketType == 2 ? SOCK_DGRAM : 0;
        nativeHints.ai_protocol = hints->protocol;
        if (hints->flags & 1) nativeHints.ai_flags |= AI_PASSIVE;
        if (hints->flags & 2) nativeHints.ai_flags |= AI_CANONNAME;
        if (hints->flags & 4) nativeHints.ai_flags |= AI_NUMERICHOST;
        if (hints->flags & 8) nativeHints.ai_flags |= AI_NUMERICSERV;
        if (hints->flags & 0x100) nativeHints.ai_flags |= AI_ALL;
        if (hints->flags & 0x400) nativeHints.ai_flags |= AI_ADDRCONFIG;
        if (hints->flags & 0x800) nativeHints.ai_flags |= AI_V4MAPPED;
    }
    if (!Ready()) return 4;
    addrinfo* native = nullptr;
    const auto error = ::getaddrinfo(node, service, &nativeHints, &native);
    if (error) return GuestError(error);
    GuestResolver::AddressInfo* first = nullptr;
    auto** tail = &first;
    int failure = 0;
    for (auto* item = native; item; item = item->ai_next) {
        if (item->ai_family != AF_INET && item->ai_family != AF_INET6) { failure = 5; break; }
        auto* converted = static_cast<GuestResolver::AddressInfo*>(std::calloc(1, sizeof(GuestResolver::AddressInfo)));
        if (!converted) { failure = 6; break; }
        *tail = converted;
        tail = &converted->next;
        converted->flags = hints ? hints->flags : 0;
        converted->family = item->ai_family == AF_INET ? 2 : 28;
        converted->socketType = item->ai_socktype == SOCK_STREAM ? 1 : item->ai_socktype == SOCK_DGRAM ? 2 : 0;
        converted->protocol = item->ai_protocol;
        converted->addressLength = item->ai_family == AF_INET ? 16 : 28;
        auto* bytes = static_cast<unsigned char*>(std::calloc(1, converted->addressLength));
        converted->address = bytes;
        if (!bytes) { failure = 6; break; }
        bytes[0] = static_cast<unsigned char>(converted->addressLength);
        bytes[1] = static_cast<unsigned char>(converted->family);
        if (item->ai_family == AF_INET) {
            const auto* v4 = reinterpret_cast<const sockaddr_in*>(item->ai_addr);
            std::memcpy(bytes + 2, &v4->sin_port, 2);
            std::memcpy(bytes + 4, &v4->sin_addr, 4);
        } else {
            const auto* v6 = reinterpret_cast<const sockaddr_in6*>(item->ai_addr);
            std::memcpy(bytes + 2, &v6->sin6_port, 2);
            std::memcpy(bytes + 4, &v6->sin6_flowinfo, 4);
            std::memcpy(bytes + 8, &v6->sin6_addr, 16);
            std::memcpy(bytes + 24, &v6->sin6_scope_id, 4);
        }
        if (item->ai_canonname) {
            const auto size = std::strlen(item->ai_canonname) + 1;
            converted->canonicalName = static_cast<char*>(std::malloc(size));
            if (!converted->canonicalName) { failure = 6; break; }
            std::memcpy(converted->canonicalName, item->ai_canonname, size);
        }
    }
    ::freeaddrinfo(native);
    if (failure) { freeaddrinfo_nid_postfix(first); return failure; }
    *output = first;
    return 0;
}

int APS5_VABI getnameinfo_nid_postfix(const void* address, std::uint32_t length,
    char* host, std::uint32_t hostLength, char* service, std::uint32_t serviceLength, int flags) {
    // PS5 sockaddr begins with byte-sized length and family. The host's does not.
    if (!address || length < 2) return 5;
    if ((!host && hostLength) || (!service && serviceLength)) return 4;
    if ((flags & ~0x1f) != 0) return 3;
    const auto* bytes = static_cast<const unsigned char*>(address);
    sockaddr_storage native{};
    socklen_t nativeLength;
    if (bytes[1] == 2) {
        if (length < 16 || bytes[0] != 16) return 5;
        auto& v4 = reinterpret_cast<sockaddr_in&>(native);
        v4.sin_family = AF_INET;
        std::memcpy(&v4.sin_port, bytes + 2, 2);
        std::memcpy(&v4.sin_addr, bytes + 4, 4);
        nativeLength = sizeof(v4);
    } else if (bytes[1] == 28) {
        if (length < 28 || bytes[0] != 28) return 5;
        auto& v6 = reinterpret_cast<sockaddr_in6&>(native);
        v6.sin6_family = AF_INET6;
        std::memcpy(&v6.sin6_port, bytes + 2, 2);
        std::memcpy(&v6.sin6_flowinfo, bytes + 4, 4);
        std::memcpy(&v6.sin6_addr, bytes + 8, 16);
        std::memcpy(&v6.sin6_scope_id, bytes + 24, 4);
        nativeLength = sizeof(v6);
    } else return 5;
    int nativeFlags = 0;
    if (flags & 1) nativeFlags |= NI_NOFQDN;
    if (flags & 2) nativeFlags |= NI_NUMERICHOST;
    if (flags & 4) nativeFlags |= NI_NAMEREQD;
    if (flags & 8) nativeFlags |= NI_NUMERICSERV;
    if (flags & 16) nativeFlags |= NI_DGRAM;
    if (!Ready()) return 4;
    // Stage results so host-specific short-buffer errors do not leak into the
    // guest ABI, and neither guest output is partially modified on overflow.
    char resolvedHost[NI_MAXHOST]{};
    char resolvedService[NI_MAXSERV]{};
    const bool wantHost = host && hostLength;
    const bool wantService = service && serviceLength;
    const int error = ::getnameinfo(reinterpret_cast<const sockaddr*>(&native), nativeLength,
        wantHost ? resolvedHost : nullptr, wantHost ? sizeof(resolvedHost) : 0,
        wantService ? resolvedService : nullptr, wantService ? sizeof(resolvedService) : 0, nativeFlags);
    if (error) return GuestError(error);
    if ((wantHost && std::strlen(resolvedHost) >= hostLength) ||
        (wantService && std::strlen(resolvedService) >= serviceLength)) return 14;
    if (wantHost) std::memcpy(host, resolvedHost, std::strlen(resolvedHost) + 1);
    if (wantService) std::memcpy(service, resolvedService, std::strlen(resolvedService) + 1);
    return 0;
}

const char* APS5_VABI gai_strerror_nid_postfix(int error) {
    static const char* messages[] = {
        "Success", "Address family for hostname not supported", "Temporary failure in name resolution",
        "Invalid value for ai_flags", "Non-recoverable failure in name resolution", "ai_family not supported",
        "Memory allocation failure", "No address associated with hostname", "hostname nor servname provided, or not known",
        "servname not supported for ai_socktype", "ai_socktype not supported", "System error returned in errno",
        "Invalid value for hints", "Resolved protocol is unknown", "Argument buffer overflow"
    };
    if (error < 0 || error >= static_cast<int>(sizeof(messages) / sizeof(messages[0])))
        return "Unknown error";
    return messages[error];
}
}
