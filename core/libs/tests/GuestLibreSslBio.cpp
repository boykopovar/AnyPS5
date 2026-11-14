#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <fstream>
#include <initializer_list>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef ANYPS5_BIO_DYNAMIC
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

using LibreSsl::Bio;
using LibreSsl::BioBuffer;
using LibreSsl::BioMethod;

struct TestBioSslData {
    void* ssl;
    int renegotiates;
    std::uint64_t renegotiateCount;
    std::uint64_t byteCount;
    std::uint64_t renegotiateTimeout;
    std::int64_t lastTime;
};

static_assert(sizeof(TestBioSslData) == 48);
static_assert(offsetof(TestBioSslData, renegotiateCount) == 16);
static_assert(offsetof(TestBioSslData, byteCount) == 24);
static_assert(offsetof(TestBioSslData, renegotiateTimeout) == 32);
static_assert(offsetof(TestBioSslData, lastTime) == 40);


#ifdef ANYPS5_BIO_DYNAMIC
namespace {
decltype(&BIO_s_mem_nid_postfix) bioMemoryMethod;
decltype(&BIO_new_nid_postfix) bioNew;
decltype(&BIO_new_mem_buf_nid_postfix) bioNewBuffer;
decltype(&BIO_free_nid_postfix) bioFree;
decltype(&BIO_ctrl_nid_postfix) bioControl;
decltype(&BIO_write_nid_postfix) bioWrite;
decltype(&BIO_puts_nid_postfix) bioPuts;
decltype(&calloc_nid_postfix) guestCalloc;
decltype(&free_nid_postfix) guestFree;
decltype(&ApplicationHeapRegister_nid_no_patch) registerHeap;
decltype(&ERR_clear_error_nid_postfix) clearError;
decltype(&ERR_peek_error_nid_postfix) peekError;
decltype(&ERR_get_error_nid_postfix) getError;

void* Load(const std::filesystem::path& path) {
#ifdef _WIN32
    auto* module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    if (!module) throw std::runtime_error("failed to load patched BIO dependency: " + path.string());
    return module;
}

template<class TFunction>
void Resolve(TFunction& function, void* module, const char* name) {
#ifdef _WIN32
    function = reinterpret_cast<TFunction>(GetProcAddress(static_cast<HMODULE>(module), name));
#else
    function = reinterpret_cast<TFunction>(dlsym(module, name));
#endif
    if (!function) throw std::runtime_error(std::string("missing patched BIO dependency: ") + name);
}

void InitializePatched(const std::filesystem::path& directory) {
    void* libc = Load(directory / "libc.prx");
    void* bio = Load(directory / "libSceLibreSsl.prx");
    Resolve(bioMemoryMethod, bio, "R86sXhW699g");
    Resolve(bioNew, bio, "ARlY6zhSacE");
    Resolve(bioNewBuffer, bio, "GMzjS9jvyDQ");
    Resolve(bioFree, bio, "lpJ3zhgftCg");
    Resolve(bioControl, bio, "RHpq9hQfTj8");
    Resolve(bioWrite, bio, "nH7XM+HhXWc");
    Resolve(bioPuts, bio, "eVcWmVlimPA");
    Resolve(guestCalloc, libc, "2X5agFjKxMc");
    Resolve(guestFree, libc, "tIhsqj0qsFE");
    Resolve(registerHeap, libc, "ApplicationHeapRegister_nid_no_patch");
    Resolve(clearError, bio, "sieQ--PINWg");
    Resolve(peekError, bio, "Aytjey6FPTU");
    Resolve(getError, bio, "DJg+qWX1rwY");
}
}  // namespace

#define BIO_s_mem_nid_postfix bioMemoryMethod
#define BIO_new_nid_postfix bioNew
#define BIO_new_mem_buf_nid_postfix bioNewBuffer
#define BIO_free_nid_postfix bioFree
#define BIO_ctrl_nid_postfix bioControl
#define BIO_write_nid_postfix bioWrite
#define BIO_puts_nid_postfix bioPuts
#define calloc_nid_postfix guestCalloc
#define free_nid_postfix guestFree
#define ApplicationHeapRegister_nid_no_patch registerHeap
#define ERR_clear_error_nid_postfix clearError
#define ERR_peek_error_nid_postfix peekError
#define ERR_get_error_nid_postfix getError
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int allocationsUntilFailure = -1;
std::atomic<int> allocations{0};
void* wipePointer = nullptr;
std::size_t wipeSize = 0;

void* APS5_VABI Allocate(std::size_t) { throw std::logic_error("unexpected nonzeroed BIO allocation"); }
void* APS5_VABI Reallocate(void*, std::size_t) { throw std::logic_error("unexpected BIO reallocation"); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::logic_error("unexpected BIO aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected BIO realignment"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected BIO aligned allocation"); }

void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    void* result = std::calloc(count, size);
    if (result) ++allocations;
    return result;
}

void APS5_VABI Free(void* pointer) {
    if (pointer && pointer == wipePointer) {
        for (std::size_t i = 0; i < wipeSize; ++i) Require(static_cast<unsigned char*>(pointer)[i] == 0, "freed buffer was not wiped");
        wipePointer = nullptr;
    }
    if (pointer) --allocations;
    std::free(pointer);
}

void InitializeHeap() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

template<class TFunction>
void RequireUnsupported(TFunction function) {
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "unsupported BIO path did not throw");
}

void RequireError(std::uint64_t expected) {
    Require(ERR_peek_error_nid_postfix() == expected, "BIO error code or ordering");
    Require(ERR_get_error_nid_postfix() == expected, "consume BIO error");
    Require(ERR_get_error_nid_postfix() == 0, "unexpected extra BIO error");
}

BioBuffer* Buffer(Bio* bio) {
    BioBuffer* buffer = nullptr;
    Require(BIO_ctrl_nid_postfix(bio, 115, 0, &buffer) == 1 && buffer, "get buffer");
    return buffer;
}

void RequireData(Bio* bio, const std::string& expected) {
    char* bytes = nullptr;
    Require(BIO_ctrl_nid_postfix(bio, 3, 0, &bytes) == static_cast<std::int64_t>(expected.size()), "buffer data length");
    Require(expected.empty() || (bytes && std::memcmp(bytes, expected.data(), expected.size()) == 0), "buffer data bytes");
    Require(BIO_ctrl_nid_postfix(bio, 10, 0, nullptr) == static_cast<std::int64_t>(expected.size()), "pending bytes");
    Require(BIO_ctrl_nid_postfix(bio, 2, 0, nullptr) == expected.empty(), "EOF flag");
}

