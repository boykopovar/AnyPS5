#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
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
using AgcDriver::Graphics::TextureDetiler;

constexpr std::uint32_t Width = 256;
constexpr std::uint32_t Height = 256;
constexpr std::uint32_t Texels = Width * Height;

std::uint32_t D16ToD24(std::uint32_t value) {
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(value) * 16777215u + 32767u) / 65535u);
}

std::uint32_t D24ToD16(std::uint32_t value) {
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(value & 0xffffffu) * 65535u + 8388607u) / 16777215u);
}

std::uint32_t D32ToD16(float value) {
    if (std::isnan(value)) return 0u;
    if (value <= 0.0f) return 0u;
    if (value >= 1.0f) return 65535u;
    return static_cast<std::uint32_t>(std::rintl(static_cast<long double>(value) * 65535.0L));
}

std::uint32_t FloatBits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::vector<std::uint16_t> ReadU16(Buffer& buffer, std::size_t count) {
    std::vector<std::uint16_t> values(count);
    std::memcpy(values.data(), buffer.Bytes().data(), count * sizeof(std::uint16_t));
    return values;
}

std::vector<std::uint32_t> ReadU32(Buffer& buffer, std::size_t count) {
    std::vector<std::uint32_t> values(count);
    std::memcpy(values.data(), buffer.Bytes().data(), count * sizeof(std::uint32_t));
    return values;
}

void WriteU16(Buffer& buffer, const std::vector<std::uint16_t>& values) {
    std::memcpy(buffer.Bytes().data(), values.data(), values.size() * sizeof(std::uint16_t));
}

void WriteU32(Buffer& buffer, const std::vector<std::uint32_t>& values) {
    std::memcpy(buffer.Bytes().data(), values.data(), values.size() * sizeof(std::uint32_t));
}

