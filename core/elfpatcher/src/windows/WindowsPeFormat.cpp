#include "WindowsPeFormat.hpp"
#include <domain/Types.hpp>
#include <algorithm>
#include <limits>

namespace Elfpatcher::Windows {

std::uint32_t CheckedRva(const std::uint64_t value) {
    if (value > std::numeric_limits<std::int32_t>::max())
        throw Domain::RelinkerException("PE image exceeds the supported 2 GiB address range", value);
    return static_cast<std::uint32_t>(value);
}

std::uint32_t AlignRva(const std::uint64_t value) {
    return CheckedRva((static_cast<std::uint64_t>(CheckedRva(value)) + SectionAlignment - 1) & ~(static_cast<std::uint64_t>(SectionAlignment) - 1));
}

std::string ReadString(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
    if (offset >= bytes.size())
        throw Domain::RelinkerException("String offset is outside its table", offset);
    const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end = std::find(begin, bytes.end(), 0);
    if (end == bytes.end())
        throw Domain::RelinkerException("Unterminated string in ELF table", offset);
    return {begin, end};
}

}