void TestWritable() {
    const auto* method = BIO_s_mem_nid_postfix();
    Require(method && method == BIO_s_mem_nid_postfix(), "stable method pointer");
    Require(method->type == 0x401 && std::strcmp(method->name, "memory buffer") == 0, "method identity");
    Require(method->write && method->read && method->puts && method->gets && method->control && method->create && method->destroy, "method callbacks");
    Require(method->callbackControl == nullptr, "no callback control method");
    Bio* bio = BIO_new_nid_postfix(method);
    Require(bio && bio->method == method && bio->initialized == 1 && bio->references == 1, "new BIO state");
    Require(!bio->callback && !bio->callbackArgument && !bio->flags && !bio->retryReason && !bio->next && !bio->previous && !bio->extraData, "new BIO zero fields");
    Require(bio->number == -1 && bio->bytesRead == 0 && bio->bytesWritten == 0, "new BIO counters");
    Require(BIO_write_nid_postfix(bio, nullptr, 7) == -1, "null write reaches memory method");
    RequireError(0x20fff073);
    Require(BIO_write_nid_postfix(bio, "x", 0) == 0, "zero-length owned write");
    RequireError(0);
    RequireData(bio, "");
    auto* buffer = Buffer(bio);
    Require(buffer->capacity == 0 && buffer->data == nullptr, "new buffer storage");
    char output[16]{};
    Require(method->read(bio, output, sizeof(output)) == -1 && (bio->flags & 0xf) == 9, "empty writable BIO retry");
    Require(BIO_write_nid_postfix(bio, "abc", 3) == 3 && (bio->flags & 0xf) == 0, "write clears retry flags");
    RequireData(bio, "abc");
    Require(buffer->capacity == 8 && buffer->data[3] == 0, "first buffer expansion");
    std::memset(buffer->data + buffer->length, 0xa5, buffer->capacity - buffer->length);
    wipePointer = buffer->data;
    wipeSize = buffer->capacity;
    Require(BIO_write_nid_postfix(bio, "0123456789", 10) == 10, "second write");
    Require(wipePointer == nullptr, "growth frees the wiped allocation");
    Require(buffer->capacity == 20 && buffer->data[13] == 0, "second buffer expansion");
    RequireData(bio, "abc0123456789");
    Require(method->read(bio, output, 4) == 4 && std::memcmp(output, "abc0", 4) == 0, "partial memory read");
    RequireData(bio, "123456789");
    Require(BIO_puts_nid_postfix(bio, "xy") == 2 && bio->bytesWritten == 15, "puts and byte accounting");
    RequireData(bio, "123456789xy");
    Require(method->read(bio, nullptr, 3) == 3, "null raw read reports available count");
    RequireData(bio, "123456789xy");
    Require(method->read(bio, output, -3) == -3, "negative raw read");
    Require(BIO_write_nid_postfix(bio, nullptr, 5) == -1, "reject null public source");
    RequireError(0x20fff073);
    RequireData(bio, "123456789xy");
    Require(BIO_ctrl_nid_postfix(bio, 11, 0, nullptr) == 1 && BIO_ctrl_nid_postfix(bio, 12, 0, nullptr) == 1, "flush and duplicate controls");
    Require(BIO_ctrl_nid_postfix(bio, 13, 0, nullptr) == 0, "no pending output");
    Require(BIO_ctrl_nid_postfix(bio, 6, 0, nullptr) == 0 && BIO_ctrl_nid_postfix(bio, 7, 0, nullptr) == 0, "push and pop controls");
    Require(BIO_ctrl_nid_postfix(bio, 0x7fffffff, 0, nullptr) == 0, "unknown memory control");
    Require(BIO_ctrl_nid_postfix(bio, 1, 0, nullptr) == 1, "reset writable BIO");
    RequireData(bio, "");
    for (std::size_t i = 0; i < buffer->capacity; ++i) Require(buffer->data[i] == 0, "reset wipes full allocation");
    Require(BIO_ctrl_nid_postfix(bio, 130, 7, nullptr) == 1, "set EOF return");
    Require(method->read(bio, output, 1) == 7 && (bio->flags & 0xf) == 9, "configured EOF return and retry");
    Require(BIO_ctrl_nid_postfix(bio, 130, 0, nullptr) == 1, "set terminal EOF");
    Require(method->read(bio, output, 1) == 0 && (bio->flags & 0xf) == 0, "terminal EOF clears retry");
    Require(BIO_puts_nid_postfix(bio, "secret") == 6, "write before destruction");
    wipePointer = buffer->data;
    wipeSize = buffer->capacity;
    Require(BIO_free_nid_postfix(bio) == 1, "free writable BIO");
    Require(wipePointer == nullptr, "destruction frees wiped storage");
}

void TestReadOnlyAndLines() {
    const std::array<char, 8> source{'a', '\0', 'b', '\n', 'c', 'd', '\n', 'e'};
    Bio* bio = BIO_new_mem_buf_nid_postfix(source.data(), source.size());
    Require(bio && (bio->flags & 0x200) && bio->number == 0, "read-only BIO state");
    auto* buffer = Buffer(bio);
    Require(buffer->data == source.data() && buffer->capacity == source.size(), "read-only buffer is borrowed");
    char output[16];
    std::memset(output, 0x55, sizeof(output));
    Require(bio->method->gets(bio, output, sizeof(output)) == 4 && std::memcmp(output, source.data(), 4) == 0 && output[4] == 0, "line includes embedded NUL and newline");
    Require(buffer->data == source.data() + 4 && buffer->length == 4, "borrowed buffer advances");
    Require(bio->method->gets(bio, output, 3) == 2 && std::strcmp(output, "cd") == 0, "line length bound");
    Require(bio->method->gets(bio, output, 1) == 0 && output[0] == 0 && buffer->length == 2, "one-byte line buffer");
    Require(bio->method->gets(bio, output, sizeof(output)) == 1 && std::strcmp(output, "\n") == 0, "newline on next read");
    Require(bio->method->read(bio, output, sizeof(output)) == 1 && output[0] == 'e', "last read-only byte");
    Require(bio->method->read(bio, output, 1) == 0 && (bio->flags & 0xf) == 0, "read-only EOF");
    Require(BIO_ctrl_nid_postfix(bio, 1, 0, nullptr) == 1, "read-only reset");
    Require(buffer->data == source.data() && buffer->length == source.size(), "read-only reset restores original range");
    Require(BIO_write_nid_postfix(bio, "x", 1) == -1, "reject read-only write");
    RequireError(0x20fff07e);
    Require(buffer->data == source.data() && buffer->length == source.size(), "rejected write preserves input");
    Require(BIO_free_nid_postfix(bio) == 1 && source[0] == 'a' && source[7] == 'e', "borrowed bytes survive free");
    for (const int size : {-1, -99}) {
        bio = BIO_new_mem_buf_nid_postfix("ab\0cd", size);
        RequireData(bio, "ab");
        Require(BIO_free_nid_postfix(bio) == 1, "free string BIO");
    }
    bio = BIO_new_mem_buf_nid_postfix("", 0);
    RequireData(bio, "");
    output[0] = 'x';
    Require(bio->method->gets(bio, output, 8) == 0 && output[0] == 0, "empty gets");
    Require(BIO_free_nid_postfix(bio) == 1, "free empty BIO");
}

void TestBufferOwnership() {
    Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(BIO_puts_nid_postfix(bio, "retained") == 8, "write retained buffer");
    auto* retained = Buffer(bio);
    Require(BIO_ctrl_nid_postfix(bio, 8, 0, nullptr) == 1, "default close flag");
    Require(BIO_ctrl_nid_postfix(bio, 9, 0, nullptr) == 1 && BIO_ctrl_nid_postfix(bio, 8, 0, nullptr) == 0, "disable close");
    Require(BIO_free_nid_postfix(bio) == 1, "free BIO without its buffer");
    Require(retained->length == 8 && std::memcmp(retained->data, "retained", 8) == 0, "detached storage remains valid");
    free_nid_postfix(retained->data);
    free_nid_postfix(retained);

    bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(BIO_puts_nid_postfix(bio, "old") == 3, "write old buffer");
    auto* replacement = static_cast<BioBuffer*>(calloc_nid_postfix(1, sizeof(BioBuffer)));
    replacement->data = static_cast<char*>(calloc_nid_postfix(8, 1));
    replacement->length = 3;
    replacement->capacity = 8;
    std::memcpy(replacement->data, "new", 3);
    Require(BIO_ctrl_nid_postfix(bio, 114, 0, replacement) == 1 && Buffer(bio) == replacement, "replace buffer");
    RequireData(bio, "new");
    Require(BIO_write_nid_postfix(bio, "!", 1) == 1, "write replacement buffer");
    RequireData(bio, "new!");
    Require(BIO_free_nid_postfix(bio) == 1, "free BIO retaining replacement");
    Require(replacement->length == 4 && std::memcmp(replacement->data, "new!", 4) == 0, "caller owns replacement");
    free_nid_postfix(replacement->data);
    free_nid_postfix(replacement);

    bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(BIO_ctrl_nid_postfix(bio, 114, 1, nullptr) == 1, "detach without replacement");
    Require(BIO_ctrl_nid_postfix(bio, 8, 0, nullptr) == 1, "close control without storage");
    Require(BIO_ctrl_nid_postfix(bio, 11, 0, nullptr) == 1, "flush without storage");
    void* missing = bio;
    Require(BIO_ctrl_nid_postfix(bio, 115, 0, &missing) == 1 && missing == nullptr, "query detached storage");
    Require(BIO_free_nid_postfix(bio) == 1, "free detached BIO");
}

void TestInterleavedTransfers() {
    Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    std::string pending;
    std::uint32_t sequence = 0x346de821;
    std::uint64_t written = 0;
    for (int step = 0; step < 2000; ++step) {
        sequence = sequence * 1664525 + 1013904223;
        const int count = static_cast<int>((sequence >> 16) % 65);
        if ((sequence & 3) != 0) {
            std::array<char, 65> input{};
            for (int i = 0; i < count; ++i) input[i] = static_cast<char>(sequence + i * 37);
            Require(BIO_write_nid_postfix(bio, input.data(), count) == count, "interleaved write count");
            pending.append(input.data(), count);
            written += count;
        } else {
            std::array<char, 66> output;
            output.fill(static_cast<char>(0xa5));
            const int expected = static_cast<int>(std::min<std::size_t>(count, pending.size()));
            const int result = bio->method->read(bio, output.data(), count);
            Require(result == (pending.empty() ? -1 : expected), "interleaved read count");
            if (expected != 0) Require(std::memcmp(output.data(), pending.data(), expected) == 0, "interleaved read bytes");
            Require(output[expected] == static_cast<char>(0xa5), "read preserves destination tail");
            pending.erase(0, expected);
        }
        RequireData(bio, pending);
        Require(bio->bytesWritten == written && bio->bytesRead == 0, "interleaved transfer counters");
        if (step % 127 == 0) {
            Require(BIO_ctrl_nid_postfix(bio, 1, 0, nullptr) == 1, "interleaved reset");
            pending.clear();
        }
    }
    Require(BIO_free_nid_postfix(bio) == 1, "free interleaved BIO");
}

