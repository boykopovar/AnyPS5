#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using LibreSsl::Bio;
using LibreSsl::BioMethod;

namespace {

constexpr std::uint64_t BioUnsupportedMethod = (std::uint64_t{32} << 24) | (std::uint64_t{0xfff} << 12) | 121;
constexpr std::uint64_t BioUninitialized = (std::uint64_t{32} << 24) | (std::uint64_t{0xfff} << 12) | 120;
constexpr std::uint64_t PemNoStartLine = (std::uint64_t{9} << 24) | (std::uint64_t{0xfff} << 12) | 108;
constexpr std::uint64_t BufferMallocFailure = (std::uint64_t{7} << 24) | (std::uint64_t{0xfff} << 12) | 65;
constexpr std::uint64_t PemMallocFailure = (std::uint64_t{9} << 24) | (std::uint64_t{0xfff} << 12) | 65;

int allocationCalls = 0;
int failureAt = -1;
int liveAllocations = 0;

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    const int call = allocationCalls++;
    if (call == failureAt) {
        failureAt = -1;
        return nullptr;
    }
    void* const result = std::calloc(count, size);
    if (result) ++liveAllocations;
    return result;
}
void APS5_VABI Free(void* pointer) {
    if (pointer) --liveAllocations;
    std::free(pointer);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) {
    void* const result = std::realloc(pointer, size);
    if (result && !pointer) ++liveAllocations;
    return result;
}
void* APS5_VABI Align(std::size_t, std::size_t) {
    throw std::logic_error("unexpected aligned PEM allocation");
}
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t alignment) {
    static_cast<void>(pointer);
    static_cast<void>(size);
    static_cast<void>(alignment);
    return nullptr;
}
int APS5_VABI PosixAlign(void** output, std::size_t alignment, std::size_t size) {
    static_cast<void>(output);
    static_cast<void>(alignment);
    static_cast<void>(size);
    return 12;
}

void InitializeHeap() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

struct ReadState {
    int calls;
    int mode;
};

