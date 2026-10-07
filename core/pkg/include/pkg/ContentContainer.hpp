#ifndef PKG_CONTENTCONTAINER_HPP
#define PKG_CONTENTCONTAINER_HPP

#include <pkg/PackageFile.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Pkg {

struct ContentEntry {
    std::string Name;
    std::uint64_t Offset = 0;
    std::uint32_t Size = 0;
};

std::vector<ContentEntry> ReadNamedContentEntries(const PackageFile& file, std::uint64_t containerOffset);

std::string ReadContentId(const PackageFile& file, std::uint64_t containerOffset);

}

#endif