struct WriteEvent {
    int operation;
    const char* source;
    int size;
    std::int64_t result;
    int flags;
    std::uint64_t bytesWritten;
};

struct WriteTrace {
    std::vector<WriteEvent> events;
    int veto = 0;
    int postAdjustment = 0;
};

std::int64_t APS5_VABI TraceWrite(Bio* bio, int operation, const char* source, int size, std::int64_t, std::int64_t result) {
    auto& trace = *reinterpret_cast<WriteTrace*>(bio->callbackArgument);
    trace.events.push_back({operation, source, size, result, bio->flags, bio->bytesWritten});
    if (operation == 3 && trace.veto) return trace.veto;
    return (operation & 0x80) ? result + trace.postAdjustment : result;
}

void TestWriteBoundaries() {
    const char source[] = "abc";
    for (const bool readOnly : {false, true}) {
        Bio* bio = readOnly ? BIO_new_mem_buf_nid_postfix(source, 3) : BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
        WriteTrace trace;
        bio->callback = TraceWrite;
        bio->callbackArgument = reinterpret_cast<char*>(&trace);
        for (const int size : {0, 1}) {
            bio->flags |= 0x4009;
            const int flags = bio->flags;
            trace.events.clear();
            Require(BIO_write_nid_postfix(bio, nullptr, size) == -1, "null source dispatch result");
            RequireError(0x20fff073);
            Require(trace.events.size() == 2, "null source has pre/post callbacks");
            Require(trace.events[0].operation == 3 && trace.events[1].operation == 0x83, "null source callback operations");
            Require(trace.events[0].source == nullptr && trace.events[1].source == nullptr &&
                trace.events[0].size == size && trace.events[1].size == size, "null source callback arguments");
            Require(trace.events[0].result == 1 && trace.events[1].result == -1, "null source callback results");
            Require(trace.events[0].flags == flags && trace.events[1].flags == flags && bio->flags == flags, "null source preserves retry flags");
            Require(trace.events[0].bytesWritten == 0 && trace.events[1].bytesWritten == 0 && bio->bytesWritten == 0, "failed write does not count bytes");
        }
        bio->flags |= 0x4009;
        const int flags = bio->flags;
        trace.events.clear();
        Require(BIO_write_nid_postfix(bio, source, 0) == (readOnly ? -1 : 0), "zero-length method result");
        RequireError(readOnly ? 0x20fff07e : 0);
        Require(trace.events.size() == 2 && trace.events[0].operation == 3 && trace.events[1].operation == 0x83, "zero-length write callback operations");
        Require(trace.events[0].source == source && trace.events[1].source == source &&
            trace.events[0].size == 0 && trace.events[1].size == 0, "zero-length callback arguments");
        Require(trace.events[0].result == 1 && trace.events[1].result == (readOnly ? -1 : 0), "zero-length callback results");
        Require(trace.events[0].flags == flags && trace.events[1].flags == (readOnly ? flags : flags & ~0xf), "retry flags change during the method");
        Require(bio->flags == (readOnly ? flags : flags & ~0xf) && bio->bytesWritten == 0, "zero-length retry flags and byte count");
        RequireData(bio, readOnly ? "abc" : "");
        trace.events.clear();
        trace.veto = -7;
        Require(BIO_write_nid_postfix(bio, nullptr, 0) == -7 && trace.events.size() == 1, "null write callback veto prevents method");
        RequireError(0);
        trace.veto = 0;
        trace.postAdjustment = 5;
        trace.events.clear();
        Require(BIO_write_nid_postfix(bio, nullptr, 0) == 4 && bio->bytesWritten == 0, "post callback can override failure without counting bytes");
        RequireError(0x20fff073);
        bio->callback = nullptr;
        Require(BIO_free_nid_postfix(bio) == 1, "free boundary BIO");
    }
    Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    WriteTrace trace;
    bio->callback = TraceWrite;
    bio->callbackArgument = reinterpret_cast<char*>(&trace);
    const auto* method = bio->method;
    bio->method = nullptr;
    Require(BIO_write_nid_postfix(bio, nullptr, 0) == -2 && trace.events.empty(), "method validation precedes callback");
    RequireError(0x20fff079);
    bio->method = method;
    bio->initialized = 0;
    Require(BIO_write_nid_postfix(bio, nullptr, 0) == -2 && trace.events.size() == 1 && trace.events[0].operation == 3, "initialization validation follows pre-callback");
    RequireError(0x20fff078);
    bio->initialized = 1;
    bio->callback = nullptr;
    Require(BIO_free_nid_postfix(bio) == 1, "free dispatch-order BIO");
}

struct CallbackState {
    std::vector<int> events;
    std::int64_t argument = 0;
    int veto = 0;
};

std::int64_t APS5_VABI Callback(Bio* bio, int operation, const char*, int, std::int64_t argument, std::int64_t result) {
    auto& state = *reinterpret_cast<CallbackState*>(bio->callbackArgument);
    state.events.push_back(operation);
    state.argument = argument;
    if (state.veto && !(operation & 0x80)) return state.veto;
    return (operation & 0x80) ? result + 2 : 1;
}

int APS5_VABI CreateCustom(Bio* bio) {
    bio->initialized = 1;
    return 1;
}

int APS5_VABI WriteCustom(Bio*, const char*, int count) {
    return count - 1;
}

std::int64_t APS5_VABI ControlCustom(Bio*, int command, std::int64_t argument, void*) {
    return argument + command;
}

int APS5_VABI RejectCreate(Bio*) {
    return 0;
}

void TestDispatchAndReferences() {
    Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    CallbackState state;
    bio->callback = Callback;
    bio->callbackArgument = reinterpret_cast<char*>(&state);
    Require(BIO_write_nid_postfix(bio, "abc", 3) == 5 && bio->bytesWritten == 3, "callback changes result after accounting");
    Require(state.events == std::vector<int>{3, 0x83}, "write callback ordering");
    state.events.clear();
    Require(BIO_puts_nid_postfix(bio, "d") == 3 && state.events == std::vector<int>{4, 0x84}, "puts callbacks");
    state.events.clear();
    Require(BIO_ctrl_nid_postfix(bio, 10, 0x1234567887654321LL, nullptr) == 6, "control callback result");
    Require(state.argument == 0x1234567887654321LL && state.events == std::vector<int>{6, 0x86}, "64-bit control argument");
    state.events.clear();
    state.veto = -7;
    Require(BIO_write_nid_postfix(bio, "x", 1) == -7 && state.events == std::vector<int>{3}, "write callback veto");
    Require(bio->bytesWritten == 4 && static_cast<BioBuffer*>(bio->data)->length == 4, "veto prevents transfer");
    state.events.clear();
    Require(BIO_free_nid_postfix(bio) == -7 && state.events == std::vector<int>{1} && bio->references == 0, "free callback veto");
    bio->callback = nullptr;
    Require(BIO_free_nid_postfix(bio) == 1, "free after veto");

    BioMethod custom{0x480, "test", WriteCustom, nullptr, nullptr, nullptr, ControlCustom, CreateCustom, nullptr, nullptr};
    bio = BIO_new_nid_postfix(&custom);
    Require(bio && bio->method == &custom, "title-defined method");
    Require(BIO_write_nid_postfix(bio, "abc", 3) == 2 && bio->bytesWritten == 2, "title-defined writer");
    Require(BIO_write_nid_postfix(bio, nullptr, 3) == 2 && bio->bytesWritten == 4, "custom method receives null source");
    Require(BIO_write_nid_postfix(bio, "", 0) == -1 && bio->bytesWritten == 4, "custom method receives zero length");
    RequireError(0);
    Require(BIO_ctrl_nid_postfix(bio, 17, 0x1234567800000000LL, nullptr) == 0x1234567800000011LL, "64-bit method result");
    constexpr int referenceCount = 8;
    std::atomic_ref(bio->references).store(referenceCount + 1);
    std::array<std::thread, referenceCount> threads;
    for (auto& thread : threads) thread = std::thread([bio] { Require(BIO_free_nid_postfix(bio) == 1, "release shared reference"); });
    for (auto& thread : threads) thread.join();
    Require(bio->references == 1 && BIO_free_nid_postfix(bio) == 1, "last reference release");
    custom.create = RejectCreate;
    Require(BIO_new_nid_postfix(&custom) == nullptr, "custom create can reject allocation");
}

