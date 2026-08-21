#ifndef NID_ELFPATCHER_HPP
#define NID_ELFPATCHER_HPP

#include <nid/IBinaryPatcher.hpp>

namespace Nid {

class ElfPatcher final : public IBinaryPatcher {
public:
    void PatchNids(std::vector<std::uint8_t>& binary, const std::string& libraryName) const override;
};

}

#endif
