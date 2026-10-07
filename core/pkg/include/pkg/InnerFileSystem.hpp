#ifndef PKG_INNERFILESYSTEM_HPP
#define PKG_INNERFILESYSTEM_HPP

#include <pkg/InnerMount.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Pkg {

struct InnerEntry {
    std::string Path;
    bool Directory = false;
    std::uint64_t Offset = 0;
    std::uint64_t Size = 0;
};

std::vector<InnerEntry> ReadApplicationEntries(InnerMount& mount);

bool IsSafeEntryName(const std::string& name);

}

#endif