void TestUnsupported() {
    Require(BIO_free_nid_postfix(nullptr) == 0, "free null");
    Require(BIO_ctrl_nid_postfix(nullptr, 1, 0, nullptr) == 0, "control null");
    Require(BIO_write_nid_postfix(nullptr, "a", 1) == 0, "write null BIO");
    Require(BIO_new_mem_buf_nid_postfix(nullptr, 0) == nullptr, "reject null borrowed input");
    RequireError(0x20fff073);
    Require(BIO_puts_nid_postfix(nullptr, "a") == -2, "reject puts without a method");
    RequireError(0x20fff079);
    Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    bio->initialized = 0;
    Require(BIO_write_nid_postfix(bio, "a", 1) == -2, "reject uninitialized write");
    RequireError(0x20fff078);
    bio->initialized = 1;
    bio->extraData = bio;
    RequireUnsupported([&] { BIO_free_nid_postfix(bio); });
    Require(bio->references == 1, "unsupported ex-data preserves reference");
    bio->extraData = nullptr;
    const BioMethod* method = bio->method;
    bio->method = nullptr;
    Require(BIO_ctrl_nid_postfix(bio, 1, 0, nullptr) == -2, "reject control without a method");
    RequireError(0x20fff079);
    Require(BIO_write_nid_postfix(bio, "a", 1) == -2, "reject write without a method");
    RequireError(0x20fff079);
    bio->method = method;
    auto* buffer = Buffer(bio);
    buffer->length = 0x5ffffffd;
    Require(BIO_write_nid_postfix(bio, "a", 1) == -1, "reject oversized growth");
    RequireError(0x07fff041);
    buffer->length = 0;
    Require(BIO_free_nid_postfix(bio) == 1, "free after unsupported cases");
    for (const int failAfter : {0, 1}) {
        allocationsUntilFailure = failAfter;
        Require(BIO_new_nid_postfix(BIO_s_mem_nid_postfix()) == nullptr, "failed BIO allocation");
        RequireError(failAfter == 0 ? 0x20fff041 : 0x07fff041);
        Require(allocations == 0, "failed creation leaked guest heap storage");
    }
    allocationsUntilFailure = -1;
    bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(BIO_puts_nid_postfix(bio, "abc") == 3, "write before growth failure");
    allocationsUntilFailure = 0;
    Require(BIO_puts_nid_postfix(bio, "0123456789") == -1, "failed buffer expansion");
    RequireError(0x07fff041);
    RequireData(bio, "abc");
    allocationsUntilFailure = -1;
    Require(BIO_free_nid_postfix(bio) == 1, "free after growth failure");

    bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    bio->initialized = 0;
    Require(BIO_puts_nid_postfix(bio, "a") == -2, "reject uninitialized puts");
    RequireError(0x20fff078);
    bio->initialized = 1;
    Require(bio->method->write(bio, nullptr, 1) == -1, "reject null raw write source");
    RequireError(0x20fff073);
    Require(BIO_free_nid_postfix(bio) == 1, "free after method errors");
}

#ifndef ANYPS5_BIO_DYNAMIC
void TestSslMethod() {
    const BioMethod* method = BIO_f_ssl_nid_postfix();
    Require(method && method->type == 0x207 && std::strcmp(method->name, "ssl") == 0, "SSL BIO method descriptor");
    Require(method->write && method->read && method->puts && method->control && method->create && method->destroy && method->callbackControl,
        "SSL BIO method has its callbacks");
    Bio* bio = BIO_new_nid_postfix(method);
    Require(bio && !bio->initialized && !bio->flags && bio->shutdown && bio->data, "SSL BIO creates zeroed private state");
    const auto* state = static_cast<const TestBioSslData*>(bio->data);
    Require(!state->ssl && !state->renegotiates && !state->renegotiateCount && !state->byteCount && !state->renegotiateTimeout && !state->lastTime,
        "SSL BIO private state starts zeroed");
    Require(BIO_ctrl_nid_postfix(bio, 1, 0, nullptr) == 0, "unattached reset returns zero");
    Require(BIO_ctrl_nid_postfix(bio, 3, 0, nullptr) == 0, "unattached info returns zero");
    Require(BIO_ctrl_nid_postfix(bio, 8, 0, nullptr) == 0, "unattached get-close returns zero");
    Require(BIO_ctrl_nid_postfix(bio, 9, 0, nullptr) == 0 && bio->shutdown == 1,
        "unattached set-close returns zero without changing shutdown");
    void* sslSentinel = bio;
    Require(BIO_ctrl_nid_postfix(bio, 110, 0, &sslSentinel) == 0 && sslSentinel == bio,
        "unattached get-SSL returns zero without writing its output");
    Require(BIO_ctrl_nid_postfix(bio, 119, 1, nullptr) == 0, "unattached SSL mode returns zero");
    Require(BIO_ctrl_nid_postfix(bio, 126, 0, nullptr) == 0, "unattached renegotiate count returns zero");
    bool nullSetThrew = false;
    try { BIO_ctrl_nid_postfix(bio, 109, 0, nullptr); }
    catch (const std::invalid_argument&) { nullSetThrew = true; }
    Require(nullSetThrew && bio->shutdown == 0 && !bio->initialized && !state->ssl,
        "setting a null SSL updates shutdown then faults before initialization");
    Require(method->read(nullptr, nullptr, 1) == 0 && method->write(nullptr, nullptr, 1) == 0, "null SSL BIO buffers return zero before BIO or SSL access");
    Require(BIO_free_nid_postfix(bio) == 1 && allocations == 0, "unattached SSL BIO frees its guest state");
    LibreSslPutError_nid_no_patch(2, 3, 17);
    allocationsUntilFailure = 1;
    Require(BIO_new_nid_postfix(method) == nullptr, "SSL BIO state allocation failure rejects creation");
    Require(ERR_get_error_nid_postfix() == 0x02003011, "SSL BIO allocation error preserves older FIFO entries");
    RequireError(0x14fff041);
    Require(allocations == 0, "failed SSL BIO creation frees its outer BIO");
    allocationsUntilFailure = -1;
}
void TestPkcs12BioReadErrors() {
    constexpr std::uint64_t bioUnsupported = 0x20fff079;
    constexpr std::uint64_t notEnoughData = 0x0dfff08e;
    constexpr std::uint64_t headerTooLong = 0x0dfff07b;
    void* sentinel = reinterpret_cast<void*>(std::uintptr_t{1});
    void* output = sentinel;

    ERR_clear_error_nid_postfix();
    Require(d2i_PKCS12_bio_nid_postfix(nullptr, &output) == nullptr && output == sentinel, "null BIO leaves PKCS12 output untouched");
    Require(ERR_get_error_nid_postfix() == bioUnsupported && ERR_get_error_nid_postfix() == notEnoughData
        && ERR_get_error_nid_postfix() == 0, "null BIO publishes its BIO and ASN.1 errors");

    Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(d2i_PKCS12_bio_nid_postfix(bio, &output) == nullptr && output == sentinel, "empty retrying BIO leaves PKCS12 output untouched");
    Require(ERR_get_error_nid_postfix() == notEnoughData && ERR_get_error_nid_postfix() == 0, "negative initial read publishes NOT_ENOUGH_DATA");
    Require(BIO_ctrl_nid_postfix(bio, 130, 0, nullptr) == 1, "set zero BIO EOF result");
    Require(d2i_PKCS12_bio_nid_postfix(bio, &output) == nullptr && output == sentinel, "zero EOF BIO leaves PKCS12 output untouched");
    Require(ERR_get_error_nid_postfix() == headerTooLong && ERR_get_error_nid_postfix() == 0, "zero initial read publishes HEADER_TOO_LONG");
    bio->initialized = 0;
    CallbackState state;
    state.veto = -7;
    bio->callback = Callback;
    bio->callbackArgument = reinterpret_cast<char*>(&state);
    Require(d2i_PKCS12_bio_nid_postfix(bio, &output) == nullptr && output == sentinel, "read callback veto preserves output");
    Require(state.events == std::vector<int>{2}, "read callback runs before initialization check");
    Require(ERR_get_error_nid_postfix() == notEnoughData && ERR_get_error_nid_postfix() == 0, "veto bypasses uninitialized BIO error");
    bio->callback = nullptr;
    Require(d2i_PKCS12_bio_nid_postfix(bio, nullptr) == nullptr, "initial read errors do not require an output pointer");
    Require(ERR_get_error_nid_postfix() == 0x20fff078 && ERR_get_error_nid_postfix() == notEnoughData
        && ERR_get_error_nid_postfix() == 0, "uninitialized BIO error precedes ASN.1 read error");
    bio->initialized = 1;
    Require(BIO_free_nid_postfix(bio) == 1, "free empty PKCS12 BIO");

}

int passwordCallbackCalls = 0;
std::array<int, 32> bioCallbackEvents{};
std::size_t bioCallbackEventCount = 0;

std::int64_t APS5_VABI ObserveBioCallback(Bio*, int operation, const char*, int, std::int64_t, std::int64_t result) {
    bioCallbackEvents[bioCallbackEventCount++] = operation;
    return result;
}

