#ifndef PKG_PACKAGE_HPP
#define PKG_PACKAGE_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Pkg {

class PackageFile;
class OuterImage;
class InnerMount;

struct PackageNode {
    std::string Name;
    bool Directory = false;
    bool Content = false;
    std::uint64_t Offset = 0;
    std::uint64_t Size = 0;
    std::vector<std::uint32_t> Children;
};

class Package {
public:
    Package(const std::filesystem::path& path, const std::filesystem::path& oodlePath);
    ~Package();
    Package(const Package&) = delete;
    Package& operator=(const Package&) = delete;

    const std::string& ContentId() const;
    std::uint64_t FileSize() const;
    const PackageNode& Root() const;
    const PackageNode& Node(std::uint32_t index) const;
    const PackageNode* Find(std::string_view path) const;
    std::size_t Read(const PackageNode& node, std::uint64_t offset, void* destination, std::size_t size) const;
    std::vector<std::uint8_t> ReadAll(const PackageNode& node) const;

private:
    std::uint32_t addNode(const std::string& path, PackageNode node);
    std::uint32_t ensureDirectory(const std::string& path);

    std::unique_ptr<PackageFile> _file;
    std::unique_ptr<OuterImage> _outer;
    std::unique_ptr<InnerMount> _mount;
    std::string _contentId;
    std::vector<PackageNode> _nodes;
    std::unordered_map<std::string, std::uint32_t> _paths;
    std::unordered_map<std::string, std::uint32_t> _foldedPaths;
};

}

#endif
