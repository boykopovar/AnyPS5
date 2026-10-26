#include <io/FileReader.hpp>
#include <io/NativePath.hpp>
#include <domain/Types.hpp>
#include <filesystem>
#include <fstream>
#include <limits>

namespace Io {

std::vector<std::uint8_t> FileReader::ReadPrefix(const std::string& path, const std::size_t size) const {
    if (size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        throw Domain::RelinkerException("File prefix is too large: " + path);
    const auto native = NativePath(path);
    std::error_code error;
    if (!std::filesystem::is_regular_file(native, error))
        throw Domain::RelinkerException("Cannot open file: " + path);
    std::ifstream f(native, std::ios::binary);
    if (!f)
        throw Domain::RelinkerException("Cannot open file: " + path);
    std::vector<std::uint8_t> buf(size);
    f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
    const auto count = f.gcount();
    if (f.bad())
        throw Domain::RelinkerException("Cannot read file: " + path);
    buf.resize(static_cast<std::size_t>(count));
    return buf;
}

std::vector<std::uint8_t> FileReader::Read(const std::string& path) {
    const auto native = NativePath(path);
    std::error_code error;
    if (!std::filesystem::is_regular_file(native, error))
        throw Domain::RelinkerException("Cannot open file: " + path);
    std::ifstream f(native, std::ios::binary | std::ios::ate);
    if (!f)
        throw Domain::RelinkerException("Cannot open file: " + path);
    const std::streamsize size = f.tellg();
    if (size < 0)
        throw Domain::RelinkerException("Cannot read file: " + path);
    f.seekg(0);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(size));
    if (!f.read(reinterpret_cast<char*>(buf.data()), size))
        throw Domain::RelinkerException("Cannot read file: " + path);
    return buf;
}

}
