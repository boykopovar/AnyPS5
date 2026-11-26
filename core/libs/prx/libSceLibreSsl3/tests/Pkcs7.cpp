#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Pkcs7.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;
constexpr std::size_t NoFailure = std::numeric_limits<std::size_t>::max();
std::size_t allocationsUntilFailure = NoFailure;

bool AllocationFails() {
    if (allocationsUntilFailure == NoFailure) return false;
    if (allocationsUntilFailure == 0) return true;
    --allocationsUntilFailure;
    return false;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (AllocationFails()) return nullptr;
    void* result = std::malloc(size ? size : 1);
    if (result) allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    if (allocations.erase(pointer) == 0) std::abort();
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (AllocationFails()) return nullptr;
    void* result = std::calloc(count ? count : 1, size ? size : 1);
    if (result) allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    if (AllocationFails()) return nullptr;
    if (allocations.erase(pointer) == 0) std::abort();
    void* result = std::realloc(pointer, size ? size : 1);
    if (result) allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 d2i_PKCS7 check failed at line %u\n", location.line());
    std::abort();
}

template <typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

constexpr unsigned char SignedMessage[] = {
    0x30, 0x82, 0x03, 0x5e, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02, 0xa0,
    0x82, 0x03, 0x4f, 0x30, 0x82, 0x03, 0x4b, 0x02, 0x01, 0x01, 0x31, 0x0f, 0x30, 0x0d, 0x06, 0x09,
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x30, 0x14, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x07, 0x04, 0x05, 0x68, 0x65, 0x6c, 0x6c,
    0x6f, 0xa0, 0x82, 0x01, 0x91, 0x30, 0x82, 0x01, 0x8d, 0x30, 0x82, 0x01, 0x33, 0xa0, 0x03, 0x02,
    0x01, 0x02, 0x02, 0x14, 0x1a, 0x4c, 0x20, 0x77, 0x64, 0x7c, 0xae, 0x2a, 0xbb, 0x29, 0x03, 0xd2,
    0x0a, 0x35, 0x8b, 0xcc, 0xd6, 0x37, 0x81, 0x13, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce,
    0x3d, 0x04, 0x03, 0x02, 0x30, 0x1c, 0x31, 0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
    0x11, 0x41, 0x6e, 0x79, 0x50, 0x53, 0x35, 0x20, 0x50, 0x4b, 0x43, 0x53, 0x37, 0x20, 0x74, 0x65,
    0x73, 0x74, 0x30, 0x1e, 0x17, 0x0d, 0x32, 0x36, 0x31, 0x31, 0x32, 0x30, 0x31, 0x30, 0x34, 0x33,
    0x31, 0x34, 0x5a, 0x17, 0x0d, 0x33, 0x36, 0x31, 0x31, 0x31, 0x37, 0x31, 0x30, 0x34, 0x33, 0x31,
    0x34, 0x5a, 0x30, 0x1c, 0x31, 0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x11, 0x41,
    0x6e, 0x79, 0x50, 0x53, 0x35, 0x20, 0x50, 0x4b, 0x43, 0x53, 0x37, 0x20, 0x74, 0x65, 0x73, 0x74,
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a,
    0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x61, 0xb3, 0x79, 0x3c, 0x38,
    0x5c, 0xfe, 0xbb, 0xea, 0xdb, 0x48, 0x38, 0x8c, 0xe9, 0x52, 0x88, 0x86, 0xc1, 0x67, 0x3d, 0xcd,
    0x6d, 0xfa, 0x22, 0xdd, 0x4e, 0xae, 0x24, 0x31, 0x71, 0xa1, 0x91, 0xd7, 0x03, 0xf4, 0x8b, 0xdd,
    0x57, 0x52, 0x1d, 0x67, 0xca, 0x09, 0x54, 0x92, 0x7a, 0x7f, 0x95, 0x5c, 0x30, 0xee, 0x3a, 0x1a,
    0x8d, 0xf0, 0xf8, 0xb1, 0x90, 0xf6, 0x5c, 0xe2, 0xd1, 0x5e, 0xc1, 0xa3, 0x53, 0x30, 0x51, 0x30,
    0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x93, 0xa9, 0x56, 0x35, 0xc8, 0x68,
    0x98, 0xe2, 0x86, 0x3a, 0x8d, 0xdd, 0x71, 0x63, 0xd8, 0x7f, 0x6c, 0xdd, 0x28, 0x14, 0x30, 0x1f,
    0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x93, 0xa9, 0x56, 0x35, 0xc8,
    0x68, 0x98, 0xe2, 0x86, 0x3a, 0x8d, 0xdd, 0x71, 0x63, 0xd8, 0x7f, 0x6c, 0xdd, 0x28, 0x14, 0x30,
    0x0f, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xff,
    0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x48, 0x00, 0x30,
    0x45, 0x02, 0x21, 0x00, 0xf1, 0xea, 0xe4, 0xf9, 0x97, 0x79, 0x25, 0x8f, 0x76, 0xf5, 0xb3, 0xcb,
    0x60, 0xe2, 0xf4, 0xcc, 0xea, 0x9d, 0xb3, 0xe7, 0x3f, 0xaa, 0xd5, 0xc7, 0x1e, 0x35, 0x98, 0x40,
    0x4a, 0xc7, 0x9a, 0xb3, 0x02, 0x20, 0x06, 0x16, 0x1e, 0xba, 0xd3, 0x07, 0x64, 0x04, 0x77, 0x04,
    0x74, 0xa8, 0x04, 0x58, 0x4e, 0xb2, 0x90, 0x30, 0x91, 0x69, 0x04, 0xc6, 0x2d, 0xa6, 0x23, 0x16,
    0x80, 0xc4, 0xaf, 0xde, 0x5a, 0x07, 0x31, 0x82, 0x01, 0x88, 0x30, 0x82, 0x01, 0x84, 0x02, 0x01,
    0x01, 0x30, 0x34, 0x30, 0x1c, 0x31, 0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x11,
    0x41, 0x6e, 0x79, 0x50, 0x53, 0x35, 0x20, 0x50, 0x4b, 0x43, 0x53, 0x37, 0x20, 0x74, 0x65, 0x73,
    0x74, 0x02, 0x14, 0x1a, 0x4c, 0x20, 0x77, 0x64, 0x7c, 0xae, 0x2a, 0xbb, 0x29, 0x03, 0xd2, 0x0a,
    0x35, 0x8b, 0xcc, 0xd6, 0x37, 0x81, 0x13, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65,
    0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0xa0, 0x81, 0xe4, 0x30, 0x18, 0x06, 0x09, 0x2a, 0x86, 0x48,
    0x86, 0xf7, 0x0d, 0x01, 0x09, 0x03, 0x31, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x01, 0x07, 0x01, 0x30, 0x1c, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x05,
    0x31, 0x0f, 0x17, 0x0d, 0x32, 0x36, 0x31, 0x31, 0x32, 0x30, 0x31, 0x30, 0x34, 0x33, 0x31, 0x34,
    0x5a, 0x30, 0x2f, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x04, 0x31, 0x22,
    0x04, 0x20, 0x2c, 0xf2, 0x4d, 0xba, 0x5f, 0xb0, 0xa3, 0x0e, 0x26, 0xe8, 0x3b, 0x2a, 0xc5, 0xb9,
    0xe2, 0x9e, 0x1b, 0x16, 0x1e, 0x5c, 0x1f, 0xa7, 0x42, 0x5e, 0x73, 0x04, 0x33, 0x62, 0x93, 0x8b,
    0x98, 0x24, 0x30, 0x79, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x0f, 0x31,
    0x6c, 0x30, 0x6a, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x2a,
    0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x16, 0x30, 0x0b, 0x06,
    0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86,
    0x48, 0x86, 0xf7, 0x0d, 0x03, 0x07, 0x30, 0x0e, 0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x03, 0x02, 0x02, 0x02, 0x00, 0x80, 0x30, 0x0d, 0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x03, 0x02, 0x02, 0x01, 0x40, 0x30, 0x07, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x07, 0x30, 0x0d,
    0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x03, 0x02, 0x02, 0x01, 0x28, 0x30, 0x0a, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x04, 0x47, 0x30, 0x45, 0x02, 0x21, 0x00,
    0xfd, 0x0c, 0xc5, 0x00, 0x26, 0x6f, 0xb3, 0x13, 0x45, 0x2c, 0xd2, 0x3c, 0xcf, 0xe8, 0x3e, 0x3d,
    0xf6, 0x03, 0xbc, 0xf8, 0x77, 0x5b, 0x1d, 0x66, 0xd8, 0x2e, 0x1a, 0xa7, 0x50, 0x23, 0x7e, 0x55,
    0x02, 0x20, 0x76, 0x80, 0x5f, 0x79, 0x5c, 0x90, 0x99, 0x4d, 0x3c, 0x25, 0x1c, 0x94, 0x86, 0x36,
    0x5b, 0xfb, 0xc7, 0xc3, 0x6a, 0xb1, 0x6d, 0xc0, 0x81, 0xa3, 0x04, 0x2a, 0x54, 0x31, 0x81, 0x7f,
    0x7f, 0xf9,
};

