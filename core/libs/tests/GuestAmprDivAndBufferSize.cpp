#include "prx/libSceAmpr/include/Api.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool value) {
    if (value) return;
    std::fputs("Ampr command buffer size check failed\n", stderr);
    std::abort();
}

int main() {
    Require(sceAmprCommandBufferGetSize(nullptr) == 0);
    Require(sceAmprCommandBufferGetBufferBaseAddress(nullptr) == nullptr);
    Require(sceAmprCommandBufferGetCurrentOffset(nullptr) == 0);
    Require(sceAmprCommandBufferGetNumCommands(nullptr) == 0);
    Require(sceAmprCommandBufferGetType(nullptr) == 0);

    alignas(64) std::array<std::uint8_t, 256> storage{};
    Apr::CommandBufferObject buffer{};
    Require(sceAmprCommandBufferConstructor(&buffer) == 0);
    Require(sceAmprCommandBufferSetBuffer(&buffer, storage.data(), storage.size()) == 0);
    Require(sceAmprCommandBufferGetSize(&buffer) == storage.size());
    Require(sceAmprCommandBufferGetBufferBaseAddress(&buffer) == storage.data());
    Require(sceAmprCommandBufferGetCurrentOffset(&buffer) == 0);
    Require(sceAmprCommandBufferGetNumCommands(&buffer) == 0);
    Require(sceAmprCommandBufferDestructor(&buffer) == 0);
    return 0;
}
