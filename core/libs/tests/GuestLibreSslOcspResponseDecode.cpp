#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

using Decode = LibreSslOcspResponse* (APS5_VABI*)(LibreSslOcspResponse**, const unsigned char**, std::int64_t);
static_assert(std::is_same_v<decltype(&d2i_OCSP_RESPONSE_nid_postfix), Decode>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL OCSP response decode check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false, location);
}

std::unordered_map<void*, std::size_t> live;
int allocations = 0;
int reallocations = 0;
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
    ++reallocations;
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

constexpr unsigned char TryLater[] {0x30, 0x03, 0x0a, 0x01, 0x03};
constexpr unsigned char MalformedRequest[] {0x30, 0x03, 0x0a, 0x01, 0x01};
constexpr unsigned char Unauthorized[] {0x30, 0x03, 0x0a, 0x01, 0x06};
constexpr unsigned char Successful[] {
    0x30, 0x82, 0x01, 0x02, 0x0a, 0x01, 0x00, 0xa0, 0x81, 0xfc, 0x30, 0x81, 0xf9, 0x06, 0x09, 0x2b,
    0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01, 0x04, 0x81, 0xeb, 0x30, 0x81, 0xe8, 0x30, 0x81,
    0x8f, 0xa2, 0x16, 0x04, 0x14, 0x09, 0x65, 0xfe, 0x88, 0x54, 0x3e, 0x79, 0xc6, 0x68, 0x6d, 0x1b,
    0x2e, 0x7a, 0xc8, 0xf3, 0x60, 0xe1, 0x91, 0x17, 0x97, 0x18, 0x0f, 0x32, 0x30, 0x32, 0x36, 0x31,
    0x31, 0x31, 0x32, 0x31, 0x39, 0x33, 0x34, 0x32, 0x31, 0x5a, 0x30, 0x64, 0x30, 0x62, 0x30, 0x3a,
    0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14, 0xfd, 0x6e, 0xf7,
    0x31, 0xd8, 0xfe, 0x64, 0xb8, 0x8b, 0x8a, 0x95, 0x54, 0x3b, 0x47, 0x2d, 0xa2, 0xde, 0xcc, 0xdc,
    0x13, 0x04, 0x14, 0x09, 0x65, 0xfe, 0x88, 0x54, 0x3e, 0x79, 0xc6, 0x68, 0x6d, 0x1b, 0x2e, 0x7a,
    0xc8, 0xf3, 0x60, 0xe1, 0x91, 0x17, 0x97, 0x02, 0x01, 0x02, 0x80, 0x00, 0x18, 0x0f, 0x32, 0x30,
    0x32, 0x36, 0x31, 0x31, 0x31, 0x32, 0x31, 0x32, 0x30, 0x30, 0x30, 0x30, 0x5a, 0xa0, 0x11, 0x18,
    0x0f, 0x32, 0x30, 0x32, 0x36, 0x31, 0x31, 0x31, 0x33, 0x31, 0x32, 0x30, 0x30, 0x30, 0x30, 0x5a,
    0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x48, 0x00, 0x30,
    0x45, 0x02, 0x21, 0x00, 0xc0, 0x1b, 0xd6, 0xf1, 0xe3, 0x59, 0xc8, 0xb5, 0xb8, 0x9f, 0x0e, 0x57,
    0x8b, 0xdd, 0x9c, 0xd4, 0x66, 0x9f, 0x5a, 0xb0, 0x20, 0x60, 0xdb, 0x37, 0x8a, 0x11, 0x4a, 0x26,
    0xf9, 0x3c, 0x68, 0x5e, 0x02, 0x20, 0x2a, 0x91, 0xca, 0x82, 0x8b, 0x08, 0x02, 0xfe, 0x98, 0x50,
    0xc9, 0x91, 0x51, 0xad, 0x12, 0xca, 0x0d, 0xa1, 0x4d, 0x5e, 0x6d, 0x60, 0x1d, 0x71, 0xce, 0xfe,
    0x09, 0x07, 0xf4, 0xde, 0xf5, 0x67,
};
constexpr unsigned char BasicResponseType[] {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01};
constexpr std::size_t SuccessfulTypeOffset = 15;
constexpr std::size_t SuccessfulResponseOffset = 27;
constexpr int SuccessfulResponseLength = 235;

