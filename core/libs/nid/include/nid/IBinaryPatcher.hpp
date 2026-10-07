#ifndef NID_IBINARYPATCHER_HPP
#define NID_IBINARYPATCHER_HPP

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace Nid {

class IBinaryPatcher {
public:
    virtual ~IBinaryPatcher() = default;

    virtual void PatchNids(std::vector<std::uint8_t>& binary, const std::string& libraryName,
                           const std::unordered_set<std::string>& excludedExports) const = 0;
};

}

#endif
