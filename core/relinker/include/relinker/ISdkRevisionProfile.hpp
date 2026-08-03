#ifndef RELINKER_ISDKREVISIONPROFILE_HPP
#define RELINKER_ISDKREVISIONPROFILE_HPP

#include <relinker/Types.hpp>

namespace Relinker {

class ISdkRevisionProfile {
public:
    virtual ~ISdkRevisionProfile() = default;

    virtual std::uint32_t GetSceModuleParamHeaderSize() const = 0;
    virtual std::uint32_t GetSceModuleParamEntrySize() const = 0;
    virtual std::uint32_t GetSceDynlibDataHeaderSize() const = 0;
    virtual std::uint32_t GetSceDynlibDataEntrySize() const = 0;
    virtual std::uint32_t GetRelocationEntrySize() const = 0;

    virtual std::uint32_t GetImportsOffset() const = 0;
    virtual std::uint32_t GetImportsCount(const std::vector<std::uint8_t>& Data) const = 0;
    virtual std::uint32_t GetRelocationTableOffset() const = 0;
};

}

#endif // RELINKER_ISDKREVISIONPROFILE_HPP