constexpr unsigned char CertificateAndCrl[] = {
    0x30, 0x82, 0x02, 0x75, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02, 0xa0,
    0x82, 0x02, 0x66, 0x30, 0x82, 0x02, 0x62, 0x02, 0x01, 0x01, 0x31, 0x00, 0x30, 0x0b, 0x06, 0x09,
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x82, 0x01, 0x91, 0x30, 0x82, 0x01,
    0x8d, 0x30, 0x82, 0x01, 0x33, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x14, 0x1a, 0x4c, 0x20, 0x77,
    0x64, 0x7c, 0xae, 0x2a, 0xbb, 0x29, 0x03, 0xd2, 0x0a, 0x35, 0x8b, 0xcc, 0xd6, 0x37, 0x81, 0x13,
    0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x30, 0x1c, 0x31, 0x1a,
    0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x11, 0x41, 0x6e, 0x79, 0x50, 0x53, 0x35, 0x20,
    0x50, 0x4b, 0x43, 0x53, 0x37, 0x20, 0x74, 0x65, 0x73, 0x74, 0x30, 0x1e, 0x17, 0x0d, 0x32, 0x36,
    0x31, 0x31, 0x32, 0x30, 0x31, 0x30, 0x34, 0x33, 0x31, 0x34, 0x5a, 0x17, 0x0d, 0x33, 0x36, 0x31,
    0x31, 0x31, 0x37, 0x31, 0x30, 0x34, 0x33, 0x31, 0x34, 0x5a, 0x30, 0x1c, 0x31, 0x1a, 0x30, 0x18,
    0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x11, 0x41, 0x6e, 0x79, 0x50, 0x53, 0x35, 0x20, 0x50, 0x4b,
    0x43, 0x53, 0x37, 0x20, 0x74, 0x65, 0x73, 0x74, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x61, 0xb3, 0x79, 0x3c, 0x38, 0x5c, 0xfe, 0xbb, 0xea, 0xdb, 0x48, 0x38, 0x8c,
    0xe9, 0x52, 0x88, 0x86, 0xc1, 0x67, 0x3d, 0xcd, 0x6d, 0xfa, 0x22, 0xdd, 0x4e, 0xae, 0x24, 0x31,
    0x71, 0xa1, 0x91, 0xd7, 0x03, 0xf4, 0x8b, 0xdd, 0x57, 0x52, 0x1d, 0x67, 0xca, 0x09, 0x54, 0x92,
    0x7a, 0x7f, 0x95, 0x5c, 0x30, 0xee, 0x3a, 0x1a, 0x8d, 0xf0, 0xf8, 0xb1, 0x90, 0xf6, 0x5c, 0xe2,
    0xd1, 0x5e, 0xc1, 0xa3, 0x53, 0x30, 0x51, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16,
    0x04, 0x14, 0x93, 0xa9, 0x56, 0x35, 0xc8, 0x68, 0x98, 0xe2, 0x86, 0x3a, 0x8d, 0xdd, 0x71, 0x63,
    0xd8, 0x7f, 0x6c, 0xdd, 0x28, 0x14, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x18, 0x30,
    0x16, 0x80, 0x14, 0x93, 0xa9, 0x56, 0x35, 0xc8, 0x68, 0x98, 0xe2, 0x86, 0x3a, 0x8d, 0xdd, 0x71,
    0x63, 0xd8, 0x7f, 0x6c, 0xdd, 0x28, 0x14, 0x30, 0x0f, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01,
    0xff, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xff, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce,
    0x3d, 0x04, 0x03, 0x02, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x21, 0x00, 0xf1, 0xea, 0xe4, 0xf9,
    0x97, 0x79, 0x25, 0x8f, 0x76, 0xf5, 0xb3, 0xcb, 0x60, 0xe2, 0xf4, 0xcc, 0xea, 0x9d, 0xb3, 0xe7,
    0x3f, 0xaa, 0xd5, 0xc7, 0x1e, 0x35, 0x98, 0x40, 0x4a, 0xc7, 0x9a, 0xb3, 0x02, 0x20, 0x06, 0x16,
    0x1e, 0xba, 0xd3, 0x07, 0x64, 0x04, 0x77, 0x04, 0x74, 0xa8, 0x04, 0x58, 0x4e, 0xb2, 0x90, 0x30,
    0x91, 0x69, 0x04, 0xc6, 0x2d, 0xa6, 0x23, 0x16, 0x80, 0xc4, 0xaf, 0xde, 0x5a, 0x07, 0xa1, 0x81,
    0xb6, 0x30, 0x81, 0xb3, 0x30, 0x5b, 0x02, 0x01, 0x01, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48,
    0xce, 0x3d, 0x04, 0x03, 0x02, 0x30, 0x1c, 0x31, 0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03,
    0x0c, 0x11, 0x41, 0x6e, 0x79, 0x50, 0x53, 0x35, 0x20, 0x50, 0x4b, 0x43, 0x53, 0x37, 0x20, 0x74,
    0x65, 0x73, 0x74, 0x17, 0x0d, 0x32, 0x36, 0x31, 0x31, 0x32, 0x30, 0x31, 0x30, 0x34, 0x33, 0x31,
    0x34, 0x5a, 0x17, 0x0d, 0x33, 0x36, 0x31, 0x31, 0x31, 0x37, 0x31, 0x30, 0x34, 0x33, 0x31, 0x34,
    0x5a, 0xa0, 0x0e, 0x30, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x1d, 0x14, 0x04, 0x03, 0x02, 0x01,
    0x01, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x48, 0x00,
    0x30, 0x45, 0x02, 0x20, 0x2d, 0xe0, 0xa8, 0xc6, 0xeb, 0x6c, 0x7d, 0x4c, 0xb6, 0xf2, 0x3d, 0x15,
    0x49, 0x0f, 0x6c, 0xfa, 0x86, 0xa0, 0x78, 0xdc, 0xb4, 0x67, 0x2e, 0x3c, 0xb1, 0xa4, 0x9a, 0xfe,
    0xc0, 0x5d, 0x78, 0x82, 0x02, 0x21, 0x00, 0xb5, 0x0f, 0x21, 0x36, 0x16, 0xa8, 0x8f, 0x60, 0xef,
    0x4b, 0x21, 0xc4, 0xda, 0x9c, 0x29, 0x33, 0x84, 0x3f, 0x48, 0x9f, 0xa4, 0x17, 0xd7, 0x10, 0xfe,
    0x07, 0xf4, 0x6d, 0x29, 0x58, 0x0f, 0xd6, 0x31, 0x00,
};

constexpr unsigned char Aes128CbcOid[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02};
constexpr unsigned char Aes256CbcOid[] = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x2a};

constexpr unsigned char EncryptedWithData[] = {
    0x30, 0x50, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x06, 0xa0, 0x43, 0x30,
    0x41, 0x02, 0x01, 0x00, 0x30, 0x3c, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07,
    0x01, 0x30, 0x1d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02, 0x04, 0x10,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    0x80, 0x10, 0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08,
};

constexpr unsigned char EncryptedWithoutData[] = {
    0x30, 0x2c, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x06, 0xa0, 0x1f, 0x30,
    0x1d, 0x02, 0x01, 0x00, 0x30, 0x18, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07,
    0x01, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x2a,
};
constexpr unsigned char Enveloped[] = {
    0x30, 0x81, 0x99, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x03, 0xa0, 0x81,
    0x8b, 0x30, 0x81, 0x88, 0x02, 0x01, 0x00, 0x31, 0x63, 0x30, 0x2f, 0x02, 0x01, 0x00, 0x30, 0x14,
    0x30, 0x0e, 0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x03, 0x42, 0x6f, 0x62,
    0x02, 0x02, 0x45, 0x67, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
    0x01, 0x05, 0x00, 0x04, 0x05, 0x55, 0x66, 0x77, 0x88, 0x99, 0x30, 0x30, 0x02, 0x01, 0x00, 0x30,
    0x16, 0x30, 0x10, 0x31, 0x0e, 0x30, 0x0c, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x05, 0x41, 0x6c,
    0x69, 0x63, 0x65, 0x02, 0x02, 0x01, 0x23, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
    0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x04, 0x11, 0x22, 0x33, 0x44, 0x30, 0x1e, 0x06, 0x09,
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48,
    0x01, 0x65, 0x03, 0x04, 0x01, 0x02, 0x80, 0x04, 0xaa, 0xbb, 0xcc, 0xdd,
};

constexpr unsigned char SignedAndEnveloped[] = {
    0x30, 0x81, 0xb7, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x04, 0xa0, 0x81,
    0xa9, 0x30, 0x81, 0xa6, 0x02, 0x01, 0x01, 0x31, 0x32, 0x30, 0x30, 0x02, 0x01, 0x00, 0x30, 0x16,
    0x30, 0x10, 0x31, 0x0e, 0x30, 0x0c, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x05, 0x43, 0x61, 0x72,
    0x6f, 0x6c, 0x02, 0x02, 0x01, 0x23, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x04, 0x11, 0x22, 0x33, 0x44, 0x31, 0x0d, 0x30, 0x0b, 0x06,
    0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x30, 0x1e, 0x06, 0x09, 0x2a, 0x86,
    0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65,
    0x03, 0x04, 0x01, 0x2a, 0x80, 0x04, 0xaa, 0xbb, 0xcc, 0xdd, 0x31, 0x3e, 0x30, 0x3c, 0x02, 0x01,
    0x01, 0x30, 0x17, 0x30, 0x12, 0x31, 0x10, 0x30, 0x0e, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x07,
    0x53, 0x69, 0x67, 0x6e, 0x65, 0x72, 0x31, 0x02, 0x01, 0x4d, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
    0x0d, 0x01, 0x01, 0x01, 0x04, 0x04, 0xfe, 0xed, 0xfa, 0xce,
};

constexpr unsigned char EncryptedData[] = {0xaa, 0xbb, 0xcc, 0xdd};
constexpr unsigned char BobSerial[] = {0x45, 0x67};
constexpr unsigned char BobKey[] = {0x55, 0x66, 0x77, 0x88, 0x99};
constexpr unsigned char RecipSerial[] = {0x01, 0x23};
constexpr unsigned char RecipKey[] = {0x11, 0x22, 0x33, 0x44};
constexpr unsigned char SignerSerial[] = {0x4d};
constexpr unsigned char SignerDigest[] = {0xfe, 0xed, 0xfa, 0xce};

constexpr unsigned char DataOid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01};

constexpr unsigned char DigestedData[] = {
    0x30, 0x5e, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x05, 0xa0, 0x51, 0x30,
    0x4f, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
    0x01, 0x05, 0x00, 0x30, 0x19, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01,
    0xa0, 0x0c, 0x04, 0x0a, 0x70, 0x6b, 0x37, 0x2d, 0x64, 0x69, 0x67, 0x65, 0x73, 0x74, 0x04, 0x20,
    0xaa, 0xbb, 0xcc, 0xdd, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

constexpr unsigned char DigestedDetached[] = {
    0x30, 0x3e, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x05, 0xa0, 0x31, 0x30,
    0x2f, 0x02, 0x01, 0x00, 0x30, 0x07, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x30, 0x0b, 0x06,
    0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0x04, 0x14, 0xaa, 0xbb, 0xcc, 0xdd,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x10, 0x11, 0x12, 0x13};

constexpr unsigned char DigestedWithData[] = {
    0x30, 0x46, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x05, 0xa0, 0x39, 0x30,
    0x37, 0x02, 0x01, 0x02, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
    0x01, 0x30, 0x13, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x06,
    0x04, 0x04, 0x6e, 0x65, 0x73, 0x74, 0x04, 0x10, 0xaa, 0xbb, 0xcc, 0xdd, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};

std::vector<unsigned char> Wrap(unsigned char tag, std::vector<unsigned char> content) {
    std::vector<unsigned char> result{tag};
    if (content.size() < 0x80) {
        result.push_back(static_cast<unsigned char>(content.size()));
    } else if (content.size() < 0x100) {
        result.push_back(0x81);
        result.push_back(static_cast<unsigned char>(content.size()));
    } else {
        result.push_back(0x82);
        result.push_back(static_cast<unsigned char>(content.size() >> 8));
        result.push_back(static_cast<unsigned char>(content.size()));
    }
    result.insert(result.end(), content.begin(), content.end());
    return result;
}

std::vector<unsigned char> WrapIndef(unsigned char tag, const std::vector<unsigned char>& content) {
    std::vector<unsigned char> result{tag, 0x80};
    result.insert(result.end(), content.begin(), content.end());
    result.push_back(0x00);
    result.push_back(0x00);
    return result;
}

std::vector<unsigned char> ContentInfo(std::vector<unsigned char> oid, std::vector<unsigned char> explicitContent) {
    std::vector<unsigned char> body = Wrap(0x06, std::move(oid));
    if (!explicitContent.empty()) {
        const std::vector<unsigned char> tagged = Wrap(0xa0, std::move(explicitContent));
        body.insert(body.end(), tagged.begin(), tagged.end());
    }
    return Wrap(0x30, std::move(body));
}

std::vector<unsigned char> Pkcs7Oid(unsigned char type) {
    std::vector<unsigned char> oid(std::begin(DataOid), std::end(DataOid));
    oid.back() = type;
    return oid;
}

std::vector<unsigned char> Nested(int levels) {
    std::vector<unsigned char> nested = ContentInfo(Pkcs7Oid(1), Wrap(0x04, {0x01}));
    const std::vector<unsigned char> empty = Wrap(0x31, {});
    for (int level = 0; level < levels; ++level) {
        std::vector<unsigned char> body = Wrap(0x02, {0x01});
        body.insert(body.end(), empty.begin(), empty.end());
        body.insert(body.end(), nested.begin(), nested.end());
        body.insert(body.end(), empty.begin(), empty.end());
        nested = ContentInfo(Pkcs7Oid(2), Wrap(0x30, body));
    }
    return nested;
}

struct DerElement {
    unsigned char tag;
    std::vector<unsigned char> content;
    std::vector<unsigned char> encoded;
};

DerElement ReadElement(const std::vector<unsigned char>& bytes, std::size_t& offset) {
    const std::size_t start = offset;
    Require(offset + 2 <= bytes.size());
    const unsigned char tag = bytes[offset++];
    std::size_t length = bytes[offset++];
    if (length & 0x80) {
        const std::size_t count = length & 0x7f;
        Require(count > 0 && count <= sizeof(std::size_t) && offset + count <= bytes.size());
        length = 0;
        for (std::size_t index = 0; index < count; ++index) length = (length << 8) | bytes[offset++];
    }
    Require(offset + length <= bytes.size());
    const std::size_t begin = offset;
    offset += length;
    return {tag, std::vector<unsigned char>(bytes.begin() + begin, bytes.begin() + offset),
            std::vector<unsigned char>(bytes.begin() + start, bytes.begin() + offset)};
}

std::vector<DerElement> Elements(const std::vector<unsigned char>& bytes) {
    std::size_t offset = 0;
    std::vector<DerElement> result;
    while (offset < bytes.size()) result.push_back(ReadElement(bytes, offset));
    return result;
}

std::vector<unsigned char> Concat(const std::vector<std::vector<unsigned char>>& parts) {
    std::vector<unsigned char> result;
    for (const auto& part : parts) result.insert(result.end(), part.begin(), part.end());
    return result;
}

std::vector<DerElement> EnvelopeFields(const std::vector<unsigned char>& der) {
    std::size_t offset = 0;
    const DerElement info = ReadElement(der, offset);
    const auto parts = Elements(info.content);
    Require(parts.size() == 2 && parts[1].tag == 0xa0);
    const auto inner = Elements(parts[1].content);
    Require(inner.size() == 1 && inner[0].tag == 0x30);
    return Elements(inner[0].content);
}

std::vector<unsigned char> BuildEnvelope(unsigned char type, const std::vector<unsigned char>& version,
    const std::vector<unsigned char>& recipients, const std::vector<unsigned char>& mdAlgs,
    const std::vector<unsigned char>& enc, const std::vector<unsigned char>& certs,
    const std::vector<unsigned char>& crls, const std::vector<unsigned char>& signers) {
    std::vector<unsigned char> body = version;
    body.insert(body.end(), recipients.begin(), recipients.end());
    if (type == 4) body.insert(body.end(), mdAlgs.begin(), mdAlgs.end());
    body.insert(body.end(), enc.begin(), enc.end());
    if (type == 4) {
        if (!certs.empty()) {
            const std::vector<unsigned char> tagged = Wrap(0xa0, certs);
            body.insert(body.end(), tagged.begin(), tagged.end());
        }
        if (!crls.empty()) {
            const std::vector<unsigned char> tagged = Wrap(0xa1, crls);
            body.insert(body.end(), tagged.begin(), tagged.end());
        }
    }
    body.insert(body.end(), signers.begin(), signers.end());
    return ContentInfo(Pkcs7Oid(type), Wrap(0x30, body));
}

std::vector<unsigned char> EncWithoutData(const DerElement& enc) {
    const auto parts = Elements(enc.content);
    Require(parts.size() == 3 && parts[2].tag == 0x80);
    return Wrap(0x30, Concat({parts[0].encoded, parts[1].encoded}));
}

LibreSsl3Pkcs7* Decode(const std::vector<unsigned char>& der, LibreSsl3Pkcs7** output = nullptr) {
    const unsigned char* cursor = der.data();
    LibreSsl3Pkcs7* pkcs7 = d2i_PKCS7_nid_postfix(output, &cursor, static_cast<std::int64_t>(der.size()));
    if (pkcs7) Require(cursor == der.data() + der.size());
    return pkcs7;
}

int LastReason() {
    return static_cast<int>(ERR_peek_last_error_nid_postfix() & 0xfff);
}

bool IsPkcs7Type(const Asn1Object_nid_no_patch* object, unsigned char type) {
    return object && object->length == 9 && std::memcmp(object->data, DataOid, 8) == 0 && object->data[8] == type;
}

template <typename T>
T* At(const LibreSsl3Stack* stack, int index) {
    Require(stack != nullptr && index < stack->num);
    return reinterpret_cast<T*>(stack->data[index]);
}

std::vector<std::uint64_t> DrainErrors() {
    std::vector<std::uint64_t> errors;
    for (std::uint64_t code = ERR_get_error_nid_postfix(); code != 0; code = ERR_get_error_nid_postfix()) errors.push_back(code);
    return errors;
}

LibreSsl3Pkcs7* DecodeClean(const std::vector<unsigned char>& der, std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    LibreSsl3Pkcs7* pkcs7 = Decode(der);
    Require(pkcs7 != nullptr, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
    return pkcs7;
}

void RequireRefused(const std::vector<unsigned char>& der, unsigned reason, std::source_location location = std::source_location::current()) {
    const unsigned char* cursor = der.data();
    ERR_clear_error_nid_postfix();
    Require(d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size())) == nullptr, location);
    Require(cursor == der.data(), location);
    Require(ERR_get_error_nid_postfix() == (0x0dfff000u | reason), location);
    ERR_clear_error_nid_postfix();
    Require(allocations.empty(), location);
}