int APS5_VABI RejectBioGets(Bio*, char*, int) { return -1; }

int APS5_VABI ObservePasswordCallback(char*, int, int, void*) {
    ++passwordCallbackCalls;
    return 0;
}

void RequirePemQueue(std::initializer_list<std::uint64_t> expected) {
    for (const auto error : expected) Require(ERR_get_error_nid_postfix() == error, "PEM error order");
    Require(ERR_get_error_nid_postfix() == 0, "PEM path queues no additional errors");
}

void TestPemReadBioX509NoStartLine() {
    constexpr std::uint64_t pemNoStartLine = 0x09fff06c;
    constexpr std::uint64_t bufferMallocFailure = 0x07fff041;
    constexpr std::uint64_t pemMallocFailure = 0x09fff041;
    auto* output = reinterpret_cast<LibreSslX509*>(std::uintptr_t{0x12345678});
    passwordCallbackCalls = 0;

    ERR_clear_error_nid_postfix();
    Bio* bio = BIO_new_mem_buf_nid_postfix("", 0);
    Require(bio != nullptr, "empty PEM input BIO");
    bio->callback = &ObserveBioCallback;
    bioCallbackEventCount = 0;
    const int baseline = allocations;
    Require(PEM_read_bio_X509_nid_postfix(bio, &output, &ObservePasswordCallback, bio) == nullptr,
        "empty PEM input returns null");
    Require(output == reinterpret_cast<LibreSslX509*>(std::uintptr_t{0x12345678}), "empty PEM preserves output");
    Require(passwordCallbackCalls == 0, "empty PEM does not call password callback");
    Require(bioCallbackEventCount == 2 && bioCallbackEvents[0] == 5 && bioCallbackEvents[1] == 0x85,
        "empty PEM BIO callback order");
    RequirePemQueue({pemNoStartLine});
    Require(allocations == baseline, "empty PEM frees all three BUF_MEM objects");
    Require(BIO_free_nid_postfix(bio) == 1, "free empty PEM BIO");

    constexpr char preamble[] = "unrelated text\n";
    ERR_clear_error_nid_postfix();
    bio = BIO_new_mem_buf_nid_postfix(preamble, sizeof(preamble) - 1);
    Require(bio != nullptr, "PEM preamble BIO");
    bio->callback = &ObserveBioCallback;
    bioCallbackEventCount = 0;
    const int preambleBaseline = allocations;
    Require(PEM_read_bio_X509_nid_postfix(bio, &output, nullptr, nullptr) == nullptr,
        "EOF before a BEGIN line returns null");
    Require(bioCallbackEventCount == 4, "PEM reader scans preamble and EOF");
    RequirePemQueue({pemNoStartLine});
    Require(allocations == preambleBaseline, "EOF after preamble frees all three BUF_MEM objects");
    Require(BIO_free_nid_postfix(bio) == 1, "free PEM preamble BIO");

    constexpr char beginOnly[] = "-----BEGIN CERTIFICATE-----\n";
    for (int failAt = 0; failAt < 5; ++failAt) {
        ERR_clear_error_nid_postfix();
        bio = BIO_new_mem_buf_nid_postfix(failAt < 3 ? "" : beginOnly,
            failAt < 3 ? 0 : sizeof(beginOnly) - 1);
        Require(bio != nullptr, "PEM allocation failure BIO");
        const int allocationBaseline = allocations;
        allocationsUntilFailure = failAt;
        const auto result = PEM_read_bio_X509_nid_postfix(bio, &output, nullptr, nullptr);
        Require(result == nullptr, "BUF_MEM allocation or growth failure returns null");
        RequirePemQueue({bufferMallocFailure, pemMallocFailure});
        Require(allocations == allocationBaseline, "BUF_MEM allocation failure frees partial allocations");
        Require(BIO_free_nid_postfix(bio) == 1, "free PEM allocation failure BIO");
    }
    allocationsUntilFailure = -1;
}

void TestFileAndSocket() {
    const BioMethod* fileMethod = BIO_s_file_nid_postfix();
    Require(fileMethod && fileMethod->type == 0x402, "file BIO method descriptor");
    Require(BIO_new_fp_nid_postfix(nullptr, 0) == nullptr, "reject null file BIO");
    std::FILE* native = std::tmpfile();
    Require(native != nullptr, "open temporary file for BIO");
    FileStream stream(native);
    Bio* file = BIO_new_fp_nid_postfix(&stream, 0);
    Require(file && file->method == fileMethod && file->data == &stream, "file BIO guest stream");
    Require(file->initialized && BIO_ctrl_nid_postfix(file, 8, 0, nullptr) == 0, "file BIO no-close flag");
    Require(BIO_write_nid_postfix(file, "abc", 3) == 3, "write guest file through BIO");
    Require(BIO_ctrl_nid_postfix(file, 11, 0, nullptr) == 1, "flush guest file through BIO");
    Require(BIO_ctrl_nid_postfix(file, 1, 0, nullptr) == 0, "rewind guest file through BIO");
    char buffer[8]{};
    Require(file->method->read(file, buffer, 3) == 3 && std::memcmp(buffer, "abc", 3) == 0, "read guest file through BIO");
    Require(file->method->read(file, buffer, 1) == 0, "clean guest file EOF returns zero");
    Require(feof_nid_postfix(&stream) != 0 && ferror_nid_postfix(&stream) == 0, "clean EOF does not set stream error");
    Require(ERR_get_error_nid_postfix() == 0, "clean EOF does not queue LibreSSL errors");
    Require(BIO_ctrl_nid_postfix(file, 1, 0, nullptr) == 0, "file reset returns fseek success status");
    Require(file->method->read(file, buffer, 3) == 3 && std::memcmp(buffer, "abc", 3) == 0, "read after file BIO reset");
    FileStream* queried = nullptr;
    Require(BIO_ctrl_nid_postfix(file, 107, 0, &queried) == 1 && queried == &stream, "get guest file pointer");
    Require(BIO_free_nid_postfix(file) == 1, "free BIO without closing file");
    Require(fseek_nid_postfix(&stream, 0, SEEK_SET) == 0, "borrowed guest stream remains open");
    Require(fclose_nid_postfix(&stream) == 0, "close borrowed guest stream");

    std::FILE* ownedNative = std::tmpfile();
    Require(ownedNative != nullptr, "open owned temporary BIO file");
    FileStream owned(ownedNative);
    Bio* ownedBio = BIO_new_fp_nid_postfix(&owned, 1);
    Require(ownedBio && BIO_ctrl_nid_postfix(ownedBio, 8, 0, nullptr) == 1, "owned guest file BIO");
    Require(BIO_free_nid_postfix(ownedBio) == 1 && !owned.IsOpen(), "owned BIO closes guest file");

    int descriptors[2]{-1, -1};
#ifdef _WIN32
    Require(_pipe(descriptors, 4096, _O_BINARY) == 0, "create non-seekable guest file pipe");
    std::FILE* writeOnly = _fdopen(descriptors[1], "wb");
#else
    Require(::pipe(descriptors) == 0, "create non-seekable guest file pipe");
    std::FILE* writeOnly = ::fdopen(descriptors[1], "wb");
#endif
    Require(writeOnly != nullptr, "wrap pipe writer in host stream");
    FileStream pipeStream(writeOnly);
    Bio* pipeBio = BIO_new_fp_nid_postfix(&pipeStream, 0);
    Require(pipeBio != nullptr, "construct guest file BIO around pipe writer");
    Require(pipeBio->method->read(pipeBio, buffer, 1) == -1, "guest file read error must not report clean EOF");
    Require(ferror_nid_postfix(&pipeStream) != 0 && feof_nid_postfix(&pipeStream) == 0, "pipe read failure sets error, not EOF");
    const auto systemError = ERR_get_error_nid_postfix();
    const auto bioError = ERR_get_error_nid_postfix();
    Require((systemError >> 24) == 2 && (bioError >> 24) == 32 && (bioError & 0xfff) == 2,
            "failed file BIO read queues system and BIO errors");
    Require(ERR_get_error_nid_postfix() == 0, "file read queues exactly two errors");
    Require(BIO_ctrl_nid_postfix(pipeBio, 1, 0, nullptr) == -1, "file reset preserves non-seekable stream failure");
    Require(BIO_free_nid_postfix(pipeBio) == 1, "free borrowed pipe BIO");
    Require(fclose_nid_postfix(&pipeStream) == 0, "close borrowed pipe writer");
#ifdef _WIN32
    Require(_close(descriptors[0]) == 0, "close pipe reader");
#else
    Require(::close(descriptors[0]) == 0, "close pipe reader");
#endif

    Require(BIO_new_socket_nid_postfix(-1, 0) == nullptr, "reject negative guest socket");
    Bio* socket = BIO_new_socket_nid_postfix(42, 0);
    Require(socket && socket->initialized && socket->number == 42, "guest socket BIO descriptor");
    Require(socket->method && socket->method->type == 0x405, "socket BIO method descriptor");
    Require(BIO_ctrl_nid_postfix(socket, 8, 0, nullptr) == 0, "socket BIO no-close flag");
    Require(BIO_free_nid_postfix(socket) == 1, "free unowned guest socket BIO");
}

