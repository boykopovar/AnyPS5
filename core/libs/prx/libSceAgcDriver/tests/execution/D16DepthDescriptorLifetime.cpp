#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "VulkanTestDevice.hpp"
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Buffer;
using AgcDriver::Graphics::CommandBatch;
using AgcDriver::Graphics::Require;

std::uint32_t D16ToD24(std::uint32_t value) {
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(value) * 16777215u + 32767u) / 65535u);
}

void Record(CommandBatch& batch, const AgcDriver::Graphics::Context& context, Buffer& input, Buffer& output, std::uint32_t width, std::uint32_t rows, bool toDepth, VkFormat format) {
    context.detiler->DispatchDepthConvert(batch.Handle(), input.Handle(), 0, output.Handle(), 0, width, rows, width * (toDepth ? 2u : 4u), width * (toDepth ? 4u : 2u), format, toDepth, [&](std::function<void()> validation) { batch.OnValidated(std::move(validation)); });
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        Require(context.detiler != nullptr, "the test device has no texture detiler");
        auto* recorder = AgcDriver::Graphics::Recorder::Active();
        Require(recorder != nullptr, "the test device has no active recorder");
        const auto batchCodes = [](std::uint32_t index) {
            return std::array<std::uint16_t, 2>{static_cast<std::uint16_t>(0x1000u + index * 0x100u), static_cast<std::uint16_t>(0x8000u + index * 0x100u)};
        };
        const auto checkD24Pair = [](Buffer& output, const std::array<std::uint16_t, 2>& expected, const char* what) {
            output.Invalidate();
            std::array<std::uint32_t, 2> values{};
            std::memcpy(values.data(), output.Bytes().data(), sizeof(values));
            Require((values[0] & 0xffffffu) == D16ToD24(expected[0]) && (values[1] & 0xffffffu) == D16ToD24(expected[1]), what);
        };
        Buffer input(context, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const std::array<std::uint16_t, 2> defaultCodes{0x8000u, 0x4000u};
        std::memcpy(input.Bytes().data(), defaultCodes.data(), sizeof(defaultCodes));
        Buffer recorderInput(context, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const std::array<std::uint16_t, 2> recorderCodes{0x5555u, 0xaaaau};
        std::memcpy(recorderInput.Bytes().data(), recorderCodes.data(), sizeof(recorderCodes));
        std::vector<std::unique_ptr<Buffer>> inputs;
        std::vector<std::unique_ptr<Buffer>> outputs;
        std::vector<std::unique_ptr<CommandBatch>> batches;
        Buffer recorderOutput(context, 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            context.detiler->BeginBatch();
            inputs.reserve(65);
            outputs.reserve(65);
            batches.reserve(65);
            for (std::uint32_t index = 0; index < 65u; ++index) {
                if (index == 1u) context.detiler->BeginBatch();
                inputs.push_back(std::make_unique<Buffer>(context, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
                const auto codes = batchCodes(index);
                std::memcpy(inputs.back()->Bytes().data(), codes.data(), sizeof(codes));
                outputs.push_back(std::make_unique<Buffer>(context, 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
                batches.push_back(std::make_unique<CommandBatch>(context));
                Record(*batches.back(), context, *inputs.back(), *outputs.back(), 2, 1, true, VK_FORMAT_D24_UNORM_S8_UINT);
            }
            context.detiler->BeginBatch();
            recorder->OpenBatch();
            context.detiler->DispatchDepthConvert(recorder->Commands(), recorderInput.Handle(), 0, recorderOutput.Handle(), 0, 2, 1, 4, 8, VK_FORMAT_D24_UNORM_S8_UINT, true, [&](std::function<void()> validation) { recorder->OnValidated(std::move(validation)); });
            recorder->Submit();
        }
        for (auto& batch : batches) batch->Submit();
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            for (auto& batch : batches) batch->Wait();
        }
        device->WaitIdle();
        for (std::uint32_t index = 0; index < outputs.size(); ++index) {
            checkD24Pair(*outputs[index], batchCodes(index), "a converted output does not hold both expected D24 codes");
        }
        checkD24Pair(recorderOutput, recorderCodes, "the recorder output does not hold both expected D24 codes");
        auto retained = std::move(batches.back());
        batches.pop_back();
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            context.detiler->BeginBatch();
            CommandBatch afterReset(context);
            Buffer output(context, 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            Record(afterReset, context, input, output, 2, 1, true, VK_FORMAT_D24_UNORM_S8_UINT);
            afterReset.SubmitAndWait();
            checkD24Pair(output, defaultCodes, "the post-reset output does not hold both expected D24 codes");
        }
        device->WaitIdle();
        Buffer faulty(context, 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const std::uint32_t nan = 0x7fc00000u;
        std::memcpy(faulty.Bytes().data(), &nan, sizeof(nan));
        Buffer failedOutput(context, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        std::string failure;
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            CommandBatch bad(context);
            Record(bad, context, faulty, failedOutput, 2, 1, false, VK_FORMAT_D32_SFLOAT_S8_UINT);
            try {
                bad.SubmitAndWait();
            } catch (const std::exception& error) {
                failure = error.what();
                if (failure.find("nonfinite D32 depth value") == std::string::npos) throw;
            }
        }
        Require(!failure.empty(), "a faulted standalone wait did not report its conversion failure");
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            CommandBatch afterFault(context);
            Buffer output(context, 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            Record(afterFault, context, input, output, 2, 1, true, VK_FORMAT_D24_UNORM_S8_UINT);
            afterFault.SubmitAndWait();
        }
        device->WaitIdle();
        static_cast<void>(retained);
        std::puts("d16 fallback descriptor lifetime tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