void TestSignedMessage() {
    const std::vector<unsigned char> der(std::begin(SignedMessage), std::end(SignedMessage));
    LibreSsl3Pkcs7* pkcs7 = Decode(der);
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 2));
    Require(pkcs7->asn1 == nullptr && pkcs7->length == 0 && pkcs7->detached == 0);
    const auto* data = static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d);
    Require(data != nullptr && data->version->length == 1 && data->version->data[0] == 1);
    Require(data->mdAlgs->num == 1 && data->crl == nullptr);
    Require(IsPkcs7Type(data->contents->type, 1));
    const auto* content = static_cast<const Asn1String*>(data->contents->d);
    Require(content->type == 4 && content->length == 5 && std::memcmp(content->data, "hello", 5) == 0);
    Require(data->cert->num == 1);
    const auto* certificate = At<LibreSsl3X509>(data->cert, 0);
    Require(certificate->references == 1 && certificate->certInfo != nullptr);
    Require(data->signerInfo->num == 1);
    const auto* signer = At<LibreSsl3Pkcs7SignerInfo>(data->signerInfo, 0);
    Require(signer->version->data[0] == 1 && signer->unauthAttr == nullptr && signer->pkey == nullptr);
    Require(signer->issuerAndSerial->issuer != nullptr && signer->issuerAndSerial->serial->length == 20);
    Require(signer->issuerAndSerial->issuer->canonEnc != nullptr);
    Require(signer->digestAlg != nullptr && signer->digestEncAlg != nullptr && signer->encDigest->length == 71);
    Require(signer->authAttr->num == 4);
    const int expectedTypes[] = {6, 23, 4, 16};
    for (int index = 0; index < 4; ++index) {
        const auto* attribute = At<LibreSsl3X509Attribute>(signer->authAttr, index);
        Require(attribute->object != nullptr && attribute->set->num == 1);
        Require(At<LibreSsl3Asn1Type>(attribute->set, 0)->type == expectedTypes[index]);
    }
    const auto* capabilities = static_cast<const Asn1String*>(At<LibreSsl3Asn1Type>(At<LibreSsl3X509Attribute>(signer->authAttr, 3)->set, 0)->value);
    Require(capabilities->length == 108 && capabilities->data[0] == 0x30);
    const auto* key = X509_get0_pubkey_nid_postfix(reinterpret_cast<const LibreSsl3Certificate*>(certificate));
    Require(key != nullptr && X509_get0_pubkey_nid_postfix(reinterpret_cast<const LibreSsl3Certificate*>(certificate)) == key);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestCertificatesAndCrl() {
    const std::vector<unsigned char> der(std::begin(CertificateAndCrl), std::end(CertificateAndCrl));
    LibreSsl3Pkcs7* pkcs7 = Decode(der);
    Require(pkcs7 != nullptr);
    const auto* data = static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d);
    Require(data->mdAlgs->num == 0 && data->cert->num == 1 && data->crl->num == 1 && data->signerInfo->num == 0);
    Require(IsPkcs7Type(data->contents->type, 1) && data->contents->d == nullptr);
    Require(At<LibreSsl3X509Crl>(data->crl, 0)->references == 1);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestDigestedContent() {
    const std::vector<unsigned char> data(std::begin(DigestedData), std::end(DigestedData));
    LibreSsl3Pkcs7* pkcs7 = DecodeClean(data);
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 5));
    const auto* digest = static_cast<const LibreSsl3Pkcs7Digest*>(pkcs7->d);
    Require(digest != nullptr && digest->version->length == 1 && digest->version->data[0] == 0);
    Require(digest->md != nullptr);
    Require(OBJ_obj2nid_nid_postfix(static_cast<const LibreSslAsn1Object*>(digest->md->algorithm)) == 672);
    Require(digest->md->parameter != nullptr && digest->md->parameter->type == 5 && digest->md->parameter->value == nullptr);
    Require(IsPkcs7Type(digest->contents->type, 1));
    const auto* content = static_cast<const Asn1String*>(digest->contents->d);
    Require(content != nullptr && content->type == 4 && content->length == 10);
    Require(std::memcmp(content->data, "pk7-digest", 10) == 0);
    Require(digest->digest != nullptr && digest->digest->type == 4 && digest->digest->length == 32);
    const unsigned char expected[] = {
        0xaa, 0xbb, 0xcc, 0xdd, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
    Require(std::memcmp(digest->digest->data, expected, 32) == 0);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    const std::vector<unsigned char> detached(std::begin(DigestedDetached), std::end(DigestedDetached));
    pkcs7 = DecodeClean(detached);
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 5));
    digest = static_cast<const LibreSsl3Pkcs7Digest*>(pkcs7->d);
    Require(digest != nullptr && digest->version->data[0] == 0);
    Require(OBJ_obj2nid_nid_postfix(static_cast<const LibreSslAsn1Object*>(digest->md->algorithm)) == 64);
    Require(digest->md->parameter == nullptr);
    Require(IsPkcs7Type(digest->contents->type, 1) && digest->contents->d == nullptr);
    Require(digest->digest->length == 20 && digest->digest->data[0] == 0xaa && digest->digest->data[19] == 0x13);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    const std::vector<unsigned char> withData(std::begin(DigestedWithData), std::end(DigestedWithData));
    pkcs7 = DecodeClean(withData);
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 5));
    digest = static_cast<const LibreSsl3Pkcs7Digest*>(pkcs7->d);
    Require(digest != nullptr && digest->version->data[0] == 2);
    Require(OBJ_obj2nid_nid_postfix(static_cast<const LibreSslAsn1Object*>(digest->md->algorithm)) == 672);
    Require(digest->md->parameter == nullptr);
    content = static_cast<const Asn1String*>(digest->contents->d);
    Require(content != nullptr && content->length == 4 && std::memcmp(content->data, "nest", 4) == 0);
    Require(digest->digest->length == 16 && digest->digest->data[0] == 0xaa && digest->digest->data[15] == 0x0c);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestDigestedErrors() {
    const std::vector<unsigned char> der(std::begin(DigestedData), std::end(DigestedData));
    std::vector<unsigned char> truncated(der.begin(), der.end() - 1);
    const unsigned char* cursor = truncated.data();
    ERR_clear_error_nid_postfix();
    Require(d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(truncated.size())) == nullptr);
    Require(cursor == truncated.data() && LastReason() == 155);
    ERR_clear_error_nid_postfix();
    Require(allocations.empty());

    std::vector<unsigned char> wrongVersion = der;
    wrongVersion[17] = 0x03;
    RequireRefused(wrongVersion, 168);

    std::vector<unsigned char> wrongAlgorithm = der;
    wrongAlgorithm[20] = 0x31;
    RequireRefused(wrongAlgorithm, 168);

    std::vector<unsigned char> wrongContent = der;
    wrongContent[13] = 0xa1;
    RequireRefused(wrongContent, 168);

    std::vector<unsigned char> wrongDigest = der;
    wrongDigest[62] = 0x02;
    RequireRefused(wrongDigest, 168);

    std::vector<unsigned char> longDigest = der;
    longDigest[63] = 0x21;
    RequireRefused(longDigest, 155);
}

std::vector<unsigned char> DigestedNested(int levels) {
    std::vector<unsigned char> nested = ContentInfo(Pkcs7Oid(1), Wrap(0x04, {0x01}));
    const std::vector<unsigned char> algorithm = Wrap(0x30, Wrap(0x06, {0x2b, 0x0e, 0x03, 0x02, 0x1a}));
    for (int level = 0; level < levels; ++level) {
        std::vector<unsigned char> body = Wrap(0x02, {0x00});
        body.insert(body.end(), algorithm.begin(), algorithm.end());
        body.insert(body.end(), nested.begin(), nested.end());
        const std::vector<unsigned char> digest = Wrap(0x04, {0x01});
        body.insert(body.end(), digest.begin(), digest.end());
        nested = ContentInfo(Pkcs7Oid(5), Wrap(0x30, body));
    }
    return nested;
}

void TestDigestedNesting() {
    LibreSsl3Pkcs7* pkcs7 = DecodeClean(DigestedNested(14));
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 5));
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
    RequireRefused(DigestedNested(15), 219);
}

