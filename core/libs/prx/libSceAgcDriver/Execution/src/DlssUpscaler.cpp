#include "prx/libSceAgcDriver/Execution/include/DlssUpscaler.hpp"
#include "prx/libSceAgcDriver/Execution/include/NgxApi.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL_loadso.h>
#include <string>

namespace AgcDriver {
namespace {

PFN_vkGetInstanceProcAddr GetVkGIPA() {
    static PFN_vkGetInstanceProcAddr gipa = nullptr;
    if (!gipa) {
#ifdef _WIN32
        void* vk = SDL_LoadObject("vulkan-1.dll");
#elif defined(__APPLE__)
        void* vk = SDL_LoadObject("libvulkan.1.dylib");
#else
        void* vk = SDL_LoadObject("libvulkan.so.1");
        if (!vk) vk = SDL_LoadObject("libvulkan.so");
#endif
        if (vk) {
            gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(vk, "vkGetInstanceProcAddr"));
        }
    }
    return gipa;
}

PFN_vkGetDeviceProcAddr GetVkGDPA(VkInstance instance) {
    auto gipa = GetVkGIPA();
    if (gipa && instance) {
        return reinterpret_cast<PFN_vkGetDeviceProcAddr>(gipa(instance, "vkGetDeviceProcAddr"));
    }
    return nullptr;
}

struct NgxApi {
    void* handle = nullptr;
    using PFN_Init = NVSDK_NGX_Result (*)(
        unsigned long long, const wchar_t*, VkInstance, VkPhysicalDevice, VkDevice, NVSDK_NGX_Version);
    using PFN_Shutdown1 = NVSDK_NGX_Result (*)(VkDevice);
    using PFN_Shutdown = NVSDK_NGX_Result (*)();
    using PFN_GetCapabilityParameters = NVSDK_NGX_Result (*)(NVSDK_NGX_Parameter**);
    using PFN_DestroyParameters = NVSDK_NGX_Result (*)(NVSDK_NGX_Parameter*);
    using PFN_CreateFeature = NVSDK_NGX_Result (*)(VkCommandBuffer, NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
    using PFN_ReleaseFeature = NVSDK_NGX_Result (*)(NVSDK_NGX_Handle*);
    using PFN_EvaluateFeature = NVSDK_NGX_Result (*)(VkCommandBuffer, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);

    PFN_Init Init = nullptr;
    PFN_Shutdown1 Shutdown1 = nullptr;
    PFN_Shutdown Shutdown = nullptr;
    PFN_GetCapabilityParameters GetCapabilityParameters = nullptr;
    PFN_DestroyParameters DestroyParameters = nullptr;
    PFN_CreateFeature CreateFeature = nullptr;
    PFN_ReleaseFeature ReleaseFeature = nullptr;
    PFN_EvaluateFeature EvaluateFeature = nullptr;

    VkDevice initializedDevice = VK_NULL_HANDLE;
    std::size_t activeCount = 0;

    bool Load() {
        if (handle) return true;
#ifdef _WIN32
        handle = SDL_LoadObject("nvsdk_ngx.dll");
        if (!handle) handle = SDL_LoadObject("nvsdk_ngx_d.dll");
#else
        handle = SDL_LoadObject("libnvidia-ngx.so");
        if (!handle) handle = SDL_LoadObject("libnvidia-ngx.so.1");
#endif
        if (!handle) return false;

        Init = reinterpret_cast<PFN_Init>(SDL_LoadFunction(handle, "NVSDK_NGX_VULKAN_Init"));
        Shutdown1 = reinterpret_cast<PFN_Shutdown1>(SDL_LoadFunction(handle, "NVSDK_NGX_VULKAN_Shutdown1"));
        Shutdown = reinterpret_cast<PFN_Shutdown>(SDL_LoadFunction(handle, "NVSDK_NGX_VULKAN_Shutdown"));
        GetCapabilityParameters = reinterpret_cast<PFN_GetCapabilityParameters>(SDL_LoadFunction(handle, "NVSDK_NGX_VULKAN_GetCapabilityParameters"));
        DestroyParameters = reinterpret_cast<PFN_DestroyParameters>(SDL_LoadFunction(handle, "NVSDK_NGX_VULKAN_DestroyParameters"));
        CreateFeature = reinterpret_cast<PFN_CreateFeature>(SDL_LoadFunction(handle, "NVSDK_NGX_VULKAN_CreateFeature"));
        ReleaseFeature = reinterpret_cast<PFN_ReleaseFeature>(SDL_LoadFunction(handle, "NVSDK_NGX_VULKAN_ReleaseFeature"));
        EvaluateFeature = reinterpret_cast<PFN_EvaluateFeature>(SDL_LoadFunction(handle, "NVSDK_NGX_VULKAN_EvaluateFeature"));

        if (!Init || !GetCapabilityParameters ||
            !DestroyParameters || !CreateFeature || !ReleaseFeature || !EvaluateFeature) {
            SDL_UnloadObject(handle);
            handle = nullptr;
            return false;
        }
        return true;
    }

    NVSDK_NGX_Result InitDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
        if (!Load()) return NVSDK_NGX_Result_FAIL_FeatureNotSupported;
        if (initializedDevice == device) {
            ++activeCount;
            return NVSDK_NGX_Result_Success;
        }
        if (initializedDevice != VK_NULL_HANDLE) {
            ShutdownDevice(initializedDevice);
        }
        const wchar_t* logDir = L".";
        const auto result = Init(
            0x1337ull,
            logDir,
            instance,
            physicalDevice,
            device,
            NVSDK_NGX_Version_API
        );
        if (NVSDK_NGX_SUCCEED(result)) {
            initializedDevice = device;
            ++activeCount;
        }
        return result;
    }