using Bytes = std::vector<unsigned char>;

Bytes Tlv(unsigned char tag, std::initializer_list<Bytes> parts) {
    Bytes content;
    for (const auto& part : parts) content.insert(content.end(), part.begin(), part.end());
    Require(content.size() < 0x80);
    Bytes result {tag, static_cast<unsigned char>(content.size())};
    result.insert(result.end(), content.begin(), content.end());
    return result;
}

Bytes Join(std::initializer_list<Bytes> parts) {
    Bytes result;
    for (const auto& part : parts) result.insert(result.end(), part.begin(), part.end());
    return result;
}

const Bytes ResponseType {0x06, 0x09, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01};
const Bytes Eoc {0x00, 0x00};

constexpr std::uint64_t Asn1(int reason) {
    return (std::uint64_t{13} << 24) | (std::uint64_t{0xfff} << 12) | static_cast<std::uint64_t>(reason);
}

std::vector<std::uint64_t> Errors() {
    std::vector<std::uint64_t> errors;
    while (const auto error = ERR_get_error_nid_postfix()) errors.push_back(error);
    return errors;
}

void RequireErrors(const std::vector<int>& reasons, std::source_location location = std::source_location::current()) {
    std::vector<std::uint64_t> expected;
    for (const int reason : reasons) expected.push_back(Asn1(reason));
    Require(Errors() == expected, location);
}

LibreSslOcspResponseBytes* ResponseBytes(const LibreSslOcspResponse* response) {
    return static_cast<LibreSslOcspResponseBytes*>(response->responseBytes);
}

void RequireBytes(const LibreSslOcspResponse* response, const unsigned char* octets, int length) {
    const auto* bytes = ResponseBytes(response);
    Require(bytes && bytes->responseType && bytes->response);
    const auto* type = bytes->responseType;
    Require(type->length == static_cast<int>(sizeof(BasicResponseType)));
    Require(std::memcmp(type->data, BasicResponseType, sizeof(BasicResponseType)) == 0);
    Require(type->flags == 0x09 && type->nid == 0 && !type->shortName && !type->longName);
    Require(bytes->response->type == 4 && bytes->response->length == length && bytes->response->flags == 0);
    Require(std::memcmp(bytes->response->data, octets, static_cast<std::size_t>(length)) == 0);
    Require(bytes->response->data[length] == 0);
}

LibreSslOcspResponse* DecodeAll(const Bytes& der, std::size_t consumed) {
    const unsigned char* input = der.data();
    auto* response = d2i_OCSP_RESPONSE_nid_postfix(nullptr, &input, static_cast<std::int64_t>(der.size()));
    Require(response && input == der.data() + consumed);
    RequireErrors({});
    return response;
}

void RequireFailure(const Bytes& der, const std::vector<int>& reasons, std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    const unsigned char* input = der.data();
    LibreSslOcspResponse* response = nullptr;
    Require(d2i_OCSP_RESPONSE_nid_postfix(&response, &input, static_cast<std::int64_t>(der.size())) == nullptr, location);
    Require(!response && input == der.data(), location);
    RequireErrors(reasons, location);
    Require(live.empty(), location);
}

void TestUnsuccessfulResponses() {
    const struct {
        const unsigned char* der;
        int status;
    } cases[] {{TryLater, 3}, {MalformedRequest, 1}, {Unauthorized, 6}};
    for (const auto& item : cases) {
        ERR_clear_error_nid_postfix();
        const unsigned char* input = item.der;
        LibreSslOcspResponse* target = nullptr;
        auto* response = d2i_OCSP_RESPONSE_nid_postfix(&target, &input, 5);
        Require(response && target == response && input == item.der + 5);
        RequireErrors({});
        Require(OCSP_response_status_nid_postfix(response) == item.status);
        const auto* status = response->responseStatus;
        Require(status->type == 10 && status->length == 1 && status->flags == 0 && status->data[0] == item.status);
        Require(!response->responseBytes);
        Require(OCSP_response_get1_basic_nid_postfix(response) == nullptr);
        Require(Errors() == std::vector<std::uint64_t>{(std::uint64_t{39} << 24) | (std::uint64_t{0xfff} << 12) | 108});
        Require(live.size() == 3);
        OCSP_RESPONSE_free_nid_postfix(response);
        Require(live.empty());
    }
}

