#include "../../tools/OracleDispatch.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t ExtraBase = 0x5a000000;
constexpr std::uint32_t ExtraStep = 0x00010203;

constexpr std::array<std::uint32_t, 40> Text{
    0xf4080100, 0xfa000000, 0x34020084, 0x34040086, 0xbf8cc07f, 0xdc388000, 0x04040001, 0xbf8c3f70,
    0x7e140280, 0x7e160280, 0x7e180280, 0x7e1a0280, 0x7e1c0280, 0x7e1e0280, 0x7e200280, 0x7e220280,
    0x7e240280, 0x7e260280, 0x7e280280, 0x7e2a0280, 0x7e2c0280, 0x7e2e0280, 0x7e300280, 0x7e320280,
    0x34500082, 0x4a5050ff, 0x00000400, 0xdc308000, 0x0b040028, 0xbf8c3f70, 0x64140b04, 0xdc788000,
    0x00060a02, 0xdc788010, 0x00060e02, 0xdc788020, 0x00061202, 0xdc788030, 0x00061602, 0xbf810000,
};

struct Row {
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t expected;
    bool denormal;
};

constexpr std::array<Row, 6> Rows{{
    {0x3c00, 0x3c00, 0x4000, false},
    {0x0001, 0x0001, 0x0002, true},
    {0x4200, 0x4400, 0x4700, false},
    {0x3c00, 0xbc00, 0x0000, false},
    {0x7bff, 0x3c00, 0x7bff, false},
    {0x7bff, 0x7bff, 0x7c00, false},
}};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void Rejects(const std::string& label, AgcDriver::VulkanDevice& device, const OracleKernel& kernel, std::uint32_t threads, std::span<const std::byte> input) {
    try {
        RunOracleKernel(device, kernel, threads, input);
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error(label + ": accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        OracleKernel kernel{};
        kernel.text.assign(Text.begin(), Text.end());
        kernel.ldsBytes = 4096;
        kernel.waveSize = 32;
        kernel.floatMode = {0xc0u, true, false, false};
        kernel.groupIdEnable = {true, false, false};
        kernel.tgSizeEnable = false;
        kernel.threadIdComponentCount = 1;

        constexpr std::uint32_t threads = Threads;
        std::vector<std::byte> input(threads * 16 + threads * 4);
        for (std::uint32_t lane = 0; lane < threads; ++lane) {
            const auto& row = Rows[lane % Rows.size()];
            const std::array<std::uint32_t, 4> words{row.a, row.b, 0, 0};
            std::memcpy(input.data() + lane * 16, words.data(), sizeof(words));
            const std::uint32_t extra = ExtraBase + lane * ExtraStep;
            std::memcpy(input.data() + threads * 16 + lane * 4, &extra, sizeof(extra));
        }
        const auto output = RunOracleKernel(*device, kernel, threads, input);
        Require(output.size() == threads * 64, "output size is not 64 bytes per lane");
        const bool keepsDenormals = device->Target().float16DenormPreserve;
        std::uint32_t flushedDenormalRows = 0;
        for (std::uint32_t lane = 0; lane < threads; ++lane) {
            std::array<std::uint32_t, 16> dwords{};
            std::memcpy(dwords.data(), output.data() + lane * 64, sizeof(dwords));
            const auto& row = Rows[lane % Rows.size()];
            if (dwords[0] != row.expected && row.denormal && !keepsDenormals && dwords[0] == 0u) {
                ++flushedDenormalRows;
            } else {
                Require(dwords[0] == row.expected, "lane " + std::to_string(lane) + " v10 is " + std::to_string(dwords[0]) + ", expected " + std::to_string(row.expected));
            }
            Require(dwords[1] == ExtraBase + lane * ExtraStep, "lane " + std::to_string(lane) + " v11 is " + std::to_string(dwords[1]) + ", expected its extra dword " + std::to_string(ExtraBase + lane * ExtraStep));
            for (std::size_t index = 2; index < dwords.size(); ++index) {
                Require(dwords[index] == 0, "lane " + std::to_string(lane) + " v" + std::to_string(10 + index) + " is not 0");
            }
        }

        if (flushedDenormalRows != 0) std::printf("the device does not preserve f16 denormals and flushed the denormal row to 0 in %u lanes; that row is not checked there\n", flushedDenormalRows);

        Rejects("zero threads", *device, kernel, 0, input);
        Rejects("too many threads", *device, kernel, 1025, input);
        Rejects("threads not a wave multiple", *device, kernel, 48, input);
        Rejects("short input", *device, kernel, threads, std::span<const std::byte>(input).first(threads * 16 - 1));
        std::puts("oracle runner passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
