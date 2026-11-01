#include "prx/libSceRudp/include/Api.hpp"
#include "SceTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int NotInitialized = static_cast<int>(0x80770001u);
constexpr int InvalidContext = static_cast<int>(0x80770003u);
constexpr int InvalidArgument = static_cast<int>(0x80770004u);
constexpr int NotAcceptable = static_cast<int>(0x80770011u);

using FlushFn = int (APS5_VABI*)(int);
using BufferStatusFn = int (APS5_VABI*)(int, void*, std::size_t);

template <typename Function>
Function Resolve(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceRudp.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceRudp\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Rudp unconnected: %s\n", message);
    std::abort();
}

void APS5_VABI Handler(int, int, int, void*) {}

bool RemoteInfoUntouched(int context, int expected) {
    std::array<std::uint8_t, 0x1c> address;
    address.fill(0x5a);
    std::uint32_t addressSize = 0x1c;
    std::uint16_t port = 0x5a5a;
    if (sceRudpGetRemoteInfo(context, address.data(), &addressSize, &port) != expected) return false;
    for (const auto byte : address) {
        if (byte != 0x5a) return false;
    }
    return addressSize == 0x1c && port == 0x5a5a && sceRudpGetRemoteInfo(context, nullptr, nullptr, nullptr) == expected;
}

bool BufferStatusIs(BufferStatusFn bufferStatus, int context, std::size_t size, int expected, std::uint8_t fill) {
    std::array<std::uint8_t, 32> status;
    status.fill(0x5a);
    if (bufferStatus(context, status.data(), size) != expected) return false;
    for (std::size_t index = 0; index < status.size(); ++index) {
        if (status[index] != (index < size ? fill : 0x5a)) return false;
    }
    return true;
}

}

int main() {
    const auto flush = Resolve<FlushFn>("sceRudpFlush");
    const auto bufferStatus = Resolve<BufferStatusFn>("sceRudpGetContextBufferStatus");
    alignas(16) static std::array<unsigned char, 0x10000> pool{};

    Require(flush(0) == NotInitialized && flush(-1) == NotInitialized, "a flush before sceRudpInit did not return NOT_INITIALIZED");
    Require(RemoteInfoUntouched(0, NotInitialized), "remote info before sceRudpInit did not return NOT_INITIALIZED");
    Require(BufferStatusIs(bufferStatus, 0, 16, NotInitialized, 0x5a) && bufferStatus(-1, nullptr, 0) == NotInitialized, "a buffer status before sceRudpInit did not return NOT_INITIALIZED");

    Require(sceRudpInit_nid_postfix(pool.data(), static_cast<int>(pool.size())) == 0, "sceRudpInit failed");
    Require(flush(-1) == InvalidArgument && flush(0) == InvalidContext && flush(12345) == InvalidContext, "a flush without a context was not refused in order");
    Require(RemoteInfoUntouched(-1, InvalidArgument) && RemoteInfoUntouched(12345, InvalidContext), "remote info without a context was not refused in order");
    Require(BufferStatusIs(bufferStatus, -1, 16, InvalidArgument, 0x5a) && BufferStatusIs(bufferStatus, 12345, 16, InvalidContext, 0x5a), "a buffer status without a context was not refused in order");
    Require(bufferStatus(12345, nullptr, 16) == InvalidArgument && BufferStatusIs(bufferStatus, 12345, 0, InvalidArgument, 0x5a) && BufferStatusIs(bufferStatus, 12345, 17, InvalidArgument, 0x5a), "the buffer status arguments were not checked before the context");
    Require(BufferStatusIs(bufferStatus, 12345, static_cast<std::size_t>(0x100000010ull), InvalidArgument, 0x5a), "a buffer status size above 32 bits was cut");

    int context = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &context) == 0 && context >= 0, "sceRudpCreateContext failed");
    Require(flush(context) == NotAcceptable, "a flush on an unconnected context did not return NOT_ACCEPTABLE");
    Require(RemoteInfoUntouched(context, NotAcceptable), "remote info on an unconnected context did not return NOT_ACCEPTABLE");
    for (std::size_t size = 1; size <= 16; ++size) {
        Require(BufferStatusIs(bufferStatus, context, size, 0, 0), "the buffer status was not size zero bytes");
    }
    Require(BufferStatusIs(bufferStatus, context, 17, InvalidArgument, 0x5a), "a buffer status of 17 bytes was taken");

    Require(sceRudpTerminate(context) == 0, "sceRudpTerminate failed");
    Require(flush(context) == InvalidContext && RemoteInfoUntouched(context, InvalidContext) && BufferStatusIs(bufferStatus, context, 16, InvalidContext, 0x5a), "a terminated context was taken");
    Require(sceRudpEnd() == 0, "sceRudpEnd failed");
    Require(flush(context) == NotInitialized && RemoteInfoUntouched(context, NotInitialized) && BufferStatusIs(bufferStatus, context, 16, NotInitialized, 0x5a), "a call after sceRudpEnd did not return NOT_INITIALIZED");

    std::puts("Rudp unconnected tests passed");
    return 0;
}