void TestSuccessfulResponse() {
    ERR_clear_error_nid_postfix();
    const unsigned char* input = Successful;
    auto* response = d2i_OCSP_RESPONSE_nid_postfix(nullptr, &input, sizeof(Successful));
    Require(response && input == Successful + sizeof(Successful));
    RequireErrors({});
    Require(OCSP_response_status_nid_postfix(response) == 0);
    Require(response->responseStatus->length == 1 && response->responseStatus->data[0] == 0);
    RequireBytes(response, Successful + SuccessfulResponseOffset, SuccessfulResponseLength);
    Require(std::memcmp(ResponseBytes(response)->responseType->data, Successful + SuccessfulTypeOffset, sizeof(BasicResponseType)) == 0);
    Require(live.size() == 8);
    OCSP_RESPONSE_free_nid_postfix(response);
    Require(live.empty());

    Bytes trailing(TryLater, TryLater + sizeof(TryLater));
    trailing.insert(trailing.end(), {0xde, 0xad, 0x30});
    response = DecodeAll(trailing, sizeof(TryLater));
    Require(OCSP_response_status_nid_postfix(response) == 3);
    OCSP_RESPONSE_free_nid_postfix(response);
    Require(live.empty());
}

void TestReuse() {
    ERR_clear_error_nid_postfix();
    LibreSslOcspResponse* target = nullptr;
    const unsigned char* input = Successful;
    auto* response = d2i_OCSP_RESPONSE_nid_postfix(&target, &input, sizeof(Successful));
    Require(response && target == response);
    auto* bytes = ResponseBytes(response);
    auto* type = bytes->responseType;
    auto* octets = bytes->response;
    auto* status = response->responseStatus;

    input = Successful;
    Require(d2i_OCSP_RESPONSE_nid_postfix(&target, &input, sizeof(Successful)) == response);
    Require(target == response && input == Successful + sizeof(Successful));
    Require(ResponseBytes(response) == bytes && bytes->responseType == type && bytes->response == octets);
    Require(response->responseStatus == status);
    RequireBytes(response, Successful + SuccessfulResponseOffset, SuccessfulResponseLength);
    Require(live.size() == 8);

    input = TryLater;
    Require(d2i_OCSP_RESPONSE_nid_postfix(&target, &input, sizeof(TryLater)) == response);
    Require(target == response && input == TryLater + sizeof(TryLater));
    Require(!response->responseBytes && response->responseStatus == status);
    Require(OCSP_response_status_nid_postfix(response) == 3);
    Require(live.size() == 3);

    const Bytes small = Tlv(0x30, {{0x0a, 0x01, 0x00}, Tlv(0xa0, {Tlv(0x30, {ResponseType, {0x04, 0x02, 0xaa, 0xbb}})})});
    input = small.data();
    Require(d2i_OCSP_RESPONSE_nid_postfix(&target, &input, static_cast<std::int64_t>(small.size())) == response);
    const unsigned char shortOctets[] {0xaa, 0xbb};
    RequireBytes(response, shortOctets, 2);
    octets = ResponseBytes(response)->response;
    input = Successful;
    Require(d2i_OCSP_RESPONSE_nid_postfix(&target, &input, sizeof(Successful)) == response);
    RequireBytes(response, Successful + SuccessfulResponseOffset, SuccessfulResponseLength);
    Require(ResponseBytes(response)->response == octets);
    const auto* data = octets->data;
    input = small.data();
    reallocations = 0;
    Require(d2i_OCSP_RESPONSE_nid_postfix(&target, &input, static_cast<std::int64_t>(small.size())) == response);
    RequireBytes(response, shortOctets, 2);
    Require(octets->data == data && reallocations == 0 && live.size() == 8);
    RequireErrors({});

    const unsigned char integerStatus[] {0x30, 0x03, 0x02, 0x01, 0x03};
    input = integerStatus;
    Require(d2i_OCSP_RESPONSE_nid_postfix(&target, &input, sizeof(integerStatus)) == nullptr);
    Require(!target && input == integerStatus && live.empty());
    RequireErrors({168, 58, 58});

    target = nullptr;
    input = TryLater;
    response = d2i_OCSP_RESPONSE_nid_postfix(&target, &input, sizeof(TryLater));
    Require(response && target == response);
    input = Successful;
    Require(d2i_OCSP_RESPONSE_nid_postfix(&target, &input, 0) == nullptr);
    Require(!target && input == Successful && live.empty());
    RequireErrors({123, 102, 58});

    target = nullptr;
    input = TryLater;
    response = d2i_OCSP_RESPONSE_nid_postfix(&target, &input, sizeof(TryLater));
    Require(response && !response->responseBytes);
    input = Successful;
    Require(d2i_OCSP_RESPONSE_nid_postfix(&target, &input, sizeof(Successful)) == response);
    RequireBytes(response, Successful + SuccessfulResponseOffset, SuccessfulResponseLength);
    OCSP_RESPONSE_free_nid_postfix(response);
    Require(live.empty());
    RequireErrors({});
}

