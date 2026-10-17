#ifndef RELINKER_ANALYSIS_INLINEJUMPTABLES_HPP
#define RELINKER_ANALYSIS_INLINEJUMPTABLES_HPP

#include <relinker/domain/Types.hpp>
#include <optional>
#include <span>

namespace Relinker {

std::optional<VirtualAddress> FindInlineJumpTables(std::span<const std::uint8_t> function, VirtualAddress begin);

}

#endif
