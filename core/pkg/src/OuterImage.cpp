#include <pkg/OuterImage.hpp>
#include <pkg/ByteOrder.hpp>
#include <pkg/PackageError.hpp>
#include <algorithm>
#include <cstring>

namespace Pkg {

namespace {

constexpr std::uint64_t BlockSize = 0x10000;
constexpr std::size_t SignedInodeSize = 0x2c8;
constexpr std::size_t SignedBlockEntrySize = 36;
constexpr std::uint64_t SuperblockMagic = 20130315;
constexpr std::uint32_t FileDirent = 2;
constexpr std::uint32_t DirectoryDirent = 3;
constexpr char PlaintextMarker[] = "PPRPLAIN-NOAUTH!";

}

OuterImage::OuterImage(const PackageFile& file) : _file(file) {
    const auto header = _file.Read(0, 0x100);
    if (header[0] != 0x7f || header[1] != 'F' || header[2] != 'I' || header[3] != 'H') throw PackageError("Not a finalized PS5 package (missing FIH magic)");
    _imageOffset = LoadLittle64(header.data() + 0x10);
    const std::uint64_t imageSize = LoadLittle64(header.data() + 0x18);
    const std::uint64_t superblockOffset = LoadLittle64(header.data() + 0x20);
    _contentContainerOffset = LoadLittle64(header.data() + 0x58);
    if (_imageOffset == 0 || _imageOffset % BlockSize != 0 || imageSize == 0 || imageSize % BlockSize != 0 || _imageOffset > _file.Size() || imageSize > _file.Size() - _imageOffset)
        throw PackageError("Invalid package PFS image range");
    if (superblockOffset < _imageOffset || superblockOffset - _imageOffset > imageSize - BlockSize || (superblockOffset - _imageOffset) % BlockSize != 0)
        throw PackageError("Invalid package PFS superblock offset", superblockOffset);
    _blockCount = imageSize / BlockSize;

    const auto superblock = _file.Read(superblockOffset, 0x400);
    if (LoadLittle64(superblock.data()) != 2 || LoadLittle64(superblock.data() + 8) != SuperblockMagic)
        throw PackageError("Unsupported package PFS superblock", superblockOffset);
    if (LoadLittle32(superblock.data() + 0x20) != BlockSize) throw PackageError("Unsupported package PFS block size", superblockOffset);
    if (std::memcmp(superblock.data() + 0x370, PlaintextMarker, 16) != 0)
        throw PackageError("Encrypted PS5 packages are not supported; only plaintext debug packages (PPRPLAIN-NOAUTH) can be read");
    if ((LoadLittle16(superblock.data() + 0x1c) & 3) != 1) throw PackageError("Unsupported package PFS inode format", superblockOffset);
    const std::uint64_t inodeCount = LoadLittle64(superblock.data() + 0x30);
    const std::uint64_t inodeBlockCount = LoadLittle64(superblock.data() + 0x40);
    const std::uint64_t inodeBlock = LoadLittle64(superblock.data() + 0xd8);
    constexpr std::uint64_t inodesPerBlock = BlockSize / SignedInodeSize;
    if (inodeCount == 0 || inodeBlockCount == 0 || inodeBlockCount > _blockCount || inodeBlock > _blockCount - inodeBlockCount || inodeCount > inodeBlockCount * inodesPerBlock)
        throw PackageError("Invalid package PFS inode table", superblockOffset);

    const auto table = _file.Read(blockOffset(inodeBlock), static_cast<std::size_t>(inodeBlockCount * BlockSize));
    _inodes.resize(static_cast<std::size_t>(inodeCount));
    for (std::size_t index = 0; index < _inodes.size(); ++index) {
        const std::uint8_t* record = table.data() + (index / inodesPerBlock) * BlockSize + (index % inodesPerBlock) * SignedInodeSize;
        auto& inode = _inodes[index];
        inode.Mode = LoadLittle16(record);
        inode.Size = LoadLittle64(record + 8);
        inode.LogicalSize = LoadLittle64(record + 0x10);
        inode.BlockCount = LoadLittle32(record + 0x60);
        for (std::size_t slot = 0; slot < 12; ++slot) inode.Direct[slot] = LoadLittle32(record + 0x64 + slot * SignedBlockEntrySize + 32);
        for (std::size_t slot = 0; slot < 5; ++slot) inode.Indirect[slot] = LoadLittle32(record + 0x64 + (12 + slot) * SignedBlockEntrySize + 32);
    }
    std::set<std::uint32_t> active;
    walk(0, "", active);
}

const OuterFile& OuterImage::File(const std::string& path) const {
    const auto found = _files.find(path);
    if (found == _files.end()) throw PackageError("Package PFS does not contain " + path);
    return found->second;
}

std::uint64_t OuterImage::ContentContainerOffset() const {
    return _contentContainerOffset;
}

void OuterImage::Read(const OuterFile& file, std::uint64_t offset, std::uint8_t* destination, std::size_t size) const {
    if (offset > file.Size || size > file.Size - offset) throw PackageError("Read outside package PFS file", offset);
    while (size > 0) {
        const std::uint64_t within = offset % BlockSize;
        const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(size, BlockSize - within));
        _file.Read(blockOffset(file.Blocks[static_cast<std::size_t>(offset / BlockSize)]) + within, destination, count);
        offset += count;
        destination += count;
        size -= count;
    }
}

