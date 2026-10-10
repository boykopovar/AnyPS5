#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <string>
#include <string_view>

extern "C" {
int APS5_VABI posix_memalign_nid_postfix(void**, std::size_t, std::size_t);
void* APS5_VABI _Znwm_nid_postfix(std::size_t);
void* APS5_VABI _ZnwmRKSt9nothrow_t_nid_postfix(std::size_t, const void*) noexcept;
void* APS5_VABI _ZnamRKSt9nothrow_t_nid_postfix(std::size_t, const void*) noexcept;
void APS5_VABI _ZdlPv_nid_postfix(void*);
void APS5_VABI _ZdaPv_nid_postfix(void*);
void APS5_VABI _ZdlPvSt11align_val_t_nid_postfix(void*, std::size_t);
void APS5_VABI _ZdlPvmSt11align_val_t_nid_postfix(void*, std::size_t, std::size_t);
void* ApplicationHeapRealign_nid_no_patch(void*, std::size_t, std::size_t);
char* APS5_VABI strdup_nid_postfix(const char*);
char* APS5_VABI strndup_nid_postfix(const char*, std::size_t);
char* APS5_VABI getcwd_nid_postfix(char*, std::size_t);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI atexit_nid_postfix(void (APS5_VABI*)());
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int enomem = 12;
constexpr int einval = 22;

alignas(64) std::array<std::byte, 256> storage{};
std::size_t lastSize = 0;
std::size_t lastAlignment = 0;
unsigned posixCalls = 0;
unsigned initializes = 0;
unsigned frees = 0;
unsigned foreignPointers = 0;
bool fail = false;
bool recurse = false;
bool nullPosixResult = false;

[[noreturn]] void FailAtExit(const char* message) {
    std::fprintf(stderr, "application heap exit check failed: %s\n", message);
    std::fflush(stderr);
    std::_Exit(1);
}

void APS5_VABI initialize() { ++initializes; }

void APS5_VABI finalize() {
    if (initializes != 1) FailAtExit("finalize ran without exactly one initialize");
}

void APS5_VABI exitCallbackAllocates() {
    void* pointer = nullptr;
    try {
        pointer = ApplicationHeapAllocate_nid_no_patch(16);
    } catch (const std::exception&) {
        FailAtExit("a guest exit handler could not allocate from the replacement heap");
    }
    if (pointer != storage.data()) FailAtExit("a guest exit handler allocated outside the replacement heap");
}

void* APS5_VABI allocate(std::size_t bytes) {
    lastSize = bytes;
    if (recurse) return ApplicationHeapAllocate_nid_no_patch(bytes);
    return fail ? nullptr : storage.data();
}

void APS5_VABI release(void* pointer) {
    if (pointer != storage.data()) ++foreignPointers;
    ++frees;
}

void* APS5_VABI reallocate(void* pointer, std::size_t bytes) {
    if (pointer != storage.data()) ++foreignPointers;
    return allocate(bytes);
}

void* APS5_VABI allocateZeroed(std::size_t count, std::size_t bytes) {
    return allocate(count * bytes);
}

void* APS5_VABI align(std::size_t alignment, std::size_t bytes) {
    lastAlignment = alignment;
    return allocate(bytes);
}

void* APS5_VABI realign(void* pointer, std::size_t bytes, std::size_t alignment) {
    if (pointer != storage.data()) ++foreignPointers;
    lastAlignment = alignment;
    return allocate(bytes);
}

int APS5_VABI posixAlign(void** pointer, std::size_t alignment, std::size_t bytes) {
    ++posixCalls;
    if (fail) {
        *__error_nid_postfix() = enomem;
        *pointer = nullptr;
        return enomem;
    }
    if (nullPosixResult) {
        *pointer = nullptr;
        return 0;
    }
    *pointer = align(alignment, bytes);
    return 0;
}

template<typename TValue, std::size_t TSize>
void Write(std::array<std::byte, TSize>& data, std::size_t offset, TValue value) {
    Require(offset <= data.size() && sizeof(value) <= data.size() - offset, "metadata write stays inside its table");
    std::memcpy(data.data() + offset, &value, sizeof(value));
}

struct HeapMetadata {
    std::array<std::byte, 0x40> process{};
    std::array<std::byte, 0x38> libc{};
    std::array<std::byte, 0x78> replacement{};

    HeapMetadata() {
        Write(process, 0, std::uint64_t{0x40});
        Write(process, 8, std::uint32_t{0x4942524f});
        Write(process, 0x38, libc.data());
        Write(libc, 0, std::uint64_t{0x38});
        Write(libc, 0x30, replacement.data());
        Write(replacement, 0, std::uint64_t{0x78});
        Write(replacement, 8, std::uint64_t{2});
        Write(replacement, 0x10, &initialize);
        Write(replacement, 0x18, &finalize);
        Write(replacement, 0x20, &allocate);
        Write(replacement, 0x28, &release);
        Write(replacement, 0x30, &allocateZeroed);
        Write(replacement, 0x38, &reallocate);
        Write(replacement, 0x40, &align);
        Write(replacement, 0x48, &realign);
        Write(replacement, 0x50, &posixAlign);
    }
};

HeapMetadata& Metadata() {
    static HeapMetadata metadata;
    return metadata;
}

template<typename TOperation>
void RequireRejected(const TOperation& operation, std::string_view message) {
    try {
        operation();
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::exception&) {
        return;
    }
    Testing::Fail(std::string(message) + ": did not throw");
}

void RequireMode(std::string_view mode) {
    const auto& arguments = Testing::Arguments();
    const std::string_view selected = arguments.empty() ? std::string_view{} : std::string_view(arguments.front());
    if (selected != mode) {
        Testing::Skip("needs its own process, selected by the argument '" + std::string(mode) + "'");
    }
}

void RequireUninitializedHeapRejects() {
    RequireRejected([] { ApplicationHeapAllocate_nid_no_patch(64); }, "allocation before initialization");
    RequireRejected([] { ApplicationHeapRegister_nid_no_patch(nullptr); }, "registering a null table");
}

void InitializeReplacementHeapOnce() {
    static bool initialized = false;
    if (initialized) return;
    RequireUninitializedHeapRejects();
    ApplicationHeapInitialize_nid_no_patch(Metadata().process.data());
    ApplicationHeapInitialize_nid_no_patch(Metadata().process.data());
    initialized = true;
}

class ReplacementHeap {
public:
    ReplacementHeap() {
        RequireMode("");
        InitializeReplacementHeapOnce();
        Reset();
    }

    ~ReplacementHeap() {
        Reset();
    }

    ReplacementHeap(const ReplacementHeap&) = delete;
    ReplacementHeap& operator=(const ReplacementHeap&) = delete;

private:
    static void Reset() {
        fail = false;
        recurse = false;
        nullPosixResult = false;
        lastSize = 0;
        lastAlignment = 0;
        foreignPointers = 0;
        *__error_nid_postfix() = 0;
    }
};

void RequireOnlyHeapPointers() {
    RequireEqual(foreignPointers, 0u, "callbacks received pointers outside the replacement heap");
}

const Case initializeTwice{"ApplicationHeap_InitializedTwice_RunsInitializeCallbackOnce", [] {
    const ReplacementHeap heap;
    RequireEqual(initializes, 1u, "initialize callback runs");
}};

const Case basicCallbacks{"ApplicationHeap_BasicOperations_ForwardToReplacementCallbacks", [] {
    const ReplacementHeap heap;
    const unsigned freesBefore = frees;
    void* pointer = ApplicationHeapAlign_nid_no_patch(4, 64);
    Require(pointer == storage.data(), "aligned allocation comes from the replacement heap");
    RequireEqual(lastAlignment, std::size_t{4}, "aligned allocation alignment");
    RequireEqual(lastSize, std::size_t{64}, "aligned allocation size");
    release(pointer);
    RequireEqual(frees, freesBefore + 1, "direct release callback");
    pointer = ApplicationHeapAllocate_nid_no_patch(32);
    Require(pointer == storage.data(), "allocation comes from the replacement heap");
    RequireEqual(lastSize, std::size_t{32}, "allocation size");
    Require(ApplicationHeapReallocate_nid_no_patch(pointer, 96) == storage.data(), "reallocation result");
    RequireEqual(lastSize, std::size_t{96}, "reallocation size");
    ApplicationHeapFree_nid_no_patch(pointer);
    RequireEqual(frees, freesBefore + 2, "free reaches the release callback");
    Require(ApplicationHeapCalloc_nid_no_patch(3, 16) == storage.data(), "calloc result");
    RequireEqual(lastSize, std::size_t{48}, "calloc size");
    RequireEqual(ApplicationHeapPosixAlign_nid_no_patch(&pointer, 64, 128), 0, "posix align status");
    Require(pointer == storage.data(), "posix align result");
    RequireEqual(lastAlignment, std::size_t{64}, "posix align alignment");
    RequireOnlyHeapPointers();
}};

const Case invalidRequests{"ApplicationHeap_InvalidAlignmentOrOverflowingCalloc_Throws", [] {
    const ReplacementHeap heap;
    RequireRejected([] { ApplicationHeapAlign_nid_no_patch(3, 64); }, "alignment 3");
    RequireRejected([] { ApplicationHeapCalloc_nid_no_patch(2, std::numeric_limits<std::size_t>::max()); },
        "calloc overflow");
}};

const Case operatorNew{"OperatorNewDelete_ReplacementHeap_ForwardsSizesAndFrees", [] {
    const ReplacementHeap heap;
    const unsigned freesBefore = frees;
    Require(_Znwm_nid_postfix(0) == storage.data(), "new of zero bytes");
    RequireEqual(lastSize, std::size_t{1}, "new of zero bytes allocates one byte");
    _ZdlPv_nid_postfix(storage.data());
    _ZdlPv_nid_postfix(nullptr);
    RequireEqual(frees, freesBefore + 1, "delete frees once and ignores null");
    Require(_ZnamRKSt9nothrow_t_nid_postfix(24, nullptr) == storage.data(), "nothrow new[]");
    RequireEqual(lastSize, std::size_t{24}, "nothrow new[] size");
    _ZdaPv_nid_postfix(storage.data());
    RequireEqual(frees, freesBefore + 2, "delete[] frees");
    RequireOnlyHeapPointers();
}};

const Case realignment{"ApplicationHeapRealign_ValidAndTooLargeAlignment_ForwardsOrThrows", [] {
    const ReplacementHeap heap;
    Require(ApplicationHeapRealign_nid_no_patch(storage.data(), 48, 32) == storage.data(), "realign result");
    RequireEqual(lastSize, std::size_t{48}, "realign size");
    RequireEqual(lastAlignment, std::size_t{32}, "realign alignment");
    RequireRejected([] { ApplicationHeapRealign_nid_no_patch(storage.data(), 16, 4096); }, "realign to 4096");
    RequireOnlyHeapPointers();
}};

const Case alignedDelete{"AlignedDelete_HeapAndNullPointers_FreesOnlyHeapPointers", [] {
    const ReplacementHeap heap;
    const unsigned freesBefore = frees;
    void* pointer = ApplicationHeapAlign_nid_no_patch(64, 37);
    _ZdlPvSt11align_val_t_nid_postfix(pointer, 64);
    RequireEqual(frees, freesBefore + 1, "aligned delete");
    _ZdlPvSt11align_val_t_nid_postfix(nullptr, 64);
    RequireEqual(frees, freesBefore + 1, "aligned delete of null");
    pointer = ApplicationHeapAlign_nid_no_patch(64, 37);
    _ZdlPvmSt11align_val_t_nid_postfix(pointer, 37, 64);
    RequireEqual(frees, freesBefore + 2, "sized aligned delete");
    _ZdlPvmSt11align_val_t_nid_postfix(nullptr, 37, 64);
    RequireEqual(frees, freesBefore + 2, "sized aligned delete of null");
    RequireOnlyHeapPointers();
}};

const Case exhausted{"ApplicationHeap_CallbacksReturnNull_NothrowReturnsNullAndOthersThrow", [] {
    const ReplacementHeap heap;
    fail = true;
    Require(_ZnwmRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr, "nothrow new");
    Require(_ZnamRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr, "nothrow new[]");
    RequireRejected([] { _Znwm_nid_postfix(8); }, "throwing new");
    RequireRejected([] { ApplicationHeapAlign_nid_no_patch(4, 64); }, "aligned allocation");
    RequireRejected([] { ApplicationHeapAllocate_nid_no_patch(64); }, "allocation");
}};

const Case posixCallbackFails{"PosixMemalign_CallbackFails_ReturnsEnomemAndKeepsErrnoAndPointer", [] {
    const ReplacementHeap heap;
    fail = true;
    void* unchanged = storage.data();
    *__error_nid_postfix() = 77;
    int error = 0;
    try {
        error = posix_memalign_nid_postfix(&unchanged, 64, 64);
    } catch (const std::exception& exception) {
        Testing::Fail(std::string("expected ENOMEM, received exception: ") + exception.what());
    }
    RequireEqual(error, enomem, "posix_memalign status");
    RequireEqual(*__error_nid_postfix(), 77, "errno");
    Require(unchanged == storage.data(), "output pointer untouched");
}};

const Case posixCallbackReturnsNull{"PosixMemalign_CallbackSucceedsWithNull_ThrowsBadAlloc", [] {
    const ReplacementHeap heap;
    nullPosixResult = true;
    void* unchanged = storage.data();
    Testing::RequireThrows<std::bad_alloc>([&] { posix_memalign_nid_postfix(&unchanged, 64, 64); }, "success with a null pointer");
    Require(unchanged == storage.data(), "output pointer untouched");
}};

const Case posixInvalid{"PosixMemalign_InvalidAlignmentOrNullOutput_ReturnsEinvalWithoutCallback", [] {
    const ReplacementHeap heap;
    void* unchanged = storage.data();
    *__error_nid_postfix() = 77;
    const auto callsBefore = posixCalls;
    for (const std::size_t alignment : {std::size_t{0}, std::size_t{1}, std::size_t{4}, std::size_t{24}}) {
        const std::string input = "alignment " + std::to_string(alignment);
        RequireEqual(posix_memalign_nid_postfix(&unchanged, alignment, 32), einval, input);
        Require(unchanged == storage.data(), input + " leaves the output pointer untouched");
        RequireEqual(*__error_nid_postfix(), 77, input + " leaves errno untouched");
    }
    RequireEqual(posix_memalign_nid_postfix(nullptr, 64, 32), einval, "null output pointer");
    RequireEqual(posixCalls, callsBefore, "invalid requests never reach the callback");
}};

const Case posixValid{"PosixMemalign_ValidRequest_ForwardsSizeAndAlignment", [] {
    const ReplacementHeap heap;
    void* pointer = nullptr;
    *__error_nid_postfix() = 77;
    RequireEqual(posix_memalign_nid_postfix(&pointer, 64, 31), 0, "posix_memalign status");
    Require(pointer == storage.data(), "posix_memalign result");
    RequireEqual(lastSize, std::size_t{31}, "size");
    RequireEqual(lastAlignment, std::size_t{64}, "alignment");
    RequireEqual(*__error_nid_postfix(), 77, "errno untouched");
}};

const Case nothrowSizes{"NothrowNew_ZeroAndNonZeroSizes_AllocatesAtLeastOneByte", [] {
    const ReplacementHeap heap;
    Require(_ZnwmRKSt9nothrow_t_nid_postfix(16, nullptr) == storage.data(), "nothrow new of 16 bytes");
    RequireEqual(lastSize, std::size_t{16}, "nothrow new of 16 bytes size");
    Require(_ZnwmRKSt9nothrow_t_nid_postfix(0, nullptr) == storage.data(), "nothrow new of 0 bytes");
    RequireEqual(lastSize, std::size_t{1}, "nothrow new of 0 bytes size");
    Require(_ZnamRKSt9nothrow_t_nid_postfix(0, nullptr) == storage.data(), "nothrow new[] of 0 bytes");
    RequireEqual(lastSize, std::size_t{1}, "nothrow new[] of 0 bytes size");
}};

const Case reentrant{"ApplicationHeap_CallbackReentersHeap_ThrowsAndRecovers", [] {
    const ReplacementHeap heap;
    recurse = true;
    RequireRejected([] { ApplicationHeapAllocate_nid_no_patch(64); }, "reentrant allocation");
    recurse = false;
    Require(ApplicationHeapAllocate_nid_no_patch(64) == storage.data(), "allocation after the reentrant failure");
}};

const Case duplicates{"StringDuplicates_ReplacementHeap_AllocateExactSizes", [] {
    const ReplacementHeap heap;
    const char text[] = "guest string";
    char* copy = strdup_nid_postfix(text);
    Require(copy == reinterpret_cast<char*>(storage.data()), "strdup allocates from the heap");
    RequireEqual(lastSize, sizeof(text), "strdup size");
    RequireEqual(std::string_view(copy), std::string_view(text), "strdup copy");
    Require(copy != text, "strdup returns a new buffer");
    ApplicationHeapFree_nid_no_patch(copy);
    copy = strdup_nid_postfix("");
    RequireEqual(lastSize, std::size_t{1}, "strdup of an empty string size");
    RequireEqual(copy[0], '\0', "strdup of an empty string");
    ApplicationHeapFree_nid_no_patch(copy);
    copy = strndup_nid_postfix(text, 5);
    RequireEqual(lastSize, std::size_t{6}, "strndup size");
    RequireEqual(std::string_view(copy), std::string_view("guest"), "strndup copy");
    ApplicationHeapFree_nid_no_patch(copy);
}};

const Case workingDirectory{"Getcwd_NullBuffer_AllocatesFromReplacementHeap", [] {
    const ReplacementHeap heap;
    char* directory = getcwd_nid_postfix(nullptr, 0);
    Require(directory == reinterpret_cast<char*>(storage.data()), "getcwd allocates from the heap");
    RequireEqual(directory[0], '/', "guest working directory is absolute");
    RequireEqual(lastSize, std::strlen(directory) + 1, "getcwd without a size allocates the exact length");
    ApplicationHeapFree_nid_no_patch(directory);
    directory = getcwd_nid_postfix(nullptr, 200);
    Require(directory == reinterpret_cast<char*>(storage.data()), "getcwd with a size allocates from the heap");
    RequireEqual(lastSize, std::size_t{200}, "getcwd with a size allocates that size");
    ApplicationHeapFree_nid_no_patch(directory);
}};

const Case duplicatesExhausted{"StringDuplicates_CallbacksReturnNull_ReturnNullWithEnomem", [] {
    const ReplacementHeap heap;
    fail = true;
    const char text[] = "guest string";
    *__error_nid_postfix() = 0;
    Require(strdup_nid_postfix(text) == nullptr, "strdup");
    RequireEqual(*__error_nid_postfix(), enomem, "strdup errno");
    *__error_nid_postfix() = 0;
    Require(strdup_nid_postfix("") == nullptr, "strdup of an empty string");
    RequireEqual(*__error_nid_postfix(), enomem, "strdup of an empty string errno");
    *__error_nid_postfix() = 0;
    Require(strndup_nid_postfix(text, 5) == nullptr, "strndup");
    RequireEqual(*__error_nid_postfix(), enomem, "strndup errno");
    *__error_nid_postfix() = 0;
    Require(getcwd_nid_postfix(nullptr, 0) == nullptr, "getcwd");
    RequireEqual(*__error_nid_postfix(), enomem, "getcwd errno");
}};

const Case invalidMetadata{"ApplicationHeapInitialize_InvalidReplacementMetadata_ThrowsWithoutInitializing", [] {
    RequireMode("invalid");
    RequireUninitializedHeapRejects();
    auto& metadata = Metadata();
    Write(metadata.replacement, 8, std::uint64_t{99});
    RequireRejected([&] { ApplicationHeapInitialize_nid_no_patch(metadata.process.data()); },
        "unknown replacement version");
    Write(metadata.replacement, 8, std::uint64_t{2});
    RequireRejected([&] { ApplicationHeapInitialize_nid_no_patch(metadata.process.data()); },
        "initialize after a rejected table");
    RequireRejected([] { ApplicationHeapAlign_nid_no_patch(4, 64); }, "allocation after a rejected table");
    RequireEqual(initializes, 0u, "initialize callback runs");
}};

const Case defaultHeap{"ApplicationHeap_NoReplacementCallbacks_UsesDefaultHeap", [] {
    RequireMode("default");
    RequireUninitializedHeapRejects();
    auto& metadata = Metadata();
    std::array<void*, 10> partial{};
    partial[0] = reinterpret_cast<void*>(&allocate);
    RequireRejected([&] { ApplicationHeapRegister_nid_no_patch(partial.data()); }, "partial table");
    std::memset(metadata.replacement.data() + 0x20, 0, sizeof(partial));
    ApplicationHeapInitialize_nid_no_patch(metadata.process.data());
    RequireEqual(initializes, 1u, "initialize callback runs");
    auto* pointer = static_cast<unsigned char*>(ApplicationHeapCalloc_nid_no_patch(7, 9));
    for (unsigned i = 0; i < 63; ++i) RequireEqual(pointer[i], 0, "calloc byte " + std::to_string(i));
    std::memset(pointer, 0x5a, 63);
    pointer = static_cast<unsigned char*>(ApplicationHeapReallocate_nid_no_patch(pointer, 150));
    for (unsigned i = 0; i < 63; ++i) RequireEqual(pointer[i], 0x5a, "grown byte " + std::to_string(i));
    pointer = static_cast<unsigned char*>(ApplicationHeapReallocate_nid_no_patch(pointer, 11));
    for (unsigned i = 0; i < 11; ++i) RequireEqual(pointer[i], 0x5a, "shrunk byte " + std::to_string(i));
    Require(ApplicationHeapReallocate_nid_no_patch(pointer, 0) == nullptr, "reallocation to zero bytes frees");
    pointer = static_cast<unsigned char*>(ApplicationHeapReallocate_nid_no_patch(nullptr, 32));
    Require(pointer != nullptr, "reallocation of null allocates");
    ApplicationHeapFree_nid_no_patch(pointer);
    for (const std::size_t alignment : {std::size_t{16}, std::size_t{64}, std::size_t{4096}}) {
        auto* aligned = ApplicationHeapAlign_nid_no_patch(alignment, 37);
        RequireEqual(reinterpret_cast<std::uintptr_t>(aligned) % alignment, std::uintptr_t{0},
            "alignment " + std::to_string(alignment));
        _ZdlPvSt11align_val_t_nid_postfix(aligned, alignment);
    }
    void* aligned = nullptr;
    RequireEqual(ApplicationHeapPosixAlign_nid_no_patch(&aligned, 256, 99), 0, "posix align status");
    RequireEqual(reinterpret_cast<std::uintptr_t>(aligned) % 256, std::uintptr_t{0}, "posix align alignment");
    ApplicationHeapFree_nid_no_patch(aligned);
    ApplicationHeapFree_nid_no_patch(nullptr);
    RequireRejected([] { ApplicationHeapCalloc_nid_no_patch(SIZE_MAX, 2); }, "calloc overflow");
    RequireRejected([] { ApplicationHeapAlign_nid_no_patch(3, 16); }, "alignment 3");
    ApplicationHeapInitialize_nid_no_patch(metadata.process.data());
    RequireEqual(initializes, 1u, "second initialize is ignored");
}};

const Case exitOrder{"ApplicationHeap_GuestExitHandler_AllocatesBeforeHeapFinalizes", [] {
    RequireMode("exit-order");
    RequireUninitializedHeapRejects();
    RequireEqual(atexit_nid_postfix(exitCallbackAllocates), 0, "atexit registration");
    ApplicationHeapInitialize_nid_no_patch(Metadata().process.data());
    RequireEqual(initializes, 1u, "initialize callback runs");
}};

} // namespace
