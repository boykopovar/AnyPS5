#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "stream purge: %s\n", message);
        std::abort();
    }
}

std::int64_t Position(std::FILE* stream) {
#ifdef _WIN32
    return ::_telli64(::_fileno(stream));
#else
    return ::lseek(::fileno(stream), 0, SEEK_CUR);
#endif
}

struct BufferedStream {
    std::array<char, 4096> buffer{};
    std::FILE* native = TemporaryFile::Open();
    FileStream stream{native};

    BufferedStream() { Require(std::setvbuf(native, buffer.data(), _IOFBF, buffer.size()) == 0, "set full buffering"); }
    ~BufferedStream() { if (stream.IsOpen()) stream.Close(); }
};

void CheckOutput() {
    BufferedStream file;
    Require(fwrite_nid_postfix("discard", 1, 7, &file.stream) == 7, "buffer output");
    Require(Position(file.native) == 0, "output is pending in the stream buffer");
    errno = 123;
    Require(fpurge_nid_postfix(&file.stream) == 0 && errno == 123, "purge preserves errno on success");
    Require(Position(file.native) == 0, "purge does not write or seek");
    Require(fpurge_nid_postfix(&file.stream) == 0, "purge an empty buffer twice");
    for (int cycle = 0; cycle < 4; ++cycle) {
        Require(fwrite_nid_postfix("cycle", 1, 5, &file.stream) == 5, "cycle write");
        Require(fpurge_nid_postfix(&file.stream) == 0, "cycle purge");
    }
    Require(fwrite_nid_postfix("kept", 1, 4, &file.stream) == 4, "write after purging");
    Require(std::fflush(file.native) == 0 && Position(file.native) == 4, "flush only subsequent output");
    Require(std::fseek(file.native, 0, SEEK_SET) == 0, "rewind subsequent output");
    char bytes[8]{};
    Require(std::fread(bytes, 1, sizeof(bytes), file.native) == 4 && std::memcmp(bytes, "kept", 4) == 0, "discarded output never reaches the file");
}

void CheckInput() {
    BufferedStream file;
    Require(std::fwrite("abcdef", 1, 6, file.native) == 6 && std::fflush(file.native) == 0, "prepare input");
    Require(std::fseek(file.native, 0, SEEK_SET) == 0, "rewind input");
    Require(fgetc_nid_postfix(&file.stream) == 'a', "read one buffered byte");
    const auto position = Position(file.native);
    Require(position > 1, "input has read-ahead data");
    Require(fpurge_nid_postfix(&file.stream) == 0, "purge buffered input");
    Require(std::ftell(file.native) == 1, "purging input leaves the stream at the logical position");
    Require(fgetc_nid_postfix(&file.stream) == 'b', "the next read continues after the byte already read");
    Require(ungetc_nid_postfix('Z', &file.stream) == 'Z', "push back a different byte");
    Require(fpurge_nid_postfix(&file.stream) == 0, "purge input with a pushed-back byte");
    Require(fgetc_nid_postfix(&file.stream) == 'Z', "the pushed-back byte survives the purge");
    Require(fgetc_nid_postfix(&file.stream) == 'c', "the read after the pushed-back byte continues in order");
    Require(ungetc_nid_postfix('Y', &file.stream) == 'Y' && fgetc_nid_postfix(&file.stream) == 'Y', "a consumed pushback");
    Require(fpurge_nid_postfix(&file.stream) == 0, "purge after the pushed-back byte was read");
    Require(fgetc_nid_postfix(&file.stream) == 'd', "a consumed pushback does not come back");
    for (const char expected : {'e', 'f'}) Require(fgetc_nid_postfix(&file.stream) == expected, "read the rest of the file");
    Require(fgetc_nid_postfix(&file.stream) == EOF, "read to the end");
    Require((file.stream.GuestState().flags & 0x20) != 0, "publish EOF");
    Require(fpurge_nid_postfix(&file.stream) == 0 && (file.stream.GuestState().flags & 0x20) != 0, "purge preserves EOF");
    file.stream.SetError();
    Require(fpurge_nid_postfix(&file.stream) == 0 && (file.stream.GuestState().flags & 0x60) == 0x60, "purge preserves EOF and error");
}

void CheckPushbackReadByFread() {
    BufferedStream file;
    Require(std::fwrite("abcdef", 1, 6, file.native) == 6 && std::fflush(file.native) == 0, "prepare input");
    Require(std::fseek(file.native, 0, SEEK_SET) == 0, "rewind input");
    Require(fgetc_nid_postfix(&file.stream) == 'a', "read one byte");
    Require(ungetc_nid_postfix('Q', &file.stream) == 'Q', "push back a different byte");
    char byte = 0;
    Require(fread_nid_postfix(&byte, 1, 1, &file.stream) == 1 && byte == 'Q', "read the pushed-back byte with fread");
    Require(fpurge_nid_postfix(&file.stream) == 0, "purge after fread consumed the pushback");
    Require(fgetc_nid_postfix(&file.stream) == 'b', "a pushback read by fread does not come back");
}

