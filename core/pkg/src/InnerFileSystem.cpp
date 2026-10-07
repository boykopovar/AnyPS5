#include <pkg/InnerFileSystem.hpp>
#include <pkg/ByteOrder.hpp>
#include <pkg/PackageError.hpp>
#include <algorithm>
#include <functional>
#include <set>

namespace Pkg {

namespace {

constexpr std::uint64_t BlockSize = 0x10000;
constexpr std::uint64_t InodeSize = 0xa8;
constexpr std::uint64_t InodesPerBlock = BlockSize / InodeSize;
constexpr std::uint64_t SuperblockMagic = 20130315;
constexpr std::uint64_t MaximumInodes = 4000000;
constexpr std::uint64_t MaximumDirectorySize = 0x4000000;
constexpr std::uint32_t FileDirent = 2;
constexpr std::uint32_t DirectoryDirent = 3;

struct Inode {
    std::uint16_t Mode = 0;
    std::uint64_t Size = 0;
    std::uint64_t Offset = 0;

    bool IsDirectory() const { return (Mode & 0xf000) == 0x4000; }
    bool IsFile() const { return (Mode & 0xf000) == 0x8000; }
};

bool isSuperblock(InnerMount& mount, const std::uint64_t offset) {
    if (offset % BlockSize != 0 || offset > mount.Size() || mount.Size() - offset < BlockSize) return false;
    std::uint8_t header[0x40];
    mount.Read(offset, header, sizeof(header));
    return LoadLittle64(header) == 2 && LoadLittle64(header + 8) == SuperblockMagic && LoadLittle32(header + 0x20) == BlockSize;
}

std::uint64_t findSuperblock(InnerMount& mount) {
    std::vector<std::uint64_t> candidates;
    for (const auto offset : mount.FileOffsets())
        if (offset < mount.Size()) candidates.push_back(offset);
    std::sort(candidates.begin(), candidates.end(), std::greater<>());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    for (const auto candidate : candidates)
        if (isSuperblock(mount, candidate)) return candidate;
    throw PackageError("Inner package PFS superblock was not found");
}

std::vector<Inode> readInodes(InnerMount& mount, const std::uint64_t superblock) {
    std::uint8_t count[8];
    mount.Read(superblock + 0x30, count, sizeof(count));
    const std::uint64_t inodeCount = LoadLittle64(count);
    const std::uint64_t blocks = (inodeCount + InodesPerBlock - 1) / InodesPerBlock;
    if (inodeCount == 0 || inodeCount > MaximumInodes || blocks > (mount.Size() - superblock) / BlockSize - 1)
        throw PackageError("Invalid inner package inode table", superblock);
    std::vector<Inode> inodes(static_cast<std::size_t>(inodeCount));
    std::vector<std::uint8_t> block(BlockSize);
    for (std::uint64_t index = 0; index < blocks; ++index) {
        mount.Read(superblock + (index + 1) * BlockSize, block.data(), block.size());
        for (std::uint64_t slot = 0; slot < InodesPerBlock && index * InodesPerBlock + slot < inodeCount; ++slot) {
            const std::uint8_t* record = block.data() + slot * InodeSize;
            auto& inode = inodes[static_cast<std::size_t>(index * InodesPerBlock + slot)];
            inode.Mode = LoadLittle16(record);
            inode.Size = LoadLittle64(record + 8);
            inode.Offset = LoadLittle64(record + 0x60);
        }
    }
    return inodes;
}

std::vector<std::pair<std::string, std::uint32_t>> readDirectory(InnerMount& mount, const std::vector<Inode>& inodes, const std::uint32_t index) {
    const auto& directory = inodes[index];
    if (!directory.IsDirectory() || directory.Size > MaximumDirectorySize || directory.Offset > mount.Size() || directory.Size > mount.Size() - directory.Offset)
        throw PackageError("Invalid inner package directory inode", index);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(directory.Size));
    mount.Read(directory.Offset, bytes.data(), bytes.size());
    std::vector<std::pair<std::string, std::uint32_t>> children;
    for (std::size_t position = 0; position + 16 <= bytes.size();) {
        const std::uint32_t child = LoadLittle32(bytes.data() + position);
        const std::uint32_t type = LoadLittle32(bytes.data() + position + 4);
        const std::uint32_t nameLength = LoadLittle32(bytes.data() + position + 8);
        const std::uint32_t entrySize = LoadLittle32(bytes.data() + position + 12);
        if (entrySize == 0) break;
        if (entrySize < 16 || entrySize > bytes.size() - position || nameLength > entrySize - 16) throw PackageError("Malformed inner package directory entry", index);
        const std::string name(reinterpret_cast<const char*>(bytes.data() + position + 16), nameLength);
        position += entrySize;
        if (type != FileDirent && type != DirectoryDirent) continue;
        if (!IsSafeEntryName(name) || child >= inodes.size()) throw PackageError("Invalid inner package entry name: " + name);
        if ((type == DirectoryDirent) != inodes[child].IsDirectory() || (type == FileDirent && !inodes[child].IsFile()))
            throw PackageError("Inner package entry type disagrees with its inode: " + name);
        children.emplace_back(name, child);
    }
    return children;
}

void walk(InnerMount& mount, const std::vector<Inode>& inodes, const std::uint32_t index, const std::string& prefix, std::set<std::uint32_t>& active, std::vector<InnerEntry>& entries) {
    if (!active.insert(index).second) throw PackageError("Inner package directory graph contains a cycle", index);
    for (const auto& [name, child] : readDirectory(mount, inodes, index)) {
        const std::string path = prefix.empty() ? name : prefix + "/" + name;
        const auto& inode = inodes[child];
        if (inode.IsDirectory()) {
            entries.push_back(InnerEntry{path, true, inode.Offset, 0});
            walk(mount, inodes, child, path, active, entries);
            continue;
        }
        if (inode.Offset > mount.Size() || inode.Size > mount.Size() - inode.Offset) throw PackageError("Inner package file lies outside the mount: " + path);
        entries.push_back(InnerEntry{path, false, inode.Offset, inode.Size});
    }
    active.erase(index);
}

}

bool IsSafeEntryName(const std::string& name) {
    if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') return false;
    return std::none_of(name.begin(), name.end(), [](const char character) {
        return static_cast<unsigned char>(character) < 0x20 || character == '/' || character == '\\' || character == ':';
    });
}

std::vector<InnerEntry> ReadApplicationEntries(InnerMount& mount) {
    const auto superblock = findSuperblock(mount);
    const auto inodes = readInodes(mount, superblock);
    std::uint32_t applicationRoot = 0;
    for (const auto& [name, child] : readDirectory(mount, inodes, 0))
        if (name == "uroot") applicationRoot = child;
    if (applicationRoot == 0 || !inodes[applicationRoot].IsDirectory()) throw PackageError("Inner package PFS has no uroot directory");
    std::vector<InnerEntry> entries;
    std::set<std::uint32_t> active{0};
    walk(mount, inodes, applicationRoot, "", active, entries);
    return entries;
}

}