void TestDigestedOutOfMemory(const std::vector<unsigned char>& der) {
    for (std::size_t fail = 0; fail != 10000; ++fail) {
        ERR_clear_error_nid_postfix();
        allocationsUntilFailure = fail;
        const unsigned char* cursor = der.data();
        LibreSsl3Pkcs7* pkcs7 = d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
        allocationsUntilFailure = NoFailure;
        if (pkcs7) {
            Require(cursor == der.data() + der.size());
            PKCS7_free_nid_postfix(pkcs7);
            Require(allocations.empty());
            ERR_clear_error_nid_postfix();
            return;
        }
        Require(cursor == der.data());
        Require(allocations.empty());
    }
    Require(false);
}

void TestDigestedAllocationFailures() {
    TestDigestedOutOfMemory(std::vector<unsigned char>(std::begin(DigestedData), std::end(DigestedData)));
    TestDigestedOutOfMemory(std::vector<unsigned char>(std::begin(DigestedDetached), std::end(DigestedDetached)));
    TestDigestedOutOfMemory(std::vector<unsigned char>(std::begin(DigestedWithData), std::end(DigestedWithData)));
}

bool IsObject(const Asn1Object_nid_no_patch* object, const unsigned char* oid, std::size_t length) {
    return object && object->length == static_cast<int>(length) && std::memcmp(object->data, oid, length) == 0;
}

void TestEncryptedWithData() {
    const std::vector<unsigned char> der(std::begin(EncryptedWithData), std::end(EncryptedWithData));
    LibreSsl3Pkcs7* pkcs7 = Decode(der);
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 6));
    const auto* encrypted = static_cast<const LibreSsl3Pkcs7Encrypt*>(pkcs7->d);
    Require(encrypted != nullptr && encrypted->version->length == 1 && encrypted->version->data[0] == 0);
    const auto* content = encrypted->encData;
    Require(content != nullptr && content->cipher == nullptr);
    Require(IsObject(content->contentType, DataOid, sizeof(DataOid)));
    Require(IsObject(static_cast<const Asn1Object_nid_no_patch*>(content->algorithm->algorithm), Aes128CbcOid, sizeof(Aes128CbcOid)));
    Require(content->algorithm->parameter != nullptr && content->algorithm->parameter->type == 4);
    const auto* iv = static_cast<const Asn1String*>(content->algorithm->parameter->value);
    Require(iv->length == 16 && iv->data[0] == 0x01 && iv->data[15] == 0x10);
    const unsigned char expected[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    Require(content->encData != nullptr && content->encData->type == 4 && content->encData->length == 16);
    Require(std::memcmp(content->encData->data, expected, sizeof(expected)) == 0);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestEncryptedWithoutData() {
    const std::vector<unsigned char> der(std::begin(EncryptedWithoutData), std::end(EncryptedWithoutData));
    LibreSsl3Pkcs7* pkcs7 = Decode(der);
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 6));
    const auto* encrypted = static_cast<const LibreSsl3Pkcs7Encrypt*>(pkcs7->d);
    Require(encrypted != nullptr && encrypted->version->length == 1 && encrypted->version->data[0] == 0);
    const auto* content = encrypted->encData;
    Require(content != nullptr && content->cipher == nullptr && content->encData == nullptr);
    Require(IsObject(content->contentType, DataOid, sizeof(DataOid)));
    Require(IsObject(static_cast<const Asn1Object_nid_no_patch*>(content->algorithm->algorithm), Aes256CbcOid, sizeof(Aes256CbcOid)));
    Require(content->algorithm->parameter == nullptr);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestEncryptedErrors() {
    std::vector<unsigned char> truncated(std::begin(EncryptedWithData), std::end(EncryptedWithData) - 1);
    const unsigned char* cursor = truncated.data();
    ERR_clear_error_nid_postfix();
    Require(d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(truncated.size())) == nullptr);
    Require(cursor == truncated.data() && LastReason() == 155);
    Require(allocations.empty());

    Require(Decode(ContentInfo(Pkcs7Oid(6), Wrap(0x31, {}))) == nullptr && LastReason() == 168);
    Require(allocations.empty());

    std::vector<unsigned char> encryptedBody = Wrap(0x02, {0x00});
    const std::vector<unsigned char> encContent = Wrap(0x30, Wrap(0x06, std::vector<unsigned char>(std::begin(DataOid), std::end(DataOid))));
    encryptedBody.insert(encryptedBody.end(), encContent.begin(), encContent.end());
    const std::vector<unsigned char> noAlgorithm = ContentInfo(Pkcs7Oid(6), Wrap(0x30, encryptedBody));
    Require(Decode(noAlgorithm) == nullptr && LastReason() == 102);
    Require(allocations.empty());

    std::vector<unsigned char> constructed(std::begin(EncryptedWithData), std::end(EncryptedWithData));
    constructed[64] = 0xa0;
    RequireRefused(constructed, 102);
}

void TestAllocationFailures() {
    const std::vector<std::vector<unsigned char>> sources{
        std::vector<unsigned char>(std::begin(EncryptedWithData), std::end(EncryptedWithData)),
        std::vector<unsigned char>(std::begin(EncryptedWithoutData), std::end(EncryptedWithoutData))};
    for (const auto& der : sources) {
        for (int failure = 0;; ++failure) {
            allocationsUntilFailure = failure;
            const unsigned char* cursor = der.data();
            ERR_clear_error_nid_postfix();
            LibreSsl3Pkcs7* pkcs7 = d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
            if (pkcs7) {
                allocationsUntilFailure = NoFailure;
                Require(cursor == der.data() + der.size());
                PKCS7_free_nid_postfix(pkcs7);
                Require(allocations.empty());
                break;
            }
            Require(allocationsUntilFailure == 0);
            Require(cursor == der.data());
            Require(allocations.empty());
            Require(failure < 64);
        }
    }
}

void RequireOctets(const Asn1String* value, const unsigned char* expected, int length) {
    Require(value != nullptr && value->type == 4 && value->length == length);
    Require(std::memcmp(value->data, expected, static_cast<std::size_t>(length)) == 0);
}

void RequireInteger(const Asn1String* value, const unsigned char* expected, int length) {
    Require(value != nullptr && value->type == 2 && value->length == length);
    Require(std::memcmp(value->data, expected, static_cast<std::size_t>(length)) == 0);
}

void RequireEncContent(const LibreSsl3Pkcs7EncContent* enc) {
    Require(enc != nullptr && IsPkcs7Type(enc->contentType, 1));
    Require(enc->algorithm != nullptr && enc->cipher == nullptr);
    RequireOctets(enc->encData, EncryptedData, 4);
}

void TestEnveloped() {
    const std::vector<unsigned char> der(std::begin(Enveloped), std::end(Enveloped));
    LibreSsl3Pkcs7* pkcs7 = Decode(der);
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 3));
    const auto* data = static_cast<const LibreSsl3Pkcs7Envelope*>(pkcs7->d);
    Require(data->version->length == 1 && data->version->data[0] == 0);
    Require(data->recipientInfo->num == 2);
    const auto* bob = At<LibreSsl3Pkcs7RecipInfo>(data->recipientInfo, 0);
    Require(bob->version->length == 1 && bob->version->data[0] == 0 && bob->cert == nullptr);
    Require(bob->issuerAndSerial->issuer != nullptr);
    RequireInteger(bob->issuerAndSerial->serial, BobSerial, 2);
    Require(bob->keyEncAlgor != nullptr);
    RequireOctets(bob->encKey, BobKey, 5);
    const auto* alice = At<LibreSsl3Pkcs7RecipInfo>(data->recipientInfo, 1);
    Require(alice->version->data[0] == 0 && alice->cert == nullptr);
    RequireInteger(alice->issuerAndSerial->serial, RecipSerial, 2);
    RequireOctets(alice->encKey, RecipKey, 4);
    RequireEncContent(data->encData);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestSignedAndEnveloped() {
    const std::vector<unsigned char> der(std::begin(SignedAndEnveloped), std::end(SignedAndEnveloped));
    LibreSsl3Pkcs7* pkcs7 = Decode(der);
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 4));
    const auto* data = static_cast<const LibreSsl3Pkcs7SignEnvelope*>(pkcs7->d);
    Require(data->version->length == 1 && data->version->data[0] == 1);
    Require(data->recipientInfo->num == 1 && data->mdAlgs->num == 1);
    Require(data->cert == nullptr && data->crl == nullptr && data->signerInfo->num == 1);
    const auto* carol = At<LibreSsl3Pkcs7RecipInfo>(data->recipientInfo, 0);
    Require(carol->version->data[0] == 0 && carol->cert == nullptr);
    RequireInteger(carol->issuerAndSerial->serial, RecipSerial, 2);
    RequireOctets(carol->encKey, RecipKey, 4);
    const auto* signer = At<LibreSsl3Pkcs7SignerInfo>(data->signerInfo, 0);
    Require(signer->version->data[0] == 1 && signer->authAttr == nullptr && signer->unauthAttr == nullptr);
    Require(signer->pkey == nullptr && signer->digestAlg != nullptr && signer->digestEncAlg != nullptr);
    RequireInteger(signer->issuerAndSerial->serial, SignerSerial, 1);
    RequireOctets(signer->encDigest, SignerDigest, 4);
    RequireEncContent(data->encData);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestEnvelopedErrors() {
    std::vector<unsigned char> constructed(std::begin(Enveloped), std::end(Enveloped));
    constructed[constructed.size() - 6] = 0xa0;
    RequireRefused(constructed, 102);

    const std::vector<unsigned char> signedMessage(std::begin(SignedAndEnveloped), std::end(SignedAndEnveloped));
    const auto fields = EnvelopeFields(signedMessage);
    const auto encFields = Elements(fields[3].content);
    std::vector<unsigned char> constructedData = encFields[2].encoded;
    constructedData[0] = 0xa0;
    const auto signedConstructed = BuildEnvelope(4, fields[0].encoded, fields[1].encoded, fields[2].encoded,
        Wrap(0x30, Concat({encFields[0].encoded, encFields[1].encoded, constructedData})), {}, {}, fields[4].encoded);
    RequireRefused(signedConstructed, 102);

    const std::vector<unsigned char> der(std::begin(Enveloped), std::end(Enveloped));
    std::vector<unsigned char> truncated(der.begin(), der.end() - 1);
    const unsigned char* cursor = truncated.data();
    Require(d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(truncated.size())) == nullptr);
    Require(allocations.empty());

    Require(Decode(ContentInfo(Pkcs7Oid(3), Wrap(0x30, {}))) == nullptr);
    Require(Decode(ContentInfo(Pkcs7Oid(4), Wrap(0x30, {}))) == nullptr);
    Require(allocations.empty());
}

