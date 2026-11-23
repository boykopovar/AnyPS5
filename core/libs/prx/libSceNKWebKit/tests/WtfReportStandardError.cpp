#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "tests/TemporaryFile.hpp"

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
        std::fprintf(stdout, "WTF standard error check failed at line %u\n", location.line());
        std::fflush(stdout);
        std::abort();
    }
}

std::string Escaped(const std::string& text) {
    std::string shown;
    for (const unsigned char c : text) {
        if (c == '\n') {
            shown += "\\n";
        } else if (c < 32 || c >= 127) {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "\\x%02x", c);
            shown += hex;
        } else {
            shown += static_cast<char>(c);
        }
    }
    return shown;
}

template<class Function>
std::string Captured(Function function) {
    std::fflush(stdout);
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
    std::fflush(stdout);
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

void RequireText(const std::string& actual, const std::string& expected, std::source_location location = std::source_location::current()) {
    if (actual == expected) return;
    std::fprintf(stdout, "expected \"%s\"\nstderr   \"%s\"\n", Escaped(expected).c_str(), Escaped(actual).c_str());
    Require(false, location);
}

}

int main() {
    RequireText(Captured([] { WTFReportError("Error.cpp", 9, "load", "count %d of %s", 3, "many"); }),
                "ERROR: count 3 of many\n(Error.cpp:9 load)\n");
    RequireText(Captured([] { WTFReportFatalError("Fatal.cpp", 2, "stop", "code %u", 7u); }),
                "FATAL ERROR: code 7\n(Fatal.cpp:2 stop)\n");
    RequireText(Captured([] { WTFReportAssertionFailureWithMessage("Check.cpp", 5, "verify", "size > 0", "size %d", -1); }),
                "ASSERTION FAILED: size -1\nsize > 0\n(Check.cpp:5 verify)\n");
    RequireText(Captured([] { WTFLogAlways("logged %s %d", "value", 42); }), "logged value 42\n");
    RequireText(Captured([] { _ZN3WTF8dataLogFEPKcz("data %d", 11); }), "data 11");
    std::puts("WTF reports and logs write their formatted messages to standard error");
    return 0;
}
