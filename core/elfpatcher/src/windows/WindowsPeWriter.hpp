#ifndef ELFPATCHER_WINDOWS_PEWRITER_HPP
#define ELFPATCHER_WINDOWS_PEWRITER_HPP

#include "WindowsPeFormat.hpp"

namespace Elfpatcher::Windows {

class WindowsPeWriter {
public:
    std::vector<std::uint8_t> Write(const std::vector<PeSection>& sections, std::uint32_t entryRva, const std::array<PeDirectory, 16>& directories) const;
};

}

#endif
