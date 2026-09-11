#include <io/FileWriter.hpp>
#include <domain/Types.hpp>
#include <fstream>

namespace Io {

void FileWriter::Write(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw Domain::RelinkerException("Cannot open output file: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f)
        throw Domain::RelinkerException("Failed to write file: " + path);
}

void FileWriter::Write(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f)
        throw Domain::RelinkerException("Cannot open output file: " + path);
    f << content;
    if (!f)
        throw Domain::RelinkerException("Failed to write file: " + path);
}

}
