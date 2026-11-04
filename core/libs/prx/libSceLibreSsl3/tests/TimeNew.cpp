#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <source_location>
#include <string_view>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ASN1_TIME check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::uint64_t MallocFailure = 0x0dfff041;
int liveBlocks = 0;
int callsBeforeFailure = -1;

bool Fail() {
    if (callsBeforeFailure == 0) {
        callsBeforeFailure = -1;
        return true;
    }
    if (callsBeforeFailure > 0) --callsBeforeFailure;
    return false;
}

void* APS5_VABI Allocate(std::size_t bytes) {
    if (Fail()) return nullptr;
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (Fail()) return nullptr;
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

void FreeTime(Asn1String* time) {
    free_nid_postfix(time->data);
    free_nid_postfix(time);
}

}

int main() {
    InitializeHeap();
    {
        auto* time = ASN1_TIME_new_nid_postfix();
        Require(time && time->type == -1 && time->length == 0 && !time->data && time->flags == 0x040);
        Require(liveBlocks == 1 && ERR_get_error_nid_postfix() == 0);
        FreeTime(time);
        callsBeforeFailure = 0;
        Require(ASN1_TIME_new_nid_postfix() == nullptr && liveBlocks == 0);
        Require(ERR_get_error_nid_postfix() == MallocFailure);
        Require(ERR_get_error_nid_postfix() == MallocFailure);
        Require(ERR_get_error_nid_postfix() == 0);
    }
}