void TestStatusValues() {
    const struct {
        Bytes content;
        int status;
        int type;
        Bytes data;
    } cases[] {
        {{0x0a, 0x02, 0x00, 0x05}, 5, 10, {0x05}},
        {{0x0a, 0x01, 0xff}, -1, 0x10a, {0x01}},
        {{0x0a, 0x02, 0xff, 0x00}, -256, 0x10a, {0x01, 0x00}},
        {{0x0a, 0x02, 0xff, 0x80}, -128, 0x10a, {0x80}},
        {{0x0a, 0x02, 0x80, 0x01}, -32767, 0x10a, {0x7f, 0xff}},
        {{0x0a, 0x02, 0x01, 0x00}, 256, 10, {0x01, 0x00}},
        {{0x0a, 0x00}, 0, 10, {}},
        {{0x2a, 0x06, 0x04, 0x01, 0x01, 0x81, 0x01, 0x02}, 0x0102, 10, {0x01, 0x02}},
    };
    for (const auto& item : cases) {
        const Bytes der = Tlv(0x30, {item.content});
        auto* response = DecodeAll(der, der.size());
        const auto* status = response->responseStatus;
        Require(status->type == item.type && status->length == static_cast<int>(item.data.size()));
        Require(item.data.empty() || std::memcmp(status->data, item.data.data(), item.data.size()) == 0);
        Require(OCSP_response_status_nid_postfix(response) == item.status);
        OCSP_RESPONSE_free_nid_postfix(response);
        Require(live.empty());
    }
}

void TestIndefiniteAndConstructed() {
    const unsigned char octets[] {0xaa, 0xbb};
    Bytes der = Join({{0x30, 0x80, 0x0a, 0x01, 0x03}, Eoc, {0x55}});
    auto* response = DecodeAll(der, der.size() - 1);
    Require(OCSP_response_status_nid_postfix(response) == 3 && !response->responseBytes);
    OCSP_RESPONSE_free_nid_postfix(response);

    der = Join({{0x30, 0x80, 0x0a, 0x01, 0x00, 0xa0, 0x80, 0x30, 0x80}, ResponseType, {0x04, 0x02, 0xaa, 0xbb}, Eoc, Eoc, Eoc, {0x55}});
    response = DecodeAll(der, der.size() - 1);
    Require(OCSP_response_status_nid_postfix(response) == 0);
    RequireBytes(response, octets, 2);
    OCSP_RESPONSE_free_nid_postfix(response);

    der = Tlv(0x30, {{0x0a, 0x01, 0x00}, Tlv(0xa0, {Tlv(0x30, {ResponseType, {0x24, 0x80, 0x04, 0x01, 0xaa, 0x04, 0x01, 0xbb}, Eoc})})});
    response = DecodeAll(der, der.size());
    RequireBytes(response, octets, 2);
    OCSP_RESPONSE_free_nid_postfix(response);

    der = Tlv(0x30, {{0x0a, 0x01, 0x00}, Tlv(0xa0, {Tlv(0x30, {ResponseType, Tlv(0x24, {Tlv(0x24, {{0x04, 0x01, 0xaa}}), {0x04, 0x00}, Tlv(0x84, {{0xbb}})})})})});
    response = DecodeAll(der, der.size());
    RequireBytes(response, octets, 2);
    OCSP_RESPONSE_free_nid_postfix(response);

    der = Tlv(0x30, {{0x0a, 0x01, 0x00}, Tlv(0xa0, {Tlv(0x30, {Tlv(0x26, {{0x06, 0x05, 0x2b, 0x06, 0x01, 0x05, 0x05}, {0x04, 0x04, 0x07, 0x30, 0x01, 0x01}}), {0x04, 0x02, 0xaa, 0xbb}})})});
    response = DecodeAll(der, der.size());
    RequireBytes(response, octets, 2);
    OCSP_RESPONSE_free_nid_postfix(response);
    Require(live.empty());
}

