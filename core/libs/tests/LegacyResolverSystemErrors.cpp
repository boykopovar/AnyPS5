#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <netdb.h>

namespace {
int resolverError = NETDB_INTERNAL;
int nativeError = ELOOP;
int calls = 0;

void Require(bool condition, int expectedError) {
    if (condition) return;
    std::fprintf(stderr, "legacy resolver: errno %d (expected %d), resolver error %d (expected %d)\n",
                 errno, expectedError, *sceNetErrnoLoc(), resolverError);
    std::abort();
}
}

extern "C" int gethostbyname_r(const char*, hostent*, char*, std::size_t, hostent** output, int* error) {
    ++calls;
    *output = nullptr;
    *error = resolverError;
    errno = 77;
    return resolverError == NETDB_INTERNAL ? nativeError : 0;
}

int main() {
    struct ErrorCase {
        int host;
        int guest;
    };
    const ErrorCase cases[]{
        {EMFILE, 24}, {EAGAIN, 35}, {ELOOP, 62}, {ENOMEM, 12}, {EACCES, 13},
        {ECONNREFUSED, 61}, {ENETUNREACH, 51}, {ETIMEDOUT, 60}, {EOVERFLOW, 84},
    };
    for (const auto& entry : cases) {
        nativeError = entry.host;
        errno = 77;
        *sceNetErrnoLoc() = 99;
        const int before = calls;
        auto* result = gethostbyname_nid_postfix("localhost");
        Require(!result && calls == before + 1 && errno == entry.guest && *sceNetErrnoLoc() == NETDB_INTERNAL, entry.guest);
    }
    for (const int error : {HOST_NOT_FOUND, NO_DATA}) {
        resolverError = error;
        errno = 77;
        *sceNetErrnoLoc() = 99;
        const int before = calls;
        auto* result = gethostbyname_nid_postfix("localhost");
        Require(!result && calls == before + 1 && errno == 77 && *sceNetErrnoLoc() == error, 77);
    }
    std::printf("%zu legacy resolver failure cases passed\n", std::size(cases) + 2);
}
