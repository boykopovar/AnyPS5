#pragma once
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

namespace AgcDriver {

struct FrameDumpIoFailure {
    const char* operation = nullptr;
    int error = 0;
    explicit operator bool() const noexcept { return operation != nullptr; }
};

inline FrameDumpIoFailure WriteBmpStream_nid_no_patch(std::FILE* file, std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) noexcept {
    const std::uint32_t imageBytes = width * height * 4;
    const std::uint32_t header[13] = {0, 0, 54, 40, width, static_cast<std::uint32_t>(-static_cast<std::int32_t>(height)), 1u | (32u << 16u), 0, imageBytes, 2835, 2835, 0, 0};
    const std::uint16_t magic = 0x4d42;
    const std::uint32_t fileBytes = 54 + imageBytes;
    const auto write = [&](const void* data, std::size_t size, const char* operation) {
        errno = 0;
        if (std::fwrite(data, 1, size, file) != size) return FrameDumpIoFailure{operation, errno};
        return FrameDumpIoFailure{};
    };
    if (const auto failure = write(&magic, 2, "header")) return failure;
    if (const auto failure = write(&fileBytes, 4, "header")) return failure;
    if (const auto failure = write(header + 1, 48, "header")) return failure;
    return write(pixels.data(), pixels.size(), "pixels");
}

inline FrameDumpIoFailure WriteBmpAndClose_nid_no_patch(std::FILE* file, std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) noexcept {
    auto failure = WriteBmpStream_nid_no_patch(file, width, height, pixels);
    errno = 0;
    if (std::fclose(file) != 0 && !failure) failure = {"close", errno};
    return failure;
}

class FrameDumpOutput {
public:
    bool Enabled_nid_no_patch() const noexcept { return enabled.load(std::memory_order_relaxed); }

    bool Write_nid_no_patch(const char* path, std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) noexcept {
        if (!Enabled_nid_no_patch()) return false;
        errno = 0;
        auto* file = std::fopen(path, "wb");
        const auto failure = file != nullptr ? WriteBmpAndClose_nid_no_patch(file, width, height, pixels) : FrameDumpIoFailure{"open", errno};
        if (!failure) return true;
        if (enabled.exchange(false, std::memory_order_relaxed)) {
            std::fprintf(stderr, "[gpu] optional BMP dumps disabled: %s failed (errno=%d)\n", failure.operation, failure.error);
        }
        return false;
    }

private:
    std::atomic<bool> enabled{true};
};

}
