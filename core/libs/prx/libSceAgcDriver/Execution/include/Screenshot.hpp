#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SCREENSHOT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SCREENSHOT_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

namespace AgcDriver::Screenshot {

void Request();
bool Take();
std::filesystem::path Write(std::string_view titleId, std::uint32_t width, std::uint32_t height, std::span<const std::byte> bgra);

}

#endif
