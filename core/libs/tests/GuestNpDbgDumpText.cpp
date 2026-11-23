#include "prx/libSceNpCommon/include/Api.hpp"
#include "tests/TemporaryFile.hpp"

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
        std::fprintf(stderr, "sceNpDbgDumpText check failed at line %u\n", location.line());
        std::abort();
    }
}

std::string Dumped(const char* text, int length, int expected) {
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
    const int result = sceNpDbgDumpText(text, length);
    std::fflush(stdout);
#ifdef _WIN32
    Require(::_dup2(saved, ::_fileno(stdout)) == 0 && ::_close(saved) == 0);
#else
    Require(::dup2(saved, ::fileno(stdout)) == ::fileno(stdout) && ::close(saved) == 0);
#endif
    Require(result == expected);
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
    Require(Dumped("hello world", 5, 5) == "hello");
    Require(Dumped("abc", 10, 3) == "abc");
    Require(Dumped("abc", -1, 3) == "abc");
    Require(Dumped("abc", 0, 0).empty());
    std::string longText(10000, 'x');
    for (std::size_t index = 0; index < longText.size(); ++index) longText[index] = static_cast<char>('a' + index % 26);
    Require(Dumped(longText.c_str(), 9000, 9000) == longText.substr(0, 9000));
    Require(Dumped(longText.c_str(), -1, 10000) == longText);
    Require(Dumped(nullptr, 4, 0).empty());
    Require(Dumped(reinterpret_cast<const char*>(std::uintptr_t{0x10}), 4, 0).empty());
    std::puts("sceNpDbgDumpText prints its text through checked copies");
    return 0;
}
