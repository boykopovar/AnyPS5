#ifndef RELINKER_DOMAIN_IELFPATCHER_HPP
#define RELINKER_DOMAIN_IELFPATCHER_HPP

#include <relinker/domain/Types.hpp>
#include <relinker/domain/ISysVDynamicSectionBuilder.hpp>
#include <string>
#include <vector>

namespace Relinker {

class IElfPatcher {
public:
    virtual ~IElfPatcher() = default;

    virtual void PatchAndWrite(
        const std::string& inputPath,
        const std::string& outputPath,
        const std::vector<ProgramHeader>& originalHeaders,
        const SysVDynamicSection& dynamicSection
    ) = 0;
};

}

#endif
