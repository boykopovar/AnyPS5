#ifndef NID_IBINARYPATCHER_HPP
#define NID_IBINARYPATCHER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace Nid {

class IBinaryPatcher {
public:
    virtual ~IBinaryPatcher() = default;

    virtual void PatchNids(std::vector<std::uint8_t>& binary, const std::string& libraryName) const = 0;
};

}

#endif