void TestOmittedEncData() {
    const std::vector<unsigned char> envelope(std::begin(Enveloped), std::end(Enveloped));
    auto fields = EnvelopeFields(envelope);
    Require(fields.size() == 3 && fields[2].tag == 0x30);
    LibreSsl3Pkcs7* pkcs7 = Decode(BuildEnvelope(3, fields[0].encoded, fields[1].encoded, {},
        EncWithoutData(fields[2]), {}, {}, {}));
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 3));
    const auto* data = static_cast<const LibreSsl3Pkcs7Envelope*>(pkcs7->d);
    Require(data->encData != nullptr && data->encData->contentType != nullptr && data->encData->algorithm != nullptr);
    Require(data->encData->encData == nullptr && data->encData->cipher == nullptr);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    const std::vector<unsigned char> signedEnvelope(std::begin(SignedAndEnveloped), std::end(SignedAndEnveloped));
    fields = EnvelopeFields(signedEnvelope);
    Require(fields.size() == 5 && fields[3].tag == 0x30);
    pkcs7 = Decode(BuildEnvelope(4, fields[0].encoded, fields[1].encoded, fields[2].encoded,
        EncWithoutData(fields[3]), {}, {}, fields[4].encoded));
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 4));
    const auto* signedData = static_cast<const LibreSsl3Pkcs7SignEnvelope*>(pkcs7->d);
    Require(signedData->cert == nullptr && signedData->crl == nullptr);
    Require(signedData->encData != nullptr && signedData->encData->contentType != nullptr);
    Require(signedData->encData->encData == nullptr && signedData->encData->cipher == nullptr);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestCertAndCrlInEnvelope() {
    const std::vector<unsigned char> message(std::begin(SignedAndEnveloped), std::end(SignedAndEnveloped));
    const auto fields = EnvelopeFields(message);
    Require(fields.size() == 5);
    const std::vector<unsigned char> certificates(std::begin(CertificateAndCrl), std::end(CertificateAndCrl));
    const auto sources = EnvelopeFields(certificates);
    Require(sources.size() == 6 && sources[3].tag == 0xa0 && sources[4].tag == 0xa1);
    const auto certs = Elements(sources[3].content);
    const auto crls = Elements(sources[4].content);
    Require(!certs.empty() && !crls.empty());
    const std::vector<unsigned char> der = BuildEnvelope(4, fields[0].encoded, fields[1].encoded, fields[2].encoded,
        fields[3].encoded, Concat({certs[0].encoded}), Concat({crls[0].encoded}), fields[4].encoded);
    LibreSsl3Pkcs7* pkcs7 = Decode(der);
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 4));
    const auto* data = static_cast<const LibreSsl3Pkcs7SignEnvelope*>(pkcs7->d);
    Require(data->cert != nullptr && data->cert->num == 1 && data->crl != nullptr && data->crl->num == 1);
    Require(At<LibreSsl3X509Crl>(data->crl, 0)->references == 1);
    RequireEncContent(data->encData);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestEnvelopeTrailingAndTags() {
    std::vector<unsigned char> trailing(std::begin(Enveloped), std::end(Enveloped));
    trailing.push_back(0x00);
    const unsigned char* cursor = trailing.data();
    LibreSsl3Pkcs7* pkcs7 = d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(trailing.size()));
    Require(pkcs7 != nullptr && cursor == trailing.data() + trailing.size() - 1);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    const std::vector<unsigned char> envelope(std::begin(Enveloped), std::end(Enveloped));
    const auto fields = EnvelopeFields(envelope);
    std::vector<unsigned char> badRecipients = fields[1].encoded;
    badRecipients[0] = 0x30;
    Require(Decode(BuildEnvelope(3, fields[0].encoded, badRecipients, {}, fields[2].encoded, {}, {}, {})) == nullptr);
    Require(allocations.empty());

    const auto parts = Elements(fields[2].content);
    std::vector<unsigned char> badData = parts[2].encoded;
    badData[0] = 0x81;
    const std::vector<unsigned char> badEnc = Wrap(0x30, Concat({parts[0].encoded, parts[1].encoded, badData}));
    Require(Decode(BuildEnvelope(3, fields[0].encoded, fields[1].encoded, {}, badEnc, {}, {}, {})) == nullptr);
    Require(allocations.empty());
}

void TestEnvelopeAllocationFailures() {
    const std::vector<unsigned char> message(std::begin(SignedAndEnveloped), std::end(SignedAndEnveloped));
    const auto fields = EnvelopeFields(message);
    const std::vector<unsigned char> certificates(std::begin(CertificateAndCrl), std::end(CertificateAndCrl));
    const auto sources = EnvelopeFields(certificates);
    const auto certs = Elements(sources[3].content);
    const auto crls = Elements(sources[4].content);
    const std::vector<unsigned char> der = BuildEnvelope(4, fields[0].encoded, fields[1].encoded, fields[2].encoded,
        fields[3].encoded, Concat({certs[0].encoded}), Concat({crls[0].encoded}), fields[4].encoded);
    bool completed = false;
    for (int budget = 0; budget < 250; ++budget) {
        const unsigned char* cursor = der.data();
        allocationsUntilFailure = budget;
        LibreSsl3Pkcs7* pkcs7 = d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
        allocationsUntilFailure = NoFailure;
        if (pkcs7) {
            PKCS7_free_nid_postfix(pkcs7);
            completed = true;
            break;
        }
        Require(allocations.empty());
        ERR_clear_error_nid_postfix();
    }
    Require(completed);
    Require(allocations.empty());
}

void TestDataAndOtherTypes() {
    LibreSsl3Pkcs7* pkcs7 = Decode(ContentInfo(Pkcs7Oid(1), Wrap(0x04, {'a', 'b'})));
    Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 1));
    Require(static_cast<Asn1String*>(pkcs7->d)->length == 2);
    PKCS7_free_nid_postfix(pkcs7);

    pkcs7 = Decode(ContentInfo(Pkcs7Oid(5), {}));
    Require(pkcs7 != nullptr && pkcs7->d == nullptr);
    PKCS7_free_nid_postfix(pkcs7);

    pkcs7 = Decode(ContentInfo({0x2a, 0x03}, Wrap(0x01, {0xff})));
    Require(pkcs7 != nullptr);
    const auto* boolean = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    Require(boolean->type == 1 && *reinterpret_cast<const int*>(&boolean->value) == 0xff);
    PKCS7_free_nid_postfix(pkcs7);

    const std::vector<unsigned char> sequence = Wrap(0x30, Wrap(0x02, {0x05}));
    pkcs7 = Decode(ContentInfo({0x2a, 0x03}, sequence));
    const auto* any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    const auto* encoded = static_cast<const Asn1String*>(any->value);
    Require(any->type == 16 && encoded->length == 5 && std::memcmp(encoded->data, sequence.data(), 5) == 0);
    PKCS7_free_nid_postfix(pkcs7);

    pkcs7 = Decode(ContentInfo({0x2a, 0x03}, Wrap(0x0a, {0xff})));
    any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    Require(any->type == 10 && static_cast<const Asn1String*>(any->value)->type == (10 | 0x100));
    PKCS7_free_nid_postfix(pkcs7);

    pkcs7 = Decode(ContentInfo({0x2a, 0x03}, Wrap(0x81, {0x07})));
    any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    Require(any->type == -3 && static_cast<const Asn1String*>(any->value)->length == 3);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

void TestOutputAndErrors() {
    const std::vector<unsigned char> der(std::begin(SignedMessage), std::end(SignedMessage));
    LibreSsl3Pkcs7* output = Decode(ContentInfo(Pkcs7Oid(1), {}));
    LibreSsl3Pkcs7* replaced = Decode(der, &output);
    Require(replaced != nullptr && output == replaced);

    std::vector<unsigned char> truncated(der.begin(), der.end() - 1);
    const unsigned char* cursor = truncated.data();
    Require(d2i_PKCS7_nid_postfix(&output, &cursor, static_cast<std::int64_t>(truncated.size())) == nullptr);
    Require(output == nullptr && cursor == truncated.data() && LastReason() == 155);
    Require(allocations.empty());

    std::vector<unsigned char> trailing = der;
    trailing.push_back(0);
    cursor = trailing.data();
    LibreSsl3Pkcs7* pkcs7 = d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(trailing.size()));
    Require(pkcs7 != nullptr && cursor == trailing.data() + der.size());
    PKCS7_free_nid_postfix(pkcs7);

    output = Decode(ContentInfo(Pkcs7Oid(1), {}));
    cursor = der.data();
    Require(d2i_PKCS7_nid_postfix(&output, &cursor, -1) == nullptr && output != nullptr && cursor == der.data());
    PKCS7_free_nid_postfix(output);

    Require(Decode(Wrap(0x31, Wrap(0x06, Pkcs7Oid(1)))) == nullptr && LastReason() == 168);
    Require(Decode(ContentInfo(Pkcs7Oid(1), Wrap(0x02, {0x01}))) == nullptr && LastReason() == 168);
    Require(Decode(ContentInfo({0x2a, 0x83}, {})) == nullptr && LastReason() == 216);
    Require(Decode(ContentInfo({0x2a, 0x03}, Wrap(0x01, {0x01, 0x02}))) == nullptr && LastReason() == 106);
    Require(allocations.empty());

    LibreSsl3Pkcs7* deepest = Decode(Nested(14));
    Require(deepest != nullptr);
    PKCS7_free_nid_postfix(deepest);
    const std::vector<unsigned char> nested = Nested(15);
    Require(Decode(nested) == nullptr && LastReason() == 219);
    Require(allocations.empty());
}

std::vector<unsigned char> WrapLong(unsigned char tag, const std::vector<unsigned char>& content, std::size_t lengthBytes) {
    std::vector<unsigned char> result{tag, static_cast<unsigned char>(0x80 | lengthBytes)};
    for (std::size_t index = lengthBytes; index != 0; --index) {
        const std::size_t shift = (index - 1) * 8;
        result.push_back(shift < sizeof(std::size_t) * 8 ? static_cast<unsigned char>(content.size() >> shift) : 0);
    }
    result.insert(result.end(), content.begin(), content.end());
    return result;
}

std::vector<unsigned char> Content(const std::vector<unsigned char>& der) {
    std::size_t header = 2;
    if (der[1] & 0x80) header += der[1] & 0x7f;
    return std::vector<unsigned char>(der.data() + header, der.data() + der.size());
}

std::vector<unsigned char> Joined(std::vector<unsigned char> first, const std::vector<unsigned char>& second) {
    first.insert(first.end(), second.begin(), second.end());
    return first;
}

void RequireData(const LibreSsl3Pkcs7* pkcs7, std::source_location location = std::source_location::current()) {
    Require(IsPkcs7Type(pkcs7->type, 1) && pkcs7->d != nullptr, location);
    const auto* content = static_cast<const Asn1String*>(pkcs7->d);
    Require(content->type == 4 && content->length == 2 && std::memcmp(content->data, "ab", 2) == 0, location);
}

std::vector<unsigned char> SignedData(std::size_t lengthBytes) {
    const auto encode = [lengthBytes](unsigned char tag, const std::vector<unsigned char>& content) {
        return lengthBytes ? WrapLong(tag, content, lengthBytes) : Wrap(tag, content);
    };
    const std::vector<unsigned char> empty = encode(0x31, {});
    const std::vector<unsigned char> inner =
        encode(0x30, Joined(encode(0x06, Pkcs7Oid(1)), encode(0xa0, encode(0x04, {0x07}))));
    const std::vector<unsigned char> body = Joined(Joined(Joined(encode(0x02, {0x01}), empty), inner), empty);
    return encode(0x30, Joined(encode(0x06, Pkcs7Oid(2)), encode(0xa0, encode(0x30, body))));
}

