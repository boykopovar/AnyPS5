#include "prx/libSceAgcDriver/Graphics/include/DescriptorValidation.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void ExpectFailure(const char* name, auto&& function, std::string_view expected) {
    try {
        function();
        std::fprintf(stderr, "%s: expected exception containing '%s', but none was thrown\n", name, expected.data());
        ++failures;
    } catch (const std::exception& error) {
        if (std::string_view(error.what()).find(expected) == std::string_view::npos) {
            std::fprintf(stderr, "%s: exception '%s' does not contain '%s'\n", name, error.what(), expected.data());
            ++failures;
        }
    }
}

void ExpectSuccess(const char* name, auto&& function) {
    try {
        function();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: unexpected exception: %s\n", name, error.what());
        ++failures;
    }
}

AgcDriver::Graphics::Context BaseContext() {
    AgcDriver::Graphics::Context context{};
    context.limits.maxPerStageResources = 128;
    context.limits.maxPerStageDescriptorStorageBuffers = 8;
    context.limits.maxDescriptorSetStorageBuffers = 10;
    context.limits.maxPerStageDescriptorSampledImages = 16;
    context.limits.maxDescriptorSetSampledImages = 16;
    context.limits.maxPerStageDescriptorStorageImages = 8;
    context.limits.maxDescriptorSetStorageImages = 8;
    context.limits.maxPerStageDescriptorSamplers = 4;
    context.limits.maxDescriptorSetSamplers = 4;
    context.descriptorIndexingLimits.maxPerStageUpdateAfterBindResources = 256;
    context.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindStorageBuffers = 16;
    context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindStorageBuffers = 20;
    context.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindSampledImages = 32;
    context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindSampledImages = 32;
    context.descriptorIndexingLimits.maxPerStageDescriptorUpdateAfterBindSamplers = 8;
    context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindSamplers = 8;
    return context;
}

void TestStorageBufferPerStageLimitExceeded() {
    auto context = BaseContext();
    const std::array<VkDescriptorSetLayoutBinding, 1> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9, VK_SHADER_STAGE_VERTEX_BIT, nullptr}}};
    ExpectFailure("StorageBufferPerStageLimitExceeded", [&] {
        AgcDriver::Graphics::ValidateDescriptorLayout(context, bindings, false);
    }, "typed heaps exceed per-stage storage buffer capacity");
}

void TestStorageBufferPerSetLimitExceeded() {
    auto context = BaseContext();
    const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, VK_SHADER_STAGE_VERTEX_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
    ExpectFailure("StorageBufferPerSetLimitExceeded", [&] {
        AgcDriver::Graphics::ValidateDescriptorLayout(context, bindings, false);
    }, "typed heaps exceed descriptor set storage buffer capacity");
}

void TestStorageBufferUpdateAfterBindExceeded() {
    auto context = BaseContext();
    context.storageBufferUpdateAfterBind = true;
    const std::array<VkDescriptorSetLayoutBinding, 1> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 17, VK_SHADER_STAGE_VERTEX_BIT, nullptr}}};
    ExpectFailure("StorageBufferUpdateAfterBindExceeded", [&] {
        AgcDriver::Graphics::ValidateDescriptorLayout(context, bindings, true);
    }, "typed heaps exceed per-stage storage buffer capacity");
}

void TestStorageBufferUpdateAfterBindSetExceeded() {
    auto context = BaseContext();
    context.storageBufferUpdateAfterBind = true;
    const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 12, VK_SHADER_STAGE_VERTEX_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 12, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
    ExpectFailure("StorageBufferUpdateAfterBindSetExceeded", [&] {
        AgcDriver::Graphics::ValidateDescriptorLayout(context, bindings, true);
    }, "typed heaps exceed descriptor set storage buffer capacity");
}

void TestStorageBufferUpdateAfterBindDisabledFallsBack() {
    auto context = BaseContext();
    context.storageBufferUpdateAfterBind = false;
    const std::array<VkDescriptorSetLayoutBinding, 1> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 12, VK_SHADER_STAGE_VERTEX_BIT, nullptr}}};
    ExpectFailure("StorageBufferUpdateAfterBindDisabledFallsBack", [&] {
        AgcDriver::Graphics::ValidateDescriptorLayout(context, bindings, true);
    }, "typed heaps exceed per-stage storage buffer capacity");
}

void TestStorageBufferValidWithinLimits() {
    auto context = BaseContext();
    const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, VK_SHADER_STAGE_VERTEX_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
    ExpectSuccess("StorageBufferValidWithinLimits", [&] {
        AgcDriver::Graphics::ValidateDescriptorLayout(context, bindings, false);
    });
}

void TestStorageBufferValidWithinUpdateAfterBindLimits() {
    auto context = BaseContext();
    context.storageBufferUpdateAfterBind = true;
    const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10, VK_SHADER_STAGE_VERTEX_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
    ExpectSuccess("StorageBufferValidWithinUpdateAfterBindLimits", [&] {
        AgcDriver::Graphics::ValidateDescriptorLayout(context, bindings, true);
    });
}

}

int main() {
    TestStorageBufferPerStageLimitExceeded();
    TestStorageBufferPerSetLimitExceeded();
    TestStorageBufferUpdateAfterBindExceeded();
    TestStorageBufferUpdateAfterBindSetExceeded();
    TestStorageBufferUpdateAfterBindDisabledFallsBack();
    TestStorageBufferValidWithinLimits();
    TestStorageBufferValidWithinUpdateAfterBindLimits();
    return failures == 0 ? 0 : 1;
}
