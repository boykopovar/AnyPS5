#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF::setDataFile check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

using Jsc::WtfAbi::PrintStream;

using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using FlushFunction = void (APS5_VABI*)(PrintStream*);

static void APS5_VABI Write(PrintStream& out, const char* format, ...) {
    auto vprintf = reinterpret_cast<VprintfFunction>(const_cast<void*>(out.vtable[Jsc::WtfAbi::PrintStreamVprintf]));
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    vprintf(&out, format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    vprintf(&out, format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
    reinterpret_cast<FlushFunction>(const_cast<void*>(out.vtable[Jsc::WtfAbi::PrintStreamFlush]))(&out);
}

static void* APS5_VABI Allocate(std::size_t size) {
    return std::malloc(size);
}

static void APS5_VABI Free(void* pointer) {
    std::free(pointer);
}

static void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    return std::calloc(count, size);
}

static void* APS5_VABI Reallocate(void* pointer, std::size_t size) {
    return std::realloc(pointer, size);
}

static void* APS5_VABI Align(std::size_t, std::size_t size) {
    return std::malloc(size);
}

static void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) {
    return std::realloc(pointer, size);
}

static int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

static void RegisterHeap() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign), reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

static std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int main() {
    RegisterHeap();
    const auto root = std::filesystem::temp_directory_path()
        / ("anyps5-nkwebkit-data-file-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(root));
    AddPathAlias_nid_no_patch("/data-file-test", root.string().c_str());
    PrintStream& initial = _ZN3WTF8dataFileEv();

    _ZN3WTF11setDataFileEPKc("/data-file-test/log.%pid.txt");
    PrintStream& first = _ZN3WTF8dataFileEv();
    Require(&first == &initial);
    Write(first, "%s=%d\n", "value", 42);
    std::string pid;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        const std::string name = entry.path().filename().string();
        Require(pid.empty() && name.size() > 8 && name.rfind("log.", 0) == 0 && name.substr(name.size() - 4) == ".txt");
        pid = name.substr(4, name.size() - 8);
    }
    Require(!pid.empty() && pid.find_first_not_of("0123456789") == std::string::npos);
    Require(Read(root / ("log." + pid + ".txt")) == "value=42\n");

    _ZN3WTF11setDataFileEPKc("/data-file-test/%pid-%pid");
    Write(_ZN3WTF8dataFileEv(), "second");
    Require(Read(root / (pid + "-%pid")) == "second");
    Require(Read(root / ("log." + pid + ".txt")) == "value=42\n");

    _ZN3WTF11setDataFileEPKc("/data-file-test/plain.txt");
    Write(_ZN3WTF8dataFileEv(), "plain %u", 7u);
    Require(Read(root / "plain.txt") == "plain 7");

    _ZN3WTF11setDataFileEPKc("/data-file-test/missing/%pid.txt");
    Require(!std::filesystem::exists(root / "missing"));
    Require(&_ZN3WTF8dataFileEv() == &initial);

    std::string longPath = "/data-file-test/" + std::string(1100, 'a') + "%pid";
    _ZN3WTF11setDataFileEPKc(longPath.c_str());
    Require(&_ZN3WTF8dataFileEv() == &initial);

    _ZN3WTF11setDataFileEPKc(nullptr);
    Require(&_ZN3WTF8dataFileEv() == &initial);

    _ZN3WTF11setDataFileEPKc("/data-file-test/again.txt");
    Write(_ZN3WTF8dataFileEv(), "again");
    Require(Read(root / "again.txt") == "again");

    std::puts("libSceNKWebKit WTF::setDataFile passed");
    return 0;
}
