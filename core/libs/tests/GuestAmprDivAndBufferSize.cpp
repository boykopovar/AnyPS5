#include "prx/libSceAmpr/include/Api.hpp"
#include "Testing.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main() {
    Require(sceAmprCommandBufferGetSize(nullptr) == 0);

    alignas(64) std::array<std::uint8_t, 256> storage{};
    Apr::CommandBufferObject buffer{};
    Require(sceAmprCommandBufferConstructor(&buffer) == 0);
    Require(sceAmprCommandBufferSetBuffer(&buffer, storage.data(), storage.size()) == 0);
    Require(sceAmprCommandBufferGetSize(&buffer) == storage.size());
    return 0;
}
