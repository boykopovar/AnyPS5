#ifndef RELINKER_ANALYSIS_REPLACEDPLTSTUBS_HPP
#define RELINKER_ANALYSIS_REPLACEDPLTSTUBS_HPP

#include <relinker/domain/Types.hpp>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Relinker {

struct ReplacedPltStub {
    VirtualAddress Address;
    VirtualAddress GotEntry;
    std::vector<std::uint8_t> Found;
    std::vector<std::uint8_t> Linked;
    std::optional<VirtualAddress> JumpTarget;
};

std::vector<ReplacedPltStub> FindReplacedPltStubs(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, VirtualAddress pltGot, ByteCount pltRelocationsSize);
std::string DescribeReplacedPltStub(const ReplacedPltStub& stub, std::string_view target, std::string_view import);
std::string DescribeReplacedPltStubReaching(const std::vector<ReplacedPltStub>& stubs, std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, VirtualAddress address);

}

#endif
