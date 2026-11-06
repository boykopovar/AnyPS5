#ifndef ELFPATCHER_WINDOWS_WINDOWSTLSBUILDER_HPP
#define ELFPATCHER_WINDOWS_WINDOWSTLSBUILDER_HPP

#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <functional>
#include <string_view>

namespace Elfpatcher::Windows {

class WindowsTlsBuilder {
public:
    PeDirectory Build(const std::vector<std::uint8_t>& source, const std::vector<Domain::ProgramHeader>& headers, const WindowsLoadImage& image, std::vector<PeSection>& sections, std::vector<std::uint32_t>& relocations, std::uint32_t& nextRva, std::uint32_t* tlsIndexRva = nullptr, const std::function<void(std::uint64_t, std::uint64_t)>& progress = {}, const std::function<void(std::string_view)>& message = {}) const;
};

}

#endif
