#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" {
extern FileStream* __stdinp_nid_postfix;

}

namespace {

using Gets = char* (APS5_VABI*)(char*);
Gets readLine;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "gets: %s\n", message);
    std::abort();
}

class Input {
    FileStream* previous = __stdinp_nid_postfix;
    FileStream stream;

public:
    explicit Input(std::FILE* handle) : stream(handle) { __stdinp_nid_postfix = &stream; }
    Input(const void* bytes, std::size_t size) : stream(TemporaryFile::Open()) {
        Require(std::fwrite(bytes, 1, size, stream.GetHandle()) == size, "seed input");
        std::rewind(stream.GetHandle());
        __stdinp_nid_postfix = &stream;
    }
    ~Input() {
        __stdinp_nid_postfix = previous;
        if (stream.IsOpen()) stream.Close();
    }
    FileStream& Stream() { return stream; }
};

void CheckBytes() {
    std::array<unsigned char, 257> bytes;
    std::size_t size = 0;
    for (unsigned value = 0; value < 256; ++value)
        if (value != '\n') bytes[size++] = static_cast<unsigned char>(value);
    bytes[size++] = '\n';
    Input input(bytes.data(), size);
    std::array<unsigned char, 264> output;
    output.fill(0xa5);
    *__error_nid_postfix() = 123;
    Require(readLine(reinterpret_cast<char*>(output.data() + 4)) == reinterpret_cast<char*>(output.data() + 4), "byte read pointer");
    const int error = *__error_nid_postfix();
    Require(std::memcmp(output.data() + 4, bytes.data(), 255) == 0 && output[259] == 0, "all byte values and terminator");
    Require(error == 123, "successful read preserves errno");
    for (std::size_t index = 0; index < 4; ++index) Require(output[index] == 0xa5, "prefix guard");
    for (std::size_t index = 260; index < output.size(); ++index) Require(output[index] == 0xa5, "suffix guard");
    output.fill(0xa5);
    Require(readLine(reinterpret_cast<char*>(output.data() + 4)) == nullptr, "EOF return");
    for (const auto value : output) Require(value == 0xa5, "initial EOF leaves buffer unchanged");
    Require((input.Stream().GuestState().flags & 0x20) != 0, "guest EOF flag");
}

void CheckLines() {
    const char text[] = "\nfirst\r\nlast";
    Input input(text, sizeof(text) - 1);
    char output[16];
    Require(readLine(output) == output && output[0] == 0, "empty line");
    Require(readLine(output) == output && std::strcmp(output, "first\r") == 0, "newline removed without dropping carriage return");
    Require(readLine(output) == output && std::strcmp(output, "last") == 0, "partial last line at EOF");
    output[0] = 'x';
    Require(readLine(output) == nullptr && output[0] == 'x', "repeated EOF preserves destination");
}

void CheckErrors() {
    Input input("", 0);
    input.Stream().Close();
    char output[8] = "keep";
    Require(readLine(output) == nullptr && std::strcmp(output, "keep") == 0 && *__error_nid_postfix() == 9, "closed input");
    bool threw = false;
    try { readLine(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw, "null buffer is explicit unsupported input");
}

void CheckReadError(const char* path) {
    {
        Input input(std::fopen(path, "wb"));
        std::array<char, 8> output;
        output.fill('x');
        Require(readLine(output.data()) == nullptr, "write-only input read error");
        for (const auto value : output) Require(value == 'x', "initial read error leaves destination unchanged");
        Require((input.Stream().GuestState().flags & 0x40) != 0, "guest error flag");
    }
    Require(std::remove(path) == 0, "remove error fixture");
}

void CheckThreads() {
    constexpr unsigned lines = 2048;
    std::array<char, lines * 9> text;
    for (unsigned index = 0; index < lines; ++index)
        std::snprintf(text.data() + index * 8, 9, "%07u\n", index);
    Input input(text.data(), lines * 8);
    std::array<std::atomic<unsigned>, lines> seen{};
    std::atomic<bool> invalid{false};
    std::vector<std::thread> workers;
    for (unsigned thread = 0; thread < 8; ++thread) workers.emplace_back([&] {
        char output[lines * 8 + 1];
        while (readLine(output)) {
            if (std::strlen(output) != 7) { invalid = true; continue; }
            unsigned index = 0;
            bool digits = true;
            for (unsigned digit = 0; digit < 7; ++digit) {
                if (output[digit] < '0' || output[digit] > '9') { digits = false; break; }
                index = index * 10 + static_cast<unsigned>(output[digit] - '0');
            }
            if (!digits || index >= lines) invalid = true;
            else ++seen[index];
        }
    });
    for (auto& worker : workers) worker.join();
    Require(!invalid, "concurrent line contents");
    for (auto& count : seen) Require(count == 1, "each line read exactly once");
}

}

int main(int argc, char** argv) {
    Require(argc == 3, "expected libc path and scratch file");
#ifdef _WIN32
    const auto library = LoadLibraryA(argv[1]);
    Require(library != nullptr, "load libc");
    readLine = reinterpret_cast<Gets>(GetProcAddress(library, "gets_nid_postfix"));
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Require(library != nullptr, "load libc");
    readLine = reinterpret_cast<Gets>(dlsym(library, "gets_nid_postfix"));
#endif
    Require(readLine != nullptr, "missing gets export");
    CheckBytes();
    CheckLines();
    CheckErrors();
    CheckReadError(argv[2]);
    CheckThreads();
    std::puts("gets checks passed");
}
