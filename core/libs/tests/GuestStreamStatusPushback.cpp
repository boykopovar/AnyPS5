#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "stream status pushback: %s\n", message);
        std::abort();
    }
}

void Query(FileStream& stream, std::string_view operation) {
    const bool clear = operation == "clearerr" || operation == "clearerr_unlocked";
    errno = 123;
    if (operation == "feof") Require(feof_nid_postfix(&stream) == 0, "EOF remains cleared by ungetc");
    else if (operation == "ferror") Require(ferror_nid_postfix(&stream) != 0, "error query retains the error");
    else if (operation == "feof_unlocked") Require(feof_unlocked_nid_postfix(&stream) == 0, "unlocked EOF remains cleared by ungetc");
    else if (operation == "ferror_unlocked") Require(ferror_unlocked_nid_postfix(&stream) != 0, "unlocked error query retains the error");
    else if (operation == "clearerr") clearerr_nid_postfix(&stream);
    else if (operation == "clearerr_unlocked") clearerr_unlocked_nid_postfix(&stream);
    else Require(false, "unknown status operation");
    Require(errno == 123, "status operation preserves errno");
    Require((stream.GuestState().flags & 0x60) == (clear ? 0 : 0x40), "publish the expected status indicators");
}

void Check(std::string_view operation, bool atEnd) {
    auto* native = TemporaryFile::Open();
    Require(native != nullptr, "create a temporary stream");
    std::array<char, 4096> buffer{};
    Require(std::setvbuf(native, buffer.data(), _IOFBF, buffer.size()) == 0, "buffer the temporary stream");
    FileStream stream(native);
    Require(std::fwrite("abcdef", 1, 6, native) == 6 && std::fflush(native) == 0, "prepare input");
    Require(std::fseek(native, 0, SEEK_SET) == 0, "rewind input");
    for (const char byte : {'a', 'b', 'c'}) Require(fgetc_nid_postfix(&stream) == byte, "read the initial bytes");
    if (atEnd) {
        for (const char byte : {'d', 'e', 'f'}) Require(fgetc_nid_postfix(&stream) == byte, "read the remaining bytes");
        Require(fgetc_nid_postfix(&stream) == EOF, "set the EOF indicator");
    }
    stream.SetError();
    Require(ungetc_nid_postfix('Q', &stream) == 'Q', "push back a replacement byte");
    const auto position = std::ftell(native);
    Query(stream, operation);
    Query(stream, operation);
    Require(std::ftell(native) == position, "status operation preserves logical position");
    Require(fpurge_nid_postfix(&stream) == 0, "purge after status operations");
    Require(fgetc_nid_postfix(&stream) == 'Q', "pending input survives a status operation and purge");
    Query(stream, operation);
    Require(fpurge_nid_postfix(&stream) == 0, "purge after consuming the replacement");
    Require(fgetc_nid_postfix(&stream) == (atEnd ? EOF : 'd'), "status operation does not resurrect consumed input");
    Require(stream.Close() == 0, "close the stream");
}

}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    Check(argv[1], false);
    Check(argv[1], true);
    std::printf("stream status pushback: %s middle and EOF cases passed\n", argv[1]);
}
