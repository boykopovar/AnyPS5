#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

class VideoOutTestEnvironment final {
    std::filesystem::path previous;
    std::filesystem::path root;

public:
    VideoOutTestEnvironment() : previous(std::filesystem::current_path()) {
        const auto seed = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) ^
                          (static_cast<std::uint64_t>(std::random_device{}()) << 32u) ^ std::random_device{}();
        for (std::uint32_t attempt = 0; attempt < 64; ++attempt) {
            const auto candidate = std::filesystem::temp_directory_path() /
                                   ("anyps5-video-out-" + std::to_string(seed + attempt));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                root = candidate;
                break;
            }
            if (error) throw std::runtime_error("Cannot create VideoOut test directory: " + error.message());
        }
        if (root.empty()) throw std::runtime_error("Cannot allocate a unique VideoOut test directory");
        try {
            std::filesystem::current_path(root);
            std::filesystem::create_directories("app0/sce_sys");
            std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
            param << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}},"downloadDataSize":0})";
            if (!param) throw std::runtime_error("Cannot write VideoOut param.json fixture");
        } catch (...) {
            std::error_code error;
            std::filesystem::current_path(previous, error);
            std::filesystem::remove_all(root, error);
            throw;
        }
    }

    ~VideoOutTestEnvironment() {
        std::error_code error;
        std::filesystem::current_path(previous, error);
        std::filesystem::remove_all(root, error);
    }

    VideoOutTestEnvironment(const VideoOutTestEnvironment&) = delete;
    VideoOutTestEnvironment& operator=(const VideoOutTestEnvironment&) = delete;
};
