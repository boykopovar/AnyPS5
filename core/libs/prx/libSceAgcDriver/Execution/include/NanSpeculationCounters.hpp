#pragma once

#include <cstdint>
#include <optional>

namespace AgcDriver {

struct NanSpeculationCount {
    std::uint32_t entered = 0;
    std::uint32_t exact = 0;
};

[[nodiscard]] std::optional<NanSpeculationCount> ReadNanSpeculationCount(std::uint64_t address, std::uint32_t stage);
void ReportNanSpeculationCounters(const char* label);

}