void TestLongFormLengths() {
    const std::size_t forms[] = {1, 2, 3, 8, 9, 126};
    const std::vector<unsigned char> octets = Wrap(0x04, {'a', 'b'});
    const std::vector<unsigned char> reference = ContentInfo(Pkcs7Oid(1), octets);
    const std::vector<unsigned char> body = Content(reference);
    Require(Wrap(0x30, body) == reference && body.size() < 0x80);
    LibreSsl3Pkcs7* pkcs7 = DecodeClean(reference);
    RequireData(pkcs7);
    PKCS7_free_nid_postfix(pkcs7);

    for (const std::size_t form : forms) {
        pkcs7 = DecodeClean(WrapLong(0x30, body, form));
        RequireData(pkcs7);
        PKCS7_free_nid_postfix(pkcs7);

        const std::vector<unsigned char> inner = Joined(WrapLong(0x06, Pkcs7Oid(1), form),
            WrapLong(0xa0, WrapLong(0x04, {'a', 'b'}, form), form));
        pkcs7 = DecodeClean(WrapLong(0x30, inner, form));
        RequireData(pkcs7);
        PKCS7_free_nid_postfix(pkcs7);
        Require(allocations.empty());
    }

    pkcs7 = DecodeClean(Wrap(0x30, Joined(WrapLong(0x06, Pkcs7Oid(1), 1), Wrap(0xa0, octets))));
    RequireData(pkcs7);
    PKCS7_free_nid_postfix(pkcs7);
    pkcs7 = DecodeClean(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), WrapLong(0xa0, octets, 2))));
    RequireData(pkcs7);
    PKCS7_free_nid_postfix(pkcs7);
    pkcs7 = DecodeClean(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), Wrap(0xa0, WrapLong(0x04, {'a', 'b'}, 3)))));
    RequireData(pkcs7);
    PKCS7_free_nid_postfix(pkcs7);

    pkcs7 = DecodeClean({0x30, 0x81, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01});
    Require(IsPkcs7Type(pkcs7->type, 1) && pkcs7->d == nullptr);
    PKCS7_free_nid_postfix(pkcs7);

    std::vector<unsigned char> trailing = WrapLong(0x30, body, 2);
    trailing.push_back(0x5a);
    const unsigned char* cursor = trailing.data();
    ERR_clear_error_nid_postfix();
    pkcs7 = d2i_PKCS7_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(trailing.size()));
    Require(pkcs7 != nullptr && cursor == trailing.data() + trailing.size() - 1);
    Require(ERR_get_error_nid_postfix() == 0);
    RequireData(pkcs7);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    for (const std::size_t form : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{9}, std::size_t{126}}) {
        pkcs7 = DecodeClean(SignedData(form));
        Require(IsPkcs7Type(pkcs7->type, 2));
        const auto* data = static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d);
        Require(data != nullptr && data->version->length == 1 && data->version->data[0] == 1);
        Require(data->mdAlgs->num == 0 && data->signerInfo->num == 0 && data->cert == nullptr && data->crl == nullptr);
        Require(IsPkcs7Type(data->contents->type, 1));
        const auto* content = static_cast<const Asn1String*>(data->contents->d);
        Require(content != nullptr && content->type == 4 && content->length == 1 && content->data[0] == 0x07);
        PKCS7_free_nid_postfix(pkcs7);
        Require(allocations.empty());
    }

    const std::vector<unsigned char> message(std::begin(SignedMessage), std::end(SignedMessage));
    ERR_clear_error_nid_postfix();
    pkcs7 = Decode(message);
    Require(pkcs7 != nullptr);
    const std::vector<std::uint64_t> messageErrors = DrainErrors();
    PKCS7_free_nid_postfix(pkcs7);
    for (const std::size_t form : {std::size_t{3}, std::size_t{8}, std::size_t{9}, std::size_t{126}}) {
        pkcs7 = Decode(WrapLong(0x30, Content(message), form));
        Require(pkcs7 != nullptr && IsPkcs7Type(pkcs7->type, 2));
        Require(DrainErrors() == messageErrors);
        const auto* data = static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d);
        Require(data->mdAlgs->num == 1 && data->cert->num == 1 && data->signerInfo->num == 1);
        const auto* content = static_cast<const Asn1String*>(data->contents->d);
        Require(content->type == 4 && content->length == 5 && std::memcmp(content->data, "hello", 5) == 0);
        const auto* signer = At<LibreSsl3Pkcs7SignerInfo>(data->signerInfo, 0);
        Require(signer->authAttr->num == 4 && signer->encDigest->length == 71);
        PKCS7_free_nid_postfix(pkcs7);
        Require(allocations.empty());
    }

    const std::vector<unsigned char> sequence = WrapLong(0x30, Wrap(0x02, {0x05}), 2);
    pkcs7 = DecodeClean(ContentInfo({0x2a, 0x03}, sequence));
    const auto* any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    const auto* encoded = static_cast<const Asn1String*>(any->value);
    Require(any->type == 16 && encoded->length == 7 && std::memcmp(encoded->data, sequence.data(), 7) == 0);
    PKCS7_free_nid_postfix(pkcs7);

    pkcs7 = DecodeClean(ContentInfo({0x2a, 0x03}, WrapLong(0x01, {0xff}, 1)));
    any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    Require(any->type == 1 && *reinterpret_cast<const int*>(&any->value) == 0xff);
    PKCS7_free_nid_postfix(pkcs7);

    pkcs7 = DecodeClean(ContentInfo({0x2a, 0x03}, WrapLong(0x02, {0x05}, 2)));
    any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    encoded = static_cast<const Asn1String*>(any->value);
    Require(any->type == 2 && encoded->type == 2 && encoded->length == 1 && encoded->data[0] == 0x05);
    PKCS7_free_nid_postfix(pkcs7);

    pkcs7 = DecodeClean(ContentInfo({0x2a, 0x03}, WrapLong(0x0a, {0xff}, 3)));
    any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    Require(any->type == 10 && static_cast<const Asn1String*>(any->value)->type == (10 | 0x100));
    PKCS7_free_nid_postfix(pkcs7);

    pkcs7 = DecodeClean(ContentInfo({0x2a, 0x03}, WrapLong(0x81, {0x07}, 1)));
    any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    Require(any->type == -3 && static_cast<const Asn1String*>(any->value)->length == 4);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    const unsigned char bodySize = static_cast<unsigned char>(body.size());
    std::vector<unsigned char> reserved{0x30, 0xff};
    reserved.insert(reserved.end(), 126, 0x00);
    reserved.push_back(bodySize);
    RequireRefused(Joined(reserved, body), 102);
    RequireRefused(Joined({0x30, 0x89, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, bodySize}, body), 102);
    RequireRefused(Joined({0x30, 0x88, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, body), 155);
    RequireRefused(Joined({0x30, 0x88, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, body), 155);
    RequireRefused(Joined({0x30, 0x81, static_cast<unsigned char>(bodySize + 1)}, body), 155);
    RequireRefused(Joined({0x30, 0x82, 0x00, static_cast<unsigned char>(bodySize + 1)}, body), 155);
    RequireRefused(Joined(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), {0xa0, 0x82, 0x00})), Joined({0x04}, octets)), 102);
    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), {0xa0, 0x81})), 102);
    RequireRefused({0x30, 0x82, 0x00}, 102);
    RequireRefused({0x30, 0x81}, 102);
    RequireRefused({0x30}, 102);
    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)),
        Joined({0xa0, 0x89, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}, octets))), 102);
    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), Joined({0xa0, 0x81, 0x05}, octets))), 155);
    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), Joined({0xa0, 0xff}, octets))), 102);
}

void TestBerIndefinite() {
    std::vector<unsigned char> der = ContentInfo(Pkcs7Oid(1),
        Wrap(0x24, Joined(Wrap(0x04, {'h', 'i'}), Wrap(0x04, {'!', '!'}))));
    LibreSsl3Pkcs7* pkcs7 = DecodeClean(der);
    Require(IsPkcs7Type(pkcs7->type, 1));
    const auto* octets = static_cast<const Asn1String*>(pkcs7->d);
    Require(octets->type == 4 && octets->length == 4 && std::memcmp(octets->data, "hi!!", 4) == 0);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    der = WrapIndef(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)),
        WrapIndef(0xa0, WrapIndef(0x24, Joined(Wrap(0x04, {'b', 'e', 'r'}), Wrap(0x04, {'t', 'e', 's', 't'}))))));
    pkcs7 = DecodeClean(der);
    octets = static_cast<const Asn1String*>(pkcs7->d);
    Require(octets->type == 4 && octets->length == 7 && std::memcmp(octets->data, "bertest", 7) == 0);
    PKCS7_free_nid_postfix(pkcs7);

    const auto nest = [](int levels) {
        std::vector<unsigned char> nested = Wrap(0x04, {'d', 'e', 'e', 'p', 'e', 'r'});
        for (int level = 0; level < levels; ++level) nested = Wrap(0x24, nested);
        return ContentInfo(Pkcs7Oid(1), nested);
    };
    pkcs7 = DecodeClean(nest(6));
    octets = static_cast<const Asn1String*>(pkcs7->d);
    Require(octets->length == 6 && std::memcmp(octets->data, "deeper", 6) == 0);
    PKCS7_free_nid_postfix(pkcs7);
    RequireRefused(nest(7), 197);

    RequireRefused(Joined({0x30, 0x80}, Wrap(0x06, Pkcs7Oid(1))), 137);
    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), {0x00, 0x00})), 159);
    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), Wrap(0xa0, {0x04, 0x80, 0x00}))), 102);

    der = WrapIndef(0x30, Joined(Wrap(0x06, {0x55, 0x04, 0x03}),
        WrapIndef(0xa0, WrapIndef(0x2c, Joined(Wrap(0x0c, {'f', 'o', 'o'}), Wrap(0x0c, {'b', 'a', 'r'}))))));
    pkcs7 = DecodeClean(der);
    const auto* any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    Require(any->type == 12);
    const auto* text = static_cast<const Asn1String*>(any->value);
    Require(text->type == 12 && text->length == 6 && std::memcmp(text->data, "foobar", 6) == 0);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    const std::vector<unsigned char> integer = Wrap(0x02, {0x01});
    const std::vector<unsigned char> emptySet = Wrap(0x31, {});
    const std::vector<unsigned char> innerData = Wrap(0x30, Wrap(0x06, Pkcs7Oid(1)));
    der = WrapIndef(0x30, Joined(Wrap(0x06, Pkcs7Oid(2)),
        WrapIndef(0xa0, WrapIndef(0x30, Joined(Joined(integer, emptySet), Joined(innerData, emptySet))))));
    pkcs7 = DecodeClean(der);
    Require(IsPkcs7Type(pkcs7->type, 2));
    const auto* signedData = static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d);
    Require(IsPkcs7Type(signedData->contents->type, 1) && signedData->contents->d == nullptr);
    Require(signedData->mdAlgs->num == 0 && signedData->signerInfo->num == 0 &&
            signedData->cert == nullptr && signedData->crl == nullptr);
    PKCS7_free_nid_postfix(pkcs7);

    const std::vector<unsigned char> enc = Wrap(0x30, Joined(Joined(Wrap(0x06, Pkcs7Oid(1)),
        Wrap(0x30, Joined(Wrap(0x06, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01}), Wrap(0x05, {})))),
        Wrap(0x80, {0xaa, 0xbb, 0xcc, 0xdd})));
    der = WrapIndef(0x30, Joined(Wrap(0x06, Pkcs7Oid(3)),
        WrapIndef(0xa0, WrapIndef(0x30, Joined(Joined(Wrap(0x02, {0x00}), emptySet), enc)))));
    pkcs7 = DecodeClean(der);
    Require(IsPkcs7Type(pkcs7->type, 3));
    const auto* envelope = static_cast<const LibreSsl3Pkcs7Envelope*>(pkcs7->d);
    Require(envelope->recipientInfo->num == 0 && envelope->encData->encData->length == 4);
    PKCS7_free_nid_postfix(pkcs7);

    der = WrapIndef(0x30, Joined(Wrap(0x06, Pkcs7Oid(6)),
        WrapIndef(0xa0, WrapIndef(0x30, Joined(Wrap(0x02, {0x00}), enc)))));
    pkcs7 = DecodeClean(der);
    Require(IsPkcs7Type(pkcs7->type, 6));
    const auto* encrypted = static_cast<const LibreSsl3Pkcs7Encrypt*>(pkcs7->d);
    Require(encrypted->encData->encData->length == 4);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    der = Wrap(0x30, Joined(Wrap(0x06, {0x55, 0x04, 0x03}),
        Wrap(0xa0, Wrap(0x23, Wrap(0x03, {0x04, 0xa0, 0xb0})))));
    pkcs7 = DecodeClean(der);
    any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    Require(any->type == 3);
    const auto* bits = static_cast<const Asn1String*>(any->value);
    Require(bits->type == 3 && bits->length == 2 && bits->flags == 0x0c);
    Require(bits->data[0] == 0xa0 && bits->data[1] == 0xb0);
    PKCS7_free_nid_postfix(pkcs7);

    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, {0x55, 0x04, 0x03}),
        Wrap(0xa0, Wrap(0x23, Wrap(0x03, {0x09, 0xa0, 0xb0}))))), 220);

    der = Wrap(0x30, Joined(Wrap(0x06, {0x55, 0x04, 0x03}),
        Wrap(0xa0, Wrap(0x23, {0x03, 0x02, 0x00, 0xaa, 0x03, 0x02, 0x00, 0xbb}))));
    pkcs7 = DecodeClean(der);
    any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    bits = static_cast<const Asn1String*>(any->value);
    Require(bits->type == 3 && bits->length == 3 && bits->flags == 0x08);
    Require(bits->data[0] == 0xaa && bits->data[1] == 0x00 && bits->data[2] == 0xbb);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    for (const unsigned char childTag : {0x04, 0x80}) {
        der = Wrap(0x30, Joined(Wrap(0x06, {0x55, 0x04, 0x03}),
            Wrap(0xa0, Wrap(0x2c, Joined(Wrap(0x0c, {'a', 'b'}), Wrap(childTag, {'c', 'd'}))))));
        pkcs7 = DecodeClean(der);
        any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
        text = static_cast<const Asn1String*>(any->value);
        Require(any->type == 12 && text->type == 12 && text->length == 4 && std::memcmp(text->data, "abcd", 4) == 0);
        PKCS7_free_nid_postfix(pkcs7);
    }
    pkcs7 = DecodeClean(ContentInfo(Pkcs7Oid(1), Wrap(0x24, Joined(Wrap(0x04, {'a', 'b'}), Wrap(0x0c, {'c', 'd'})))));
    octets = static_cast<const Asn1String*>(pkcs7->d);
    Require(octets->type == 4 && octets->length == 4 && std::memcmp(octets->data, "abcd", 4) == 0);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    const auto lengthCase = [](const std::vector<unsigned char>& child) {
        return Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), Wrap(0xa0, Wrap(0x24, child))));
    };
    RequireRefused(lengthCase({0x04, 0x84, 0x7f, 0xff, 0xff, 0xff}), 155);
    RequireRefused(lengthCase({0x04, 0x84, 0x80, 0x00, 0x00, 0x00}), 155);
    RequireRefused(lengthCase({0x04, 0x88, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}), 155);
    RequireRefused(lengthCase({0x04, 0x89, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}), 102);

    std::vector<unsigned char> large;
    for (int index = 0; index < 64; ++index) {
        std::vector<unsigned char> part;
        for (int byte = 0; byte < 1024; ++byte) part.push_back(static_cast<unsigned char>(index + byte));
        const std::vector<unsigned char> chunk = Wrap(0x04, part);
        large.insert(large.end(), chunk.begin(), chunk.end());
    }
    der = WrapLong(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), WrapLong(0xa0, WrapLong(0x24, large, 3), 3)), 3);
    pkcs7 = DecodeClean(der);
    octets = static_cast<const Asn1String*>(pkcs7->d);
    Require(octets->type == 4 && octets->length == 65536);
    Require(octets->data[0] == 0x00 && octets->data[1023] == 0xff && octets->data[1024] == 0x01);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
}

