#include "prx/libSceAgc/include/Agc.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::size_t Guard = 16;
alignas(256) std::array<std::uint32_t, 3 * 65536 / 4 + 64> output{};
constexpr std::array<std::uint32_t, 4> Pattern{0x7fc01234u, 0x80000000u, 0x01234567u, 0xffffffffu};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void fill(std::uint64_t size, bool alias) {
    output.fill(Untouched);
    auto pattern = Pattern;
    auto* destination = output.data() + Guard;
    if (alias) std::copy(Pattern.begin(), Pattern.end(), destination);
    const auto before = output;
    std::array<std::uint32_t, 256> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    check(sceAgcCbMemsetExclusive(&buffer, destination, alias ? destination : pattern.data(), size) == words.data(), "wrong command pointer");
    check(output == before, "recording performed the fill on the CPU");
    pattern.fill(0);
    Packet packet{words.data(), static_cast<std::uint32_t>(buffer.cursor_up - words.data()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(output.data()), sizeof(output));
    const auto written = static_cast<std::size_t>(size / 16u) * 4u;
    for (std::size_t i = 0; i < output.size(); ++i) {
        const auto expected = i >= Guard && i - Guard < written ? Pattern[(i - Guard) % 4u] : before[i];
        if (output[i] != expected) throw std::runtime_error("size " + std::to_string(size) + " word " + std::to_string(i) + " got " + std::to_string(output[i]) + " expected " + std::to_string(expected));
    }
}

} // namespace

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        for (const auto size : {0ull, 1ull, 15ull, 16ull, 17ull, 65535ull, 65536ull, 65537ull, 196624ull}) fill(size, false);
        fill(196624, true);
        AgcDriverShutdown_nid_postfix();
        std::puts("exclusive memset GPU pattern, boundary, alias and guard tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception&) {
        }
        return 1;
    }
}
