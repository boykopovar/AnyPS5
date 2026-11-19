#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>

namespace {

using StreamWriteFunction = int(APS5_VABI*)(std::int64_t, std::int64_t, std::int64_t, std::int64_t, std::int64_t);

constexpr int NoEntry = static_cast<int>(0x80020002);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void RequireNoEntry(StreamWriteFunction function, std::int64_t first) {
    *__error_nid_postfix() = 42;
    Require(function(first, 0, 0, 0, 0) == NoEntry);
    Require(*__error_nid_postfix() == ENOENT);
}

}

int main() {
    RequireNoEntry(reinterpret_cast<StreamWriteFunction>(&sceKernelStreamWriteCreate), 0);
    const StreamWriteFunction others[] = {
        reinterpret_cast<StreamWriteFunction>(&sceKernelStreamWriteActivate),
        reinterpret_cast<StreamWriteFunction>(&sceKernelStreamWriteDeactivate),
        reinterpret_cast<StreamWriteFunction>(&sceKernelStreamWriteDelete),
        reinterpret_cast<StreamWriteFunction>(&sceKernelStreamWriteRead),
        reinterpret_cast<StreamWriteFunction>(&sceKernelStreamWriteStat),
        reinterpret_cast<StreamWriteFunction>(&sceKernelStreamWriteWrite),
    };
    for (const StreamWriteFunction function : others) {
        RequireNoEntry(function, 0);
        RequireNoEntry(function, -1);
    }
    return 0;
}