std::vector<unsigned char> Bytes(const char* text) {
    return std::vector<unsigned char>(text, text + std::strlen(text));
}

std::vector<unsigned char> Slice(const std::vector<unsigned char>& bytes, std::size_t begin, std::size_t end) {
    return std::vector<unsigned char>(bytes.begin() + static_cast<std::ptrdiff_t>(begin),
        bytes.begin() + static_cast<std::ptrdiff_t>(end));
}

std::vector<unsigned char> OtherContent(const std::vector<unsigned char>& value) {
    return Wrap(0x30, Joined(Wrap(0x06, {0x2a, 0x03}), Wrap(0xa0, value)));
}

void RequireOtherString(const std::vector<unsigned char>& der, int type, const std::vector<unsigned char>& expected,
    std::source_location location = std::source_location::current()) {
    LibreSsl3Pkcs7* pkcs7 = DecodeClean(der, location);
    const auto* any = static_cast<const LibreSsl3Asn1Type*>(pkcs7->d);
    Require(any->type == type, location);
    const auto* value = static_cast<const Asn1String*>(any->value);
    Require(value->type == type && value->length == static_cast<int>(expected.size()), location);
    Require(std::memcmp(value->data, expected.data(), expected.size()) == 0, location);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty(), location);
}

void RequireDataString(const std::vector<unsigned char>& der, const std::vector<unsigned char>& expected,
    std::source_location location = std::source_location::current()) {
    LibreSsl3Pkcs7* pkcs7 = DecodeClean(der, location);
    const auto* octets = static_cast<const Asn1String*>(pkcs7->d);
    Require(octets->type == 4 && octets->length == static_cast<int>(expected.size()), location);
    Require(std::memcmp(octets->data, expected.data(), expected.size()) == 0, location);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty(), location);
}

void RequireUnsupported(const std::vector<unsigned char>& der, std::source_location location = std::source_location::current()) {
    RequireThrows([&der] { Decode(der); }, location);
    ERR_clear_error_nid_postfix();
    Require(allocations.empty(), location);
}

void RequireBytes(const Asn1String* value, const std::vector<unsigned char>& expected,
    std::source_location location = std::source_location::current()) {
    Require(value != nullptr && value->length == static_cast<int>(expected.size()), location);
    Require(std::memcmp(value->data, expected.data(), expected.size()) == 0, location);
}

void TestConstructedForms() {
    const std::vector<std::vector<unsigned char>> primitiveOnly = {
        {0x30, 0x0b, 0x06, 0x02, 0x2a, 0x03, 0xa0, 0x05, 0x2a, 0x03, 0x02, 0x01, 0x05},
        {0x30, 0x0d, 0x06, 0x02, 0x2a, 0x03, 0xa0, 0x07, 0x26, 0x05, 0x04, 0x03, 0x2a, 0x03, 0x04},
        {0x30, 0x08, 0x06, 0x02, 0x2a, 0x03, 0xa0, 0x02, 0x25, 0x00},
        OtherContent(Wrap(0x21, Wrap(0x01, {0xff}))),
        OtherContent(Wrap(0x22, Wrap(0x02, {0x05}))),
        OtherContent(Wrap(0x29, {})),
        OtherContent(Wrap(0x2d, Wrap(0x0d, {0x01}))),
        OtherContent(Wrap(0x2e, {})),
    };
    for (const auto& der : primitiveOnly) RequireRefused(der, 231);

    RequireOtherString({0x30, 0x0d, 0x06, 0x02, 0x2a, 0x03, 0xa0, 0x07, 0x27, 0x80, 0x04, 0x01, 0x41, 0x00, 0x00}, 7, {0x41});
    RequireOtherString(OtherContent(Wrap(0x28, Wrap(0x04, {'A'}))), 8, {'A'});
    RequireOtherString(OtherContent(Wrap(0x2b, Wrap(0x04, {'B'}))), 11, {'B'});
    RequireOtherString(OtherContent(Wrap(0x2f, Wrap(0x04, {'C'}))), 15, {'C'});
    RequireOtherString(OtherContent(Wrap(0x3d, Wrap(0x04, {'D'}))), 29, {'D'});
    RequireOtherString(OtherContent(Wrap(0x20, Wrap(0x04, {'E'}))), 0, {'E'});
    RequireOtherString(OtherContent(Wrap(0x37, Joined(Wrap(0x04, Bytes("991231")), Wrap(0x04, Bytes("235959Z"))))), 23,
        Bytes("991231235959Z"));
    RequireOtherString(OtherContent(WrapIndef(0x24, Wrap(0x04, {'x', 'y'}))), 4, {'x', 'y'});
    RequireOtherString(OtherContent(Joined(Wrap(0x2c, Wrap(0x0c, {'h', 'i'})), Wrap(0x0c, {'!'}))), 12, {'h', 'i', '!'});
    TestDigestedOutOfMemory(OtherContent(Joined(Wrap(0x2c, Wrap(0x0c, {'h', 'i'})), Wrap(0x0c, {'!'}))));
    RequireRefused(OtherContent(Wrap(0x3e, Joined(Wrap(0x04, {'a'}), Wrap(0x04, {'b', 'c'})))), 214);
    RequireRefused(OtherContent(Wrap(0x10, {})), 156);
    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, {0x2a, 0x03}), WrapIndef(0xa0, Wrap(0x2c, Wrap(0x0c, {'h', 'i'}))))), 159);

    const auto data = [](const std::vector<unsigned char>& content) { return ContentInfo(Pkcs7Oid(1), content); };
    RequireDataString(data(Joined(Wrap(0x24, Wrap(0x04, {'h', 'i'})), Wrap(0x04, {'!'}))), {'h', 'i', '!'});
    RequireDataString(data(Joined(Wrap(0x24, Wrap(0x04, {'h', 'i'})), Wrap(0x81, {'!'}))), {'h', 'i', '!'});
    RequireDataString(data(Wrap(0x24, Joined(Wrap(0x24, Wrap(0x04, {'h', 'i'})), Wrap(0x04, {'!'})))), {'h', 'i', '!'});
    RequireRefused(data(Joined(WrapIndef(0x24, Wrap(0x04, {'h', 'i'})), Wrap(0x04, {'!'}))), 148);
    RequireRefused(data(WrapIndef(0x24, Joined(Wrap(0x24, Wrap(0x04, {'h', 'i'})), Wrap(0x04, {'!'})))), 159);
    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(1)), WrapIndef(0xa0, Wrap(0x24, Wrap(0x04, {'h', 'i'}))))), 159);
    RequireRefused(Wrap(0x30, Joined(Wrap(0x06, Pkcs7Oid(2)), Wrap(0x80, {}))), 120);
}

void TestConstructedOctetFields() {
    const std::vector<unsigned char> envelope(std::begin(Enveloped), std::end(Enveloped));
    const auto fields = EnvelopeFields(envelope);
    const auto recipient = Elements(Elements(fields[1].content)[0].content);
    const std::vector<unsigned char> key = recipient[3].content;
    const auto withKey = [&](const std::vector<unsigned char>& encKey) {
        const auto info = Wrap(0x30, Concat({recipient[0].encoded, recipient[1].encoded, recipient[2].encoded, encKey}));
        return ContentInfo(Pkcs7Oid(3), Wrap(0x30, Concat({fields[0].encoded, Wrap(0x31, info), fields[2].encoded})));
    };
    for (const auto& encKey : {WrapIndef(0x24, Joined(Wrap(0x04, Slice(key, 0, 2)), Wrap(0x04, Slice(key, 2, key.size())))),
                               Wrap(0x24, Joined(Wrap(0x04, Slice(key, 0, 2)), Wrap(0x80, Slice(key, 2, key.size()))))}) {
        LibreSsl3Pkcs7* pkcs7 = DecodeClean(withKey(encKey));
        const auto* data = static_cast<const LibreSsl3Pkcs7Envelope*>(pkcs7->d);
        RequireBytes(At<LibreSsl3Pkcs7RecipInfo>(data->recipientInfo, 0)->encKey, {0x55, 0x66, 0x77, 0x88, 0x99});
        RequireBytes(data->encData->encData, {0xaa, 0xbb, 0xcc, 0xdd});
        PKCS7_free_nid_postfix(pkcs7);
        Require(allocations.empty());
        TestDigestedOutOfMemory(withKey(encKey));
    }

    const auto enc = Elements(fields[2].content);
    const std::vector<unsigned char> encrypted = enc[2].content;
    const auto withData = [&](const std::vector<unsigned char>& encData) {
        return ContentInfo(Pkcs7Oid(3), Wrap(0x30, Concat({fields[0].encoded, fields[1].encoded,
            Wrap(0x30, Concat({enc[0].encoded, enc[1].encoded, encData}))})));
    };
    for (const auto& encData : {Wrap(0xa0, Joined(Wrap(0x04, Slice(encrypted, 0, 1)), Wrap(0x04, Slice(encrypted, 1, encrypted.size())))),
                                Wrap(0xa0, Wrap(0x80, encrypted))}) {
        LibreSsl3Pkcs7* pkcs7 = DecodeClean(withData(encData));
        const auto* data = static_cast<const LibreSsl3Pkcs7Envelope*>(pkcs7->d);
        Require(data->recipientInfo->num == 2);
        RequireBytes(data->encData->encData, {0xaa, 0xbb, 0xcc, 0xdd});
        PKCS7_free_nid_postfix(pkcs7);
        Require(allocations.empty());
        TestDigestedOutOfMemory(withData(encData));
    }
    RequireRefused(withData(WrapIndef(0xa0, Joined(Wrap(0x04, Slice(encrypted, 0, 1)),
        Wrap(0x24, Wrap(0x04, Slice(encrypted, 1, encrypted.size())))))), 159);
    RequireRefused(withData(Wrap(0xa0, encrypted)), 102);

    const std::vector<unsigned char> digested(std::begin(DigestedData), std::end(DigestedData));
    const auto digestFields = EnvelopeFields(digested);
    const std::vector<unsigned char> digest = digestFields[3].content;
    LibreSsl3Pkcs7* pkcs7 = DecodeClean(ContentInfo(Pkcs7Oid(5), Wrap(0x30, Concat({digestFields[0].encoded,
        digestFields[1].encoded, digestFields[2].encoded,
        Wrap(0x24, Joined(Wrap(0x04, Slice(digest, 0, 3)), Wrap(0x04, Slice(digest, 3, digest.size()))))}))));
    RequireBytes(static_cast<const LibreSsl3Pkcs7Digest*>(pkcs7->d)->digest, digest);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
    TestDigestedOutOfMemory(ContentInfo(Pkcs7Oid(5), Wrap(0x30, Concat({digestFields[0].encoded, digestFields[1].encoded,
        digestFields[2].encoded, WrapIndef(0x24, Wrap(0x04, digest))}))));
    RequireRefused(ContentInfo(Pkcs7Oid(5), Wrap(0x30, Concat({Wrap(0x22, digestFields[0].encoded), digestFields[1].encoded,
        digestFields[2].encoded, digestFields[3].encoded}))), 231);
    const auto algorithm = Elements(digestFields[1].content);
    RequireRefused(ContentInfo(Pkcs7Oid(5), Wrap(0x30, Concat({digestFields[0].encoded,
        Wrap(0x30, Joined(Wrap(0x26, algorithm[0].encoded), algorithm[1].encoded)), digestFields[2].encoded,
        digestFields[3].encoded}))), 231);
    RequireRefused(ContentInfo(Pkcs7Oid(5), Wrap(0x10, {})), 149);
    RequireUnsupported(ContentInfo(Pkcs7Oid(5), Wrap(0x30, Concat({digestFields[0].encoded,
        WrapIndef(0x30, digestFields[1].content), digestFields[2].encoded, digestFields[3].encoded}))));
}

