#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::size_t GuestStride = 65536;
constexpr std::uint32_t Side = 8;
constexpr std::uint32_t Levels = 4;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t MinLodOfOne = 0x100;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        data = VirtualAlloc(nullptr, GuestStride, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        data = std::aligned_alloc(GuestStride, GuestStride);
#endif
        Require(data != nullptr, "image table min LOD: cannot allocate guest texture bytes");
        std::memset(data, 0x35, GuestStride);
        GuestAllocations::Mutation().Add(data, GuestStride, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        std::free(data);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)); }

private:
    void* data = nullptr;
};

struct ViewProbe {
    Context& context;
    PFN_vkGetDeviceProcAddr resolver;
    std::vector<float> minLods;
    static inline ViewProbe* active = nullptr;

    explicit ViewProbe(Context& context) : context(context), resolver(context.deviceProc) {
        Require(active == nullptr, "image table min LOD: a view probe is already active");
        active = this;
        context.deviceProc = resolve;
    }

    ~ViewProbe() {
        context.deviceProc = resolver;
        active = nullptr;
    }

    static VKAPI_ATTR VkResult VKAPI_CALL createView(VkDevice device, const VkImageViewCreateInfo* info, const VkAllocationCallbacks* callbacks, VkImageView* created) {
        float minLod = 0.0f;
        for (const auto* next = static_cast<const VkBaseInStructure*>(info->pNext); next != nullptr; next = next->pNext) {
            if (next->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT) minLod = reinterpret_cast<const VkImageViewMinLodCreateInfoEXT*>(next)->minLod;
        }
        active->minLods.push_back(minLod);
        return reinterpret_cast<PFN_vkCreateImageView>(active->resolver(device, "vkCreateImageView"))(device, info, callbacks, created);
    }

    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL resolve(VkDevice device, const char* name) {
        if (std::strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(createView);
        return active->resolver(device, name);
    }
};

GuestTextureResource Resource(std::uint64_t address) {
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = Side;
    resource.height = Side;
    resource.mipCount = Levels;
    resource.lastLevel = Levels - 1u;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2D;
    resource.format = Format8888UNorm;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    return resource;
}

float LargestMinLod(const std::vector<float>& minLods) {
    return minLods.empty() ? -1.0f : *std::max_element(minLods.begin(), minLods.end());
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value != nullptr ? value : "");
#else
    if (value != nullptr) setenv(name, value, 1);
    else unsetenv(name);
#endif
}

bool DeviceHasExtensionWithoutEnvironment(const Context& deviceContext) {
    const char* forced = std::getenv("APS5_NO_IMAGE_VIEW_MIN_LOD");
    if (forced == nullptr) return deviceContext.imageViewMinLod || deviceContext.imageViewMinLodExtension;
    const std::string saved = forced;
    SetEnvironment("APS5_NO_IMAGE_VIEW_MIN_LOD", nullptr);
    bool has = false;
    try {
        const auto reference = OpenVulkanTestDevice();
        if (reference) {
            const auto context = reference->graphicsContext();
            has = context.imageViewMinLod || context.imageViewMinLodExtension;
        }
    } catch (...) {
        SetEnvironment("APS5_NO_IMAGE_VIEW_MIN_LOD", saved.c_str());
        throw;
    }
    SetEnvironment("APS5_NO_IMAGE_VIEW_MIN_LOD", saved.c_str());
    return has;
}

