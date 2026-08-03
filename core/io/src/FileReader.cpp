#include <io/FileReader.hpp>
#include <domain/Types.hpp>
#include <fstream>

namespace Io {

std::vector<std::uint8_t> FileReader::Read(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        throw Domain::RelinkerException("Cannot open file: " + path);
    const std::streamsize size = f.tellg();
    f.seekg(0);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(size));
    if (!f.read(reinterpret_cast<char*>(buf.data()), size))
        throw Domain::RelinkerException("Cannot read file: " + path);
    return buf;
}

}
