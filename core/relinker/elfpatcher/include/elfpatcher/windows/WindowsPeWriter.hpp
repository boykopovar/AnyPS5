#ifndef ELFPATCHER_WINDOWS_PEWRITER_HPP
#define ELFPATCHER_WINDOWS_PEWRITER_HPP

#include <elfpatcher/windows/WindowsPeFormat.hpp>

namespace Elfpatcher::Windows {

class WindowsPeWriter {
public:
    std::vector<std::uint8_t> Write(const std::vector<PeSection>& sections, std::uint32_t entryRva, const std::array<PeDirectory, 16>& directories, bool windowsGui = false, std::uint64_t imageBase = ImageBase, bool fixedBase = false, bool dynamicBase = true) const;
};

}

#endif
