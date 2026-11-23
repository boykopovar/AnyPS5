#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <locale>
#include <memory>
#include <new>
#include <regex>
#include <stdexcept>
#include <string>
#include <typeinfo>

namespace {

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "libstdc++ DLL throws: %s\n", what);
    std::fflush(stderr);
    std::abort();
}

int destroyed = 0;
struct Cleanup {
    ~Cleanup() { ++destroyed; }
};

[[gnu::noinline]] void CompileBadRegex() {
    Cleanup cleanup;
    std::regex pattern("(");
    static_cast<void>(pattern);
}

[[gnu::noinline]] int ParseBadInteger() {
    Cleanup cleanup;
    return std::stoi("not a number");
}

void RegexErrorReachesCatch() {
    destroyed = 0;
    bool caught = false;
    try {
        CompileBadRegex();
    } catch (const std::regex_error& error) {
        caught = error.code() == std::regex_constants::error_paren;
    }
    Require(caught, "std::regex_error from the runtime DLL was not caught by type");
    Require(destroyed == 1, "the frame between the throw and the catch was not cleaned up");
}

void InvalidArgumentReachesBaseCatch() {
    destroyed = 0;
    bool caught = false;
    try {
        ParseBadInteger();
    } catch (const std::logic_error& error) {
        caught = std::strstr(error.what(), "stoi") != nullptr;
    }
    Require(caught, "std::invalid_argument from the runtime DLL was not caught as its base class");
    Require(destroyed == 1, "the frame between the throw and the catch was not cleaned up");
}

void RethrowAndExceptionPointer() {
    std::exception_ptr saved;
    try {
        try {
            ParseBadInteger();
        } catch (const std::invalid_argument&) {
            saved = std::current_exception();
            throw;
        }
    } catch (const std::exception& error) {
        Require(std::strstr(error.what(), "stoi") != nullptr, "a rethrown runtime DLL exception lost its message");
    }
    Require(static_cast<bool>(saved), "std::current_exception was empty for a runtime DLL exception");
    bool caught = false;
    try {
        std::rethrow_exception(saved);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught, "std::rethrow_exception of a runtime DLL exception was not caught");
}

[[gnu::noinline]] void OpenMissingLocale() {
    Cleanup cleanup;
    std::locale missing("no-such-locale.anyps5");
    static_cast<void>(missing);
}

void RuntimeCatchAndRethrow() {
    destroyed = 0;
    bool caught = false;
    try {
        OpenMissingLocale();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    Require(caught, "an exception the runtime DLL caught and rethrew did not reach the catch");
    Require(destroyed == 1, "the frame between the rethrow and the catch was not cleaned up");
}

[[gnu::noinline]] std::filesystem::path CanonicalMissingPath() {
    Cleanup cleanup;
    return std::filesystem::canonical(std::filesystem::path(u8"Z:/anyps5/no such directory/\u00fc/file"));
}

void FilesystemErrorReachesCatch() {
    destroyed = 0;
    bool caught = false;
    try {
        CanonicalMissingPath();
    } catch (const std::filesystem::filesystem_error& error) {
        caught = error.code().value() != 0 && error.path1().filename() == "file";
    }
    Require(caught, "std::filesystem::filesystem_error from the runtime DLL was not caught by type");
    Require(destroyed == 1, "the frame between the throw and the catch was not cleaned up");
}

[[gnu::noinline]] int* AllocateTooMany() {
    Cleanup cleanup;
    std::allocator<int> allocator;
    return allocator.allocate(static_cast<std::size_t>(PTRDIFF_MAX) / sizeof(int) + 1);
}

void BadAllocReachesCatch() {
    destroyed = 0;
    bool caught = false;
    try {
        static_cast<void>(AllocateTooMany());
    } catch (const std::bad_alloc& error) {
        caught = typeid(error) == typeid(std::bad_alloc);
    }
    Require(caught, "std::bad_alloc from the runtime DLL was not caught by type");
    Require(destroyed == 1, "the frame between the throw and the catch was not cleaned up");
}

void CatchAll() {
    bool caught = false;
    try {
        CompileBadRegex();
    } catch (...) {
        caught = true;
    }
    Require(caught, "catch (...) did not receive a runtime DLL exception");
}

}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "";
    if (std::strcmp(mode, "regex") == 0) RegexErrorReachesCatch();
    else if (std::strcmp(mode, "base") == 0) InvalidArgumentReachesBaseCatch();
    else if (std::strcmp(mode, "rethrow") == 0) RethrowAndExceptionPointer();
    else if (std::strcmp(mode, "all") == 0) CatchAll();
    else if (std::strcmp(mode, "rethrown") == 0) RuntimeCatchAndRethrow();
    else if (std::strcmp(mode, "filesystem") == 0) FilesystemErrorReachesCatch();
    else if (std::strcmp(mode, "badalloc") == 0) BadAllocReachesCatch();
    else Require(false, "unknown mode");
    for (int round = 0; round < 1000; ++round) RegexErrorReachesCatch();
    std::puts("libstdc++ DLL throw checks passed");
    return 0;
}
