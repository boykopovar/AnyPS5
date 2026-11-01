#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr VkQueueFlags Graphics = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
constexpr VkQueueFlags Compute = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
constexpr VkQueueFlags Sparse = VK_QUEUE_SPARSE_BINDING_BIT;
constexpr std::uint32_t Absent = VK_QUEUE_FAMILY_IGNORED;

VkQueueFamilyProperties family(VkQueueFlags flags, std::uint32_t count, std::uint32_t timestampBits = 64) {
    VkQueueFamilyProperties result{};
    result.queueFlags = flags;
    result.queueCount = count;
    result.timestampValidBits = timestampBits;
    return result;
}

void requirePreserved(const AgcDriver::DeviceQueuePlan& expected, const AgcDriver::DeviceQueuePlan& actual, const std::string& name) {
    Require(actual.mainCount == expected.mainCount && actual.settle == expected.settle &&
        actual.settleFamily == expected.settleFamily && actual.settleIndex == expected.settleIndex &&
        actual.sparseBinding == expected.sparseBinding && actual.sparseFamily == expected.sparseFamily &&
        actual.sparseIndex == expected.sparseIndex && actual.present == expected.present &&
        actual.presentIndex == expected.presentIndex, name + ": async changed an existing reservation");
}

struct Case {
    const char* name;
    std::vector<VkQueueFamilyProperties> families;
    bool settles;
    bool sparse;
    bool presents;
    std::uint32_t asyncFamily;
    std::uint32_t asyncIndex = 0;
    std::uint32_t timestampBits = 64;
    std::uint32_t mainFamily = 0;
};