std::vector<std::uint64_t> OuterImage::blocksOf(const Inode& inode) {
    if (inode.BlockCount < (inode.Size + BlockSize - 1) / BlockSize) throw PackageError("Package PFS inode has too few blocks");
    std::vector<std::uint64_t> blocks;
    blocks.reserve(inode.BlockCount);
    for (std::size_t slot = 0; slot < 12 && blocks.size() < inode.BlockCount; ++slot) blocks.push_back(inode.Direct[slot]);
    for (int level = 0; level < 5 && blocks.size() < inode.BlockCount; ++level) appendIndirect(inode.Indirect[level], level, inode.BlockCount, blocks);
    if (blocks.size() != inode.BlockCount) throw PackageError("Package PFS inode block list is truncated");
    for (const auto block : blocks)
        if (block >= _blockCount) throw PackageError("Package PFS block outside the image", block);
    return blocks;
}

void OuterImage::appendIndirect(const std::uint64_t block, const int level, const std::uint64_t needed, std::vector<std::uint64_t>& blocks) {
    if (block >= _blockCount) throw PackageError("Package PFS indirect block outside the image", block);
    const auto entries = _file.Read(blockOffset(block), static_cast<std::size_t>(BlockSize));
    for (std::size_t entry = 0; entry + SignedBlockEntrySize <= entries.size() && blocks.size() < needed; entry += SignedBlockEntrySize) {
        const std::uint32_t target = LoadLittle32(entries.data() + entry + 32);
        if (level == 0) blocks.push_back(target);
        else appendIndirect(target, level - 1, needed, blocks);
    }
}

void OuterImage::walk(const std::uint32_t inode, const std::string& prefix, std::set<std::uint32_t>& active) {
    if (inode >= _inodes.size() || (_inodes[inode].Mode & 0xf000) != 0x4000) throw PackageError("Package PFS directory inode is invalid");
    if (!active.insert(inode).second) throw PackageError("Package PFS directory graph contains a cycle");
    const auto& directory = _inodes[inode];
    if (directory.Size > 0x1000000) throw PackageError("Package PFS directory is too large");
    OuterFile entries{directory.Size, directory.LogicalSize, blocksOf(directory)};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(directory.Size));
    Read(entries, 0, bytes.data(), bytes.size());
    for (std::size_t position = 0; position + 16 <= bytes.size();) {
        const std::uint32_t child = LoadLittle32(bytes.data() + position);
        const std::uint32_t type = LoadLittle32(bytes.data() + position + 4);
        const std::uint32_t nameLength = LoadLittle32(bytes.data() + position + 8);
        const std::uint32_t entrySize = LoadLittle32(bytes.data() + position + 12);
        if (entrySize == 0) break;
        if (entrySize < 16 || entrySize > bytes.size() - position || nameLength > entrySize - 16) throw PackageError("Malformed package PFS directory entry");
        const std::string name(reinterpret_cast<const char*>(bytes.data() + position + 16), nameLength);
        position += entrySize;
        if (type != FileDirent && type != DirectoryDirent) continue;
        if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos || child >= _inodes.size())
            throw PackageError("Invalid package PFS directory entry: " + name);
        const std::string path = prefix + "/" + name;
        if (type == DirectoryDirent) {
            walk(child, path, active);
            continue;
        }
        const auto& file = _inodes[child];
        if ((file.Mode & 0xf000) != 0x8000) throw PackageError("Package PFS file inode is invalid: " + path);
        _files[path] = OuterFile{file.Size, file.LogicalSize, blocksOf(file)};
    }
    active.erase(inode);
}

std::uint64_t OuterImage::blockOffset(const std::uint64_t block) const {
    return _imageOffset + block * BlockSize;
}

}
