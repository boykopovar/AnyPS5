#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "binary word I/O: %s\n", message);
        std::abort();
    }
}

void CheckRoundTrip(const char* path) {
    constexpr std::array values{0, 1, -1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(),
        0x12345678, 0x0d0a1a00, -1234567};
    auto* stream = fopen_nid_postfix(path, "w+b");
    Require(stream != nullptr, "open round-trip file");
    for (const int value : values) Require(putw_nid_postfix(value, stream) == 0, "putw returns zero on success");
    Require(ftell_nid_postfix(stream) == static_cast<std::int64_t>(4 * values.size()), "write exactly four bytes per word");
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0, "rewind written words");
    std::array<unsigned char, 4 * values.size()> bytes;
    Require(fread_nid_postfix(bytes.data(), 1, bytes.size(), stream) == bytes.size(), "read raw word bytes");
    for (std::size_t word = 0; word < values.size(); ++word)
        for (unsigned byte = 0; byte < 4; ++byte)
            Require(bytes[4 * word + byte] == ((static_cast<std::uint32_t>(values[word]) >> (8 * byte)) & 0xff),
                "guest little-endian representation");
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0, "rewind for getw");
    for (const int value : values) {
        Require(getw_nid_postfix(stream) == value, "read signed word including minus one");
        Require((stream->GuestState().flags & 0x60) == 0, "a stored minus one is not EOF or an error");
    }
    Require(getw_nid_postfix(stream) == -1, "EOF returns minus one");
    Require((stream->GuestState().flags & 0x60) == 0x20, "publish EOF without an error");
    Require(fseek_nid_postfix(stream, 20, SEEK_SET) == 0, "seek to a buffered word");
    Require(fgetc_nid_postfix(stream) == 0x78, "read first word byte");
    Require(ungetc_nid_postfix(0x78, stream) == 0x78, "push back first word byte");
    Require(getw_nid_postfix(stream) == 0x12345678, "getw consumes pushed-back bytes");
    Require(ftell_nid_postfix(stream) == 24, "position after pushed-back word");
    Require(fclose_nid_postfix(stream) == 0, "close round-trip file");
}

void CheckShortReads(const char* path) {
    constexpr std::array<unsigned char, 3> bytes{0xff, 0xaa, 0xbb};
    for (std::size_t length = 0; length <= bytes.size(); ++length) {
        auto* stream = fopen_nid_postfix(path, "w+b");
        Require(stream != nullptr, "open short word file");
        Require(fwrite_nid_postfix(bytes.data(), 1, length, stream) == length, "write incomplete word");
        Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0, "rewind incomplete word");
        Require(getw_nid_postfix(stream) == -1, "incomplete word is EOF");
        Require((stream->GuestState().flags & 0x60) == 0x20, "short-read EOF flag");
        Require(ftell_nid_postfix(stream) == static_cast<std::int64_t>(length), "consume incomplete word bytes");
        clearerr_nid_postfix(stream);

        Require((stream->GuestState().flags & 0x60) == 0, "clear short-read status");
        Require(fclose_nid_postfix(stream) == 0, "close incomplete word");
    }
}

void CheckErrors(const char* path) {
    auto* writer = fopen_nid_postfix(path, "wb");
    Require(writer != nullptr, "open write-only file");
    Require(getw_nid_postfix(writer) == -1, "read on write-only stream fails");
    Require((writer->GuestState().flags & 0x60) == 0x40, "publish read error, not EOF");
    Require(fclose_nid_postfix(writer) == 0, "close write-only file");
    auto* reader = fopen_nid_postfix(path, "rb");
    Require(reader != nullptr, "open read-only file");
    Require(putw_nid_postfix(0x12345678, reader) == -1, "write on read-only stream fails");
    Require((reader->GuestState().flags & 0x60) == 0x40, "publish write error");
    Require(fclose_nid_postfix(reader) == 0, "close read-only file");
    FileStream closed(TemporaryFile::Open());
    Require(closed.Close() == 0, "close temporary stream");
    errno = 0;
    Require(getw_nid_postfix(&closed) == -1 && errno == 9, "closed read reports EBADF");
    errno = 0;
    Require(putw_nid_postfix(1, &closed) == -1 && errno == 9, "closed write reports EBADF");
    bool readThrew = false, writeThrew = false;
    try { getw_nid_postfix(nullptr); } catch (const std::runtime_error&) { readThrew = true; }
    try { putw_nid_postfix(1, nullptr); } catch (const std::invalid_argument&) { writeThrew = true; }
    Require(readThrew && writeThrew, "null streams throw");
}

void CheckConcurrentWrites(const char* path) {
    auto* stream = fopen_nid_postfix(path, "w+b");
    Require(stream != nullptr, "open shared stream");
    flockfile_nid_postfix(stream);

    Require(putw_nid_postfix(0, stream) == 0, "putw accepts a lock held by its caller");
    funlockfile_nid_postfix(stream);

    const auto writer = [stream](int value) {
        for (int count = 0; count < 256; ++count)
            Require(putw_nid_postfix(value, stream) == 0, "concurrent word write");
    };
    std::thread first(writer, 0x11223344), second(writer, 0x55667788);
    first.join();
    second.join();
    Require(fseek_nid_postfix(stream, 4, SEEK_SET) == 0, "rewind concurrent words");
    int firstCount = 0, secondCount = 0;
    for (int count = 0; count < 512; ++count) {
        const int value = getw_nid_postfix(stream);
        Require(value == 0x11223344 || value == 0x55667788, "concurrent words do not interleave their bytes");
        if (value == 0x11223344) ++firstCount;
        else ++secondCount;
    }
    Require(firstCount == 256 && secondCount == 256, "both writers preserve every word");
    Require(fclose_nid_postfix(stream) == 0, "close shared stream");
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "expected temporary file path");
    CheckRoundTrip(argv[1]);
    CheckShortReads(argv[1]);
    CheckErrors(argv[1]);
    CheckConcurrentWrites(argv[1]);
    Require(std::remove(argv[1]) == 0, "remove temporary file");
    std::puts("binary word I/O checks passed");
}