    void ShutdownDevice(VkDevice device) {
        if (activeCount > 1) {
            --activeCount;
            return;
        }
        activeCount = 0;
        if (initializedDevice != VK_NULL_HANDLE) {
            if (Shutdown) {
                Shutdown();
            }
            initializedDevice = VK_NULL_HANDLE;
        }
    }

    ~NgxApi() {
        if (initializedDevice != VK_NULL_HANDLE) {
            if (Shutdown) Shutdown();
            initializedDevice = VK_NULL_HANDLE;
        }
        if (handle) {
            SDL_UnloadObject(handle);
            handle = nullptr;
        }
    }
};

NgxApi& GetNgxApi() {
    static NgxApi api;
    return api;
}

}

struct DlssUpscaler::Impl {
    Graphics::Context context;
    VkExtent2D inputExtent{};
    VkExtent2D outputExtent{};
    int quality = 2;
    float sharpness = 0.0f;
    NVSDK_NGX_Parameter* params = nullptr;
    NVSDK_NGX_Handle* featureHandle = nullptr;
    bool initialized = false;
};

bool DlssUpscaler::IsAvailable() noexcept {
    return GetNgxApi().Load();
}

bool DlssUpscaler::QueryDeviceSupport(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, DlssSupportDetails& details) {
    details = {};
    if (!instance || !physicalDevice || !device) return false;
    auto& api = GetNgxApi();
    if (!api.Load()) return false;
    details.libraryLoaded = true;

    const auto initResult = api.InitDevice(instance, physicalDevice, device);
    details.initResult = static_cast<std::uint32_t>(initResult);
    if (NVSDK_NGX_FAILED(initResult)) {
        return false;
    }

    NVSDK_NGX_Parameter* testParams = nullptr;
    if (NVSDK_NGX_SUCCEED(api.GetCapabilityParameters(&testParams)) && testParams) {
        testParams->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &details.needsUpdatedDriver);
        testParams->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &details.isSupported);
        testParams->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &details.featureInitResult);
        api.DestroyParameters(testParams);
    }
    api.ShutdownDevice(device);
    return details.isSupported && !details.needsUpdatedDriver;
}

bool DlssUpscaler::SupportsDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
    DlssSupportDetails details;
    return QueryDeviceSupport(instance, physicalDevice, device, details);
}

DlssUpscaler::DlssUpscaler(const Graphics::Context& context, VkInstance instance, VkExtent2D inputExtent, VkExtent2D outputExtent, int quality, float sharpness)
    : impl(std::make_unique<Impl>()) {
    impl->context = context;
    impl->inputExtent = inputExtent;
    impl->outputExtent = outputExtent;
    impl->quality = quality;
    impl->sharpness = sharpness;

    auto& api = GetNgxApi();
    Graphics::Require(api.Load(), "NVIDIA NGX library (libnvidia-ngx.so) could not be loaded");

    const auto initResult = api.InitDevice(instance, context.physical, context.device);
    Graphics::Require(NVSDK_NGX_SUCCEED(initResult), "Failed to initialize NVIDIA NGX Vulkan");

    Graphics::Require(NVSDK_NGX_SUCCEED(api.GetCapabilityParameters(&impl->params)), "Failed to allocate NGX parameters");

    impl->params->Set(NVSDK_NGX_Parameter_Width, inputExtent.width);
    impl->params->Set(NVSDK_NGX_Parameter_Height, inputExtent.height);
    impl->params->Set(NVSDK_NGX_Parameter_OutWidth, outputExtent.width);
    impl->params->Set(NVSDK_NGX_Parameter_OutHeight, outputExtent.height);
    impl->params->Set(NVSDK_NGX_Parameter_PerfQualityValue, quality);
    if (sharpness > 0.0f) {
        impl->params->Set(NVSDK_NGX_Parameter_Sharpness, sharpness);
    }
    impl->params->Set(NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags,
        NVSDK_NGX_DLSS_Feature_Flags_DepthInverted | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure);

    impl->initialized = true;
}

DlssUpscaler::~DlssUpscaler() {
    Release();
}

