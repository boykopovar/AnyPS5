#ifndef RELINKER_DOMAIN_ISDKREVISIONPROFILE_HPP
#define RELINKER_DOMAIN_ISDKREVISIONPROFILE_HPP

#include <relinker/domain/Types.hpp>
#include <vector>

namespace Relinker {

class ISdkRevisionProfile {
public:
    virtual ~ISdkRevisionProfile() = default;

    [[nodiscard]] virtual std::uint32_t GetSceModuleParamHeaderSize() const = 0;
    [[nodiscard]] virtual std::uint32_t GetSceModuleParamEntrySize() const = 0;
    [[nodiscard]] virtual std::uint32_t GetSceDynlibDataHeaderSize() const = 0;
    [[nodiscard]] virtual std::uint32_t GetSceDynlibDataEntrySize() const = 0;
    [[nodiscard]] virtual std::uint32_t GetRelocationEntrySize() const = 0;
    [[nodiscard]] virtual std::uint32_t GetImportsOffset() const = 0;
    [[nodiscard]] virtual std::uint32_t GetImportsCount(const std::vector<std::uint8_t>& data) const = 0;
    [[nodiscard]] virtual std::uint32_t GetRelocationTableOffset() const = 0;
};

}

#endif
