#ifndef RELINKER_DOMAIN_ISYSVDYNAMICSECTIONBUILDER_HPP
#define RELINKER_DOMAIN_ISYSVDYNAMICSECTIONBUILDER_HPP

#include <relinker/domain/Types.hpp>
#include <vector>

namespace Relinker {

class ISysVDynamicSectionBuilder {
public:
    virtual ~ISysVDynamicSectionBuilder() = default;

    virtual SysVDynamicSection BuildDynamicSection(
        const std::vector<NidReference>& nidReferences,
        const std::vector<std::string>& neededLibraries
    ) = 0;
};

}

#endif
