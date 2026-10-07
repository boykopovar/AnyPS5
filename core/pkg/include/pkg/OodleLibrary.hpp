#ifndef PKG_OODLELIBRARY_HPP
#define PKG_OODLELIBRARY_HPP

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Pkg {

class OodleLibrary {
public:
    explicit OodleLibrary(const std::filesystem::path& path);
    ~OodleLibrary();
    OodleLibrary(const OodleLibrary&) = delete;
    OodleLibrary& operator=(const OodleLibrary&) = delete;

    void Decompress(const std::vector<std::uint8_t>& stream, std::uint8_t* destination, std::size_t size) const;

private:
    using DecompressFunction = std::intptr_t (*)(const void*, std::intptr_t, void*, std::intptr_t, int, int, int, void*, std::intptr_t, void*, void*, void*, std::intptr_t, int);

    void* _handle = nullptr;
    DecompressFunction _decompress = nullptr;
};

}

#endif
