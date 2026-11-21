#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

using Print = int (APS5_VABI*)(LibreSsl::Bio*, void*, std::uint64_t, int);
static_assert(std::is_same_v<decltype(&X509V3_EXT_print_nid_postfix), Print>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL X509V3_EXT_print check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> live;
int allocations = 0;
int failAt = -1;

bool Refuse() {
    return allocations++ == failAt;
}

void* APS5_VABI Allocate(std::size_t bytes) {
    if (Refuse()) return nullptr;
    void* pointer = std::malloc(bytes ? bytes : 1);
    live.emplace(pointer, bytes);
    return pointer;
}

void APS5_VABI Free(void* pointer) {
    if (!pointer) return;
    Require(live.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc(std::size_t count, std::size_t bytes) {
    if (Refuse()) return nullptr;
    void* pointer = std::calloc(count ? count : 1, bytes ? bytes : 1);
    live.emplace(pointer, count * bytes);
    return pointer;
}

void* APS5_VABI Reallocate(void* pointer, std::size_t bytes) {
    if (Refuse()) return nullptr;
    if (pointer) Require(live.erase(pointer) == 1);
    void* grown = std::realloc(pointer, bytes ? bytes : 1);
    live.emplace(grown, bytes);
    return grown;
}

void* APS5_VABI NoAlign(std::size_t, std::size_t) { throw std::runtime_error("unexpected align"); }
void* APS5_VABI NoRealign(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected realign"); }
int APS5_VABI NoPosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void InitializeHeap() {
    void* const api[10] {reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free),
        reinterpret_cast<void*>(Calloc), reinterpret_cast<void*>(Reallocate),
        reinterpret_cast<void*>(NoAlign), reinterpret_cast<void*>(NoRealign),
        reinterpret_cast<void*>(NoPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

struct Extension {
    LibreSslAsn1Object* object;
    int critical;
    Asn1String* value;
};

static_assert(sizeof(Extension) == 24);

constexpr unsigned char Response[] {
    0x30, 0x82, 0x02, 0x7a, 0x0a, 0x01, 0x00, 0xa0, 0x82, 0x02, 0x73, 0x30, 0x82, 0x02, 0x6f, 0x06,
    0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01, 0x04, 0x82, 0x02, 0x60, 0x30, 0x82,
    0x02, 0x5c, 0x30, 0x82, 0x02, 0x09, 0xa2, 0x16, 0x04, 0x14, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x18, 0x0f,
    0x32, 0x30, 0x32, 0x36, 0x31, 0x31, 0x31, 0x34, 0x30, 0x39, 0x32, 0x30, 0x30, 0x30, 0x5a, 0x30,
    0x82, 0x01, 0x3c, 0x30, 0x82, 0x01, 0x38, 0x30, 0x3b, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03,
    0x02, 0x1a, 0x05, 0x00, 0x04, 0x14, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x04, 0x14, 0x50, 0x51, 0x52, 0x53,
    0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63,
    0x02, 0x02, 0x12, 0x34, 0x80, 0x00, 0x18, 0x0f, 0x32, 0x30, 0x32, 0x36, 0x31, 0x31, 0x31, 0x34,
    0x30, 0x39, 0x30, 0x30, 0x30, 0x30, 0x5a, 0xa1, 0x81, 0xe5, 0x30, 0x81, 0xe2, 0x30, 0x1e, 0x06,
    0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x06, 0x04, 0x11, 0x18, 0x0f, 0x32, 0x30,
    0x32, 0x36, 0x31, 0x31, 0x31, 0x34, 0x30, 0x39, 0x33, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x45, 0x06,
    0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x03, 0x04, 0x38, 0x30, 0x36, 0xa0, 0x1b,
    0x16, 0x19, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x63, 0x72, 0x6c, 0x2e, 0x65, 0x78, 0x61,
    0x6d, 0x70, 0x6c, 0x65, 0x2f, 0x63, 0x61, 0x2e, 0x63, 0x72, 0x6c, 0xa1, 0x04, 0x02, 0x02, 0xfe,
    0xdc, 0xa2, 0x11, 0x18, 0x0f, 0x32, 0x30, 0x32, 0x36, 0x31, 0x31, 0x31, 0x33, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x5a, 0x30, 0x1b, 0x06, 0x03, 0x55, 0x1d, 0x18, 0x04, 0x14, 0x18, 0x12, 0x32,
    0x30, 0x32, 0x36, 0x31, 0x31, 0x30, 0x31, 0x31, 0x32, 0x30, 0x30, 0x30, 0x30, 0x2e, 0x32, 0x35,
    0x5a, 0x30, 0x0f, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x05, 0x04, 0x02,
    0x05, 0x00, 0x30, 0x10, 0x06, 0x03, 0x55, 0x1d, 0x17, 0x04, 0x09, 0x06, 0x07, 0x2a, 0x86, 0x48,
    0xce, 0x38, 0x02, 0x02, 0x30, 0x12, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01,
    0x06, 0x04, 0x05, 0x18, 0x03, 0x61, 0x62, 0x63, 0x30, 0x13, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05,
    0x05, 0x07, 0x30, 0x01, 0x03, 0x04, 0x06, 0x30, 0x04, 0xa1, 0x02, 0x02, 0x00, 0x30, 0x10, 0x06,
    0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x05, 0x04, 0x03, 0x05, 0x01, 0x00, 0xa1,
    0x81, 0x9d, 0x30, 0x81, 0x9a, 0x30, 0x17, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30,
    0x01, 0x02, 0x04, 0x0a, 0x04, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x30, 0x35,
    0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x02, 0x04, 0x28, 0xa0, 0xa1, 0xa2,
    0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2,
    0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2,
    0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0x30, 0x27, 0x06, 0x04, 0x2a, 0x03, 0x04, 0x05, 0x01, 0x01, 0xff,
    0x04, 0x1c, 0x75, 0x6e, 0x6b, 0x6e, 0x6f, 0x77, 0x6e, 0x20, 0x65, 0x78, 0x74, 0x65, 0x6e, 0x73,
    0x69, 0x6f, 0x6e, 0x20, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x20, 0x20, 0x00, 0x20, 0x00, 0x30, 0x10,
    0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x06, 0x04, 0x03, 0x02, 0x01, 0x05,
    0x30, 0x0d, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x02, 0x04, 0x00, 0x30,
    0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x41, 0x00, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
    0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
    0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
};

constexpr std::uint64_t Asn1(int reason) {
    return (std::uint64_t{13} << 24) | (std::uint64_t{0xfff} << 12) | static_cast<std::uint64_t>(reason);
}

constexpr std::uint64_t Ocsp(int reason) {
    return (std::uint64_t{39} << 24) | (std::uint64_t{0xfff} << 12) | static_cast<std::uint64_t>(reason);
}

std::vector<std::uint64_t> Errors() {
    std::vector<std::uint64_t> errors;
    while (const auto error = ERR_get_error_nid_postfix()) errors.push_back(error);
    return errors;
}

enum Stack { Responses, Singles };

struct Case {
    Stack stack;
    int index;
    bool bio;
    std::uint64_t flag;
    int indent;
    int result;
    const char* output;
    std::vector<std::uint64_t> errors;
};

const Case Cases[] {
    {Responses, 0, true, 0, 0, 0, "", {}},
    {Responses, 0, true, 0, 2, 1, "  04080102030405060708", {}},
    {Responses, 0, true, 1, -3, 1, "   04080102030405060708", {}},
    {Responses, 0, true, 3, 2, 1, "  04080102030405060708", {}},
    {Responses, 0, true, 4, 0, 0, "", {}},
    {Responses, 0, false, 0, 4, 0, "", {}},
    {Responses, 0, false, 1, 4, 0, "", {}},
    {Responses, 1, true, 0, 0, 0, "", {}},
    {Responses, 1, true, 0, 2, 1, "  A0A1A2A3A4A5A6A7A8A9AAABACADAEAFB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBFC0C1C2\\\nC3C4C5C6C7", {}},
    {Responses, 1, true, 1, -3, 1, "   A0A1A2A3A4A5A6A7A8A9AAABACADAEAFB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBFC0C1C2\\\nC3C4C5C6C7", {}},
    {Responses, 1, true, 3, 2, 1, "  A0A1A2A3A4A5A6A7A8A9AAABACADAEAFB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBFC0C1C2\\\nC3C4C5C6C7", {}},
    {Responses, 1, true, 4, 0, 0, "", {}},
    {Responses, 1, false, 0, 4, 0, "", {}},
    {Responses, 1, false, 1, 4, 0, "", {}},
    {Responses, 2, true, 0, 0, 0, "", {}},
    {Responses, 2, true, 0, 2, 0, "", {}},
    {Responses, 2, true, 1, 2, 1, "  <Not Supported>", {}},
    {Responses, 2, true, 1, -3, 1, "   <Not Supported>", {}},
    {Responses, 2, true, 3, 2, 166, "  0000 - 75 6e 6b 6e 6f 77 6e 20-65 78 74 65 6e 73 69 6f   unknown extensio\n  0010 - 6e 20 76 61 6c 75 65                              n value\n  001c - <SPACES/NULS>\n", {}},
    {Responses, 2, true, 3, 40, 368, "                                        0000 - 75 6e 6b 6e 6f 77 6e   unknown\n                                        0007 - 20 65 78 74 65 6e 73    extens\n                                        000e - 69 6f 6e 20 76 61 6c   ion val\n                                        0015 - 75 65                  ue\n                                        001c - <SPACES/NULS>\n", {}},
    {Responses, 2, true, 3, 71, 92, "                                                                       001c - <SPACES/NULS>\n", {}},
    {Responses, 2, true, 3, 200, 149, "                                                                                                                                001c - <SPACES/NULS>\n", {}},
    {Responses, 2, true, 4, 0, 1, "", {}},
    {Responses, 2, false, 0, 4, 0, "", {}},
    {Responses, 2, false, 1, 4, 1, "", {}},
    {Responses, 3, true, 0, 0, 0, "", {Asn1(168), Asn1(58)}},
    {Responses, 3, true, 0, 2, 0, "", {Asn1(168), Asn1(58)}},
    {Responses, 3, true, 1, 2, 1, "  <Parse Error>", {Asn1(168), Asn1(58)}},
    {Responses, 3, true, 1, -3, 1, "   <Parse Error>", {Asn1(168), Asn1(58)}},
    {Responses, 3, true, 3, 2, 63, "  0000 - 02 01 05                                          ...\n", {Asn1(168), Asn1(58)}},
    {Responses, 3, true, 3, 40, 74, "                                        0000 - 02 01 05               ...\n", {Asn1(168), Asn1(58)}},
    {Responses, 3, true, 3, 71, 0, "", {Asn1(168), Asn1(58)}},
    {Responses, 3, true, 3, 200, 138, "                                                                                                                                0000 -   \n", {Asn1(168), Asn1(58)}},
    {Responses, 3, true, 4, 0, 1, "", {Asn1(168), Asn1(58)}},
    {Responses, 3, false, 0, 4, 0, "", {Asn1(168), Asn1(58)}},
    {Responses, 3, false, 1, 4, 1, "", {Asn1(168), Asn1(58)}},
    {Responses, 4, true, 0, 0, 0, "", {}},
    {Responses, 4, true, 0, 2, 1, "  0", {}},
    {Responses, 4, true, 1, -3, 1, "   0", {}},
    {Responses, 4, true, 3, 2, 1, "  0", {}},
    {Responses, 4, true, 4, 0, 0, "", {}},
    {Responses, 4, false, 0, 4, 0, "", {}},
    {Responses, 4, false, 1, 4, 0, "", {}},
    {Singles, 0, true, 0, 0, 0, "", {}},
    {Singles, 0, true, 0, 2, 1, "  Nov 14 09:30:00 2026 GMT", {}},
    {Singles, 0, true, 1, -3, 1, "   Nov 14 09:30:00 2026 GMT", {}},
    {Singles, 0, true, 3, 2, 1, "  Nov 14 09:30:00 2026 GMT", {}},
    {Singles, 0, true, 4, 0, 0, "", {}},
    {Singles, 0, false, 0, 4, 1, "", {}},
    {Singles, 0, false, 1, 4, 1, "", {}},
    {Singles, 1, true, 0, 0, 1, "crlUrl: http://crl.example/ca.crl\ncrlNum: -0124\ncrlTime: Nov 13 00:00:00 2026 GMT\n", {}},
    {Singles, 1, true, 0, 2, 1, "  crlUrl: http://crl.example/ca.crl\n  crlNum: -0124\n  crlTime: Nov 13 00:00:00 2026 GMT\n", {}},
    {Singles, 1, true, 1, -3, 1, "   crlUrl: http://crl.example/ca.crl\n   crlNum: -0124\n   crlTime: Nov 13 00:00:00 2026 GMT\n", {}},
    {Singles, 1, true, 3, 2, 1, "  crlUrl: http://crl.example/ca.crl\n  crlNum: -0124\n  crlTime: Nov 13 00:00:00 2026 GMT\n", {}},
    {Singles, 1, true, 4, 0, 1, "crlUrl: http://crl.example/ca.crl\ncrlNum: -0124\ncrlTime: Nov 13 00:00:00 2026 GMT\n", {}},
    {Singles, 1, false, 0, 4, 0, "", {}},
    {Singles, 1, false, 1, 4, 0, "", {}},
    {Singles, 2, true, 0, 0, 0, "", {}},
    {Singles, 2, true, 0, 2, 1, "  Nov  1 12:00:00.25 2026 GMT", {}},
    {Singles, 2, true, 1, -3, 1, "   Nov  1 12:00:00.25 2026 GMT", {}},
    {Singles, 2, true, 3, 2, 1, "  Nov  1 12:00:00.25 2026 GMT", {}},
    {Singles, 2, true, 4, 0, 0, "", {}},
    {Singles, 2, false, 0, 4, 1, "", {}},
    {Singles, 2, false, 1, 4, 1, "", {}},
    {Singles, 3, true, 0, 0, 1, "", {}},
    {Singles, 3, true, 0, 2, 1, "", {}},
    {Singles, 3, true, 1, -3, 1, "", {}},
    {Singles, 3, true, 3, 2, 1, "", {}},
    {Singles, 3, true, 4, 0, 1, "", {}},
    {Singles, 3, false, 0, 4, 1, "", {}},
    {Singles, 3, false, 1, 4, 1, "", {}},
    {Singles, 4, true, 0, 0, 0, "", {}},
    {Singles, 4, true, 0, 2, 1, "  Hold Instruction Call Issuer", {}},
    {Singles, 4, true, 1, -3, 1, "   Hold Instruction Call Issuer", {}},
    {Singles, 4, true, 3, 2, 1, "  Hold Instruction Call Issuer", {}},
    {Singles, 4, true, 4, 0, 0, "", {}},
    {Singles, 4, false, 0, 4, 0, "", {}},
    {Singles, 4, false, 1, 4, 0, "", {}},
    {Singles, 5, true, 0, 0, 0, "", {}},
    {Singles, 5, true, 0, 2, 0, "  Bad time value", {}},
    {Singles, 5, true, 1, -3, 0, "   Bad time value", {}},
    {Singles, 5, true, 3, 2, 0, "  Bad time value", {}},
    {Singles, 5, true, 4, 0, 0, "", {}},
    {Singles, 5, false, 0, 4, 0, "", {}},
    {Singles, 5, false, 1, 4, 0, "", {}},
    {Singles, 6, true, 0, 0, 1, "crlNum: 00\n", {}},
    {Singles, 6, true, 0, 2, 1, "  crlNum: 00\n", {}},
    {Singles, 6, true, 1, -3, 1, "   crlNum: 00\n", {}},
    {Singles, 6, true, 3, 2, 1, "  crlNum: 00\n", {}},
    {Singles, 6, true, 4, 0, 1, "crlNum: 00\n", {}},
    {Singles, 6, false, 0, 4, 0, "", {}},
    {Singles, 6, false, 1, 4, 0, "", {}},
    {Singles, 7, true, 0, 0, 0, "", {Asn1(144)}},
    {Singles, 7, true, 0, 2, 0, "", {Asn1(144)}},
    {Singles, 7, true, 1, 2, 1, "  <Parse Error>", {Asn1(144)}},
    {Singles, 7, true, 1, -3, 1, "   <Parse Error>", {Asn1(144)}},
    {Singles, 7, true, 3, 2, 85, "  0000 - 05 01                                             ..\n  0003 - <SPACES/NULS>\n", {Asn1(144)}},
    {Singles, 7, true, 3, 40, 134, "                                        0000 - 05 01                  ..\n                                        0003 - <SPACES/NULS>\n", {Asn1(144)}},
    {Singles, 7, true, 3, 71, 92, "                                                                       0003 - <SPACES/NULS>\n", {Asn1(144)}},
    {Singles, 7, true, 3, 200, 287, "                                                                                                                                0000 -   \n                                                                                                                                0003 - <SPACES/NULS>\n", {Asn1(144)}},
    {Singles, 7, true, 4, 0, 1, "", {Asn1(144)}},
    {Singles, 7, false, 0, 4, 0, "", {Asn1(144)}},
    {Singles, 7, false, 1, 4, 1, "", {Asn1(144)}},
};

struct Printed {
    int result;
    std::string output;
};

Printed PrintTo(void* extension, bool bio, std::uint64_t flag, int indent) {
    LibreSsl::Bio* out = bio ? BIO_new_nid_postfix(BIO_s_mem_nid_postfix()) : nullptr;
    Require(!bio || out);
    const int result = X509V3_EXT_print_nid_postfix(out, extension, flag, indent);
    std::string output;
    if (out) {
        char* data = nullptr;
        const auto size = BIO_ctrl_nid_postfix(out, 3, 0, &data);
        output.assign(data ? data : "", static_cast<std::size_t>(size));
        Require(BIO_free_nid_postfix(out) == 1);
    }
    return {result, output};
}

void CheckCases(const LibreSslStack* responses, const LibreSslStack* singles) {
    for (const Case& test : Cases) {
        const LibreSslStack* stack = test.stack == Responses ? responses : singles;
        Require(test.index < stack->num);
        ERR_clear_error_nid_postfix();
        const Printed printed = PrintTo(stack->data[test.index], test.bio, test.flag << 16, test.indent);
        Require(printed.result == test.result);
        Require(printed.output == test.output);
        Require(Errors() == test.errors);
    }
}

void CheckAllocationFailures(const LibreSslStack* stack) {
    for (int index = 0; index < stack->num; ++index) {
        for (int failure = 0; failure < 8; ++failure) {
            const std::size_t before = live.size();
            ERR_clear_error_nid_postfix();
            allocations = 0;
            failAt = failure;
            X509V3_EXT_print_nid_postfix(nullptr, stack->data[index], std::uint64_t {1} << 16, 4);
            failAt = -1;
            Require(live.size() == before);
            Errors();
        }
    }
}

void CheckNonceAllocation(void* nonce) {
    for (int failure = 0; failure < 2; ++failure) {
        ERR_clear_error_nid_postfix();
        allocations = 0;
        failAt = failure;
        const Printed printed = PrintTo(nonce, false, std::uint64_t {1} << 16, 4);
        failAt = -1;
        Require(printed.result == 1);
        Require(Errors() == std::vector<std::uint64_t> {Asn1(65), Ocsp(65)});
    }
    allocations = 0;
    failAt = 2;
    const Printed printed = PrintTo(nonce, false, std::uint64_t {1} << 16, 4);
    failAt = -1;
    Require(printed.result == 0);
    Require(Errors().empty());
}

void CheckWithoutObject() {
    Extension extension {nullptr, 0, nullptr};
    Printed printed = PrintTo(&extension, true, std::uint64_t {1} << 16, 1);
    Require(printed.result == 1 && printed.output == " <Not Supported>");
    printed = PrintTo(&extension, true, 0, 1);
    Require(printed.result == 0 && printed.output.empty());
    printed = PrintTo(&extension, true, std::uint64_t {5} << 16, 1);
    Require(printed.result == 1 && printed.output.empty());
    bool threw = false;
    try {
        X509V3_EXT_print_nid_postfix(nullptr, nullptr, 0, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    InitializeHeap();
    const unsigned char* input = Response;
    auto* response = d2i_OCSP_RESPONSE_nid_postfix(nullptr, &input, static_cast<std::int64_t>(sizeof Response));
    Require(response && input == Response + sizeof Response);
    auto* basic = OCSP_response_get1_basic_nid_postfix(response);
    Require(basic && basic->tbsResponseData);
    const LibreSslStack* responses = basic->tbsResponseData->responseExtensions;
    Require(responses && responses->num == 5);
    const auto* single = static_cast<const LibreSslOcspSingleResponse*>(OCSP_resp_get0_nid_postfix(basic, 0));
    Require(single);
    const auto* singles = static_cast<const LibreSslStack*>(single->singleExtensions);
    Require(singles && singles->num == 8);
    const std::size_t decoded = live.size();
    CheckCases(responses, singles);
    Require(live.size() == decoded);
    CheckAllocationFailures(responses);
    CheckAllocationFailures(singles);
    CheckNonceAllocation(responses->data[0]);
    CheckWithoutObject();
    Require(live.size() == decoded);
    OCSP_BASICRESP_free_nid_postfix(basic);
    OCSP_RESPONSE_free_nid_postfix(response);
    Require(live.empty());
    return 0;
}
