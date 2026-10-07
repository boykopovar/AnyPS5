#ifndef PKG_NAPSLAYOUT_HPP
#define PKG_NAPSLAYOUT_HPP

#include <cstdint>
#include <vector>

namespace Pkg {

enum class InnerBlockKind {
    Stored,
    Kraken,
    Zero
};

struct InnerBlock {
    std::uint64_t SourceOffset = 0;
    std::uint32_t SourceLength = 0;
    std::uint32_t EvenSourceLength = 0;
    std::uint64_t MountOffset = 0;
    std::uint32_t Length = 0;
    InnerBlockKind Kind = InnerBlockKind::Zero;
    std::uint32_t KrakenFlags = 0;
};

struct InnerLayout {
    std::vector<InnerBlock> Blocks;
    std::uint64_t MountSize = 0;
    std::vector<std::uint64_t> FileOffsets;
};

InnerLayout ReadNapsLayout(const std::vector<std::uint8_t>& naps, std::uint64_t sourceSize);

}

#endif
