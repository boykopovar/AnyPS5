#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libScePosixForWebKit/GuestResolver.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <thread>

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Service database check failed at line %u\n", location.line());
        std::abort();
    }
}

bool NetworkPort(const GuestResolver::ServiceEntry* entry, std::uint16_t port) {
    unsigned char bytes[4];
    std::memcpy(bytes, &entry->port, sizeof(bytes));
    return bytes[0] == port >> 8 && bytes[1] == (port & 0xff) && bytes[2] == 0 && bytes[3] == 0;
}

int main() {
    auto* http = getservbyname_nid_postfix("http", "tcp");
    Require(http && std::strcmp(http->name, "http") == 0 && std::strcmp(http->protocol, "tcp") == 0);
    Require(NetworkPort(http, 80));
    Require(http->aliases && std::strcmp(http->aliases[0], "www") == 0);
    Require(std::strcmp(http->aliases[1], "www-http") == 0 && http->aliases[2] == nullptr);

    auto* https = getservbyname_nid_postfix("https", nullptr);
    Require(https && std::strcmp(https->name, "https") == 0 && std::strcmp(https->protocol, "tcp") == 0);
    Require(NetworkPort(https, 443) && https->aliases && https->aliases[0] == nullptr);

    auto* domain = getservbyname_nid_postfix("domain", "udp");
    Require(domain && std::strcmp(domain->name, "domain") == 0 && std::strcmp(domain->protocol, "udp") == 0);
    Require(NetworkPort(domain, 53));

    auto* www = getservbyname_nid_postfix("www", "sctp");
    Require(www && std::strcmp(www->name, "http") == 0 && std::strcmp(www->protocol, "sctp") == 0);
    Require(NetworkPort(www, 80));

    auto* mail = getservbyname_nid_postfix("mail", nullptr);
    Require(mail && std::strcmp(mail->name, "smtp") == 0 && NetworkPort(mail, 25));

    errno = 0;
    Require(getservbyname_nid_postfix("no-such-service", nullptr) == nullptr && errno == 0);
    Require(getservbyname_nid_postfix("domain", "sctp") == nullptr);
    Require(getservbyname_nid_postfix("ntp", "TCP") == nullptr);
    Require(getservbyname_nid_postfix("HTTP", "tcp") == nullptr);

    auto* ntp = getservbyname_nid_postfix("ntp", "udp");
    Require(ntp && NetworkPort(ntp, 123));
    std::thread([] {
        auto* ssh = getservbyname_nid_postfix("ssh", "tcp");
        Require(ssh && std::strcmp(ssh->name, "ssh") == 0 && NetworkPort(ssh, 22));
    }).join();
    Require(std::strcmp(ntp->name, "ntp") == 0 && std::strcmp(ntp->protocol, "udp") == 0 && NetworkPort(ntp, 123));
    return 0;
}
