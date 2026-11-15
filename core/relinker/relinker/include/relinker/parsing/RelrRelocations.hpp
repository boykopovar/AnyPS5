#ifndef RELINKER_PARSING_RELRRELOCATIONS_HPP
#define RELINKER_PARSING_RELRRELOCATIONS_HPP

#include <domain/Types.hpp>
#include <map>
#include <span>

namespace Relinker {

std::map<Domain::VirtualAddress, std::uint64_t> ReadRelrRelocations(
    std::span<const std::uint8_t> bytes,
    std::span<const Domain::ProgramHeader> headers,
    std::span<const Domain::DynamicTag> tags);

}

#endif
