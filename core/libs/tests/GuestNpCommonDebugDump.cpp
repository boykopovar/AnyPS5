#include "prx/libSceNpCommon/include/Api.hpp"
#include "SceTypes.hpp"
#include "tests/TemporaryFile.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "NpCommon debug dump: %s\n", message);
    std::abort();
}

class StandardOutputCapture {
public:
    StandardOutputCapture() : file_(TemporaryFile::Open()) {
        Require(file_ != nullptr, "the capture file could not be opened");
        std::fflush(stdout);
#ifdef _WIN32
        saved_ = ::_dup(::_fileno(stdout));
        Require(saved_ >= 0 && ::_dup2(::_fileno(file_), ::_fileno(stdout)) == 0, "stdout could not be redirected");
#else
        saved_ = ::dup(::fileno(stdout));
        Require(saved_ >= 0 && ::dup2(::fileno(file_), ::fileno(stdout)) == ::fileno(stdout), "stdout could not be redirected");
#endif
    }

    ~StandardOutputCapture() {
        Restore();
        if (file_ != nullptr) std::fclose(file_);
    }

    std::string Text() {
        Restore();
        std::rewind(file_);
        std::string text;
        char buffer[256];
        while (const auto count = std::fread(buffer, 1, sizeof(buffer), file_)) text.append(buffer, count);
        return text;
    }

private:
    void Restore() {
        if (saved_ < 0) return;
        std::fflush(stdout);
#ifdef _WIN32
        ::_dup2(saved_, ::_fileno(stdout));
        ::_close(saved_);
#else
        ::dup2(saved_, ::fileno(stdout));
        ::close(saved_);
#endif
        saved_ = -1;
    }

    std::FILE* file_;
    int saved_ = -1;
};

template <class TAction>
std::string Captured(TAction action) {
    StandardOutputCapture capture;
    action();
    return capture.Text();
}

void Expect(const std::string& output, const std::string& expected, const char* message) {
    if (output == expected) return;
    std::fprintf(stderr, "output was:\n[%s]\nexpected:\n[%s]\n", output.c_str(), expected.c_str());
    Require(false, message);
}

}

int main() {
    try {
        Expect(Captured([] { sceNpDbgDumpBinary(nullptr, 0); }), "\n", "an empty dump is a single blank line");

        const std::uint8_t three[] = {0x41, 0x00, 0x7f};
        Expect(Captured([&] { sceNpDbgDumpBinary(three, sizeof(three)); }),
            "4100 7f" + std::string(33, ' ') + " A..\n\n", "a short line pads the missing bytes");

        std::uint8_t seventeen[17];
        for (int index = 0; index < 17; ++index) seventeen[index] = static_cast<std::uint8_t>(0x20 + index);
        Expect(Captured([&] { sceNpDbgDumpBinary(seventeen, sizeof(seventeen)); }),
            "2021 2223 2425 2627 2829 2a2b 2c2d 2e2f   !\"#$%&'()*+,-./\n"
            "30" + std::string(38, ' ') + " 0\n\n",
            "a dump wraps after 16 bytes");

        const std::uint8_t edges[] = {0x1f, 0x20, 0x7e, 0x7f, 0x80, 0xff};
        Expect(Captured([&] { sceNpDbgDumpBinary(edges, sizeof(edges)); }),
            "1f20 7e7f 80ff" + std::string(26, ' ') + " . ~...\n\n",
            "only bytes 0x20 to 0x7e are shown as characters");

        int written = -1;
        Expect(Captured([&] { written = sceNpDbgDumpText("hello world", 5); }), "hello", "the text dump stops at the length");
        Require(written == 5, "the text dump returns the number of characters written");
        Expect(Captured([&] { written = sceNpDbgDumpText("ab\ncd", 5); }), "ab\ncd", "the text dump writes newlines unchanged");
        Require(written == 5, "the text dump counts newlines");
        Expect(Captured([&] { written = sceNpDbgDumpText("ab\0cd", 5); }), "ab", "the text dump stops at a null character");
        Require(written == 2, "the text dump does not count past a null character");

        Expect(Captured([] { sceNpHeapShowStat(reinterpret_cast<const NpHeap*>(0x10)); }),
            "[NpCommon] heap statistics unavailable\n", "an unreadable heap reports no statistics");

        NpKernelMemory memory {};
        void* address = nullptr;
        Require(sceNpAllocateKernelMemoryWithAlignment(0x10, "np-dump", &address, &memory) == 0, "kernel memory is mapped");
        NpHeap heap {};
        Require(sceNpHeapInit(&heap, memory.address, memory.size, "np-dump") == 0, "a heap is created");
        NpHeapStat stat {};
        Require(sceNpHeapGetStat(&heap, &stat) == 0, "heap statistics are read");
        const std::string expected = "[NpCommon] heap: system " + std::to_string(stat.maxSystemSize) + " bytes, peak in use "
            + std::to_string(stat.maxInuseSize) + " bytes, in use " + std::to_string(stat.currentInuseSize) + " bytes\n";
        Expect(Captured([&] { sceNpHeapShowStat(&heap); }), expected, "the heap statistics are printed as sceNpHeapGetStat reports them");
        sceNpHeapDestroy(&heap);
        Require(sceNpFreeKernelMemory(&memory) == 0, "kernel memory is released");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NpCommon debug dump: %s\n", error.what());
        return 1;
    }
    std::puts("NpCommon debug dump tests passed");
    return 0;
}
