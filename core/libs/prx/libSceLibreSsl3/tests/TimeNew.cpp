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

std::string_view Text(const Asn1String* time) {
    return {reinterpret_cast<const char*>(time->data), static_cast<std::size_t>(time->length)};
}

void Adjusted(std::int64_t target, int type, std::string_view prefix) {
    auto* time = X509_gmtime_adj_nid_postfix(nullptr, target - std::time(nullptr));
    Require(time && time->type == type && time->length == (type == 23 ? 13 : 15) && liveBlocks == 2);
    Require(Text(time).starts_with(prefix) && time->data[time->length - 1] == 'Z' && time->data[time->length] == 0);
    Require(time->flags == 0x040);
    FreeTime(time);
    Require(liveBlocks == 0 && ERR_get_error_nid_postfix() == 0);
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
    {
        const std::int64_t before = std::time(nullptr);
        auto* time = X509_gmtime_adj_nid_postfix(nullptr, 0);
        const std::int64_t after = std::time(nullptr);
        Require(time && liveBlocks == 2);
        Asn1String epoch {15, 24, reinterpret_cast<unsigned char*>(const_cast<char*>("19700101000000Z")), 0};
        int days;
        int seconds;
        Require(ASN1_TIME_diff_nid_postfix(&days, &seconds, &epoch, time) == 1);
        const std::int64_t now = static_cast<std::int64_t>(days) * 86400 + seconds;
        Require(now >= before && now <= after);
        auto* data = time->data;
        Require(X509_gmtime_adj_nid_postfix(time, -86400 * 365) == time && liveBlocks == 2 && time->data != data);
        callsBeforeFailure = 0;
        data = time->data;
        const int length = time->length;
        Require(X509_gmtime_adj_nid_postfix(time, 0) == nullptr && time->data == data && time->length == length);
        Require(liveBlocks == 2 && ERR_get_error_nid_postfix() == MallocFailure && ERR_get_error_nid_postfix() == 0);
        FreeTime(time);
        callsBeforeFailure = 1;
        Require(X509_gmtime_adj_nid_postfix(nullptr, 0) == nullptr && liveBlocks == 0);
        Require(ERR_get_error_nid_postfix() == MallocFailure && ERR_get_error_nid_postfix() == 0);
        callsBeforeFailure = 0;
        Require(X509_gmtime_adj_nid_postfix(nullptr, 0) == nullptr && liveBlocks == 0);
        Require(ERR_get_error_nid_postfix() == MallocFailure && ERR_get_error_nid_postfix() == MallocFailure);
        Require(ERR_get_error_nid_postfix() == 0);
        Adjusted(-2194689600, 24, "19000615");
        Adjusted(-616852800, 23, "500615");
        Adjusted(2507371200, 23, "490615");
        Adjusted(2538907200, 24, "20500615");
        Adjusted(-62152833600, 24, "00000615");
        Adjusted(253385064000, 24, "99990615");
        Asn1String kept {3, 4, nullptr, 5};
        Require(X509_gmtime_adj_nid_postfix(&kept, INT64_MAX) == nullptr);
        Require(X509_gmtime_adj_nid_postfix(&kept, INT64_MIN) == nullptr);
        Require(X509_gmtime_adj_nid_postfix(&kept, 253402300800 - std::time(nullptr) + 3600) == nullptr);
        Require(X509_gmtime_adj_nid_postfix(&kept, -62167219201 - std::time(nullptr) - 3600) == nullptr);
        Require(kept.length == 3 && kept.type == 4 && !kept.data && kept.flags == 5);
        Require(liveBlocks == 0 && ERR_get_error_nid_postfix() == 0);
    }
}
