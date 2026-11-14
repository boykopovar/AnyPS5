#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libScePosixForWebKit/GuestResolver.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <netdb.h>
#include <sys/socket.h>

namespace {
int hostResult = EAI_SYSTEM;
int hostError = EMFILE;
int calls = 0;

void Require(bool condition, const char* operation, int result, int expectedResult, int expectedError) {
    if (condition) return;
    std::fprintf(stderr, "%s: result %d (expected %d), errno %d (expected %d)\n",
                 operation, result, expectedResult, errno, expectedError);
    std::abort();
}

int Fail() {
    ++calls;
    if (hostResult == EAI_SYSTEM) errno = hostError;
    return hostResult;
}
}

extern "C" int __wrap_getaddrinfo(const char*, const char*, const addrinfo*, addrinfo**) {
    return Fail();
}

extern "C" int __wrap_getnameinfo(const sockaddr*, socklen_t, char*, socklen_t, char*, socklen_t, int) {
    return Fail();
}

int main() {
    struct Scenario {
        int hostResult;
        int hostError;
        int guestResult;
        int guestError;
    };
    const Scenario scenarios[]{
        {EAI_SYSTEM, EMFILE, 11, 24},
        {EAI_SYSTEM, EAGAIN, 11, 35},
        {EAI_SYSTEM, ELOOP, 11, 62},
        {EAI_SYSTEM, ENOMEM, 11, 12},
        {EAI_SYSTEM, EACCES, 11, 13},
        {EAI_SYSTEM, ECONNREFUSED, 11, 61},
        {EAI_SYSTEM, ENETUNREACH, 11, 51},
        {EAI_SYSTEM, ETIMEDOUT, 11, 60},
        {EAI_SYSTEM, EOVERFLOW, 11, 84},
        {EAI_AGAIN, 0, 2, 77},
        {EAI_FAIL, 0, 4, 77},
        {EAI_NONAME, 0, 8, 77},
    };
    const std::array<unsigned char, 16> address{16, 2, 0, 80, 127, 0, 0, 1};
    for (const auto& scenario : scenarios) {
        hostResult = scenario.hostResult;
        hostError = scenario.hostError;
        auto* output = reinterpret_cast<GuestResolver::AddressInfo*>(std::uintptr_t{1});
        int before = calls;
        errno = 77;
        int result = getaddrinfo_nid_postfix("127.0.0.1", "80", nullptr, &output);
        Require(result == scenario.guestResult && errno == scenario.guestError && !output && calls == before + 1,
                "getaddrinfo", result, scenario.guestResult, scenario.guestError);
        std::array<char, 64> host;
        std::array<char, 64> service;
        host.fill('H');
        service.fill('S');
        const auto originalHost = host;
        const auto originalService = service;
        before = calls;
        errno = 77;
        result = getnameinfo_nid_postfix(address.data(), address.size(), host.data(), host.size(),
                                       service.data(), service.size(), 2 | 8);
        Require(result == scenario.guestResult && errno == scenario.guestError && calls == before + 1 &&
                host == originalHost && service == originalService,
                "getnameinfo", result, scenario.guestResult, scenario.guestError);
    }
    std::printf("%zu resolver error cases passed\n", std::size(scenarios) * 2);
}