std::uint64_t PrivateKeyErrorCode(int library, int reason) {
    return (static_cast<std::uint64_t>(library) << 24) | (static_cast<std::uint64_t>(0xfff) << 12) |
           static_cast<std::uint64_t>(reason);
}

void RequirePrivateKeyErrors(const std::vector<std::uint64_t>& expected) {
    for (const auto code : expected) Require(ERR_get_error_nid_postfix() == code, "private-key file error order/code");
    Require(ERR_get_error_nid_postfix() == 0, "unexpected private-key file error");
}

std::string ReadPrivateKeyTestFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void TestPrivateKeyFilenameControl(const std::filesystem::path& path, const std::string& guestFilename,
                                   const std::string& guestMissingFilename) {
    constexpr int BioControlSetFilename = 108;
    constexpr int BioClose = 0x01;
    constexpr int BioFileRead = 0x02;
    constexpr int BioFileWrite = 0x04;
    constexpr int BioFileAppend = 0x08;
    const auto open = [&](int flags) {
        Bio* bio = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
        Require(bio != nullptr, "allocate file BIO for filename mode");
        Require(BIO_ctrl_nid_postfix(bio, BioControlSetFilename, flags | BioClose,
                                     const_cast<char*>(guestFilename.c_str())) == 1,
                "open file BIO in requested filename-control mode");
        return bio;
    };

    {
        Bio* bio = open(BioFileRead);
        char bytes[4]{};
        Require(bio->method->read(bio, bytes, 3) == 3 && std::memcmp(bytes, "abc", 3) == 0, "filename read mode uses r");
        Require(BIO_free_nid_postfix(bio) == 1, "close filename read mode");
    }
    {
        Bio* bio = open(BioFileRead | BioFileWrite);
        Require(BIO_write_nid_postfix(bio, "X", 1) == 1, "filename read/write mode uses r+");
        Require(BIO_free_nid_postfix(bio) == 1, "close filename read/write mode");
        Require(ReadPrivateKeyTestFile(path) == "Xbc", "read/write mode preserves the remainder");
    }
    {
        Bio* bio = open(BioFileWrite);
        Require(BIO_write_nid_postfix(bio, "W", 1) == 1, "filename write mode uses w");
        Require(BIO_free_nid_postfix(bio) == 1, "close filename write mode");
        Require(ReadPrivateKeyTestFile(path) == "W", "write mode truncates existing file");
    }
    {
        Bio* bio = open(BioFileAppend);
        Require(BIO_write_nid_postfix(bio, "A", 1) == 1, "filename append mode uses a");
        Require(BIO_free_nid_postfix(bio) == 1, "close filename append mode");
        Require(ReadPrivateKeyTestFile(path) == "WA", "append mode writes at end");
    }
    {
        Bio* bio = open(BioFileAppend | BioFileRead);
        Require(BIO_write_nid_postfix(bio, "B", 1) == 1, "filename append/read mode uses a+");
        Require(BIO_free_nid_postfix(bio) == 1, "close filename append/read mode");
        Require(ReadPrivateKeyTestFile(path) == "WAB", "append/read mode writes at end");
    }
    {
        Bio* bio = open(BioFileAppend | BioFileWrite);
        Require(BIO_write_nid_postfix(bio, "C", 1) == 1, "append takes precedence over write mode");
        Require(BIO_free_nid_postfix(bio) == 1, "close filename append/write mode");
        Require(ReadPrivateKeyTestFile(path) == "WABC", "append/write mode uses a");
    }
    {
        Bio* bio = open(BioFileAppend | BioFileRead | BioFileWrite);
        Require(BIO_write_nid_postfix(bio, "D", 1) == 1, "append takes precedence over read/write mode");
        Require(BIO_free_nid_postfix(bio) == 1, "close filename append/read/write mode");
        Require(ReadPrivateKeyTestFile(path) == "WABCD", "append/read/write mode uses a+");
    }

    {
        FileStream* oldStream = fopen_nid_postfix(guestFilename.c_str(), "rb");
        Require(oldStream != nullptr, "open owned old stream for replacement");
        Bio* bio = BIO_new_fp_nid_postfix(oldStream, 1);
        Require(bio != nullptr, "create owned file BIO for replacement");
        Require(BIO_ctrl_nid_postfix(bio, BioControlSetFilename, BioClose | BioFileRead,
                                     const_cast<char*>(guestFilename.c_str())) == 1,
                "replace owned old stream before opening its new filename");
        FileStream* current = nullptr;
        Require(BIO_ctrl_nid_postfix(bio, 107, 0, &current) == 1 && current != nullptr,
                "replacement leaves an initialized current stream");
        Require(BIO_free_nid_postfix(bio) == 1, "close replacement stream");
    }
    {
        FileStream* oldStream = fopen_nid_postfix(guestFilename.c_str(), "rb");
        Require(oldStream != nullptr, "open owned old stream for failed replacement");
        Bio* bio = BIO_new_fp_nid_postfix(oldStream, 1);
        Require(bio != nullptr, "create owned file BIO for failed replacement");
        ERR_clear_error_nid_postfix();
        Require(BIO_ctrl_nid_postfix(bio, BioControlSetFilename, BioClose | BioFileRead,
                                     const_cast<char*>(guestMissingFilename.c_str())) == 0,
                "failed replacement returns zero");
        const int openError = errno;
        Require(bio->data == nullptr && bio->initialized == 0 && bio->shutdown == 1,
                "failed replacement preserves source partial state after closing old stream");
        RequirePrivateKeyErrors({PrivateKeyErrorCode(2, openError), PrivateKeyErrorCode(32, 2)});
        Require(BIO_free_nid_postfix(bio) == 1, "free BIO after failed replacement");
    }
    {
        FileStream* oldStream = fopen_nid_postfix(guestFilename.c_str(), "rb");
        Require(oldStream != nullptr, "open owned old stream for bad-mode replacement");
        Bio* bio = BIO_new_fp_nid_postfix(oldStream, 1);
        Require(bio != nullptr, "create owned BIO for bad-mode replacement");
        ERR_clear_error_nid_postfix();
        Require(BIO_ctrl_nid_postfix(bio, BioControlSetFilename, BioClose,
                                     const_cast<char*>(guestFilename.c_str())) == 0,
                "owned replacement with no mode returns zero");
        Require(bio->data == nullptr && bio->initialized == 0 && bio->flags == 0 && bio->shutdown == 1,
                "owned bad-mode replacement closes and clears the old file first");
        RequirePrivateKeyErrors({PrivateKeyErrorCode(32, 101)});
        Require(BIO_free_nid_postfix(bio) == 1, "free owned BIO after bad-mode replacement");
    }
    {
        std::FILE* native = std::fopen(path.string().c_str(), "rb");
        Require(native != nullptr, "open borrowed old stream for bad-mode replacement");
        FileStream borrowed(native);
        Bio* bio = BIO_new_fp_nid_postfix(&borrowed, 0);
        Require(bio != nullptr, "create borrowed BIO for bad-mode replacement");
        ERR_clear_error_nid_postfix();
        Require(BIO_ctrl_nid_postfix(bio, BioControlSetFilename, 0,
                                     const_cast<char*>(guestFilename.c_str())) == 0,
                "borrowed replacement with no mode returns zero");
        Require(bio->data == &borrowed && bio->initialized == 1 && bio->shutdown == 0,
                "borrowed bad-mode replacement preserves old file state");
        RequirePrivateKeyErrors({PrivateKeyErrorCode(32, 101)});
        Require(BIO_free_nid_postfix(bio) == 1, "free borrowed BIO after bad-mode replacement");
        Require(borrowed.IsOpen() && fclose_nid_postfix(&borrowed) == 0, "borrowed old file remains caller-owned");
    }
}

