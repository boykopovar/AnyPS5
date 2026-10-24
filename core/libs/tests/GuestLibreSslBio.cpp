#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

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

extern "C" {
const BioMethod* APS5_VABI BIO_s_mem_nid_postfix();
Bio* APS5_VABI BIO_new_nid_postfix(const BioMethod*);
Bio* APS5_VABI BIO_new_mem_buf_nid_postfix(const void*, int);
int APS5_VABI BIO_free_nid_postfix(Bio*);
std::int64_t APS5_VABI BIO_ctrl_nid_postfix(Bio*, int, std::int64_t, void*);
int APS5_VABI BIO_write_nid_postfix(Bio*, const void*, int);
int APS5_VABI BIO_puts_nid_postfix(Bio*, const char*);
void* APS5_VABI calloc_nid_postfix(std::size_t, std::size_t);
void APS5_VABI free_nid_postfix(void*);
}

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
    if (allocationsUntilFailure == 0) return nullptr;
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
    Require(BIO_write_nid_postfix(bio, nullptr, 7) == 0 && BIO_write_nid_postfix(bio, "x", 0) == 0, "empty write guard");
    RequireError(0);
    RequireData(bio, "");
    auto* buffer = Buffer(bio);
    Require(buffer->capacity == 0 && buffer->data == nullptr, "new buffer storage");
    char output[16]{};
    Require(method->read(bio, output, sizeof(output)) == -1 && (bio->flags & 0xf) == 9, "empty writable BIO retry");
    Require(BIO_write_nid_postfix(bio, "abc", 3) == 3 && (bio->flags & 0xf) == 0, "write clears retry flags");
    RequireData(bio, "abc");
    Require(buffer->capacity == 8 && buffer->data[3] == 0, "first buffer expansion");
    Require(BIO_write_nid_postfix(bio, "0123456789", 10) == 10, "second write");
    Require(buffer->capacity == 20 && buffer->data[13] == 0, "second buffer expansion");
    RequireData(bio, "abc0123456789");
    Require(method->read(bio, output, 4) == 4 && std::memcmp(output, "abc0", 4) == 0, "partial memory read");
    RequireData(bio, "123456789");
    Require(BIO_puts_nid_postfix(bio, "xy") == 2 && bio->bytesWritten == 15, "puts and byte accounting");
    RequireData(bio, "123456789xy");
    Require(method->read(bio, nullptr, 3) == 3, "null raw read reports available count");
    RequireData(bio, "123456789xy");
    Require(method->read(bio, output, -3) == -3, "negative raw read");
    Require(BIO_write_nid_postfix(bio, nullptr, 5) == 0 && BIO_write_nid_postfix(bio, "z", -3) == 0, "ignored public writes");
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
    TestDispatchAndReferences();
    TestUnsupported();
    Require(allocations == 0, "BIO tests leaked guest heap storage");
    Require(ERR_get_error_nid_postfix() == 0, "BIO tests left an unexpected error");
}
