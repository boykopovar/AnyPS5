#ifndef NID_PEPATCHER_HPP
#define NID_PEPATCHER_HPP

#include <nid/IBinaryPatcher.hpp>

namespace Nid {

class PeNidPatcher final : public IBinaryPatcher {
public:
    void PatchNids(std::vector<std::uint8_t>& binary, const std::string& libraryName) const override;
};

}

#endif