void TestPrivateKeyFileErrorPaths() {
    constexpr int SslFileTypePem = 1;
    const auto path = std::filesystem::temp_directory_path() / "anyps5-libressl-private-key-test.pem";
    const auto missingPath = std::filesystem::temp_directory_path() / "anyps5-libressl-private-key-missing.pem";
    constexpr const char* AliasPrefix = "/anyps5-privatekey-test";
    const std::string guestFilename = std::string(AliasPrefix) + "/" + path.filename().string();
    const std::string guestMissingFilename = std::string(AliasPrefix) + "/" + missingPath.filename().string();
    AddPathAlias_nid_no_patch(AliasPrefix, path.parent_path().string().c_str());
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(missingPath, ignored);

    ERR_clear_error_nid_postfix();
    Require(SSL_CTX_use_PrivateKey_file_nid_postfix(nullptr, guestFilename.c_str(), SslFileTypePem) == 0,
            "missing private-key file returns failure without reading ctx");
    const int guestOpenError = errno;
    RequirePrivateKeyErrors({PrivateKeyErrorCode(2, guestOpenError), PrivateKeyErrorCode(32, 2),
                             PrivateKeyErrorCode(20, 2)});

    ERR_clear_error_nid_postfix();
    allocationsUntilFailure = 0;
    Require(SSL_CTX_use_PrivateKey_file_nid_postfix(nullptr, guestFilename.c_str(), SslFileTypePem) == 0,
            "BIO allocation failure returns failure without reading ctx");
    allocationsUntilFailure = -1;
    RequirePrivateKeyErrors({PrivateKeyErrorCode(32, 65), PrivateKeyErrorCode(20, 7)});
    Require(allocations == 0, "failed BIO allocation leaked storage");

    {
        std::ofstream output(path, std::ios::binary);
        Require(output.is_open(), "create empty invalid-type private-key file");
    }
    ERR_clear_error_nid_postfix();
    Require(SSL_CTX_use_PrivateKey_file_nid_postfix(nullptr, guestFilename.c_str(), 0) == 0,
            "invalid private-key file type returns failure without reading ctx");
    RequirePrivateKeyErrors({PrivateKeyErrorCode(20, 124)});
    Require(std::filesystem::remove(path), "invalid-type BIO closed private-key file");

    {
        std::ofstream output(path, std::ios::binary);
        Require(output.is_open(), "create initial file for BIO filename modes");
        output << "abc";
    }
    TestPrivateKeyFilenameControl(path, guestFilename, guestMissingFilename);
    Require(std::filesystem::remove(path), "remove filename-control test file");

    Bio* bio = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    Require(bio != nullptr, "allocate BIO for bad filename-control mode");
    ERR_clear_error_nid_postfix();
    Require(BIO_ctrl_nid_postfix(bio, 108, 0x01, const_cast<char*>(guestFilename.c_str())) == 0,
            "filename control rejects missing read/write/append mode");
    RequirePrivateKeyErrors({PrivateKeyErrorCode(32, 101)});
    Require(BIO_free_nid_postfix(bio) == 1, "free BIO after bad filename-control mode");
    RemovePathAlias_nid_no_patch(AliasPrefix);
}

std::uint64_t ChainFileErrorCode(int library, int function, int reason) {
    return (static_cast<std::uint64_t>(library) << 24) | (static_cast<std::uint64_t>(function) << 12) |
           static_cast<std::uint64_t>(reason);
}

void RequireChainFileErrorSequence(const std::vector<std::uint64_t>& expected) {
    for (const auto code : expected) Require(ERR_get_error_nid_postfix() == code, "certificate-chain error FIFO/code");
    Require(ERR_get_error_nid_postfix() == 0, "unexpected certificate-chain error");
}

void AddChainFilePriorErrors() {
    LibreSslPutError_nid_no_patch(8, 5, 101);
    LibreSslPutError_nid_no_patch(9, 7, 201);
}

void TestCertificateChainFileErrorPaths() {
    const auto path = std::filesystem::temp_directory_path() / "anyps5-libressl-certificate-chain-test.pem";
    const auto missingPath = std::filesystem::temp_directory_path() / "anyps5-libressl-certificate-chain-missing.pem";
    constexpr const char* AliasPrefix = "/anyps5-chainfile-test";
    const std::string guestFilename = std::string(AliasPrefix) + "/" + path.filename().string();
    const std::string guestMissingFilename = std::string(AliasPrefix) + "/" + missingPath.filename().string();
    AddPathAlias_nid_no_patch(AliasPrefix, path.parent_path().string().c_str());
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(missingPath, ignored);

    ERR_clear_error_nid_postfix();
    AddChainFilePriorErrors();
    Require(SSL_CTX_use_certificate_chain_file_nid_postfix(nullptr, guestMissingFilename.c_str()) == 0,
            "missing certificate-chain file returns failure before reading ctx");
    const int guestOpenError = errno;
    RequireChainFileErrorSequence({ChainFileErrorCode(8, 5, 101), ChainFileErrorCode(9, 7, 201),
                                   ChainFileErrorCode(2, 0xfff, guestOpenError), ChainFileErrorCode(32, 0xfff, 2),
                                   ChainFileErrorCode(20, 0xfff, 2)});
    Require(allocations == 0, "missing certificate-chain file leaked BIO storage");

    ERR_clear_error_nid_postfix();
    AddChainFilePriorErrors();
    allocationsUntilFailure = 0;
    Require(SSL_CTX_use_certificate_chain_file_nid_postfix(nullptr, guestFilename.c_str()) == 0,
            "BIO allocation failure returns before reading ctx");
    allocationsUntilFailure = -1;
    RequireChainFileErrorSequence({ChainFileErrorCode(8, 5, 101), ChainFileErrorCode(9, 7, 201),
                                   ChainFileErrorCode(32, 0xfff, 65), ChainFileErrorCode(20, 0xfff, 7)});
    Require(allocations == 0, "failed certificate-chain BIO allocation leaked storage");

    {
        std::ofstream output(path, std::ios::binary);
        Require(output.is_open(), "create certificate-chain decoder input");
        output << "-----BEGIN CERTIFICATE-----\nAA==\n-----END CERTIFICATE-----\n";
    }
    LibreSslCtx context{};
    ERR_clear_error_nid_postfix();
    AddChainFilePriorErrors();
    bool threw = false;
    try {
        static_cast<void>(SSL_CTX_use_certificate_chain_file_nid_postfix(&context, guestFilename.c_str()));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "unsupported certificate-chain decoder path must not report fake success");
    Require(ERR_get_error_nid_postfix() == 0, "chain helper clears prior errors after opening the BIO");
    Require(std::filesystem::remove(path), "certificate-chain BIO path released its input file");
    Require(allocations == 0, "unsupported certificate-chain decode leaked BIO storage");
    RemovePathAlias_nid_no_patch(AliasPrefix);
}

void TestPemReadX509FileWrapper() {
    auto* const sentinel = reinterpret_cast<LibreSslX509*>(std::uintptr_t{1});
    LibreSslX509* output = sentinel;

    ERR_clear_error_nid_postfix();
    Require(PEM_read_X509_nid_postfix(nullptr, &output, nullptr, nullptr) == nullptr,
            "null FILE returns null");
    RequireError(0x09fff007);
    Require(allocations == 0 && output == sentinel, "null FILE preserves the output");

    std::FILE* native = std::tmpfile();
    Require(native != nullptr, "open temporary PEM file");
    FileStream stream(native);
    allocationsUntilFailure = 0;
    Require(PEM_read_X509_nid_postfix(&stream, &output, nullptr, nullptr) == nullptr,
            "BIO allocation failure returns null");
    allocationsUntilFailure = -1;
    Require(ERR_get_error_nid_postfix() == 0x20fff041, "BIO allocation error precedes the PEM wrapper error");
    RequireError(0x09fff007);
    Require(allocations == 0 && output == sentinel, "BIO allocation failure has no leak or output write");

    Require(PEM_read_X509_nid_postfix(&stream, &output, nullptr, nullptr) == nullptr,
            "empty PEM file returns null through the reader");
    Require(allocations == 0 && stream.IsOpen() && output == sentinel,
            "empty PEM file frees only the borrowed BIO and keeps the stream open");
    RequireError(0x09fff06c);

    constexpr char record[] = "-----BEGIN CERTIFICATE-----\nAA==\n-----END CERTIFICATE-----\n";
    Require(fwrite_nid_postfix(record, 1, sizeof(record) - 1, &stream) == sizeof(record) - 1,
            "write a certificate record to the borrowed PEM file");
    Require(fflush_nid_postfix(&stream) == 0, "flush the certificate record before reading it back");
    Require(fseek_nid_postfix(&stream, 0, SEEK_SET) == 0, "rewind the certificate record file");
    bool conversionThrew = false;
    try {
        static_cast<void>(PEM_read_X509_nid_postfix(&stream, &output, nullptr, nullptr));
    } catch (const std::runtime_error&) {
        conversionThrew = true;
    }
    Require(conversionThrew, "a certificate record reaches the unsupported conversion and propagates");
    Require(allocations == 0 && stream.IsOpen() && output == sentinel,
            "reader exception frees only the borrowed BIO and preserves stream and output");
    Require(ERR_get_error_nid_postfix() == 0, "the unsupported conversion queues no OpenSSL errors");
    Require(fclose_nid_postfix(&stream) == 0, "close borrowed PEM file after the reader exception");
}

