#include "prx/libSceNpCommon/include/Api.hpp"
#include "tests/TemporaryFile.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "sceNpDbgDumpBinary check failed at line %u\n", location.line());
        std::abort();
    }
}

std::string Dumped(const void* data, std::size_t size) {
    std::fflush(stdout);
    auto* captured = TemporaryFile::Open();
    Require(captured != nullptr);
#ifdef _WIN32
    const int saved = ::_dup(::_fileno(stdout));
    Require(saved >= 0 && ::_dup2(::_fileno(captured), ::_fileno(stdout)) == 0);
#else
    const int saved = ::dup(::fileno(stdout));
    Require(saved >= 0 && ::dup2(::fileno(captured), ::fileno(stdout)) == ::fileno(stdout));
#endif
    sceNpDbgDumpBinary(data, size);
    std::fflush(stdout);
#ifdef _WIN32
    Require(::_dup2(saved, ::_fileno(stdout)) == 0 && ::_close(saved) == 0);
#else
    Require(::dup2(saved, ::fileno(stdout)) == ::fileno(stdout) && ::close(saved) == 0);
#endif
    std::rewind(captured);
    std::string output;
    char buffer[4096];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), captured)) != 0) output.append(buffer, read);
    Require(std::fclose(captured) == 0);
    return output;
}

}

int main() {
    const char text[] = "ABCDEFGHIJKLMNOPQRST";
    Require(Dumped(text, 20) == "4142 4344 4546 4748 494a 4b4c 4d4e 4f50  ABCDEFGHIJKLMNOP\n5152 5354                                QRST\n\n");
    const unsigned char mixed[] = {0x00, 0x1f, 0x20, 0x7e, 0x7f, 0xff};
    Require(Dumped(mixed, sizeof(mixed)) == "001f 207e 7fff                           .. ~..\n\n");
    Require(Dumped(text, 0) == "\n");
    Require(Dumped(reinterpret_cast<const void*>(std::uintptr_t{0x10}), 32) == "\n");
    std::puts("sceNpDbgDumpBinary prints its bytes through checked copies");
    return 0;
}
