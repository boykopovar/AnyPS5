#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <Testing/Test.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

extern "C" {
int APS5_VABI sceVideoOutOpen(int userId, int busType, int index, const void* param);
int APS5_VABI sceVideoOutClose(int handle);
}

inline bool& VideoOutWasOpened() {
    static bool opened = false;
    return opened;
}

class AppDirectory {
public:
    AppDirectory() : created(!std::filesystem::exists("app0")) {
        std::filesystem::create_directories("app0/sce_sys");
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}},"downloadDataSize":0})";
        Testing::Require(static_cast<bool>(param), "write app0/sce_sys/param.json");
    }

    ~AppDirectory() {
        if (!created) return;
        std::error_code ignored;
        std::filesystem::remove_all("app0", ignored);
    }

    AppDirectory(const AppDirectory&) = delete;
    AppDirectory& operator=(const AppDirectory&) = delete;

private:
    const bool created;
};

template<typename TBody>
void SkipWithoutDisplay(TBody body) {
    try {
        body();
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        Testing::Skip(std::string("no display or Vulkan device: ") + error.what());
    }
}

class OpenVideoOut {
public:
    static constexpr int systemUser = 255;
    static constexpr int mainBus = 0;

    OpenVideoOut() {
        static std::string unavailable;
        if (!unavailable.empty()) Testing::Skip("no display or Vulkan device: " + unavailable);
        try {
            handle = sceVideoOutOpen(systemUser, mainBus, 0, nullptr);
        } catch (const std::runtime_error& error) {
            if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
            unavailable = error.what();
            Testing::Skip("no display or Vulkan device: " + unavailable);
        }
        VideoOutWasOpened() = true;
        Testing::Require(handle > 0, "the main port opens with a positive handle: " + std::to_string(handle));
        open = true;
    }

    ~OpenVideoOut() {
        if (!open) return;
        try {
            sceVideoOutClose(handle);
        } catch (...) {
        }
    }

    OpenVideoOut(const OpenVideoOut&) = delete;
    OpenVideoOut& operator=(const OpenVideoOut&) = delete;

    void Close() {
        open = false;
        Testing::RequireEqual(sceVideoOutClose(handle), 0, "close the main port");
    }

    int handle = 0;

private:
    const AppDirectory app;
    bool open = false;
};

inline int RunVideoOutTests(int argc, char** argv) {
    const int result = Testing::Run(argc, argv);
    if (VideoOutWasOpened()) LibcRunShutdown_nid_postfix();
    return result;
}
