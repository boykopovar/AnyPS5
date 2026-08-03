#ifndef RELINKER_ISYSVDYNAMICSECTIONBUILDER_HPP
#define RELINKER_ISYSVDYNAMICSECTIONBUILDER_HPP

#include <relinker/Types.hpp>
#include <vector>

namespace Relinker {

struct SysVDynamicSection {
    std::vector<std::uint8_t> DynamicSegmentData;
    std::vector<std::uint8_t> DynSymData;
    std::vector<std::uint8_t> DynStrData;
    std::vector<std::uint8_t> RelaData;
    std::vector<std::uint8_t> RelaPltData;
};

class ISysVDynamicSectionBuilder {
public:
    virtual ~ISysVDynamicSectionBuilder() = default;

    virtual SysVDynamicSection BuildDynamicSection(
        const std::vector<NidReference>& nidReferences,
        const std::vector<std::string>& neededLibraries
    ) = 0;
};

}

#endif // RELINKER_ISYSVDYNAMICSECTIONBUILDER_HPP
