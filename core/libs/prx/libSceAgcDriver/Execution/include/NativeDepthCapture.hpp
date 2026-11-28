#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_NATIVEDEPTHCAPTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_NATIVEDEPTHCAPTURE_HPP

#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace AgcDriver {

struct NativeDepthSnapshot {
    Graphics::DepthTarget target{};
    VkFormat hostFormat = VK_FORMAT_UNDEFINED;
    std::uint64_t depthOrder = 0;
    std::uint64_t stencilOrder = 0;
    std::vector<std::byte> depth;
    std::vector<std::byte> stencil;
};

inline constexpr std::string_view NativeDepthCaptureName = "depth.bin";
inline constexpr std::string_view NativeDepthCaptureMagic = "aps5-native-depth 2\n";
inline constexpr std::uint64_t NativeDepthSurfaceLimit = std::uint64_t{1} << 30u;
inline constexpr std::uint64_t NativeDepthCaptureLimit = std::uint64_t{4} << 30u;

std::uint64_t ValidateNativeDepthSnapshots(std::span<const NativeDepthSnapshot> snapshots);
void WriteNativeDepthCapture(const std::filesystem::path& path, std::span<const NativeDepthSnapshot> snapshots);
std::vector<NativeDepthSnapshot> ReadNativeDepthCapture(const std::filesystem::path& path);

}

#endif