void DlssUpscaler::Release() noexcept {
    if (!impl || !impl->initialized) return;
    auto& api = GetNgxApi();
    if (impl->featureHandle && api.ReleaseFeature) {
        api.ReleaseFeature(impl->featureHandle);
        impl->featureHandle = nullptr;
    }
    if (impl->params && api.DestroyParameters) {
        api.DestroyParameters(impl->params);
        impl->params = nullptr;
    }
    api.ShutdownDevice(impl->context.device);
    impl->initialized = false;
}

void DlssUpscaler::ResetExtents(VkExtent2D inputExtent, VkExtent2D outputExtent) {
    Graphics::Require(impl && impl->initialized, "DlssUpscaler must be initialized before ResetExtents");
    if (impl->inputExtent.width == inputExtent.width && impl->inputExtent.height == inputExtent.height &&
        impl->outputExtent.width == outputExtent.width && impl->outputExtent.height == outputExtent.height) {
        return;
    }
    auto& api = GetNgxApi();
    if (impl->featureHandle && api.ReleaseFeature) {
        api.ReleaseFeature(impl->featureHandle);
        impl->featureHandle = nullptr;
    }
    impl->inputExtent = inputExtent;
    impl->outputExtent = outputExtent;
    impl->params->Set(NVSDK_NGX_Parameter_Width, inputExtent.width);
    impl->params->Set(NVSDK_NGX_Parameter_Height, inputExtent.height);
    impl->params->Set(NVSDK_NGX_Parameter_OutWidth, outputExtent.width);
    impl->params->Set(NVSDK_NGX_Parameter_OutHeight, outputExtent.height);
}

void DlssUpscaler::RecordUpscale(VkCommandBuffer commands, const DlssParameters& params) {
    Graphics::Require(impl && impl->initialized, "DlssUpscaler must be initialized before RecordUpscale");
    auto& api = GetNgxApi();

    if (!impl->featureHandle) {
        const auto createResult = api.CreateFeature(
            commands,
            NVSDK_NGX_Feature_SuperSampling,
            impl->params,
            &impl->featureHandle
        );
        Graphics::Require(NVSDK_NGX_SUCCEED(createResult), "Failed to create NVIDIA DLSS Super Resolution feature");
    }

    const auto colorFormat = params.colorFormat != VK_FORMAT_UNDEFINED ? params.colorFormat : VK_FORMAT_B8G8R8A8_UNORM;
    const auto depthFormat = params.depthFormat != VK_FORMAT_UNDEFINED ? params.depthFormat : VK_FORMAT_D32_SFLOAT;
    const auto mvecFormat = params.motionVectorsFormat != VK_FORMAT_UNDEFINED ? params.motionVectorsFormat : VK_FORMAT_R16G16_SFLOAT;
    const auto destFormat = params.destinationFormat != VK_FORMAT_UNDEFINED ? params.destinationFormat : colorFormat;

    NVSDK_NGX_Resource_VK colorResource = NVSDK_NGX_Create_ImageView_Resource_VK(
        params.color, params.colorImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        colorFormat, impl->inputExtent.width, impl->inputExtent.height, false);

    NVSDK_NGX_Resource_VK depthResource = NVSDK_NGX_Create_ImageView_Resource_VK(
        params.depth, params.depthImage, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
        depthFormat, impl->inputExtent.width, impl->inputExtent.height, false);

    NVSDK_NGX_Resource_VK mvecResource = NVSDK_NGX_Create_ImageView_Resource_VK(
        params.motionVectors, params.motionVectorsImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        mvecFormat, impl->inputExtent.width, impl->inputExtent.height, false);

    NVSDK_NGX_Resource_VK destResource = NVSDK_NGX_Create_ImageView_Resource_VK(
        params.destination, params.destinationImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        destFormat, impl->outputExtent.width, impl->outputExtent.height, true);

    impl->params->Set(NVSDK_NGX_Parameter_Color, &colorResource);
    impl->params->Set(NVSDK_NGX_Parameter_Depth, &depthResource);
    impl->params->Set(NVSDK_NGX_Parameter_MotionVectors, &mvecResource);
    impl->params->Set(NVSDK_NGX_Parameter_Output, &destResource);
    impl->params->Set(NVSDK_NGX_Parameter_Jitter_Offset_X, params.jitterX);
    impl->params->Set(NVSDK_NGX_Parameter_Jitter_Offset_Y, params.jitterY);
    impl->params->Set(NVSDK_NGX_Parameter_Reset, params.reset ? 1 : 0);
    impl->params->Set(NVSDK_NGX_Parameter_MV_Scale_X, 1.0f);
    impl->params->Set(NVSDK_NGX_Parameter_MV_Scale_Y, 1.0f);

    const auto evalResult = api.EvaluateFeature(commands, impl->featureHandle, impl->params, nullptr);
    Graphics::Require(NVSDK_NGX_SUCCEED(evalResult), "Failed to evaluate NVIDIA DLSS feature on command buffer");
}

VkExtent2D DlssUpscaler::InputExtent() const noexcept {
    return impl ? impl->inputExtent : VkExtent2D{};
}

VkExtent2D DlssUpscaler::OutputExtent() const noexcept {
    return impl ? impl->outputExtent : VkExtent2D{};
}

}