Bytes NestedOctets(int depth) {
    Bytes inner {0x04, 0x01, 0xaa};
    for (int level = 0; level < depth; ++level) inner = Tlv(0x24, {inner});
    return inner;
}

Bytes WithResponseBytes(const Bytes& sequence) {
    return Tlv(0x30, {{0x0a, 0x01, 0x00}, Tlv(0xa0, {sequence})});
}

void TestMalformed() {
    RequireFailure({}, {123, 102, 58});
    RequireFailure(Bytes(Successful, Successful + sizeof(Successful) - 1), {155, 102, 58});
    RequireFailure({0x30}, {123, 102, 58});
    RequireFailure({0x1f}, {123, 102, 58});
    RequireFailure({0x30, 0x82, 0x01}, {123, 102, 58});
    RequireFailure({0x30, 0x89, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, {123, 102, 58});
    RequireFailure({0x10, 0x80, 0x00, 0x00}, {123, 102, 58});
    RequireFailure({0x31, 0x03, 0x0a, 0x01, 0x03}, {168, 58});
    RequireFailure({0x10, 0x03, 0x0a, 0x01, 0x03}, {149});
    RequireFailure({0x30, 0x00}, {121});
    RequireFailure({0x30, 0x02, 0x00, 0x00}, {159});
    RequireFailure({0x30, 0x80, 0x0a, 0x01, 0x03}, {137});
    RequireFailure({0x30, 0x80, 0x00, 0x00}, {121});
    RequireFailure({0x30, 0x03, 0x02, 0x01, 0x03}, {168, 58, 58});
    RequireFailure({0x30, 0x03, 0x00, 0x01, 0x00}, {168, 58, 58});
    RequireFailure({0x30, 0x03, 0x0a, 0x02, 0x03}, {155, 102, 58, 58});
    RequireFailure({0x30, 0x05, 0x0a, 0x01, 0x03, 0x05, 0x00}, {168, 58});
    RequireFailure({0x30, 0x05, 0x0a, 0x01, 0x00, 0x80, 0x00}, {120});
    RequireFailure({0x30, 0x05, 0x0a, 0x01, 0x00, 0xa0, 0x00}, {123, 102, 58, 58, 58});
    RequireFailure(Join({Tlv(0x30, {{0x0a, 0x01, 0x00}, Tlv(0xa0, {Tlv(0x30, {ResponseType, {0x04, 0x00}})}), {0x05, 0x00}})}), {148});
    RequireFailure(Tlv(0x30, {{0x0a, 0x01, 0x00}, Tlv(0xa0, {Tlv(0x30, {ResponseType, {0x04, 0x00}}), {0x05, 0x00}})}), {119});
    RequireFailure(Join({{0x30, 0x80, 0x0a, 0x01, 0x00, 0xa0, 0x80}, Tlv(0x30, {ResponseType, {0x04, 0x00}}), Eoc}), {137});
    RequireFailure(WithResponseBytes(Tlv(0x30, {ResponseType})), {121, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {})), {121, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {Eoc})), {159, 58, 58});
    RequireFailure(WithResponseBytes({0x10, 0x00}), {149, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {ResponseType, {0x04, 0x00}, {0x05, 0x00}})), {148, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {{0x04, 0x00}, {0x04, 0x00}})), {168, 58, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {{0x06, 0x02, 0x80, 0x01}, {0x04, 0x00}})), {216, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {{0x06, 0x02, 0x2b, 0x81}, {0x04, 0x00}})), {216, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {{0x06, 0x00}, {0x04, 0x00}})), {216, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {{0x06, 0x03, 0x2b, 0x80, 0x01}, {0x04, 0x00}})), {216, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {ResponseType, {0x04, 0x02, 0xaa}})), {155, 102, 58, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {ResponseType, {0x04, 0x80, 0x00, 0x00}})), {123, 102, 58, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {ResponseType, NestedOctets(7)})), {197, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {ResponseType, {0x24, 0x02, 0x00, 0x00}})), {159, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {ResponseType, {0x24, 0x80, 0x04, 0x01, 0xaa}})), {137, 58, 58, 58});
    RequireFailure(WithResponseBytes(Tlv(0x30, {ResponseType, {0x24, 0x03, 0x04, 0x02, 0xaa}})), {155, 102, 58, 58, 58, 58});

    const Bytes nested = WithResponseBytes(Tlv(0x30, {ResponseType, NestedOctets(6)}));
    auto* response = DecodeAll(nested, nested.size());
    const unsigned char octet[] {0xaa};
    RequireBytes(response, octet, 1);
    OCSP_RESPONSE_free_nid_postfix(response);
    Require(live.empty());
}