void RequireRoundTrip(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context, VkFormat depthFormat) {
    const std::size_t u16Bytes = static_cast<std::size_t>(Texels) * 2u;
    const std::size_t u32Bytes = static_cast<std::size_t>(Texels) * 4u;
    Buffer input(context, u16Bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer intermediate(context, u32Bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer output(context, u16Bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::vector<std::uint16_t> codes(Texels);
    for (std::uint32_t value = 0; value < 65536u; ++value) codes[value] = static_cast<std::uint16_t>(value);
    WriteU16(input, codes);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        context.detiler->BeginBatch();
        CommandBatch batch(context);
        context.detiler->DispatchDepthConvert(batch.Handle(), input.Handle(), 0, intermediate.Handle(), 0, Width, Height, Width * 2u, Width * 4u, depthFormat, true, [&](std::function<void()> validation) { batch.OnValidated(std::move(validation)); });
        AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        context.detiler->DispatchDepthConvert(batch.Handle(), intermediate.Handle(), 0, output.Handle(), 0, Width, Height, Width * 4u, Width * 2u, depthFormat, false, [&](std::function<void()> validation) { batch.OnValidated(std::move(validation)); });
        batch.SubmitAndWait();
    }
    device.WaitIdle();
    output.Invalidate();
    const auto result = ReadU16(output, Texels);
    intermediate.Invalidate();
    const auto encoded = ReadU32(intermediate, Texels);
    if (result != codes) {
        for (std::size_t index = 0; index < Texels; ++index) {
            if (result[index] == codes[index]) continue;
            std::fprintf(stderr, "D16 roundtrip mismatch code=%u got=%u\n", codes[index], result[index]);
            break;
        }
    }
    Require(result == codes, std::string("a D16 round trip through ") + (depthFormat == VK_FORMAT_D24_UNORM_S8_UINT ? "D24" : "D32") + " changed a code");
    if (depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
        for (std::uint32_t value = 0; value < 65536u; ++value) {
            const auto expected = D16ToD24(value);
            Require((encoded[value] & 0xffffffu) == expected, "the D16 to D24 encoding of code " + std::to_string(value) + " is not the nearest value");
            Require(D24ToD16(encoded[value]) == value, "the D24 to D16 decoding of code " + std::to_string(value) + " did not return the code");
        }
    }
}

void RequireD24Rounding(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context) {
    constexpr std::uint32_t rows = 3;
    const std::size_t valuesCount = static_cast<std::size_t>(Texels) * rows;
    Buffer input(context, valuesCount * 4u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer output(context, valuesCount * 2u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::vector<std::uint32_t> values(valuesCount);
    for (std::uint32_t bucket = 0; bucket < 65536u; ++bucket) {
        const auto center = static_cast<std::uint32_t>((static_cast<std::uint64_t>(bucket) * 16777215u + 32767u) / 65535u);
        values[bucket] = 0xab000000u | center;
        values[Texels + bucket] = 0xab000000u | (center == 0u ? 0u : center - 1u);
        values[2u * Texels + bucket] = 0xab000000u | (center == 0xffffffu ? 0xffffffu : center + 1u);
    }
    WriteU32(input, values);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        context.detiler->BeginBatch();
        CommandBatch batch(context);
        context.detiler->DispatchDepthConvert(batch.Handle(), input.Handle(), 0, output.Handle(), 0, Texels, rows, Texels * 4u, Texels * 2u, VK_FORMAT_D24_UNORM_S8_UINT, false, [&](std::function<void()> validation) { batch.OnValidated(std::move(validation)); });
        batch.SubmitAndWait();
    }
    device.WaitIdle();
    output.Invalidate();
    const auto result = ReadU16(output, valuesCount);
    for (std::uint32_t index = 0; index < valuesCount; ++index) Require(result[index] == D24ToD16(values[index]), "the D24 to D16 conversion of texel " + std::to_string(index) + " differs from the exact normalized value");
}

void RequireOffsetRectangles(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context, VkFormat depthFormat) {
    constexpr std::uint32_t sourceWords = 64;
    constexpr std::uint32_t destinationWords = 64;
    Buffer input(context, sourceWords * 4u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer output(context, destinationWords * 4u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::vector<std::uint32_t> source(sourceWords);
    for (std::uint32_t index = 0; index < sourceWords; ++index) source[index] = ((index * 0x9e37u) & 0xffffu) | (((index * 0x85ebu) & 0xffffu) << 16u);
    std::vector<std::uint32_t> sentinel(destinationWords, 0xdeadbeefu);
    std::memcpy(input.Bytes().data(), source.data(), source.size() * sizeof(std::uint32_t));
    std::memcpy(output.Bytes().data(), sentinel.data(), sentinel.size() * sizeof(std::uint32_t));
    constexpr std::uint32_t firstOffset = 4;
    constexpr std::uint32_t secondOffset = 64;
    constexpr std::uint32_t firstWidth = 5;
    constexpr std::uint32_t firstRows = 3;
    constexpr std::uint32_t secondWidth = 3;
    constexpr std::uint32_t secondRows = 2;
    constexpr std::uint32_t firstSource = 8;
    constexpr std::uint32_t secondSource = 40;
    constexpr std::uint32_t sourcePitch = 12;
    constexpr std::uint32_t destinationPitch = 20;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        context.detiler->BeginBatch();
        CommandBatch batch(context);
        const auto registerValidation = [&](std::function<void()> validation) { batch.OnValidated(std::move(validation)); };
        context.detiler->DispatchDepthConvert(batch.Handle(), input.Handle(), firstSource, output.Handle(), firstOffset, firstWidth, firstRows, sourcePitch, destinationPitch, depthFormat, true, registerValidation);
        context.detiler->DispatchDepthConvert(batch.Handle(), input.Handle(), secondSource, output.Handle(), secondOffset, secondWidth, secondRows, sourcePitch, destinationPitch, depthFormat, true, registerValidation);
        batch.SubmitAndWait();
    }
    device.WaitIdle();
    output.Invalidate();
    const auto result = ReadU32(output, destinationWords);
    if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        for (std::uint32_t word = 0; word < destinationWords; ++word) {
            const bool inFirst = word >= firstOffset / 4u && word < firstOffset / 4u + firstRows * (destinationPitch / 4u) && (word - firstOffset / 4u) % (destinationPitch / 4u) < firstWidth;
            const bool inSecond = word >= secondOffset / 4u && word < secondOffset / 4u + secondRows * (destinationPitch / 4u) && (word - secondOffset / 4u) % (destinationPitch / 4u) < secondWidth;
            if (!inFirst && !inSecond) {
                Require(result[word] == 0xdeadbeefu, "a D32 conversion rectangle changed a neighbouring word");
                continue;
            }
            const auto relative = inFirst ? word - firstOffset / 4u : word - secondOffset / 4u;
            const auto column = relative % (destinationPitch / 4u);
            const auto row = relative / (destinationPitch / 4u);
            const auto sourceOffset = inFirst ? firstSource : secondSource;
            const auto packed = source[sourceOffset / 4u + row * (sourcePitch / 4u) + column / 2u];
            const auto code = (packed >> ((column & 1u) * 16u)) & 0xffffu;
            const auto got = D32ToD16(std::bit_cast<float>(result[word]));
            if (got != code) {
                char text[192];
                std::snprintf(text, sizeof(text), "a D32 conversion rectangle word %u is 0x%08x (%u), expected code %u (column %u packed 0x%08x first %d)", word, result[word], got, code, column, packed, inFirst ? 1 : 0);
                Require(false, text);
            }
        }
        return;
    }
    std::vector<std::uint32_t> expected = sentinel;
    const auto place = [&](std::uint32_t sourceOffset, std::uint32_t destinationOffset, std::uint32_t width, std::uint32_t rows, std::uint32_t destinationRow) {
        for (std::uint32_t row = 0; row < rows; ++row) {
            for (std::uint32_t column = 0; column < width; ++column) {
                const auto packed = source[sourceOffset / 4u + row * (sourcePitch / 4u) + column / 2u];
                const auto value = (packed >> ((column & 1u) * 16u)) & 0xffffu;
                expected[destinationOffset / 4u + row * (destinationRow / 4u) + column] = D16ToD24(value);
            }
        }
    };
    place(firstSource, firstOffset, firstWidth, firstRows, destinationPitch);
    place(secondSource, secondOffset, secondWidth, secondRows, destinationPitch);
    for (std::uint32_t word = 0; word < destinationWords; ++word) {
        if (result[word] == expected[word]) continue;
        char text[192];
        std::snprintf(text, sizeof(text), "a D24 conversion rectangle word %u is 0x%08x, expected 0x%08x", word, result[word], expected[word]);
        Require(false, text);
    }
}

void RequireD32Rounding(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context) {
    const std::size_t bytes = static_cast<std::size_t>(Texels) * 4u;
    Buffer input(context, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer output(context, static_cast<std::size_t>(Texels) * 2u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::vector<std::uint32_t> values(Texels);
    std::vector<float> floats(Texels);
    std::uint32_t state = 0x12345678u;
    for (std::uint32_t index = 0; index < Texels; ++index) {
        state = state * 1664525u + 1013904223u;
        floats[index] = static_cast<float>(state & 0xffffffu) / 16777215.0f;
        floats[index] = std::clamp(floats[index], 0.0f, 1.0f);
        if (index >= 14u) {
            const auto bucket = (index - 14u) / 3u;
            const auto variant = (index - 14u) % 3u;
            const auto tie = (static_cast<double>(bucket) + 0.5) / 65535.0;
            auto value = static_cast<float>(tie);
            if (variant == 1u) value = std::nextafter(value, 0.0f);
            else if (variant == 2u) value = std::nextafter(value, 1.0f);
            floats[index] = value;
        }
        values[index] = FloatBits(floats[index]);
    }
    values[0] = FloatBits(-1.0f);
    values[1] = FloatBits(0.0f);
    values[2] = FloatBits(0.5f);
    values[3] = FloatBits(1.0f);
    values[4] = FloatBits(2.0f);
    values[5] = FloatBits(0.25f);
    values[6] = FloatBits(0.75f);
    values[7] = FloatBits(1.5f);
    values[8] = FloatBits(-0.0f);
    values[9] = FloatBits(std::numeric_limits<float>::denorm_min());
    values[10] = FloatBits(std::nextafter(0.0f, 1.0f));
    values[11] = FloatBits(std::nextafter(1.0f, 0.0f));
    values[12] = FloatBits(-std::numeric_limits<float>::denorm_min());
    values[13] = FloatBits(0.999f);
    WriteU32(input, values);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        context.detiler->BeginBatch();
        CommandBatch batch(context);
        context.detiler->DispatchDepthConvert(batch.Handle(), input.Handle(), 0, output.Handle(), 0, Width, Height, Width * 4u, Width * 2u, VK_FORMAT_D32_SFLOAT_S8_UINT, false, [&](std::function<void()> validation) { batch.OnValidated(std::move(validation)); });
        batch.SubmitAndWait();
    }
    device.WaitIdle();
    output.Invalidate();
    const auto result = ReadU16(output, Texels);
    for (std::uint32_t index = 0; index < Texels; ++index) {
        const auto expected = D32ToD16(std::bit_cast<float>(values[index]));
        if (result[index] != expected) {
            char text[256];
            std::snprintf(text, sizeof(text), "the D32 to D16 conversion of texel %u differs: got %u, expected %u, bits 0x%08x", index, result[index], expected, values[index]);
            Require(false, text);
        }
    }
}

void RequireOddWidth(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context, VkFormat depthFormat) {
    constexpr std::uint32_t oddWidth = 255;
    constexpr std::uint32_t rows = 17;
    constexpr VkDeviceSize sourceOffset = 4;
    constexpr VkDeviceSize destinationOffset = 8;
    const std::size_t inputBytes = static_cast<std::size_t>((oddWidth + 1u) / 2u) * rows * 4u + sourceOffset;
    const std::size_t outputBytes = static_cast<std::size_t>(oddWidth) * rows * 4u + destinationOffset;
    Buffer input(context, inputBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer output(context, outputBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::vector<std::uint16_t> values(static_cast<std::size_t>(oddWidth) * rows);
    for (std::uint32_t index = 0; index < values.size(); ++index) values[index] = static_cast<std::uint16_t>(index * 37u);
    std::vector<std::uint32_t> packed((inputBytes - sourceOffset + 3u) / 4u, 0u);
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto row = index / oddWidth;
        const auto column = index % oddWidth;
        const auto word = row * ((oddWidth + 1u) / 2u) + column / 2u;
        packed[word] |= static_cast<std::uint32_t>(values[index]) << ((column & 1u) * 16u);
    }
    std::memcpy(input.Bytes().data() + sourceOffset, packed.data(), packed.size() * sizeof(std::uint32_t));
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        context.detiler->BeginBatch();
        CommandBatch batch(context);
        context.detiler->DispatchDepthConvert(batch.Handle(), input.Handle(), sourceOffset, output.Handle(), destinationOffset, oddWidth, rows, (oddWidth + 1u) / 2u * 4u, oddWidth * 4u, depthFormat, true, [&](std::function<void()> validation) { batch.OnValidated(std::move(validation)); });
        batch.SubmitAndWait();
    }
    device.WaitIdle();
    output.Invalidate();
    std::vector<std::uint32_t> result(static_cast<std::size_t>(oddWidth) * rows);
    std::memcpy(result.data(), output.Bytes().data() + destinationOffset, result.size() * sizeof(std::uint32_t));
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
            const auto expected = D16ToD24(values[index]);
            Require((result[index] & 0xffffffu) == expected, "an odd-width D16 to D24 conversion differs at texel " + std::to_string(index));
        } else {
            Require(D32ToD16(std::bit_cast<float>(result[index])) == values[index], "an odd-width D16 to D32 conversion does not return its code at texel " + std::to_string(index));
        }
    }
}

void RequireNonfiniteFault(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context) {
    const std::array<std::uint32_t, 6> nonfinite{FloatBits(std::numeric_limits<float>::infinity()), FloatBits(-std::numeric_limits<float>::infinity()), FloatBits(std::numeric_limits<float>::quiet_NaN()), FloatBits(-std::numeric_limits<float>::quiet_NaN()), FloatBits(std::numeric_limits<float>::signaling_NaN()), FloatBits(-std::numeric_limits<float>::signaling_NaN())};
    for (const auto value : nonfinite) {
        Buffer input(context, 12, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        Buffer output(context, 12, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const std::array<std::uint32_t, 3> texels{FloatBits(0.25f), value, FloatBits(0.75f)};
        std::memcpy(input.Bytes().data(), texels.data(), sizeof(texels));
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            context.detiler->BeginBatch();
            CommandBatch batch(context);
            context.detiler->DispatchDepthConvert(batch.Handle(), input.Handle(), 0, output.Handle(), 0, 3, 1, 12, 12, VK_FORMAT_D32_SFLOAT_S8_UINT, false, [&](std::function<void()> validation) { batch.OnValidated(std::move(validation)); });
            batch.Submit();
            bool first = false;
            try {
                batch.Wait();
            } catch (const std::exception&) {
                first = true;
            }
            Require(first, "a nonfinite D32 depth value did not fail the standalone batch");
            bool second = false;
            try {
                batch.Wait();
            } catch (const std::exception&) {
                second = true;
            }
            Require(second, "a second standalone wait did not rethrow the captured conversion failure");
            CommandBatch submitted(context);
            context.detiler->DispatchDepthConvert(submitted.Handle(), input.Handle(), 0, output.Handle(), 0, 3, 1, 12, 12, VK_FORMAT_D32_SFLOAT_S8_UINT, false, [&](std::function<void()> validation) { submitted.OnValidated(std::move(validation)); });
            bool third = false;
            try {
                submitted.SubmitAndWait();
            } catch (const std::exception&) {
                third = true;
            }
            Require(third, "SubmitAndWait did not report a nonfinite D32 depth value");
        }
    }
    device.WaitIdle();
}

void RequireRecorderFaultDrain(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::Context& context) {
    auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "the test device has no active recorder");
    Buffer validA(context, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer validC(context, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer faulty(context, 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer output(context, 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto quarter = FloatBits(0.25f);
    const std::array<std::uint32_t, 4> nonfinite{FloatBits(std::numeric_limits<float>::infinity()), FloatBits(-std::numeric_limits<float>::infinity()), FloatBits(std::numeric_limits<float>::quiet_NaN()), FloatBits(std::numeric_limits<float>::signaling_NaN())};
    std::memcpy(validA.Bytes().data(), &quarter, sizeof(quarter));
    std::memcpy(validC.Bytes().data(), &quarter, sizeof(quarter));
    std::memcpy(faulty.Bytes().data(), nonfinite.data(), sizeof(nonfinite));
    bool completionA = false;
    bool completionB = false;
    bool completionC = false;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto registerValidation = [&](std::function<void()> validation) { recorder->OnValidated(std::move(validation)); };
        recorder->OpenBatch();
        context.detiler->DispatchDepthConvert(recorder->Commands(), validA.Handle(), 0, output.Handle(), 0, 1, 1, 4, 4, VK_FORMAT_D32_SFLOAT_S8_UINT, true, registerValidation);
        recorder->OnComplete([&completionA] { completionA = true; });
        recorder->Submit();
        context.detiler->DispatchDepthConvert(recorder->Commands(), faulty.Handle(), 0, output.Handle(), 0, 4, 1, 16, 16, VK_FORMAT_D32_SFLOAT_S8_UINT, false, registerValidation);
        recorder->OnComplete([&completionB] { completionB = true; });
        recorder->Submit();
        context.detiler->DispatchDepthConvert(recorder->Commands(), validC.Handle(), 0, output.Handle(), 0, 1, 1, 4, 4, VK_FORMAT_D32_SFLOAT_S8_UINT, true, registerValidation);
        recorder->OnComplete([&completionC] { completionC = true; });
    }
    bool threw = false;
    try {
        device.WaitIdle();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "a recorder validation failure was not reported at a normal completion boundary");
    Require(completionA, "a valid earlier batch did not complete before another batch failed validation");
    Require(!completionB && !completionC, "a failed or later batch published a successful completion");
    bool repeated = false;
    try {
        device.WaitIdle();
    } catch (const std::exception&) {
        repeated = true;
    }
    Require(repeated, "a repeated host wait did not report the sticky recorder failure");
    bool through = false;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        try {
            recorder->SyncThrough(0xdead0000u, 4u, false);
        } catch (const std::exception&) {
            through = true;
        }
    }
    Require(through, "a targeted guest-address sync did not report the sticky recorder failure");
    bool reaped = false;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        try {
            recorder->Reap();
        } catch (const std::exception&) {
            reaped = true;
        }
    }
    Require(reaped, "the completion reaper did not expose the sticky validation failure");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        Require(context.detiler != nullptr, "the test device has no texture detiler");
        RequireRoundTrip(*device, context, VK_FORMAT_D24_UNORM_S8_UINT);
        RequireRoundTrip(*device, context, VK_FORMAT_D32_SFLOAT_S8_UINT);
        RequireD24Rounding(*device, context);
        RequireD32Rounding(*device, context);
        RequireOddWidth(*device, context, VK_FORMAT_D24_UNORM_S8_UINT);
        RequireOddWidth(*device, context, VK_FORMAT_D32_SFLOAT_S8_UINT);
        RequireOffsetRectangles(*device, context, VK_FORMAT_D24_UNORM_S8_UINT);
        RequireOffsetRectangles(*device, context, VK_FORMAT_D32_SFLOAT_S8_UINT);
        RequireNonfiniteFault(*device, context);
        device->WaitIdle();
        RequireRecorderFaultDrain(*device, context);
        std::puts("d16 fallback depth conversion tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
