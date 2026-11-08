#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int SolSocket = 0xffff;
constexpr int SoLinger = 0x80;
constexpr int ErrorDomain = 33;
constexpr int ErrorInvalid = 22;

struct Linger {
    std::int32_t l_onoff;
    std::int32_t l_linger;
};

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

bool SetFails(int descriptor, int level, int option, const void* value, std::uint32_t length, int error) {
    return setsockopt_nid_postfix(descriptor, level, option, value, length) == -1 && *__error_nid_postfix() == error;
}

void AuditSoLingerParameters() {
    const int sock = socket_nid_postfix(2, 1, 0);
    Require(sock >= 0, "socket failed");

    Linger linger{1, 10};
    Require(setsockopt_nid_postfix(sock, SolSocket, SoLinger, &linger, sizeof(linger)) == 0, "setsockopt SO_LINGER failed");

    Linger readVal{};
    std::uint32_t size = sizeof(readVal);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoLinger, &readVal, &size) == 0 && size == sizeof(readVal), "getsockopt SO_LINGER failed");
    Require(readVal.l_onoff == SoLinger && readVal.l_linger == 10, "SO_LINGER values mismatch");

    linger = {1, -1};
    Require(SetFails(sock, SolSocket, SoLinger, &linger, sizeof(linger), ErrorDomain), "negative l_linger must fail with EDOM");

    linger = {1, 65536};
    Require(SetFails(sock, SolSocket, SoLinger, &linger, sizeof(linger), ErrorDomain), "l_linger above 65535 must fail with EDOM");

    Require(SetFails(sock, SolSocket, SoLinger, &linger, sizeof(std::int32_t), ErrorInvalid), "short size must fail with EINVAL");

    Require(close_nid_postfix(sock) == 0, "close failed");
}

} // namespace

int main() {
    AuditSoLingerParameters();
    return 0;
}