void check(const Case& test) {
    const auto disabled = AgcDriver::PlanDeviceQueues(test.families, test.mainFamily, test.settles, test.sparse, test.presents, false);
    const auto defaulted = AgcDriver::PlanDeviceQueues(test.families, test.mainFamily, test.settles, test.sparse, test.presents);
    const auto enabled = AgcDriver::PlanDeviceQueues(test.families, test.mainFamily, test.settles, test.sparse, test.presents, true);
    const auto deferred = AgcDriver::PlanDeviceQueues(test.families, test.mainFamily, test.settles, test.sparse, test.presents, true, true);
    const std::string name = test.name;
    requirePreserved(disabled, defaulted, name);
    requirePreserved(disabled, enabled, name);
    requirePreserved(disabled, deferred, name);
    Require(!deferred.async && deferred.asyncFamily == Absent && deferred.asyncTimestampValidBits == 0,
        name + ": deferred destruction admitted an async queue");
    Require(!disabled.async && disabled.asyncFamily == Absent && !defaulted.async && defaulted.asyncFamily == Absent,
        name + ": async was enabled without an explicit request");
    Require(enabled.async == (test.asyncFamily != Absent) && enabled.asyncFamily == test.asyncFamily,
        name + ": unexpected async family");
    if (!enabled.async) return;
    Require(enabled.asyncIndex == test.asyncIndex && enabled.asyncTimestampValidBits == test.timestampBits,
        name + ": unexpected async index or timestamp capability");
    const auto& chosen = test.families.at(enabled.asyncFamily);
    Require(enabled.asyncFamily != test.mainFamily && enabled.asyncIndex < chosen.queueCount &&
        (chosen.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 && (chosen.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
        chosen.timestampValidBits != 0, name + ": async queue is not usable independently");
    Require(!enabled.settle || enabled.settleFamily != enabled.asyncFamily || enabled.settleIndex != enabled.asyncIndex,
        name + ": async aliases the settle queue");
    Require(enabled.sparseFamily != enabled.asyncFamily || enabled.sparseIndex != enabled.asyncIndex,
        name + ": async aliases the sparse queue");
}

void checkReservations() {
    const std::vector<std::vector<VkQueueFamilyProperties>> topologies{
        {family(Graphics | Sparse, 16)},
        {family(Graphics, 1), family(Compute | Sparse, 3)},
        {family(Graphics | Sparse, 3), family(Compute | Sparse, 2)},
        {family(Graphics, 1), family(VK_QUEUE_TRANSFER_BIT, 1), family(Sparse, 1), family(Compute, 1)},
    };
    for (std::size_t topology = 0; topology < topologies.size(); ++topology) {
        for (unsigned options = 0; options != 8; ++options) {
            const auto baseline = AgcDriver::PlanDeviceQueues(topologies[topology], 0, options & 1, options & 2, options & 4, false);
            const auto enabled = AgcDriver::PlanDeviceQueues(topologies[topology], 0, options & 1, options & 2, options & 4, true);
            requirePreserved(baseline, enabled, "reservation matrix " + std::to_string(topology) + ":" + std::to_string(options));
        }
    }
    const std::array windowed{family(Graphics | Sparse, 3), family(Compute, 1)};
    const auto plan = AgcDriver::PlanDeviceQueues(windowed, 0, true, true, true, true);
    Require(plan.mainCount == 3 && plan.settle && plan.settleFamily == 0 && plan.settleIndex == 1 &&
        plan.present && plan.presentIndex == 2 && plan.async && plan.asyncFamily == 1 && plan.asyncIndex == 0,
        "async did not preserve graphics index 2 for presentation");
}

void checkCreateInfos(const AgcDriver::DeviceQueuePlan& plan, std::uint32_t mainFamily,
    const std::vector<VkQueueFamilyProperties>& families, const std::vector<std::pair<std::uint32_t, std::uint32_t>>& expected) {
    const AgcDriver::DeviceQueueCreateInfos created(plan, mainFamily);
    Require(created.count == expected.size(), "unexpected number of device queue create records");
    for (std::uint32_t index = 0; index < created.count; ++index) {
        const auto& info = created.infos[index];
        const auto match = std::find_if(expected.begin(), expected.end(), [&](const auto& entry) { return entry.first == info.queueFamilyIndex; });
        Require(match != expected.end() && info.queueCount == match->second &&
            info.queueFamilyIndex < families.size() && info.queueCount <= families[info.queueFamilyIndex].queueCount,
            "device queue create record does not cover the reserved indices");
        Require(info.sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO && info.pNext == nullptr && info.flags == 0,
            "device queue create record has invalid structure metadata");
        Require(created.priorities[index].size() == info.queueCount &&
            info.pQueuePriorities == created.priorities[index].data(), "queue priority array is shorter than its queue count");
        for (const auto priority : created.priorities[index]) Require(priority == 1.0f, "queue priority was not initialized");
        for (std::uint32_t previous = 0; previous < index; ++previous)
            Require(created.infos[previous].queueFamilyIndex != info.queueFamilyIndex, "queue family was requested twice");
    }
}

void checkDeviceCreateInfos() {
    const std::vector shared{family(Graphics, 1), family(Compute | Sparse, 3)};
    const auto sharedPlan = AgcDriver::PlanDeviceQueues(shared, 0, true, true, false, true);
    Require(sharedPlan.settleFamily == 1 && sharedPlan.settleIndex == 0 && sharedPlan.sparseFamily == 1 &&
        sharedPlan.sparseIndex == 1 && sharedPlan.asyncFamily == 1 && sharedPlan.asyncIndex == 2,
        "shared family reservations did not occupy separate indices");
    checkCreateInfos(sharedPlan, 0, shared, {{0, 1}, {1, 3}});
    const std::vector separate{family(Graphics, 1), family(VK_QUEUE_TRANSFER_BIT, 1), family(Sparse, 1), family(Compute, 1)};
    const auto separatePlan = AgcDriver::PlanDeviceQueues(separate, 0, true, true, false, true);
    checkCreateInfos(separatePlan, 0, separate, {{0, 1}, {1, 1}, {2, 1}, {3, 1}});
    AgcDriver::DeviceQueuePlan widePlan;
    widePlan.mainCount = 7;
    checkCreateInfos(widePlan, 0, {family(Graphics, 8)}, {{0, 7}});
}

}

int main() {
    try {
        const std::vector<Case> cases{
            {"single queue", {family(Graphics, 1)}, false, false, false, Absent},
            {"many graphics queues", {family(Graphics, 16)}, true, false, true, Absent},
            {"second graphics family", {family(Graphics, 3), family(Graphics, 8)}, true, false, true, Absent},
            {"transfer only", {family(Graphics, 1), family(VK_QUEUE_TRANSFER_BIT, 8)}, false, false, false, Absent},
            {"sparse only", {family(Graphics, 1), family(Sparse, 8)}, false, true, false, Absent},
            {"compute spare", {family(Graphics, 1), family(Compute, 1)}, false, false, false, 1},
            {"compute without explicit transfer flag", {family(Graphics, 1), family(VK_QUEUE_COMPUTE_BIT, 1)}, false, false, false, 1},
            {"compute exhausted by settle", {family(Graphics, 1), family(Compute, 1)}, true, false, false, Absent},
            {"compute spare after settle", {family(Graphics, 1), family(Compute, 2)}, true, false, false, 1, 1},
            {"compute exhausted by sparse", {family(Graphics, 1), family(Compute | Sparse, 1)}, false, true, false, Absent},
            {"compute spare after sparse", {family(Graphics, 1), family(Compute | Sparse, 2)}, false, true, false, 1, 1},
            {"compute exhausted by settle and sparse", {family(Graphics, 1), family(Compute | Sparse, 2)}, true, true, false, Absent},
            {"compute spare after settle and sparse", {family(Graphics, 1), family(Compute | Sparse, 3)}, true, true, false, 1, 2},
            {"four separate families", {family(Graphics, 1), family(VK_QUEUE_TRANSFER_BIT, 1), family(Sparse, 1), family(Compute, 1)}, true, true, false, 3},
            {"skip exhausted family", {family(Graphics, 1), family(Compute, 1), family(Compute, 1)}, true, false, false, 2},
            {"zero queues", {family(Graphics, 1), family(Compute, 0)}, false, false, false, Absent},
            {"skip zero queues", {family(Graphics, 1), family(Compute, 0), family(Compute, 1)}, false, false, false, 2},
            {"zero timestamp bits", {family(Graphics, 1), family(Compute, 1, 0)}, false, false, false, Absent},
            {"skip zero timestamp bits", {family(Graphics, 1), family(Compute, 1, 0), family(Compute, 1, 36)}, false, false, false, 2, 0, 36},
            {"nonzero main family", {family(Compute, 1, 48), family(Graphics, 3)}, true, false, true, 0, 0, 48, 1},
        };
        for (const auto& test : cases) check(test);
        checkReservations();
        checkDeviceCreateInfos();
        std::puts("async device queue tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
