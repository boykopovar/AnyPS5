#ifndef CORE_LIBS_PRX_LIBSCEPRECOMPILEDSHADERS_INCLUDE_PRECOMPILEDSHADERS_HPP
#define CORE_LIBS_PRX_LIBSCEPRECOMPILEDSHADERS_INCLUDE_PRECOMPILEDSHADERS_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace PrecompiledShaders {

struct Entry {
    const char* name;
    std::uint64_t reserved0;
    std::uint64_t reserved1;
    std::uint64_t size;
    const void* data;
};

static_assert(sizeof(Entry) == 0x28 && offsetof(Entry, size) == 0x18 && offsetof(Entry, data) == 0x20);

inline constexpr std::string_view SourceMagic = "APS5GLSL\n";

}

#endif
