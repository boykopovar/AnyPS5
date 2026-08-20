#ifndef ELFPATCHER_IELFPATCHER_HPP
#define ELFPATCHER_IELFPATCHER_HPP

#include <domain/Types.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Elfpatcher {

class IElfPatcher {
public:
    virtual ~IElfPatcher() = default;

    virtual std::vector<std::uint8_t> Patch(
        const std::vector<std::uint8_t>& sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynamicSection,
        std::uint64_t originalPltGotVaddr,
        const std::string& runPath
    ) = 0;
};

}

#endif
