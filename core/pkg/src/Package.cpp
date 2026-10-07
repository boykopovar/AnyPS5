#include <pkg/Package.hpp>
#include <pkg/ContentContainer.hpp>
#include <pkg/InnerFileSystem.hpp>
#include <pkg/InnerMount.hpp>
#include <pkg/OuterImage.hpp>
#include <pkg/PackageError.hpp>
#include <pkg/PackageFile.hpp>
#include <algorithm>
#include <limits>

namespace Pkg {

namespace {

constexpr std::uint64_t MaximumLayoutSize = 0x40000000;
constexpr std::uint64_t MaximumWholeRead = 0x100000000;
constexpr std::uint32_t Ambiguous = std::numeric_limits<std::uint32_t>::max();

std::string fold(std::string_view path) {
    std::string folded(path);
    for (auto& character : folded)
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
    return folded;
}

std::string_view trimSlashes(std::string_view path) {
    while (!path.empty() && path.front() == '/') path.remove_prefix(1);
    while (!path.empty() && path.back() == '/') path.remove_suffix(1);
    return path;
}

std::string parentOf(const std::string& path) {
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? std::string{} : path.substr(0, slash);
}

}

Package::Package(const std::filesystem::path& path, const std::filesystem::path& oodlePath) {
    _file = std::make_unique<PackageFile>(path);
    _outer = std::make_unique<OuterImage>(*_file);
    const auto& image = _outer->File("/uroot/pfs_image.dat");
    const auto& layoutFile = _outer->File("/uroot/naps_pkg_layout.dat");
    if (layoutFile.Size > MaximumLayoutSize) throw PackageError("NAPS layout is too large");
    std::vector<std::uint8_t> layout(static_cast<std::size_t>(layoutFile.Size));
    _outer->Read(layoutFile, 0, layout.data(), layout.size());
    _mount = std::make_unique<InnerMount>(*_outer, image, ReadNapsLayout(layout, image.Size), oodlePath);
    if (image.LogicalSize != _mount->Size()) throw PackageError("NAPS layout disagrees with the pfs_image.dat mount size");
    _contentId = ReadContentId(*_file, _outer->ContentContainerOffset());

    _nodes.push_back(PackageNode{"", true});
    for (const auto& entry : ReadApplicationEntries(*_mount))
        addNode(entry.Path, PackageNode{"", entry.Directory, false, entry.Offset, entry.Size, {}});
    for (const auto& entry : ReadNamedContentEntries(*_file, _outer->ContentContainerOffset())) {
        std::string relative = "sce_sys/" + entry.Name;
        for (std::size_t start = 0; start <= relative.size();) {
            const auto end = std::min(relative.find('/', start), relative.size());
            if (!IsSafeEntryName(relative.substr(start, end - start))) throw PackageError("Unsafe package content entry name: " + entry.Name);
            start = end + 1;
        }
        ensureDirectory(parentOf(relative));
        addNode(relative, PackageNode{"", false, true, entry.Offset, entry.Size, {}});
    }
}

Package::~Package() = default;

const std::string& Package::ContentId() const {
    return _contentId;
}

std::uint64_t Package::FileSize() const {
    return _file->Size();
}

const PackageNode& Package::Root() const {
    return _nodes.front();
}

const PackageNode& Package::Node(const std::uint32_t index) const {
    return _nodes.at(index);
}

const PackageNode* Package::Find(std::string_view path) const {
    path = trimSlashes(path);
    if (path.empty()) return &_nodes.front();
    const auto exact = _paths.find(std::string(path));
    if (exact != _paths.end()) return &_nodes[exact->second];
    const auto folded = _foldedPaths.find(fold(path));
    if (folded == _foldedPaths.end() || folded->second == Ambiguous) return nullptr;
    return &_nodes[folded->second];
}

std::size_t Package::Read(const PackageNode& node, const std::uint64_t offset, void* destination, const std::size_t size) const {
    if (node.Directory) throw PackageError("Cannot read a package directory: " + node.Name);
    if (offset >= node.Size || size == 0) return 0;
    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(size, node.Size - offset));
    auto* bytes = static_cast<std::uint8_t*>(destination);
    if (node.Content) _file->Read(node.Offset + offset, bytes, count);
    else _mount->Read(node.Offset + offset, bytes, count);
    return count;
}

std::vector<std::uint8_t> Package::ReadAll(const PackageNode& node) const {
    if (node.Size > MaximumWholeRead) throw PackageError("Package file is too large to load: " + node.Name);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(node.Size));
    Read(node, 0, bytes.data(), bytes.size());
    return bytes;
}

std::uint32_t Package::addNode(const std::string& path, PackageNode node) {
    if (_paths.contains(path)) throw PackageError("Duplicate package path: " + path);
    const auto parentPath = parentOf(path);
    const auto parent = parentPath.empty() ? _paths.end() : _paths.find(parentPath);
    const std::uint32_t parentIndex = parentPath.empty() ? 0 : parent == _paths.end() ? Ambiguous : parent->second;
    if (parentIndex == Ambiguous || !_nodes[parentIndex].Directory) throw PackageError("Package path has no parent directory: " + path);
    node.Name = parentPath.empty() ? path : path.substr(parentPath.size() + 1);
    const auto index = static_cast<std::uint32_t>(_nodes.size());
    _nodes.push_back(std::move(node));
    _nodes[parentIndex].Children.push_back(index);
    _paths.emplace(path, index);
    const auto [folded, inserted] = _foldedPaths.emplace(fold(path), index);
    if (!inserted) folded->second = Ambiguous;
    return index;
}

std::uint32_t Package::ensureDirectory(const std::string& path) {
    if (path.empty()) return 0;
    const auto found = _paths.find(path);
    if (found != _paths.end()) {
        if (!_nodes[found->second].Directory) throw PackageError("Package path is not a directory: " + path);
        return found->second;
    }
    ensureDirectory(parentOf(path));
    return addNode(path, PackageNode{"", true, false, 0, 0, {}});
}

}
