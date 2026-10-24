#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_DUALSENSEHAPTICS_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_DUALSENSEHAPTICS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace DualSenseHaptics {

constexpr std::size_t ReportBytes = 142;
constexpr std::size_t PowerReportBytes = 78;
constexpr std::size_t SamplesOffset = 13;
constexpr std::size_t SamplesBytes = 64;
constexpr std::uint8_t ControllerQueueFrames = 64;

constexpr std::array<std::uint32_t, 256> CrcTable = [] {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t index = 0; index < 256; ++index) {
        std::uint32_t crc = index;
        for (int bit = 0; bit < 8; ++bit) crc = (crc & 1u) != 0 ? 0xEDB88320u ^ (crc >> 1u) : crc >> 1u;
        table[index] = crc;
    }
    return table;
}();

inline void Sign(std::span<std::uint8_t> report) {
    std::uint32_t crc = 0xFFFFFFFFu;
    const auto feed = [&crc](std::uint8_t byte) { crc = CrcTable[(crc ^ byte) & 0xFFu] ^ (crc >> 8u); };
    feed(0xA2);
    for (std::size_t index = 0; index + 4 < report.size(); ++index) feed(report[index]);
    crc ^= 0xFFFFFFFFu;
    for (std::size_t index = 0; index < 4; ++index) report[report.size() - 4 + index] = static_cast<std::uint8_t>(crc >> (8u * index));
}

inline std::array<std::uint8_t, ReportBytes> Report(std::uint8_t sequence, std::uint8_t counter, std::span<const std::int8_t, SamplesBytes> samples) {
    std::array<std::uint8_t, ReportBytes> report{};
    report[0] = 0x32;
    report[1] = static_cast<std::uint8_t>((sequence & 0x0Fu) << 4u);
    report[2] = 0x91;
    report[3] = 0x07;
    report[4] = 0xFE;
    for (std::size_t index = 5; index <= 9; ++index) report[index] = ControllerQueueFrames;
    report[10] = counter;
    report[11] = 0x92;
    report[12] = SamplesBytes;
    std::memcpy(report.data() + SamplesOffset, samples.data(), SamplesBytes);
    Sign(report);
    return report;
}

inline std::array<std::uint8_t, PowerReportBytes> PowerReport(std::uint8_t sequence) {
    std::array<std::uint8_t, PowerReportBytes> report{};
    report[0] = 0x31;
    report[1] = static_cast<std::uint8_t>((sequence & 0x0Fu) << 4u);
    report[2] = 0x10;
    report[4] = 0x02 | 0x20 | 0x40;
    report[42] = 0x01;
    Sign(report);
    return report;
}

}

#endif