void CheckInputAfterReopen() {
    constexpr const char* path = "guest_stream_purge.reopen.tmp";
    auto* replacement = std::fopen(path, "wb");
    Require(replacement != nullptr, "open replacement input");
    Require(std::fwrite("new", 1, 3, replacement) == 3 && std::fclose(replacement) == 0, "prepare replacement input");
    BufferedStream file;
    Require(std::fwrite("old", 1, 3, file.native) == 3 && std::fflush(file.native) == 0, "prepare original input");
    Require(std::fseek(file.native, 0, SEEK_SET) == 0, "rewind original input");
    Require(fgetc_nid_postfix(&file.stream) == 'o', "read original input");
    Require(ungetc_nid_postfix('Q', &file.stream) == 'Q', "push back on the original input");
    Require(freopen_nid_postfix(path, "rb", &file.stream) == &file.stream, "reopen replacement input");
    Require(fpurge_nid_postfix(&file.stream) == 0, "purge reopened input");
    char bytes[3]{};
    Require(fread_nid_postfix(bytes, 1, sizeof(bytes), &file.stream) == sizeof(bytes), "read replacement bytes");
    Require(std::memcmp(bytes, "new", sizeof(bytes)) == 0, "reopen discards the previous stream's pushback");
    Require(file.stream.Close() == 0 && std::remove(path) == 0, "close and remove replacement input");
}

void CheckUnbuffered() {
    BufferedStream file;
    Require(std::setvbuf(file.native, nullptr, _IONBF, 0) == 0, "disable buffering");
    Require(std::fwrite("ab", 1, 2, file.native) == 2, "write unbuffered input");
    Require(std::fseek(file.native, 0, SEEK_SET) == 0, "rewind unbuffered input");
    Require(fgetc_nid_postfix(&file.stream) == 'a', "read unbuffered byte");
    const auto position = Position(file.native);
    Require(position >= 1 && position <= 2, "unbuffered descriptor position");
    Require(ungetc_nid_postfix('X', &file.stream) == 'X', "push back on an unbuffered stream");
    Require(fpurge_nid_postfix(&file.stream) == 0 && std::ftell(file.native) == 0, "purge unbuffered pushback at the logical position");
    Require(fgetc_nid_postfix(&file.stream) == 'X', "the pushed-back byte survives an unbuffered purge");
    Require(fgetc_nid_postfix(&file.stream) == 'b', "resume after the byte already read");
}

void CheckLineBuffered() {
    BufferedStream file;
    std::array<char, 1024> lineBuffer{};
    Require(std::setvbuf(file.native, lineBuffer.data(), _IOLBF, lineBuffer.size()) == 0, "set line buffering");
    Require(fwrite_nid_postfix("no_newline", 1, 10, &file.stream) == 10, "buffer line output without newline");
    Require(Position(file.native) == 0, "line output remains in buffer");
    Require(fpurge_nid_postfix(&file.stream) == 0, "purge line-buffered output");
    Require(Position(file.native) == 0, "purge does not flush to disk");
    Require(fwrite_nid_postfix("flushed\n", 1, 8, &file.stream) == 8, "subsequent write with newline");
    Require(std::fflush(file.native) == 0, "flush stream");
    Require(std::fseek(file.native, 0, SEEK_SET) == 0, "rewind stream");
    char readBuffer[16]{};
    Require(std::fread(readBuffer, 1, sizeof(readBuffer), file.native) == 8, "only flushed content persisted");
    Require(std::memcmp(readBuffer, "flushed\n", 8) == 0, "purged line output was discarded");
}

void CheckPipe() {
    int descriptors[2];
#ifdef _WIN32
    Require(::_pipe(descriptors, 4096, _O_BINARY) == 0, "create pipe");
    auto* native = ::_fdopen(descriptors[0], "rb");
    Require(::_write(descriptors[1], "abcdef", 6) == 6 && ::_close(descriptors[1]) == 0, "fill pipe and close writer");
#else
    Require(::pipe(descriptors) == 0, "create pipe");
    auto* native = ::fdopen(descriptors[0], "rb");
    Require(::write(descriptors[1], "abcdef", 6) == 6 && ::close(descriptors[1]) == 0, "fill pipe and close writer");
#endif
    Require(native != nullptr, "open pipe stream");
    std::array<char, 4096> buffer{};
    Require(std::setvbuf(native, buffer.data(), _IOFBF, buffer.size()) == 0, "buffer pipe input");
    FileStream stream{native};
    Require(fgetc_nid_postfix(&stream) == 'a' && ungetc_nid_postfix('X', &stream) == 'X', "read and push back on a pipe");
    Require(fpurge_nid_postfix(&stream) == 0, "purge does not require a seekable descriptor");
    Require(fgetc_nid_postfix(&stream) == EOF, "discard pipe read-ahead and pushback");
    Require(stream.Close() == 0, "close pipe stream");
}
void CheckInvalid() {
    BufferedStream file;
    Require(file.stream.Close() == 0, "close stream");
    errno = 0;
    Require(fpurge_nid_postfix(&file.stream) == -1 && errno == 9, "closed stream reports EBADF");
    bool rejected = false;
    try { fpurge_nid_postfix(nullptr); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "null stream throws as existing stream functions do");
}

}

int main() {
    CheckOutput();
    CheckInput();
    CheckPushbackReadByFread();
    CheckInputAfterReopen();
    CheckUnbuffered();
    CheckLineBuffered();
    CheckPipe();
    CheckInvalid();
    std::puts("stream purge tests passed");
}
