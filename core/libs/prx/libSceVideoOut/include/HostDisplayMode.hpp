#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTDISPLAYMODE_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_HOSTDISPLAYMODE_HPP

#include <cstdint>
#include <optional>
#include <string>

struct HostDisplayMode {
    int refreshRate = 0;
    std::uint32_t resolution = 0;
};

HostDisplayMode KeepHostDisplayMode(const std::optional<HostDisplayMode>& known, const std::optional<HostDisplayMode>& measured, const std::string& failure);

#endif
