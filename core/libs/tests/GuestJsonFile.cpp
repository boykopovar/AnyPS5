#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct FileTrace {
    std::string_view input;
    std::size_t position = 0;
    std::size_t chunk = 1;
    std::size_t bufferSize = 997;
    int descriptor = 99;
    std::int64_t readError = 0;
    int opens = 0;
    int reads = 0;
    int closes = 0;
    int bufferAllocations = 0;
    int bufferFrees = 0;
    int notifications = 0;
    void* buffer = nullptr;
    bool failBuffer = false;
};
static FileTrace* activeFile = nullptr;
extern "C" int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode) {
    auto& trace = *activeFile;
    Require(std::strcmp(path, "input.json") == 0 && flags == 0 && mode == 0555, "File parser must pass its filename and native open arguments");
    Require(trace.buffer != nullptr, "Open must follow successful file-buffer allocation");
    ++trace.opens;
    return trace.descriptor;
}
extern "C" std::int64_t APS5_VABI sceKernelRead(ValueType descriptor, void* output, std::size_t bytes) {
    auto& trace = *activeFile;
    Require(descriptor == trace.descriptor && output == trace.buffer && bytes == trace.bufferSize, "Read must use the configured buffer and descriptor");
    ++trace.reads;
    if (trace.readError) return trace.readError;
    const auto count = std::min({bytes, trace.chunk, trace.input.size() - trace.position});
    std::memcpy(output, trace.input.data() + trace.position, count);
    trace.position += count;
    return static_cast<std::int64_t>(count);
}
extern "C" int APS5_VABI sceKernelClose(ValueType descriptor) {
    auto& trace = *activeFile;
    Require(descriptor == trace.descriptor && trace.buffer != nullptr, "Close must precede release of the file buffer");
    ++trace.closes;
    return 0;
}
static void* APS5_VABI AllocateFile(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<FileTrace*>(context);
    if (bytes == trace.bufferSize) {
        ++trace.bufferAllocations;
        if (trace.failBuffer) return nullptr;
        Require(!trace.buffer, "Only one live file buffer per parse");
        trace.buffer = std::malloc(bytes);
        return trace.buffer;
    }
    return std::malloc(bytes);
}
static void APS5_VABI FreeFile(void*, void* memory, void* context) {
    auto& trace = *static_cast<FileTrace*>(context);
    if (memory && memory == trace.buffer) { ++trace.bufferFrees; trace.buffer = nullptr; }
    std::free(memory);
}
static void APS5_VABI NotifyFile(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<FileTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && bytes == trace.bufferSize, "File allocation failure must notify its exact size and OOM status");
    ++trace.notifications;
}
static void* const FileVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateFile), reinterpret_cast<void*>(FreeFile), reinterpret_cast<void*>(NotifyFile)};
struct FileAllocator { void* const* vtable = FileVtable; };
static void CheckParse(FileTrace& trace, std::string_view input, int descriptor, std::int64_t error, int expected) {
    trace.input = input; trace.position = 0; trace.descriptor = descriptor; trace.readError = error;
    const int opens = trace.opens, closes = trace.closes, frees = trace.bufferFrees;
    Value value{};
    _ZN3sce4Json5ValueC1Ev(&value);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(&value, "input.json") == expected, "File parse must preserve success, syntax, open and read results");
    Require(trace.opens == opens + 1 && trace.closes == closes + (descriptor >= 0 ? 1 : 0) && trace.bufferFrees == frees + 1 && !trace.buffer,
            "Every completed file parse must close its owned descriptor and release its buffer exactly once");
    if (!expected) Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5ValueixEPKc(&value, "answer")) == 42, "Short reads must assemble the parsed result");
    _ZN3sce4Json5ValueD1Ev(&value);
}
int main() {
    FileTrace trace; activeFile = &trace;
    FileAllocator allocator;
    Parameter parameter{&allocator, &trace, trace.bufferSize, 2, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Value value{}; _ZN3sce4Json5ValueC1Ev(&value);
    Require(static_cast<unsigned>(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(&value, "input.json")) == 0x80848110 && !trace.opens && !trace.bufferAllocations, "Inactive file parser must refuse before allocation or open");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize file parser");
    parameter.bufferSize = 3;
    for (std::size_t chunk : {std::size_t{1}, std::size_t{7}, trace.bufferSize}) {
        trace.chunk = chunk;
        CheckParse(trace, "{\"answer\":42}", 99, 0, 0);
    }
    CheckParse(trace, "", 99, 0, static_cast<int>(0x80848101));
    CheckParse(trace, "{\"answer\":", 99, 0, static_cast<int>(0x80848101));
    CheckParse(trace, "", static_cast<int>(0x80020002), 0, static_cast<int>(0x80848103));
    CheckParse(trace, "", static_cast<int>(0x8002000d), 0, static_cast<int>(0x8002000d));
    CheckParse(trace, "", -1, 0, static_cast<int>(0x80848110));
    CheckParse(trace, "", 99, static_cast<int>(0x80020005), static_cast<int>(0x80020005));
    trace.failBuffer = true;
    const int opens = trace.opens, closes = trace.closes, frees = trace.bufferFrees;
    Require(static_cast<unsigned>(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(&value, "input.json")) == 0x80848110 && trace.notifications == 1 && trace.opens == opens && trace.closes == closes && trace.bufferFrees == frees,
            "Failed file-buffer allocation must notify, avoid open and preserve the native parser status");
    trace.failBuffer = false;
    CheckParse(trace, "{\"answer\":42}", 99, 0, 0);
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate file parser");
    parameter.bufferSize = 0;
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize without a file buffer");
    _ZN3sce4Json5ValueC1Ev(&value);
    Require(static_cast<unsigned>(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(&value, "input.json")) == 0x80848105 && trace.opens == opens + 1, "Zero configured file-buffer size must refuse before open");
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate zero-buffer configuration");
    return 0;
}
