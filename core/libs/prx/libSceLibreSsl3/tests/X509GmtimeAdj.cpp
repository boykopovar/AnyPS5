#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_gmtime_adj check failed at line %u\n", location.line());
    std::abort();
}
void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }
}

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    Asn1String time{};
    Require(X509_gmtime_adj_nid_postfix(&time, 0) == &time);
    Require(time.type == 23 && time.length == 13 && time.data != nullptr && time.data[12] == 'Z');

    const auto future = std::chrono::system_clock::now() + std::chrono::hours(24 * 365 * 100);
    const std::time_t stamp = std::chrono::system_clock::to_time_t(future);
    const std::tm* broken = std::gmtime(&stamp);
    Require(broken != nullptr);
    const auto adjustment = static_cast<std::int64_t>(stamp) - std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    Require(X509_gmtime_adj_nid_postfix(&time, adjustment) == &time);
    Require(time.type == 24 && time.length == 15 && time.data[14] == 'Z');
    char expected[16]{};
    std::snprintf(expected, sizeof(expected), "%04d%02d%02d%02d%02d%02dZ", broken->tm_year + 1900,
        broken->tm_mon + 1, broken->tm_mday, broken->tm_hour, broken->tm_min, broken->tm_sec);
    Require(std::memcmp(time.data, expected, 15) == 0);
    free_nid_postfix(time.data);
    Require(X509_gmtime_adj_nid_postfix(nullptr, 0) == nullptr);
    std::puts("libSceLibreSsl3 X509_gmtime_adj tests passed");
}
