#include "prx/libSceVideoCoreInterface/libSceVideoCoreInterface.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81793001u);
constexpr int ErrorNotInitialized = static_cast<int>(0x81793002u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

} // namespace

int main() {
    std::array<std::uint8_t, 64> buffer{};
    void* p = buffer.data();
    for (const int expectedErrno : {0, 7, 42}) {
        errno = expectedErrno;
        Require(sceVideoCoreIfSetFunctionTable(nullptr) == ErrorInvalidArgument,
                "SetFunctionTable must reject a null table");
        Require(sceVideoCoreIfSetFunctionTableUnlimited(nullptr) == ErrorInvalidArgument,
                "SetFunctionTableUnlimited must reject a null table");
        Require(sceVideoCoreIfInitializeInterface(nullptr, p) == ErrorInvalidArgument,
                "InitializeInterface must reject a null first argument");
        Require(sceVideoCoreIfInitializeInterface(p, nullptr) == ErrorInvalidArgument,
                "InitializeInterface must reject a null second argument");
        Require(sceVideoCoreIfInitializeInterface(nullptr, nullptr) == ErrorInvalidArgument,
                "InitializeInterface must reject two null arguments");
        Require(sceVideoCoreIfSetFunctionTable(p) == ErrorNotInitialized,
                "SetFunctionTable must return NOT_INITIALIZED before init");
        Require(sceVideoCoreIfSetFunctionTableUnlimited(p) == ErrorNotInitialized,
                "SetFunctionTableUnlimited must return NOT_INITIALIZED before init");
        Require(sceVideoCoreIfMainLoop() == ErrorNotInitialized,
                "MainLoop must return NOT_INITIALIZED before init");
        Require(errno == expectedErrno, "The null and pre-init checks must not change errno");
    }
    Require(buffer == std::array<std::uint8_t, 64>{}, "Rejected calls must not write");
}
