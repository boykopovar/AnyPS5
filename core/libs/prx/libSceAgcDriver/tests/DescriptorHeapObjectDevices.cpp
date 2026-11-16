#include "prx/libSceAgcDriver/Graphics/include/DescriptorHeapObjects.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

struct Fake {
    VkDevice device;
    int createViews = 0;
    int destroyViews = 0;
    int createSamplers = 0;
    int destroySamplers = 0;
    VkDevice lastDevice = VK_NULL_HANDLE;
};

Fake first{reinterpret_cast<VkDevice>(0x1000)};
Fake second{reinterpret_cast<VkDevice>(0x2000)};

Fake& owner(VkDevice device) {
    return device == first.device ? first : second;
}

const std::uint64_t SharedView = 0x77;

VKAPI_ATTR VkResult VKAPI_CALL fakeCreateView(VkDevice device, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView* view) {
    auto& fake = owner(device);
    ++fake.createViews;
    fake.lastDevice = device;
    *view = reinterpret_cast<VkImageView>(SharedView);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL fakeDestroyView(VkDevice device, VkImageView, const VkAllocationCallbacks*) {
    auto& fake = owner(device);
    ++fake.destroyViews;
    fake.lastDevice = device;
}

VKAPI_ATTR VkResult VKAPI_CALL fakeCreateSampler(VkDevice device, const VkSamplerCreateInfo*, const VkAllocationCallbacks*, VkSampler* sampler) {
    auto& fake = owner(device);
    ++fake.createSamplers;
    fake.lastDevice = device;
    *sampler = reinterpret_cast<VkSampler>(SharedView);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL fakeDestroySampler(VkDevice device, VkSampler, const VkAllocationCallbacks*) {
    auto& fake = owner(device);
    ++fake.destroySamplers;
    fake.lastDevice = device;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL fakeProc(VkDevice, const char* name) {
    if (std::strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&fakeCreateView);
    if (std::strcmp(name, "vkDestroyImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&fakeDestroyView);
    if (std::strcmp(name, "vkCreateSampler") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&fakeCreateSampler);
    if (std::strcmp(name, "vkDestroySampler") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&fakeDestroySampler);
    return nullptr;
}

}

int main() {
    const auto createView = reinterpret_cast<PFN_vkCreateImageView>(DescriptorHeapObjectFunction(first.device, fakeProc, "vkCreateImageView"));
    const auto destroyView = reinterpret_cast<PFN_vkDestroyImageView>(DescriptorHeapObjectFunction(first.device, fakeProc, "vkDestroyImageView"));
    const auto createSampler = reinterpret_cast<PFN_vkCreateSampler>(DescriptorHeapObjectFunction(first.device, fakeProc, "vkCreateSampler"));
    const auto destroySampler = reinterpret_cast<PFN_vkDestroySampler>(DescriptorHeapObjectFunction(first.device, fakeProc, "vkDestroySampler"));
    Require(createView != nullptr && destroyView != nullptr && createSampler != nullptr && destroySampler != nullptr, "the hooks resolve");
    Require(DescriptorHeapObjectFunction(second.device, fakeProc, "vkCreateImageView") == reinterpret_cast<PFN_vkVoidFunction>(createView), "the second device resolves the same hook");
    DescriptorHeapObjectFunction(second.device, fakeProc, "vkDestroyImageView");
    DescriptorHeapObjectFunction(second.device, fakeProc, "vkCreateSampler");
    DescriptorHeapObjectFunction(second.device, fakeProc, "vkDestroySampler");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageView view = VK_NULL_HANDLE;
    Require(createView(first.device, &viewInfo, nullptr, &view) == VK_SUCCESS, "the first device creates a view");
    Require(first.createViews == 1 && second.createViews == 0 && first.lastDevice == first.device, "the first device's view goes through the first device's function");
    viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    Require(createView(second.device, &viewInfo, nullptr, &view) == VK_SUCCESS, "the second device creates a view");
    Require(second.createViews == 1 && first.createViews == 1 && second.lastDevice == second.device, "the second device's view goes through the second device's function");

    const auto recordFirst = FindImageView(first.device, view);
    const auto recordSecond = FindImageView(second.device, view);
    Require(recordFirst.has_value() && recordSecond.has_value(), "both devices keep a record for the same handle value");
    Require(recordFirst->info.format == VK_FORMAT_R8G8B8A8_UNORM && recordSecond->info.format == VK_FORMAT_B8G8R8A8_UNORM, "the records stay separate per device");

    const std::byte bytes[4]{};
    KeepEncodedImageView(first.device, view, recordFirst->generation, 1u, bytes);
    std::byte copy[4]{};
    Require(CopyEncodedImageView(first.device, view, 1u, copy), "the first device keeps its encoded view");
    Require(!CopyEncodedImageView(second.device, view, 1u, copy), "the second device's record has no encoding of the first's");

    destroyView(second.device, view, nullptr);
    Require(second.destroyViews == 1 && first.destroyViews == 0, "a destroy reaches the owning device's function");
    Require(!FindImageView(second.device, view).has_value(), "the destroyed record is gone");
    Require(FindImageView(first.device, view).has_value(), "destroying on one device keeps the other device's record");

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.maxLod = 3.0f;
    VkSampler sampler = VK_NULL_HANDLE;
    Require(createSampler(first.device, &samplerInfo, nullptr, &sampler) == VK_SUCCESS && first.createSamplers == 1, "the first device creates a sampler");
    samplerInfo.maxLod = 5.0f;
    Require(createSampler(second.device, &samplerInfo, nullptr, &sampler) == VK_SUCCESS && second.createSamplers == 1, "the second device creates a sampler");
    Require(FindSampler(first.device, sampler)->info.maxLod == 3.0f && FindSampler(second.device, sampler)->info.maxLod == 5.0f, "sampler records stay separate per device");
    destroySampler(first.device, sampler, nullptr);
    Require(first.destroySamplers == 1 && second.destroySamplers == 0, "a sampler destroy reaches the owning device's function");
    Require(!FindSampler(first.device, sampler).has_value() && FindSampler(second.device, sampler).has_value(), "destroying a sampler on one device keeps the other's record");
    std::puts("descriptor heap objects per device: ok");
}
