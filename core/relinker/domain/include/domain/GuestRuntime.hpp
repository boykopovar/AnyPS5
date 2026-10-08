#ifndef DOMAIN_GUESTRUNTIME_HPP
#define DOMAIN_GUESTRUNTIME_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace Domain {

struct GuestImport {
    std::string Name;
    std::uint32_t TargetRva;
    std::uint64_t Addend;
    std::uint32_t RelocationType = 1;
    std::string Library;
};

struct GuestRuntime {
    std::string Path;
    std::vector<GuestImport> Imports;
    std::vector<std::uint32_t> InitArrayRvas;
    std::vector<std::uint32_t> FiniArrayRvas;
    std::uint32_t InitRva = 0;
    std::uint32_t FiniRva = 0;
    bool UsePlatformTlsResolver = true;
    std::vector<std::string> Names;
};

}

#endif
