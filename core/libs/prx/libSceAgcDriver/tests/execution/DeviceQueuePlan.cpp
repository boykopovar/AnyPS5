#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr VkQueueFlags Graphics = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
constexpr VkQueueFlags Sparse = VK_QUEUE_SPARSE_BINDING_BIT;
constexpr std::uint32_t Own = VK_QUEUE_FAMILY_IGNORED;

struct Case {
    const char* name;
    std::vector<VkQueueFamilyProperties> families;
    bool settles;
    std::uint32_t mainCount;
    bool settle;
    std::uint32_t settleFamily;
    std::uint32_t settleIndex;
    bool sparseBinding;
    std::uint32_t sparseFamily;
    std::uint32_t sparseIndex;
};

VkQueueFamilyProperties family(VkQueueFlags flags, std::uint32_t queues) {
    VkQueueFamilyProperties properties{};
    properties.queueFlags = flags;
    properties.queueCount = queues;
    return properties;
}

std::string describe(const AgcDriver::DeviceQueuePlan& plan) {
    return "main " + std::to_string(plan.mainCount) + " queues, settle " + (plan.settle ? std::to_string(plan.settleFamily) + ":" + std::to_string(plan.settleIndex) : std::string("none")) + ", sparse " + (!plan.sparseBinding ? std::string("none") : plan.sparseFamily == Own ? std::string("on the main queue") : std::to_string(plan.sparseFamily) + ":" + std::to_string(plan.sparseIndex));
}

void check(const Case& test) {
    const auto plan = AgcDriver::PlanDeviceQueues(test.families, 0, test.settles, true);
    const auto dense = AgcDriver::PlanDeviceQueues(test.families, 0, test.settles, false);
    Require(plan.mainCount == dense.mainCount && plan.settle == dense.settle && plan.settleFamily == dense.settleFamily && plan.settleIndex == dense.settleIndex, std::string("device queue plan, ") + test.name + ": sparse binding changed the queues: " + describe(dense) + " without it, " + describe(plan) + " with it");
    Require(!dense.sparseBinding && dense.sparseFamily == Own, std::string("device queue plan, ") + test.name + ": a device without sparse binding got a sparse queue: " + describe(dense));
    const bool expected = plan.mainCount == test.mainCount && plan.settle == test.settle && (!test.settle || (plan.settleFamily == test.settleFamily && plan.settleIndex == test.settleIndex)) && plan.sparseBinding == test.sparseBinding && plan.sparseFamily == test.sparseFamily && (test.sparseFamily == Own || plan.sparseIndex == test.sparseIndex);
    Require(expected, std::string("device queue plan, ") + test.name + ": got " + describe(plan));
}

}

int main() {
    try {
        const std::vector<Case> cases{
            {"one family with one queue", {family(Graphics | Sparse, 1)}, true, 1, false, 0, 0, true, Own, 0},
            {"one family with one queue, no settles", {family(Graphics | Sparse, 1)}, false, 1, false, 0, 0, true, Own, 0},
            {"settle family with one queue that also binds", {family(Graphics | Sparse, 1), family(VK_QUEUE_TRANSFER_BIT | Sparse, 1)}, true, 1, true, 1, 0, true, Own, 0},
            {"settle family with two queues that also binds", {family(Graphics, 1), family(VK_QUEUE_TRANSFER_BIT | Sparse, 2)}, true, 1, true, 1, 0, true, 1, 1},
            {"settle on the main family", {family(Graphics | Sparse, 2), family(VK_QUEUE_TRANSFER_BIT | Sparse, 1)}, true, 2, true, 0, 1, true, 1, 0},
            {"sparse-only family preferred", {family(Graphics | Sparse, 16), family(VK_QUEUE_TRANSFER_BIT | Sparse, 2), family(Sparse, 1)}, true, 2, true, 0, 1, true, 2, 0},
            {"no settles, a second family binds", {family(Graphics | Sparse, 1), family(VK_QUEUE_TRANSFER_BIT | Sparse, 1)}, false, 1, false, 0, 0, true, 1, 0},
            {"no queue binds", {family(Graphics, 1), family(VK_QUEUE_TRANSFER_BIT, 1)}, true, 1, true, 1, 0, false, Own, 0},
            {"only the settle queue binds", {family(Graphics, 1), family(VK_QUEUE_TRANSFER_BIT | Sparse, 1)}, true, 1, true, 1, 0, false, Own, 0},
        };
        for (const auto& test : cases) check(test);
        std::puts("device queue plan tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
