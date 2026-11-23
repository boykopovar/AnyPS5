#include "prx/libSceNKWebKit/include/WtfExports.hpp"
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
        std::fprintf(stderr, "WTF assertion report check failed at line %u\n", location.line());
        std::abort();
    }
}

const char* Unmapped() {
    return reinterpret_cast<const char*>(std::uintptr_t{0x10});
}

template<class Function>
std::string Captured(Function function) {
    std::fflush(stderr);
    auto* captured = TemporaryFile::Open();
    Require(captured != nullptr);
#ifdef _WIN32
    const int saved = ::_dup(::_fileno(stderr));
    Require(saved >= 0 && ::_dup2(::_fileno(captured), ::_fileno(stderr)) == 0);
#else
    const int saved = ::dup(::fileno(stderr));
    Require(saved >= 0 && ::dup2(::fileno(captured), ::fileno(stderr)) == ::fileno(stderr));
#endif
    function();
    std::fflush(stderr);
#ifdef _WIN32
    Require(::_dup2(saved, ::_fileno(stderr)) == 0 && ::_close(saved) == 0);
#else
    Require(::dup2(saved, ::fileno(stderr)) == ::fileno(stderr) && ::close(saved) == 0);
#endif
    std::rewind(captured);
    std::string text;
    char buffer[256];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), captured)) != 0) text.append(buffer, read);
    Require(std::fclose(captured) == 0);
    return text;
}

}

int main() {
    Require(Captured([] { WTFReportAssertionFailure("Source.cpp", 12, "run", "value > 0"); }) == "ASSERTION FAILED: value > 0\n(Source.cpp:12 run)\n");
    Require(Captured([] { WTFReportAssertionFailure(Unmapped(), 12, Unmapped(), Unmapped()); }) == "ASSERTION FAILED: \n(:12 )\n");
    Require(Captured([] { WTFReportArgumentAssertionFailure("Args.cpp", 3, "check", "count", "count > 0"); }) == "ARGUMENT BAD: count, count > 0\n(Args.cpp:3 check)\n");
    Require(Captured([] { WTFReportArgumentAssertionFailure("Args.cpp", 3, "check", Unmapped(), Unmapped()); }) == "ARGUMENT BAD: , \n(Args.cpp:3 check)\n");
    Require(Captured([] { WTFReportNotImplementedYet("Later.cpp", 7, "draw"); }) == "NOT IMPLEMENTED YET\n(Later.cpp:7 draw)\n");
    Require(Captured([] { WTFReportNotImplementedYet(Unmapped(), 7, Unmapped()); }) == "NOT IMPLEMENTED YET\n(:7 )\n");
#ifndef _WIN32
    // On Windows the formatted message goes to guest stdout through PrintWindows while the prefix and suffix go to stderr, so stderr alone cannot be compared.
    Require(Captured([] { WTFReportError("Error.cpp", 9, "load", "plain"); }) == "ERROR: plain\n(Error.cpp:9 load)\n");
    Require(Captured([] { WTFReportError(Unmapped(), 9, Unmapped(), "plain"); }) == "ERROR: plain\n(:9 )\n");
#endif
    std::puts("WTF assertion reports read their file, function, assertion and argument strings through checked copies");
    return 0;
}