void TestConstructedSignerFields() {
    const std::vector<unsigned char> message(std::begin(SignedMessage), std::end(SignedMessage));
    const auto fields = EnvelopeFields(message);
    const auto signer = Elements(Elements(fields[4].content)[0].content);
    const auto build = [&](const std::vector<std::vector<unsigned char>>& signerFields, const std::vector<unsigned char>& mdAlgs,
                           const std::vector<unsigned char>& certs) {
        return ContentInfo(Pkcs7Oid(2), Wrap(0x30, Concat({fields[0].encoded, mdAlgs, fields[2].encoded, certs,
            Wrap(0x31, Wrap(0x30, Concat(signerFields)))})));
    };
    const auto withSigner = [&](const std::vector<std::vector<unsigned char>>& signerFields) {
        return build(signerFields, fields[1].encoded, fields[3].encoded);
    };
    const std::vector<unsigned char> digest = signer[5].content;
    const std::vector<unsigned char> attributeOid = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x06};
    const auto unauth = Wrap(0xa1, Wrap(0x30, Joined(Wrap(0x06, attributeOid), Wrap(0x31, Wrap(0x04, {'Z'})))));
    std::vector<std::vector<unsigned char>> prefix;
    for (int index = 0; index < 5; ++index) prefix.push_back(signer[index].encoded);

    auto signerFields = prefix;
    signerFields.push_back(Wrap(0x24, Wrap(0x04, Slice(digest, 0, 4))));
    signerFields.push_back(unauth);
    LibreSsl3Pkcs7* pkcs7 = DecodeClean(withSigner(signerFields));
    const auto* info = At<LibreSsl3Pkcs7SignerInfo>(static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d)->signerInfo, 0);
    RequireBytes(info->encDigest, Concat({Slice(digest, 0, 4), attributeOid, {'Z'}}));
    Require(info->unauthAttr == nullptr);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
    TestDigestedOutOfMemory(withSigner(signerFields));

    signerFields = prefix;
    signerFields.push_back(WrapIndef(0x24, Wrap(0x04, Slice(digest, 0, 4))));
    signerFields.push_back(unauth);
    pkcs7 = DecodeClean(withSigner(signerFields));
    info = At<LibreSsl3Pkcs7SignerInfo>(static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d)->signerInfo, 0);
    RequireBytes(info->encDigest, Slice(digest, 0, 4));
    Require(info->unauthAttr != nullptr && info->unauthAttr->num == 1);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    const auto attributes = Elements(signer[3].content);
    const auto first = Elements(attributes[0].content);
    const auto withValues = [&](const std::vector<unsigned char>& values) {
        std::vector<std::vector<unsigned char>> list = {Wrap(0x30, Joined(first[0].encoded, values))};
        for (std::size_t index = 1; index < attributes.size(); ++index) list.push_back(attributes[index].encoded);
        return withSigner({signer[0].encoded, signer[1].encoded, signer[2].encoded, Wrap(0xa0, Concat(list)),
                           signer[4].encoded, signer[5].encoded});
    };
    const std::vector<unsigned char> object = Wrap(0x06, std::vector<unsigned char>(std::begin(DataOid), std::end(DataOid)));
    const std::vector<unsigned char> text = Wrap(0x2c, Wrap(0x0c, {'a', 'b'}));
    const auto valueAt = [](const LibreSsl3Pkcs7* decoded, int index) {
        const auto* signerInfo = At<LibreSsl3Pkcs7SignerInfo>(static_cast<const LibreSsl3Pkcs7Signed*>(decoded->d)->signerInfo, 0);
        return At<LibreSsl3Asn1Type>(At<LibreSsl3X509Attribute>(signerInfo->authAttr, 0)->set, index);
    };
    const auto setSize = [](const LibreSsl3Pkcs7* decoded) {
        const auto* signerInfo = At<LibreSsl3Pkcs7SignerInfo>(static_cast<const LibreSsl3Pkcs7Signed*>(decoded->d)->signerInfo, 0);
        return At<LibreSsl3X509Attribute>(signerInfo->authAttr, 0)->set->num;
    };
    pkcs7 = DecodeClean(withValues(Wrap(0x31, Joined(text, object))));
    Require(setSize(pkcs7) == 1 && valueAt(pkcs7, 0)->type == 12);
    RequireBytes(static_cast<const Asn1String*>(valueAt(pkcs7, 0)->value),
        Concat({{'a', 'b'}, std::vector<unsigned char>(std::begin(DataOid), std::end(DataOid))}));
    PKCS7_free_nid_postfix(pkcs7);
    TestDigestedOutOfMemory(withValues(Wrap(0x31, Joined(text, object))));
    pkcs7 = DecodeClean(withValues(Wrap(0x31, Joined(object, text))));
    Require(setSize(pkcs7) == 2 && valueAt(pkcs7, 0)->type == 6 && valueAt(pkcs7, 1)->type == 12);
    RequireBytes(static_cast<const Asn1String*>(valueAt(pkcs7, 1)->value), {'a', 'b'});
    PKCS7_free_nid_postfix(pkcs7);
    pkcs7 = DecodeClean(withValues(WrapIndef(0x31, Joined(WrapIndef(0x2c, Wrap(0x0c, {'a', 'b'})), object))));
    Require(setSize(pkcs7) == 2 && valueAt(pkcs7, 0)->type == 12 && valueAt(pkcs7, 1)->type == 6);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
    RequireRefused(withValues(WrapIndef(0x31, Joined(text, object))), 159);
    RequireRefused(withValues(Wrap(0x31, Wrap(0x22, Wrap(0x02, {0x01})))), 231);

    pkcs7 = DecodeClean(build({signer[0].encoded, signer[1].encoded, signer[2].encoded, signer[3].encoded, signer[4].encoded,
        signer[5].encoded}, Wrap(0x11, {}), fields[3].encoded));
    Require(static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d)->mdAlgs->num == 0);
    PKCS7_free_nid_postfix(pkcs7);
    pkcs7 = DecodeClean(build({signer[0].encoded, signer[1].encoded, signer[2].encoded, signer[3].encoded, signer[4].encoded,
        signer[5].encoded}, Wrap(0x11, fields[1].content), fields[3].encoded));
    Require(static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d)->mdAlgs->num == 1);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());

    const auto issuer = Elements(signer[1].content);
    const auto withIssuer = [&](const std::vector<unsigned char>& name) {
        auto list = prefix;
        list[1] = Wrap(0x30, Joined(name, issuer[1].encoded));
        list.push_back(signer[5].encoded);
        return withSigner(list);
    };
    pkcs7 = DecodeClean(withIssuer(WrapIndef(0x30, issuer[0].content)));
    info = At<LibreSsl3Pkcs7SignerInfo>(static_cast<const LibreSsl3Pkcs7Signed*>(pkcs7->d)->signerInfo, 0);
    Require(info->issuerAndSerial->issuer->canonEnc != nullptr && info->issuerAndSerial->serial->length == 20);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
    RequireUnsupported(withIssuer(Wrap(0x30, WrapIndef(0x31, Elements(issuer[0].content)[0].content))));

    const auto certificate = Elements(Elements(fields[3].content)[0].content);
    const auto toBeSigned = Elements(certificate[0].content);
    std::vector<std::vector<unsigned char>> tbs;
    for (std::size_t index = 0; index < toBeSigned.size(); ++index)
        tbs.push_back(index == 4 ? WrapIndef(0x30, toBeSigned[index].content) : toBeSigned[index].encoded);
    auto allFields = prefix;
    allFields.push_back(signer[5].encoded);
    RequireUnsupported(build(allFields, fields[1].encoded,
        Wrap(0xa0, Wrap(0x30, Concat({Wrap(0x30, Concat(tbs)), certificate[1].encoded, certificate[2].encoded})))));
    RequireUnsupported(build(allFields, fields[1].encoded,
        Wrap(0xa0, WrapIndef(0x30, Concat({certificate[0].encoded, certificate[1].encoded, certificate[2].encoded})))));
    const std::vector<unsigned char> sha256 = {0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01};
    RequireUnsupported(build(allFields, Wrap(0x31, Wrap(0x30, Joined(Wrap(0x06, sha256), Wrap(0x24, Wrap(0x04, {'p'}))))),
        fields[3].encoded));
    RequireUnsupported(build(allFields, Wrap(0x31, Joined({0x30, 0x81, 0x0d}, Elements(fields[1].content)[0].content)),
        fields[3].encoded));
}

void TestUnsupported() {
    Require(Decode(ContentInfo(Pkcs7Oid(3), Wrap(0x30, {}))) == nullptr);
    LibreSsl3Pkcs7* pkcs7 = DecodeClean({0x30, 0x80, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0x00, 0x00});
    Require(IsPkcs7Type(pkcs7->type, 1) && pkcs7->d == nullptr);
    PKCS7_free_nid_postfix(pkcs7);
    pkcs7 = DecodeClean(ContentInfo(Pkcs7Oid(1), Wrap(0x24, Wrap(0x04, {0x01}))));
    Require(IsPkcs7Type(pkcs7->type, 1));
    const auto* content = static_cast<const Asn1String*>(pkcs7->d);
    Require(content->type == 4 && content->length == 1 && content->data[0] == 0x01);
    PKCS7_free_nid_postfix(pkcs7);
    Require(allocations.empty());
    RequireThrows([] {
        const unsigned char* cursor = nullptr;
        d2i_PKCS7_nid_postfix(nullptr, &cursor, 1);
    });
    RequireThrows([] { d2i_PKCS7_nid_postfix(nullptr, nullptr, 0); });
    allocations.clear();
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestSignedMessage();
    TestCertificatesAndCrl();
    TestDigestedContent();
    TestDigestedErrors();
    TestDigestedNesting();
    TestDigestedAllocationFailures();
    TestEncryptedWithData();
    TestEncryptedWithoutData();
    TestEncryptedErrors();
    TestAllocationFailures();
    TestEnveloped();
    TestSignedAndEnveloped();
    TestEnvelopedErrors();
    TestOmittedEncData();
    TestCertAndCrlInEnvelope();
    TestEnvelopeTrailingAndTags();
    TestEnvelopeAllocationFailures();
    TestDataAndOtherTypes();
    TestOutputAndErrors();
    TestLongFormLengths();
    TestBerIndefinite();
    TestConstructedForms();
    TestConstructedOctetFields();
    TestConstructedSignerFields();
    TestUnsupported();
    std::puts("LibreSSL3 d2i_PKCS7 tests passed");
}
