#include <nid/BinaryPatcherFactory.hpp>
#include <nid/ElfPatcher.hpp>
#include <nid/PePatcher.hpp>
#include <stdexcept>

namespace Nid {

std::unique_ptr<IBinaryPatcher> MakePatcher(const std::vector<std::uint8_t>& binary) {
    if (binary.size() >= 4 && binary[0] == 0x7f && binary[1] == 'E' && binary[2] == 'L' && binary[3] == 'F')
        return std::make_unique<ElfNidPatcher>();
    if (binary.size() >= 2 && binary[0] == 'M' && binary[1] == 'Z')
        return std::make_unique<PeNidPatcher>();
    throw std::runtime_error("unrecognized binary format");
}

}
