#include "prx/libSceVdecSavc/libSceVdecSavc.hpp"
#include "prx/libSceVdecSavc2/libSceVdecSavc2.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

using Decoder = std::array<std::uint64_t, 4>;

void CheckDestroy(int (*destroy)(void*), const char* name) {
    Require(destroy(nullptr) == 0, name);

    for (const std::uint64_t field : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{0xDEADBEEFCAFEF00Dull}}) {
        Decoder decoder{0x1111, field, 0, 0x4444};
        Require(destroy(decoder.data()) == 0, name);
        Require(decoder[0] == 0x1111 && decoder[1] == 0 && decoder[2] == 0 && decoder[3] == 0x4444, name);
    }
}

} // namespace

int main() {
    for (const int expectedErrno : {0, 7, 42}) {
        errno = expectedErrno;
        CheckDestroy([](void* decoder) { return sceSdecDestroySw(decoder); }, "sceSdecDestroySw");
        CheckDestroy([](void* decoder) { return sceSdecDestroySw2(decoder); }, "sceSdecDestroySw2");
        Require(errno == expectedErrno, "Destroy must not change errno");
    }
}