std::uint64_t ErrorCode(int library, int reason) {
    return (static_cast<std::uint64_t>(library) << 24) | (static_cast<std::uint64_t>(0xfff) << 12) |
           static_cast<std::uint64_t>(reason);
}

void RequireErrorSequence(const std::vector<std::uint64_t>& expected) {
    for (const auto code : expected) {
        const auto actual = ERR_get_error_nid_postfix();
        if (actual != code) std::fprintf(stderr, "certificate error: actual=%llx expected=%llx\n", static_cast<unsigned long long>(actual), static_cast<unsigned long long>(code));
        Require(actual == code, "certificate file error order/code");
    }
    Require(ERR_get_error_nid_postfix() == 0, "unexpected certificate file error");
}

void TestCertificateFileLoadPaths() {
    constexpr int BioControlSetFilename = 108;
    constexpr int BioClose = 0x01;
    constexpr int SslFileTypePem = 1;
    const auto path = std::filesystem::temp_directory_path() / "anyps5-libressl-certificate-test.pem";
    const auto directory = path.parent_path().string();
    AddPathAlias_nid_no_patch("/anyps5-cert-test", directory.c_str());
    const std::string filename = "/anyps5-cert-test/" + path.filename().string();
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    ERR_clear_error_nid_postfix();
    Require(SSL_CTX_use_certificate_file_nid_postfix(nullptr, filename.c_str(), SslFileTypePem) == 0,
            "missing certificate file returns failure without reading ctx");
    const int guestOpenError = errno;
    RequireErrorSequence({ErrorCode(2, guestOpenError), ErrorCode(32, 2), ErrorCode(20, 2)});

    ERR_clear_error_nid_postfix();
    allocationsUntilFailure = 0;
    Require(SSL_CTX_use_certificate_file_nid_postfix(nullptr, filename.c_str(), SslFileTypePem) == 0,
            "BIO allocation failure returns failure without reading ctx");
    allocationsUntilFailure = -1;
    RequireErrorSequence({ErrorCode(32, 65), ErrorCode(20, 7)});
    Require(allocations == 0, "failed BIO allocation leaked storage");

    {
        std::ofstream output(path, std::ios::binary);
        Require(output.is_open(), "create empty invalid-type certificate file");
    }
    ERR_clear_error_nid_postfix();
    Require(SSL_CTX_use_certificate_file_nid_postfix(nullptr, filename.c_str(), 0) == 0,
            "invalid file type returns failure without reading ctx");
    RequireErrorSequence({ErrorCode(20, 124)});
    Require(std::filesystem::remove(path), "invalid-type BIO closed its file");

    for (const int flags : {2, 4, 6, 8, 10, 12, 14}) {
        { std::ofstream output(path, std::ios::binary); output << "seed"; }
        Bio* modeBio = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
        Require(modeBio != nullptr, "allocate filename mode BIO");
        Require(BIO_ctrl_nid_postfix(modeBio, 108, flags | 1, const_cast<char*>(filename.c_str())) == 1,
                "filename mode opens the stream");
        Require(modeBio->initialized == 1 && modeBio->shutdown == 1 && modeBio->data, "filename mode state");
        if (flags != 2) Require(BIO_write_nid_postfix(modeBio, "X", 1) == 1, "filename mode write");
        Require(BIO_free_nid_postfix(modeBio) == 1, "close filename mode BIO");
        std::ifstream input(path, std::ios::binary);
        std::string result((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        Require(result == (flags & 8 ? "seedX" : flags == 4 ? "X" : flags == 6 ? "Xeed" : "seed"), "filename mode contents");
    }
    Bio* replacement = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    Require(replacement && BIO_ctrl_nid_postfix(replacement, 108, 3, const_cast<char*>(filename.c_str())) == 1,
            "open owned stream before replacement");
    replacement->flags = 0x88;
    Require(BIO_ctrl_nid_postfix(replacement, 108, 1, nullptr) == 0, "invalid replacement mode fails before filename");
    Require(replacement->data == nullptr && replacement->initialized == 0 && replacement->flags == 0 && replacement->shutdown == 1,
            "owned replacement clears old stream state before mode failure");
    RequireErrorSequence({ErrorCode(32, 101)});
    Require(BIO_free_nid_postfix(replacement) == 1, "free failed replacement BIO");

    auto* borrowed = fopen_nid_postfix(filename.c_str(), "r");
    Require(borrowed != nullptr, "open borrowed stream");
    Bio* unowned = BIO_new_fp_nid_postfix(borrowed, 0);
    Require(unowned != nullptr, "allocate borrowed stream BIO");
    unowned->flags = 0x88;
    Require(BIO_ctrl_nid_postfix(unowned, 108, 0, nullptr) == 0, "unowned invalid replacement mode");
    Require(unowned->data == borrowed && unowned->initialized == 1 && unowned->flags == 0x88 && unowned->shutdown == 0,
            "unowned stream state survives failed replacement");
    RequireErrorSequence({ErrorCode(32, 101)});
    Require(BIO_free_nid_postfix(unowned) == 1 && fclose_nid_postfix(borrowed) == 0, "borrowed stream remains caller-owned");
    Require(std::filesystem::remove(path), "remove filename mode fixture");

    Bio* fileBio = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    Require(fileBio != nullptr, "allocate file BIO for filename-control mode check");
    Require(BIO_ctrl_nid_postfix(fileBio, BioControlSetFilename, BioClose, const_cast<char*>(filename.c_str())) == 0,
            "filename control rejects missing read/write/append mode");
    RequireErrorSequence({ErrorCode(32, 101)});
    Require(BIO_free_nid_postfix(fileBio) == 1, "free file BIO after bad filename mode");
    RemovePathAlias_nid_no_patch("/anyps5-cert-test");
}
#endif

#ifndef ANYPS5_BIO_DYNAMIC
int APS5_VABI ShortWrite(LibreSsl::Bio*, const char*, int size) {
    return size > 0 ? 1 : 0;
}

int APS5_VABI InitShort(LibreSsl::Bio* bio) {
    bio->initialized = 1;
    return 1;
}

void TestPrintfShortWrite() {
    LibreSsl::BioMethod method{};
    method.name = "short";
    method.write = ShortWrite;
    method.create = InitShort;
    Bio* bio = BIO_new_nid_postfix(&method);
    Require(bio != nullptr, "new short-write BIO");
    Require(BIO_printf_nid_postfix(bio, "%s", "abc") == 1, "BIO_printf returns what BIO_write returns on a short write");
    Require(bio->bytesWritten == 1, "a short write counts the bytes it wrote");
    Require(BIO_free_nid_postfix(bio) == 1, "free short-write BIO");
}

void TestPrintf() {
    Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio != nullptr, "new memory BIO for BIO_printf");
    Require(BIO_printf_nid_postfix(bio, "%s-%d", "ab", 42) == 5, "BIO_printf returns the formatted length");
    RequireData(bio, "ab-42");
    Require(BIO_printf_nid_postfix(bio, "%0600d", 7) == 600, "BIO_printf output longer than the stack buffer");
    RequireData(bio, "ab-42" + std::string(599, '0') + "7");
    Require(BIO_printf_nid_postfix(bio, "") == 0, "BIO_printf empty format writes nothing");
    Require(BIO_free_nid_postfix(bio) == 1, "free BIO_printf BIO");

    Bio* readOnly = BIO_new_mem_buf_nid_postfix("x", 1);
    Require(readOnly != nullptr, "read-only memory BIO for BIO_printf");
    Require(BIO_printf_nid_postfix(readOnly, "y") == -1, "BIO_printf on a read-only BIO fails");
    RequireError(0x20fff07e);
    Require(BIO_free_nid_postfix(readOnly) == 1, "free read-only BIO_printf BIO");
}
#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef ANYPS5_BIO_DYNAMIC
    Require(argc == 2, "patched BIO test needs the libraries directory");
    InitializePatched(std::filesystem::absolute(argv[1]));
#else
    static_cast<void>(argc);
    static_cast<void>(argv);
#endif
    InitializeHeap();
    ERR_clear_error_nid_postfix();
    TestWritable();
    TestReadOnlyAndLines();
    TestBufferOwnership();
    TestInterleavedTransfers();
    TestWriteBoundaries();
    TestDispatchAndReferences();
    TestUnsupported();
#ifndef ANYPS5_BIO_DYNAMIC
    TestSslMethod();
    TestPkcs12BioReadErrors();
    TestPemReadBioX509NoStartLine();
    TestFileAndSocket();
    TestPrivateKeyFileErrorPaths();
    TestCertificateChainFileErrorPaths();
    TestPemReadX509FileWrapper();
    TestCertificateFileLoadPaths();
    TestPrintf();
    TestPrintfShortWrite();
#endif
    Require(allocations == 0, "BIO tests leaked guest heap storage");
    Require(ERR_get_error_nid_postfix() == 0, "BIO tests left an unexpected error");
}
