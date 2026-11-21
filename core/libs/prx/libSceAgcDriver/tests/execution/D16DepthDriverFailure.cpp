#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "VulkanTestDevice.hpp"
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static std::shared_ptr<VulkanDevice> Device() {
        try {
            std::shared_ptr<VulkanDevice> created = OpenVulkanTestDevice();
            if (!created) return nullptr;
            auto& driver = Driver::Get();
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            if (driver.device == nullptr) driver.device = std::move(created);
            return driver.device.Load();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return nullptr;
        }
    }
};

}

namespace {

using AgcDriver::Graphics::Buffer;
using AgcDriver::Graphics::Require;

std::string FailureOf(const std::function<void()>& action) {
    try {
        action();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

}

int main() {
    try {
        auto device = AgcDriver::DriverDetail::DriverTestAccess::Device();
        if (!device) return VulkanTestSkipped;
        std::string first;
        std::string second;
        {
            const auto context = device->graphicsContext();
            Require(context.detiler != nullptr, "the driver device has no texture detiler");
            auto* recorder = AgcDriver::Graphics::Recorder::Active();
            Require(recorder != nullptr, "the driver device has no active recorder");
            Buffer input(context, 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            Buffer output(context, 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            const std::array<std::uint32_t, 4> nonfinite{
                std::bit_cast<std::uint32_t>(std::numeric_limits<float>::infinity()),
                std::bit_cast<std::uint32_t>(-std::numeric_limits<float>::infinity()),
                std::bit_cast<std::uint32_t>(std::numeric_limits<float>::quiet_NaN()),
                std::bit_cast<std::uint32_t>(std::numeric_limits<float>::signaling_NaN()),
            };
            std::memcpy(input.Bytes().data(), nonfinite.data(), sizeof(nonfinite));
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                recorder->OpenBatch();
                context.detiler->BeginBatch();
                context.detiler->DispatchDepthConvert(recorder->Commands(), input.Handle(), 0, output.Handle(), 0, 4, 1, 16, 16, VK_FORMAT_D32_SFLOAT_S8_UINT, false, [&](std::function<void()> validation) { recorder->OnValidated(std::move(validation)); });
                recorder->Submit();
            }
            first = FailureOf([] { AgcDriverWaitIdle_nid_postfix(); });
            Require(first.find("nonfinite") != std::string::npos, "the driver wait did not report the conversion failure: " + first);
            second = FailureOf([] { AgcDriverWaitIdle_nid_postfix(); });
            Require(second == first, "the driver failure is not sticky: first '" + first + "', second '" + second + "'");
            bool waitForFailed = false;
            try {
                static_cast<void>(AgcDriver::DriverDetail::Driver::Get().WaitIdleFor(std::chrono::milliseconds(1)));
            } catch (const std::exception& error) {
                waitForFailed = std::string(error.what()) == first;
            }
            Require(waitForFailed, "WaitIdleFor did not rethrow the sticky driver failure");
        }
        device.reset();
        const auto shutdown = FailureOf([] { LibcRunShutdown_nid_postfix(); });
        Require(shutdown.empty() || shutdown == first, "shutdown reported a different failure: " + shutdown);
        Require(AgcDriver::Graphics::Recorder::PendingWriteBackCompletions() == 0, "recorder teardown left pending validation completions");
        std::puts("d16 fallback driver failure tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
