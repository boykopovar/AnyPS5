#ifndef PKG_OUTERIMAGE_HPP
#define PKG_OUTERIMAGE_HPP

#include <pkg/PackageFile.hpp>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Pkg {

struct OuterFile {
    std::uint64_t Size = 0;
    std::uint64_t LogicalSize = 0;
    std::vector<std::uint64_t> Blocks;
};

class OuterImage {
public:
    explicit OuterImage(const PackageFile& file);

    const OuterFile& File(const std::string& path) const;
    std::uint64_t ContentContainerOffset() const;
    void Read(const OuterFile& file, std::uint64_t offset, std::uint8_t* destination, std::size_t size) const;

private:
    struct Inode {
        std::uint16_t Mode = 0;
        std::uint64_t Size = 0;
        std::uint64_t LogicalSize = 0;
        std::uint32_t BlockCount = 0;
        std::uint32_t Direct[12]{};
        std::uint32_t Indirect[5]{};
    };

    std::vector<std::uint64_t> blocksOf(const Inode& inode);
    void appendIndirect(std::uint64_t block, int level, std::uint64_t needed, std::vector<std::uint64_t>& blocks);
    void walk(std::uint32_t inode, const std::string& prefix, std::set<std::uint32_t>& active);
    std::uint64_t blockOffset(std::uint64_t block) const;

    const PackageFile& _file;
    std::uint64_t _imageOffset = 0;
    std::uint64_t _blockCount = 0;
    std::uint64_t _contentContainerOffset = 0;
    std::vector<Inode> _inodes;
    std::map<std::string, OuterFile> _files;
};

}

#endif
