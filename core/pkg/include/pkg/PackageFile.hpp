#ifndef PKG_PACKAGEFILE_HPP
#define PKG_PACKAGEFILE_HPP

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Pkg {

class PackageFile {
public:
    explicit PackageFile(const std::filesystem::path& path);
    ~PackageFile();
    PackageFile(const PackageFile&) = delete;
    PackageFile& operator=(const PackageFile&) = delete;

    std::uint64_t Size() const;
    void Read(std::uint64_t offset, std::uint8_t* destination, std::size_t size) const;
    std::vector<std::uint8_t> Read(std::uint64_t offset, std::size_t size) const;

private:
    std::filesystem::path _path;
    std::intptr_t _handle = -1;
    std::uint64_t _size = 0;
};

bool IsFinalizedPackage(const std::filesystem::path& path);

}

#endif
