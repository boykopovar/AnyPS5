#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_NGXAPI_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_NGXAPI_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>

namespace AgcDriver {

using NVSDK_NGX_Result = std::uint32_t;

inline constexpr NVSDK_NGX_Result NVSDK_NGX_Result_Success = 0x00000001;
inline constexpr NVSDK_NGX_Result NVSDK_NGX_Result_FAIL_FeatureNotSupported = 0x80040003;

inline bool NVSDK_NGX_SUCCEED(NVSDK_NGX_Result res) noexcept {
    return (res & 0x80000000) == 0;
}

inline bool NVSDK_NGX_FAILED(NVSDK_NGX_Result res) noexcept {
    return (res & 0x80000000) != 0;
}

enum NVSDK_NGX_Version : std::uint32_t {
    NVSDK_NGX_Version_API = 0x00000014
};

enum NVSDK_NGX_Feature : std::uint32_t {
    NVSDK_NGX_Feature_SuperSampling = 1
};

enum NVSDK_NGX_PerfQuality_Value : std::uint32_t {
    NVSDK_NGX_PerfQuality_Value_MaxPerf = 0,
    NVSDK_NGX_PerfQuality_Value_Balanced = 1,
    NVSDK_NGX_PerfQuality_Value_MaxQuality = 2,
    NVSDK_NGX_PerfQuality_Value_UltraPerformance = 3,
    NVSDK_NGX_PerfQuality_Value_UltraQuality = 4
};

enum NVSDK_NGX_DLSS_Feature_Flags : std::uint32_t {
    NVSDK_NGX_DLSS_Feature_Flags_AutoExposure = 1 << 0,
    NVSDK_NGX_DLSS_Feature_Flags_MVLowRes = 1 << 2,
    NVSDK_NGX_DLSS_Feature_Flags_DepthInverted = 1 << 4
};

inline constexpr const char* NVSDK_NGX_Parameter_Width = "Width";
inline constexpr const char* NVSDK_NGX_Parameter_Height = "Height";
inline constexpr const char* NVSDK_NGX_Parameter_OutWidth = "OutWidth";
inline constexpr const char* NVSDK_NGX_Parameter_OutHeight = "OutHeight";
inline constexpr const char* NVSDK_NGX_Parameter_Color = "Color";
inline constexpr const char* NVSDK_NGX_Parameter_Depth = "Depth";
inline constexpr const char* NVSDK_NGX_Parameter_MotionVectors = "MotionVectors";
inline constexpr const char* NVSDK_NGX_Parameter_Output = "Output";
inline constexpr const char* NVSDK_NGX_Parameter_Jitter_Offset_X = "Jitter.Offset.X";
inline constexpr const char* NVSDK_NGX_Parameter_Jitter_Offset_Y = "Jitter.Offset.Y";
inline constexpr const char* NVSDK_NGX_Parameter_Reset = "Reset";
inline constexpr const char* NVSDK_NGX_Parameter_MV_Scale_X = "MV.Scale.X";
inline constexpr const char* NVSDK_NGX_Parameter_MV_Scale_Y = "MV.Scale.Y";
inline constexpr const char* NVSDK_NGX_Parameter_PerfQualityValue = "PerfQualityValue";
inline constexpr const char* NVSDK_NGX_Parameter_Sharpness = "Sharpness";
inline constexpr const char* NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags = "DLSS.Feature.Create.Flags";
inline constexpr const char* NVSDK_NGX_Parameter_SuperSampling_Available = "SuperSampling.Available";
inline constexpr const char* NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver = "SuperSampling.NeedsUpdatedDriver";
inline constexpr const char* NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult = "SuperSampling.FeatureInitResult";

struct NVSDK_NGX_Handle {
    unsigned int Id;
};

enum NVSDK_NGX_Resource_VK_Type : std::uint32_t {
    NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW = 0,
    NVSDK_NGX_RESOURCE_VK_TYPE_VK_BUFFER = 1
};

struct NVSDK_NGX_ImageViewInfo_VK {
    VkImageView ImageView;
    VkImage Image;
    VkImageSubresourceRange SubresourceRange;
    VkFormat Format;
    unsigned int Width;
    unsigned int Height;
};

struct NVSDK_NGX_BufferInfo_VK {
    VkBuffer Buffer;
    unsigned int SizeInBytes;
};

struct NVSDK_NGX_Resource_VK {
    union {
        NVSDK_NGX_ImageViewInfo_VK ImageViewInfo;
        NVSDK_NGX_BufferInfo_VK BufferInfo;
    } Resource;
    NVSDK_NGX_Resource_VK_Type Type;
    bool ReadWrite;
};

inline NVSDK_NGX_Resource_VK NVSDK_NGX_Create_ImageView_Resource_VK(VkImageView imageView, VkImage image, VkImageSubresourceRange subresourceRange, VkFormat format, unsigned int width, unsigned int height, bool readWrite) {
    NVSDK_NGX_Resource_VK resourceVK{};
    resourceVK.Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
    resourceVK.Resource.ImageViewInfo.ImageView = imageView;
    resourceVK.Resource.ImageViewInfo.Image = image;
    resourceVK.Resource.ImageViewInfo.SubresourceRange = subresourceRange;
    resourceVK.Resource.ImageViewInfo.Height = height;
    resourceVK.Resource.ImageViewInfo.Width = width;
    resourceVK.Resource.ImageViewInfo.Format = format;
    resourceVK.ReadWrite = readWrite;
    return resourceVK;
}

struct ID3D11Resource;
struct ID3D12Resource;

struct NVSDK_NGX_Parameter {
    virtual void Set(const char* InName, unsigned long long InValue) = 0;
    virtual void Set(const char* InName, float InValue) = 0;
    virtual void Set(const char* InName, double InValue) = 0;
    virtual void Set(const char* InName, unsigned int InValue) = 0;
    virtual void Set(const char* InName, int InValue) = 0;
    virtual void Set(const char* InName, ID3D11Resource* InValue) = 0;
    virtual void Set(const char* InName, ID3D12Resource* InValue) = 0;
    virtual void Set(const char* InName, void* InValue) = 0;

    virtual NVSDK_NGX_Result Get(const char* InName, unsigned long long* OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char* InName, float* OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char* InName, double* OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char* InName, unsigned int* OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char* InName, int* OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char* InName, ID3D11Resource** OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char* InName, ID3D12Resource** OutValue) const = 0;
    virtual NVSDK_NGX_Result Get(const char* InName, void** OutValue) const = 0;

    virtual void Reset() = 0;
};

using PFN_NVSDK_NGX_ProgressCallback = void (*)(float InProgress, bool* OutCancel);

}

#endif
