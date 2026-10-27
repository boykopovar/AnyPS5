#include "prx/libSceAgcDriver/Execution/include/TraceFile.hpp"
#include "zlib.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace AgcDriver {

void TraceFile::WriteGzip(const std::filesystem::path& path, std::string_view text) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) throw std::runtime_error("deflateInit2 failed");
    struct End {
        z_stream& stream;
        ~End() { deflateEnd(&stream); }
    } end{stream};
    std::ofstream output;
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.open(path, std::ios::binary | std::ios::trunc);
    const std::array<unsigned char, 10> header{0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0xff};
    output.write(reinterpret_cast<const char*>(header.data()), header.size());
    uLong crc = crc32(0, Z_NULL, 0);
    std::vector<unsigned char> chunk(1 << 16);
    std::size_t offset = 0;
    int status = Z_OK;
    while (status != Z_STREAM_END) {
        if (stream.avail_in == 0 && offset < text.size()) {
            const auto piece = static_cast<uInt>(std::min<std::size_t>(text.size() - offset, std::size_t{1} << 30));
            stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(text.data() + offset));
            stream.avail_in = piece;
            crc = crc32(crc, stream.next_in, piece);
            offset += piece;
        }
        stream.next_out = chunk.data();
        stream.avail_out = static_cast<uInt>(chunk.size());
        status = deflate(&stream, offset == text.size() ? Z_FINISH : Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) throw std::runtime_error("deflate failed with " + std::to_string(status));
        output.write(reinterpret_cast<const char*>(chunk.data()), static_cast<std::streamsize>(chunk.size() - stream.avail_out));
    }
    std::array<unsigned char, 8> trailer{};
    const auto size = static_cast<std::uint32_t>(text.size());
    for (unsigned index = 0; index < 4; ++index) {
        trailer[index] = static_cast<unsigned char>((crc >> (8 * index)) & 0xff);
        trailer[4 + index] = static_cast<unsigned char>((size >> (8 * index)) & 0xff);
    }
    output.write(reinterpret_cast<const char*>(trailer.data()), trailer.size());
    output.close();
}

void TraceFile::ReplaceGzip(const std::filesystem::path& path, std::string_view text) {
    auto temporary = path;
    temporary += ".tmp";
    WriteGzip(temporary, text);
    std::error_code error;
#ifdef _WIN32
    if (::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) error = std::error_code(static_cast<int>(::GetLastError()), std::system_category());
#else
    std::filesystem::rename(temporary, path, error);
#endif
    if (!error) return;
    std::error_code removeError;
    std::filesystem::remove(temporary, removeError);
    throw std::system_error(error, "replacing " + path.string());
}

void TraceFile::AppendLine(const std::filesystem::path& path, std::string_view line) {
    std::ofstream output;
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.open(path, std::ios::binary | std::ios::app);
    output.write(line.data(), static_cast<std::streamsize>(line.size()));
    output.close();
}

}
