#include "prx/libkernel/include/Api.hpp"
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
        std::fprintf(stderr, "C++ exception info check failed at line %u\n", location.line());
        std::abort();
    }
}

const char* Unmapped() {
    return reinterpret_cast<const char*>(std::uintptr_t{0x10});
}

std::string Written(const char* typeName, const char* what) {
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
    const int result = sceKernelDebugWriteCppExceptionInfo(nullptr, 0, typeName, what);
    std::fflush(stderr);
#ifdef _WIN32
    Require(::_dup2(saved, ::_fileno(stderr)) == 0 && ::_close(saved) == 0);
#else
    Require(::dup2(saved, ::fileno(stderr)) == ::fileno(stderr) && ::close(saved) == 0);
#endif
    Require(result == 0);
    std::rewind(captured);
    std::string text;
    char buffer[256];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), captured)) != 0) text.append(buffer, read);
    Require(std::fclose(captured) == 0);
    return text;
}

bool Reports(const std::string& text, const std::string& tail) {
    const std::string head = "[coredump] uncaught C++ exception ";
    return text.rfind(head, 0) == 0 && text.size() >= head.size() + tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

}

int main() {
    Require(Reports(Written("std::runtime_error", "bad state"), " of type std::runtime_error, what(): bad state\n"));
    Require(Reports(Written("std::bad_alloc", nullptr), " of type std::bad_alloc\n"));
    Require(Reports(Written(nullptr, nullptr), " of type (unknown)\n"));
    Require(Reports(Written(Unmapped(), Unmapped()), " of type (unknown)\n"));
    Require(Reports(Written(Unmapped(), "lost type"), " of type (unknown), what(): lost type\n"));
    std::puts("sceKernelDebugWriteCppExceptionInfo reads its type name and message through checked copies");
    return 0;
}
