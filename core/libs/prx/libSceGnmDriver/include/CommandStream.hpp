#ifndef CORE_LIBS_PRX_LIBSCEGNMDRIVER_INCLUDE_COMMANDSTREAM_HPP
#define CORE_LIBS_PRX_LIBSCEGNMDRIVER_INCLUDE_COMMANDSTREAM_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace GnmDriver {

std::span<const std::uint32_t> PacketAt(std::span<const std::uint32_t> words, std::size_t at, const char* stream);
std::vector<std::uint32_t> MergeEngines(std::span<const std::uint32_t> dcb, std::span<const std::uint32_t> ccb);

}

#endif
