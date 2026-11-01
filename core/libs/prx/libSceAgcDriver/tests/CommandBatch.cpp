#include "CommandBatchFailure.hpp"
#include "AllocationFailure.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>

int main() {
    try {
        AgcDriver::Graphics::Context context{};
        context.device = reinterpret_cast<VkDevice>(std::uintptr_t{1});
        context.queue = reinterpret_cast<VkQueue>(std::uintptr_t{2});
        context.pool = reinterpret_cast<VkCommandPool>(std::uintptr_t{3});
        AgcDriver::Tests::RunCommandBatchFailureTests(context, false);
        return 0;
    } catch (const std::exception& error) {
        AgcDriver::Tests::SetAllocationFailureCountdown(-1);
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