void TestArguments() {
    ERR_clear_error_nid_postfix();
    RequireThrows([] { d2i_OCSP_RESPONSE_nid_postfix(nullptr, nullptr, 5); });
    const unsigned char* input = nullptr;
    RequireThrows([&] { d2i_OCSP_RESPONSE_nid_postfix(nullptr, &input, 5); });
    Require(Errors().empty());
    LibreSslOcspResponse* target = nullptr;
    Require(d2i_OCSP_RESPONSE_nid_postfix(&target, &input, 0) == nullptr && !input && !target);
    RequireErrors({123, 102, 58});
    input = TryLater;
    Require(d2i_OCSP_RESPONSE_nid_postfix(nullptr, &input, -1) == nullptr && input == TryLater);
    RequireErrors({123, 102, 58});
    Require(live.empty());
}

void TestAllocationFailures() {
    const Bytes constructed = WithResponseBytes(Tlv(0x30, {ResponseType, Tlv(0x24, {{0x04, 0x01, 0xaa}})}));
    for (const auto& der : {Bytes(Successful, Successful + sizeof(Successful)), constructed}) {
        allocations = 0;
        auto* response = DecodeAll(der, der.size());
        OCSP_RESPONSE_free_nid_postfix(response);
        const int total = allocations;
        Require(total == 8);
        for (int index = 0; index < total; ++index) {
            ERR_clear_error_nid_postfix();
            allocations = 0;
            failAt = index;
            const unsigned char* input = der.data();
            Require(d2i_OCSP_RESPONSE_nid_postfix(nullptr, &input, static_cast<std::int64_t>(der.size())) == nullptr);
            failAt = -1;
            Require(input == der.data() && live.empty());
            const auto errors = Errors();
            Require(!errors.empty() && errors.front() == Asn1(65));
            for (const auto error : errors) Require((error >> 24) == 13);
        }
    }

    const unsigned char statusFailure[] {0x30, 0x03, 0x0a, 0x01, 0x03};
    const struct {
        int index;
        std::vector<int> reasons;
    } cases[] {{0, {65, 58}}, {1, {65, 65, 65, 58}}, {2, {65, 58}}};
    for (const auto& item : cases) {
        ERR_clear_error_nid_postfix();
        allocations = 0;
        failAt = item.index;
        const unsigned char* input = statusFailure;
        Require(d2i_OCSP_RESPONSE_nid_postfix(nullptr, &input, sizeof(statusFailure)) == nullptr);
        failAt = -1;
        Require(live.empty());
        RequireErrors(item.reasons);
    }

    const struct {
        int index;
        std::vector<int> reasons;
    } bytesCases[] {{3, {65, 58, 58, 58}}, {4, {65, 65, 65, 58, 58, 58}}, {5, {65, 58, 58, 58}},
        {6, {65, 58, 58, 58}}, {7, {65, 65, 58, 58, 58}}};
    for (const auto& item : bytesCases) {
        ERR_clear_error_nid_postfix();
        allocations = 0;
        failAt = item.index;
        const unsigned char* input = Successful;
        Require(d2i_OCSP_RESPONSE_nid_postfix(nullptr, &input, sizeof(Successful)) == nullptr);
        failAt = -1;
        Require(live.empty());
        RequireErrors(item.reasons);
    }
}

}

int main() {
    InitializeHeap();
    TestUnsuccessfulResponses();
    TestSuccessfulResponse();
    TestReuse();
    TestStatusValues();
    TestIndefiniteAndConstructed();
    TestMalformed();
    TestArguments();
    TestAllocationFailures();
    std::puts("libSceLibreSsl d2i_OCSP_RESPONSE tests passed");
}