struct CallbackState {
    std::array<int, 2> operations{};
    std::array<std::int64_t, 2> results{};
    int calls;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

int APS5_VABI Gets(Bio* bio, char* destination, int size) {
    auto* const state = static_cast<ReadState*>(bio->data);
    ++state->calls;
    if (state->mode == 0) return 0;
    if (state->mode == 1) return -1;
    if (state->calls == 1) {
        constexpr char line[] = "-----BEGIN CERTIFICATE----\n";
        if (state->mode == 3) {
            constexpr char completeStart[] = "-----BEGIN CERTIFICATE-----\n";
            Require(static_cast<int>(sizeof(completeStart) - 1) < size, "complete start line fits BIO_gets buffer");
            std::memcpy(destination, completeStart, sizeof(completeStart));
            return static_cast<int>(sizeof(completeStart) - 1);
        }
        Require(static_cast<int>(sizeof(line) - 1) < size, "test line fits BIO_gets buffer");
        std::memcpy(destination, line, sizeof(line));
        return static_cast<int>(sizeof(line) - 1);
    }
    return 0;
}

std::int64_t APS5_VABI Trace(Bio* bio, int operation, const char*, int, std::int64_t, std::int64_t result) {
    auto* const state = reinterpret_cast<CallbackState*>(bio->callbackArgument);
    if (state->calls < static_cast<int>(state->operations.size())) {
        state->operations[state->calls] = operation;
        state->results[state->calls] = result;
    }
    ++state->calls;
    return result;
}

std::int64_t APS5_VABI Veto(Bio* bio, int operation, const char*, int, std::int64_t, std::int64_t result) {
    auto* const state = reinterpret_cast<CallbackState*>(bio->callbackArgument);
    ++state->calls;
    state->operations[0] = operation;
    state->results[0] = result;
    return 0;
}

void CheckQueue(std::uint64_t previous, bool seeded) {
    if (seeded) Require(ERR_get_error_nid_postfix() == previous, "pre-existing error remains first");
    Require(ERR_get_error_nid_postfix() == PemNoStartLine, "reader queues PEM_R_NO_START_LINE");
    Require(ERR_get_error_nid_postfix() == 0, "reader queues no additional error");
}

void CheckRead(int mode, bool callback) {
    static const BioMethod method{0, "PEM test", nullptr, nullptr, nullptr, Gets, nullptr, nullptr, nullptr, nullptr};
    ReadState readState{0, mode};
    CallbackState callbackState{};
    Bio bio{};
    bio.method = &method;
    bio.initialized = 1;
    bio.data = &readState;
    if (callback) {
        bio.callback = Trace;
        bio.callbackArgument = reinterpret_cast<char*>(&callbackState);
    }
    LibreSslX509 certificate{};
    LibreSslX509* output = &certificate;
    ERR_clear_error_nid_postfix();
    constexpr std::uint64_t previous = (std::uint64_t{1} << 24) | (std::uint64_t{2} << 12) | 3;
    LibreSslPutError_nid_no_patch(1, 2, 3);
    Require(PEM_read_bio_X509_AUX_nid_postfix(&bio, &output, nullptr, nullptr) == nullptr, "start-line failure returns null");
    Require(output == &certificate, "start-line failure leaves the output certificate unchanged");
    CheckQueue(previous, true);
    if (callback) {
        Require(callbackState.calls == 2, "BIO callback observes both gets events");
        Require(callbackState.operations[0] == 0x05 && callbackState.operations[1] == 0x85, "BIO callback event order matches BIO_gets");
        Require(callbackState.results[0] == 1 && callbackState.results[1] == (mode == 1 ? -1 : 0), "BIO callback receives the gets and return results");
    }
    Require(readState.calls == (mode == 2 ? 2 : 1), "BIO method reads only until EOF or a start marker");
}

void CheckNullBio() {
    LibreSslX509 certificate{};
    LibreSslX509* output = &certificate;
    ERR_clear_error_nid_postfix();
    constexpr std::uint64_t previous = (std::uint64_t{1} << 24) | (std::uint64_t{2} << 12) | 3;
    LibreSslPutError_nid_no_patch(1, 2, 3);
    Require(PEM_read_bio_X509_AUX_nid_postfix(nullptr, &output, nullptr, nullptr) == nullptr,
        "a null BIO returns null after BIO_gets reports an unsupported method");
    Require(output == &certificate, "a null BIO leaves the output certificate unchanged");
    Require(ERR_get_error_nid_postfix() == previous, "a null BIO preserves the pre-existing error first");
    Require(ERR_get_error_nid_postfix() == BioUnsupportedMethod, "BIO_gets queues unsupported method for a null BIO");
    Require(ERR_get_error_nid_postfix() == PemNoStartLine, "the PEM reader appends no-start-line after the BIO error");
    Require(ERR_get_error_nid_postfix() == 0, "a null BIO queues no additional errors");
}

void CheckUninitializedBio() {
    static const BioMethod method{0, "PEM test", nullptr, nullptr, nullptr, Gets, nullptr, nullptr, nullptr, nullptr};
    ReadState readState{0, 3};
    CallbackState callbackState{};
    Bio bio{};
    bio.method = &method;
    bio.callback = Trace;
    bio.callbackArgument = reinterpret_cast<char*>(&callbackState);
    bio.data = &readState;
    LibreSslX509 certificate{};
    LibreSslX509* output = &certificate;
    ERR_clear_error_nid_postfix();
    Require(PEM_read_bio_X509_AUX_nid_postfix(&bio, &output, nullptr, nullptr) == nullptr,
        "an uninitialized BIO returns null");
    Require(output == &certificate, "an uninitialized BIO leaves the output certificate unchanged");
    Require(callbackState.calls == 1 && callbackState.operations[0] == 0x05 && callbackState.results[0] == 1,
        "BIO_gets invokes the pre-callback before checking initialization");
    Require(readState.calls == 0, "an uninitialized BIO skips the gets method");
    Require(ERR_get_error_nid_postfix() == BioUninitialized, "BIO_gets queues uninitialized after the callback");
    Require(ERR_get_error_nid_postfix() == PemNoStartLine, "the PEM reader appends no-start-line");
    Require(ERR_get_error_nid_postfix() == 0, "uninitialized BIO queues no additional errors");
}

void CheckCallbackVeto() {
    static const BioMethod method{0, "PEM test", nullptr, nullptr, nullptr, Gets, nullptr, nullptr, nullptr, nullptr};
    ReadState readState{0, 1};
    CallbackState callbackState{};
    Bio bio{};
    bio.method = &method;
    bio.callback = Veto;
    bio.callbackArgument = reinterpret_cast<char*>(&callbackState);
    bio.initialized = 1;
    bio.data = &readState;
    ERR_clear_error_nid_postfix();
    Require(PEM_read_bio_X509_AUX_nid_postfix(&bio, nullptr, nullptr, nullptr) == nullptr, "a callback veto reaches the start-line error");
    Require(callbackState.calls == 1 && callbackState.operations[0] == 0x05 && callbackState.results[0] == 1,
        "a gets callback veto is observed before the BIO method");
    Require(readState.calls == 0, "a gets callback veto prevents the method call");
    Require(ERR_get_error_nid_postfix() == PemNoStartLine && ERR_get_error_nid_postfix() == 0, "callback veto adds only PEM_R_NO_START_LINE");
}

void CheckAllocationFailures() {
    static const BioMethod method{0, "PEM test", nullptr, nullptr, nullptr, Gets, nullptr, nullptr, nullptr, nullptr};
    constexpr std::uint64_t previous = (std::uint64_t{1} << 24) | (std::uint64_t{2} << 12) | 3;
    for (int failAt = 0; failAt < 5; ++failAt) {
        allocationCalls = 0;
        failureAt = failAt;
        ReadState readState{0, 3};
        Bio bio{};
        bio.method = &method;
        bio.initialized = 1;
        bio.data = &readState;
        LibreSslX509 certificate{};
        LibreSslX509* output = &certificate;
        ERR_clear_error_nid_postfix();
        LibreSslPutError_nid_no_patch(1, 2, 3);
        Require(PEM_read_bio_X509_AUX_nid_postfix(failAt < 3 ? nullptr : &bio, &output, nullptr, nullptr) == nullptr,
            "each BUF_MEM allocation failure returns null");
        Require(output == &certificate, "allocation failures leave the output certificate unchanged");
        Require(ERR_get_error_nid_postfix() == previous, "allocation failure preserves the existing error first");
        Require(ERR_get_error_nid_postfix() == BufferMallocFailure, "BUF allocation failure is queued first");
        Require(ERR_get_error_nid_postfix() == PemMallocFailure, "PEM allocation failure follows the BUF error");
        Require(ERR_get_error_nid_postfix() == 0, "allocation failure queues no additional errors");
        Require(readState.calls == (failAt < 3 ? 0 : 1), "allocation failure occurs before the matching BIO read");
        Require(liveAllocations == 0, "allocation failure frees every allocated buffer");
    }
}

}

int main() {
    InitializeHeap();
    CheckRead(0, false);
    CheckRead(1, true);
    CheckRead(2, false);
    CheckCallbackVeto();
    CheckNullBio();
    CheckUninitializedBio();
    CheckAllocationFailures();
    return 0;
}
