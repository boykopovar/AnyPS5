#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int Stream = 1;
constexpr int SolSocket = 0xffff;
constexpr int SoReuseAddr = 0x4;
constexpr int SoReusePort = 0x200;
constexpr int ErrorInvalid = 22;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

int GetInt(int descriptor, int option) {
    int value = -12345;
    std::uint32_t size = sizeof(value);
    Require(getsockopt_nid_postfix(descriptor, SolSocket, option, &value, &size) == 0 && size == sizeof(value), "getsockopt failed");
    return value;
}

void SetInt(int descriptor, int option, int value) {
    Require(setsockopt_nid_postfix(descriptor, SolSocket, option, &value, sizeof(value)) == 0, "setsockopt failed");
}

bool SetFails(int descriptor, int level, int option, const void* value, std::uint32_t length, int error) {
    return setsockopt_nid_postfix(descriptor, level, option, value, length) == -1 && *__error_nid_postfix() == error;
}

void AuditSetsockoptReuse() {
    const int stream = socket_nid_postfix(2, Stream, 0);
    Require(stream >= 0, "socket failed");

    Require(GetInt(stream, SoReuseAddr) == 0, "SO_REUSEADDR is off by default");
    Require(GetInt(stream, SoReusePort) == 0, "SO_REUSEPORT is off by default");

    SetInt(stream, SoReuseAddr, 1);
    Require(GetInt(stream, SoReuseAddr) == SoReuseAddr, "SO_REUSEADDR reads back as its bit");

    SetInt(stream, SoReusePort, 1);
    Require(GetInt(stream, SoReusePort) == SoReusePort, "SO_REUSEPORT reads back as its bit");

    SetInt(stream, SoReuseAddr, 0);
    SetInt(stream, SoReusePort, 0);
    Require(GetInt(stream, SoReuseAddr) == 0, "SO_REUSEADDR must turn off");
    Require(GetInt(stream, SoReusePort) == 0, "SO_REUSEPORT must turn off");

    const unsigned char narrow = 1;
    Require(SetFails(stream, SolSocket, SoReuseAddr, &narrow, 1, ErrorInvalid), "narrow SO_REUSEADDR must fail with EINVAL");
    Require(SetFails(stream, SolSocket, SoReusePort, &narrow, 1, ErrorInvalid), "narrow SO_REUSEPORT must fail with EINVAL");

    Require(close_nid_postfix(stream) == 0, "close failed");
}

} // namespace

int main(int, char**) {
    AuditSetsockoptReuse();
    return 0;
}