void Run(const Context& deviceContext, bool extensionExpected) {
    GuestBlock guest;
    Require(DescribeSurface(Resource(guest.Address())).guestBytes <= GuestStride, "image table min LOD: the mip chain does not fit the guest block");
    const auto unclamped = EncodeTextureResource(Resource(guest.Address()));
    auto clamped = unclamped;
    clamped[1] |= MinLodOfOne << 8u;
    Require(EffectiveMinLod(DecodeTextureResource(clamped)) == 1.0f, "image table min LOD: the clamped descriptor does not carry a clamp of 1");

    if (extensionExpected && std::getenv("APS5_NO_IMAGE_VIEW_MIN_LOD") != nullptr) {
        Require(!deviceContext.imageViewMinLod && deviceContext.imageViewMinLodExtension, "image table min LOD: APS5_NO_IMAGE_VIEW_MIN_LOD took away the extension capability, not only the view lane");
    }

    auto lane = deviceContext;
    ViewProbe probe(lane);
    Recorder recorder(deviceContext);
    recorder.Activate();
    struct Cleanup {
        const Context& context;
        ~Cleanup() { ClearCachedTextures(context.device); }
    } cleanup{deviceContext};
    ClearCachedTextures(deviceContext.device);
    const auto setLane = [&](bool view, bool extension, bool shader) {
        lane.imageViewMinLod = view;
        lane.imageViewMinLodExtension = extension;
        lane.shaderImageMinLod = shader;
        probe.minLods.clear();
    };
    const auto release = [&] {
        ClearCachedTextures(lane.device);
        recorder.Sync();
    };

    if (extensionExpected && !deviceContext.imageViewMinLod) {
        setLane(deviceContext.imageViewMinLod, deviceContext.imageViewMinLodExtension, deviceContext.shaderImageMinLod);
        auto table = CachedSampledTableTexture(lane, clamped);
        Require(table != nullptr && LargestMinLod(probe.minLods) == 1.0f, "image table min LOD: a table entry with a MIN_LOD of 1 did not keep the view clamp on the device's own shader lane, " + std::to_string(LargestMinLod(probe.minLods)));
        table.reset();
        release();
    }

    setLane(false, true, true);
    {
        auto direct = CachedSampledTexture(lane, clamped);
        Require(direct != nullptr && !probe.minLods.empty() && LargestMinLod(probe.minLods) == 0.0f, "image table min LOD: a direct image on the shader lane got a clamped view, " + std::to_string(LargestMinLod(probe.minLods)));
        probe.minLods.clear();
        auto table = CachedSampledTableTexture(lane, clamped);
        Require(table != nullptr && !probe.minLods.empty() && LargestMinLod(probe.minLods) == 1.0f, "image table min LOD: a table entry with a MIN_LOD of 1 on the shader lane did not keep the view clamp, " + std::to_string(LargestMinLod(probe.minLods)));
        Require(table != direct, "image table min LOD: a table entry shares its view with a direct image of the same T# on the shader lane");
        probe.minLods.clear();
        Require(CachedSampledTexture(lane, clamped) == direct && CachedSampledTableTexture(lane, clamped) == table && probe.minLods.empty(), "image table min LOD: a second lookup on the shader lane did not reuse its cached view");
        auto flat = CachedSampledTexture(lane, unclamped);
        Require(flat != nullptr && flat == CachedSampledTableTexture(lane, unclamped), "image table min LOD: a table entry without a clamp does not share its view with the direct image");
        direct.reset();
        table.reset();
        flat.reset();
    }
    release();

    setLane(true, true, false);
    {
        auto direct = CachedSampledTexture(lane, clamped);
        auto table = CachedSampledTableTexture(lane, clamped);
        Require(direct != nullptr && direct == table && LargestMinLod(probe.minLods) == 1.0f, "image table min LOD: the view lane did not serve a direct image and a table entry through one clamped view");
    }
    release();

    setLane(false, false, true);
    bool refused = false;
    try {
        CachedSampledTableTexture(lane, clamped);
    } catch (const std::exception& error) {
        refused = std::string_view(error.what()).find("a sampled image table entry has a minimum LOD clamp, which needs VK_EXT_image_view_min_lod") != std::string_view::npos;
    }
    Require(refused, "image table min LOD: a table entry with a MIN_LOD was not refused where the device has no VK_EXT_image_view_min_lod");
    release();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
        auto context = device->graphicsContext();
        const bool extensionExpected = DeviceHasExtensionWithoutEnvironment(context);
        if (!extensionExpected && !context.imageViewMinLod && !context.imageViewMinLodExtension) {
            std::puts("skipped, the device has no VK_EXT_image_view_min_lod");
            return VulkanTestSkipped;
        }
        context.bufferPool = std::make_shared<BufferPool>(context);
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        Run(context, extensionExpected);
        std::cout << "image table minimum LOD tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
