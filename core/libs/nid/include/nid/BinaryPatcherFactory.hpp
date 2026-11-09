#ifndef NID_BINARYPATCHERFACTORY_HPP
#define NID_BINARYPATCHERFACTORY_HPP

#include <nid/IBinaryPatcher.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace Nid {

std::unique_ptr<IBinaryPatcher> MakePatcher(const std::vector<std::uint8_t>& binary);

}

#endif
