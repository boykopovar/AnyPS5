#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t MaxNormalizedLength = 1023;
constexpr std::size_t CopyChunkBytes = 4096;

enum class ReadStatus {
    Ok,
    Unterminated,
    Fault,
};

ReadStatus ReadGuestPath(const char* path, std::size_t pathSize, std::string& result) {
    if (pathSize == 0) return ReadStatus::Unterminated;
    const auto base = reinterpret_cast<std::uintptr_t>(path);
    constexpr auto maxAddress = std::numeric_limits<std::uintptr_t>::max();
    std::size_t offset = 0;
    while (offset < pathSize) {
        if (offset > maxAddress - base) return ReadStatus::Fault;
        const auto address = base + offset;
        const std::size_t pageRemaining = CopyChunkBytes - (address % CopyChunkBytes);
        const std::size_t bytes = std::min(pageRemaining, pathSize - offset);
        char chunk[CopyChunkBytes];
        if (!GuestCopyIn_nid_no_patch(chunk, reinterpret_cast<const void*>(address), bytes)) {
            return ReadStatus::Fault;
        }
        if (const auto* terminator = static_cast<const char*>(std::memchr(chunk, '\0', bytes))) {
            result.append(chunk, static_cast<std::size_t>(terminator - chunk));
            return ReadStatus::Ok;
        }
        result.append(chunk, bytes);
        offset += bytes;
    }
    return ReadStatus::Unterminated;
}

}

extern "C" {

int APS5_VABI sceKernelNormalizePath(const char* path, std::size_t pathSize, char* normalizedPath, std::size_t normalizedSize) {
    if (path == nullptr) return SCE_KERNEL_ERROR_EINVAL;

    std::string input;
    const ReadStatus readStatus = ReadGuestPath(path, pathSize, input);
    if (readStatus == ReadStatus::Fault) return SCE_KERNEL_ERROR_EFAULT;
    if (readStatus == ReadStatus::Unterminated) return SCE_KERNEL_ERROR_EINVAL;

    if (normalizedSize < 2 || normalizedPath == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    if (input.empty() || input[0] != '/') return SCE_KERNEL_ERROR_EINVAL;

    const auto inputAddress = reinterpret_cast<std::uintptr_t>(path);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(normalizedPath);
    constexpr auto maxAddress = std::numeric_limits<std::uintptr_t>::max();
    if (pathSize > maxAddress - inputAddress || normalizedSize > maxAddress - outputAddress) {
        return SCE_KERNEL_ERROR_EINVAL;
    }
    const auto inputEnd = inputAddress + pathSize;
    const auto outputEnd = outputAddress + normalizedSize;
    if (inputEnd > outputAddress && outputEnd > inputAddress) {
        return SCE_KERNEL_ERROR_EINVAL;
    }

    std::string normalized;
    normalized.reserve(MaxNormalizedLength);
    std::size_t cursor = 0;
    while (cursor < input.size()) {
        const std::size_t componentStart = cursor + 1;
        const std::size_t nextSlash = input.find('/', componentStart);
        const bool hasMore = nextSlash != std::string::npos;
        const std::size_t componentEnd = hasMore ? nextSlash : input.size();
        const std::size_t componentLength = componentEnd - componentStart;
        cursor = componentEnd;

        if (componentLength == 0 && hasMore) continue;

        const std::string_view component(input.data() + componentStart, componentLength);
        if (component == ".") continue;
        if (component == "..") {
            const std::size_t lastSlash = normalized.rfind('/');
            if (lastSlash != std::string::npos) normalized.resize(lastSlash);
            continue;
        }
        if (componentLength >= MaxNormalizedLength || normalized.size() > MaxNormalizedLength - 1 - componentLength) {
            return SCE_KERNEL_ERROR_ENAMETOOLONG;
        }
        normalized.push_back('/');
        normalized.append(component);
    }

    if (normalized.empty()) normalized = "/";
    if (normalized.size() + 1 > normalizedSize) return SCE_KERNEL_ERROR_EINVAL;
    if (!GuestCopyOut_nid_no_patch(normalizedPath, normalized.c_str(), normalized.size() + 1)) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    return 0;
}

}
