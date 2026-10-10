#include <io/FileWriter.hpp>
#include <io/NativePath.hpp>
#include <domain/Types.hpp>
#include <fstream>

namespace Io {

void FileWriter::Write(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream f(NativePath(path), std::ios::binary);
    if (!f)
        throw Domain::RelinkerException("Cannot open output file: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    f.close();
    if (!f)
        throw Domain::RelinkerException("Failed to write file: " + path);
}

void FileWriter::Write(const std::string& path, const std::string& content) {
    std::ofstream f(NativePath(path));
    if (!f)
        throw Domain::RelinkerException("Cannot open output file: " + path);
    f << content;
    f.close();
    if (!f)
        throw Domain::RelinkerException("Failed to write file: " + path);
}

}
