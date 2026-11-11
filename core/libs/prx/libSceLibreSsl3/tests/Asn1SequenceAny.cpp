#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ASN.1 sequence check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
int failAfter = -1;

bool FailNow() {
    if (failAfter < 0) return false;
    return failAfter-- == 0;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (FailNow()) return nullptr;
    void* result = std::malloc(size ? size : 1);
    Require(result != nullptr);
    Require(allocations.emplace(result, size).second);
    std::memset(result, 0xa5, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    auto* result = Allocate_nid_no_patch(count * size);
    if (result) std::memset(result, 0, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    if (FailNow()) return nullptr;
    const auto found = allocations.find(pointer);
    Require(found != allocations.end());
    void* result = std::malloc(size ? size : 1);
    Require(result != nullptr);
    std::memcpy(result, pointer, found->second < size ? found->second : size);
    allocations.erase(found);
    std::free(pointer);
    Require(allocations.emplace(result, size).second);
    return result;
}

void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

constexpr unsigned long Asn1Error = 0x0dfff000;

void ExpectErrors(std::initializer_list<int> reasons, std::source_location location = std::source_location::current()) {
    for (const int reason : reasons) Require(ERR_get_error_nid_postfix() == (Asn1Error | static_cast<unsigned long>(reason)), location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

void Release(LibreSslStack* stack) {
    if (!stack) return;
    for (int index = 0; index < stack->num; ++index) ASN1_TYPE_free_nid_postfix(reinterpret_cast<Asn1Type*>(stack->data[index]));
    sk_free_nid_postfix(stack);
}

Bytes Sequence(const Bytes& content) {
    Require(content.size() < 0x80);
    Bytes result{0x30, static_cast<unsigned char>(content.size())};
    result.insert(result.end(), content.begin(), content.end());
    return result;
}

LibreSslStack* Decode(const Bytes& encoding, std::size_t consumed, std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    const unsigned char* cursor = encoding.data();
    auto* stack = d2i_ASN1_SEQUENCE_ANY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(encoding.size()));
    Require(stack != nullptr, location);
    Require(cursor == encoding.data() + consumed, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
    return stack;
}

void Rejects(const Bytes& encoding, std::initializer_list<int> reasons, std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    const unsigned char* cursor = encoding.data();
    Require(d2i_ASN1_SEQUENCE_ANY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(encoding.size())) == nullptr, location);
    Require(cursor == encoding.data(), location);
    ExpectErrors(reasons, location);
    Require(allocations.empty(), location);
}

const Asn1Type& Element(const LibreSslStack* stack, int index) {
    Require(index < stack->num);
    return *reinterpret_cast<const Asn1Type*>(stack->data[index]);
}

const Asn1String& String(const LibreSslStack* stack, int index, int type, std::string_view data, std::source_location location = std::source_location::current()) {
    const auto& element = Element(stack, index);
    const auto& string = *static_cast<const Asn1String*>(element.value.pointer);
    Require(string.type == type, location);
    Require(string.length == static_cast<int>(data.size()), location);
    Require(data.empty() || std::memcmp(string.data, data.data(), data.size()) == 0, location);
    return string;
}

void Values() {
    const Bytes encoding = Sequence({
        0x02, 0x01, 0x05,
        0x02, 0x02, 0xff, 0x7f,
        0x05, 0x00,
        0x01, 0x01, 0xff,
        0x06, 0x06, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
        0x04, 0x02, 'a', 'b',
        0x03, 0x02, 0x03, 0xff,
        0x0c, 0x00,
        0x30, 0x03, 0x02, 0x01, 0x01,
        0x80, 0x01, 0x07,
        0x0a, 0x01, 0xff,
        0x17, 0x0d, '2', '4', '0', '2', '2', '9', '2', '3', '5', '9', '5', '9', 'Z',
        0x02, 0x02, 0xff, 0x00,
    });
    Bytes trailing = encoding;
    trailing.push_back(0x05);
    auto* stack = Decode(trailing, encoding.size());
    Require(stack->num == 13 && stack->comp == nullptr && stack->sorted == 0);
    Require(Element(stack, 0).type == 2 && Element(stack, 1).type == 2);
    String(stack, 0, 2, "\x05");
    String(stack, 1, 0x102, "\x81");
    Require(Element(stack, 2).type == 5 && Element(stack, 2).value.pointer == nullptr);
    Require(Element(stack, 3).type == 1 && Element(stack, 3).value.boolean == 0xff);
    Require(Element(stack, 4).type == 6);
    const auto& object = *static_cast<const Asn1Object_nid_no_patch*>(Element(stack, 4).value.pointer);
    Require(object.length == 6 && object.flags == 9 && object.nid == 0 && !object.shortName && !object.longName);
    Require(std::memcmp(object.data, "\x2a\x86\x48\x86\xf7\x0d", 6) == 0);
    Require(Element(stack, 5).type == 4);
    const auto& octets = String(stack, 5, 4, "ab");
    Require(octets.data[2] == 0 && octets.flags == 0);
    Require(Element(stack, 6).type == 3);
    Require(String(stack, 6, 3, "\xf8").flags == 0x0b);
    Require(Element(stack, 7).type == 12);
    const auto& empty = String(stack, 7, 12, "");
    Require(empty.data != nullptr && empty.data[0] == 0);
    Require(Element(stack, 8).type == 16);
    String(stack, 8, 16, std::string_view("\x30\x03\x02\x01\x01", 5));
    Require(Element(stack, 9).type == -3);
    String(stack, 9, -3, std::string_view("\x80\x01\x07", 3));
    Require(Element(stack, 10).type == 10);
    String(stack, 10, 0x10a, "\x01");
    Require(Element(stack, 11).type == 23);
    String(stack, 11, 23, "240229235959Z");
    String(stack, 12, 0x102, std::string_view("\x01\x00", 2));
    Release(stack);
    Require(allocations.empty());

    stack = Decode({0x30, 0x00}, 2);
    Require(stack->num == 0 && stack->numAlloc == 4);
    Release(stack);
    stack = Decode({0x10, 0x00}, 2);
    Require(stack->num == 0);
    Release(stack);
    stack = Decode({0x30, 0x81, 0x03, 0x1f, 0x64, 0x00}, 6);
    Require(Element(stack, 0).type == 100);
    String(stack, 0, 100, "");
    Release(stack);
    Require(allocations.empty());
}

void Indefinite() {
    auto* stack = Decode({0x30, 0x80, 0x02, 0x01, 0x05, 0x00, 0x00, 0x05}, 7);
    Require(stack->num == 1);
    String(stack, 0, 2, "\x05");
    Release(stack);
    stack = Decode({0x30, 0x80, 0x30, 0x80, 0x31, 0x80, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00}, 16);
    Require(stack->num == 2 && Element(stack, 0).type == 16 && Element(stack, 1).type == 4);
    String(stack, 0, 16, std::string_view("\x30\x80\x31\x80\x00\x00\x05\x00\x00\x00", 10));
    Release(stack);
    stack = Decode({0x30, 0x80, 0x24, 0x80, 0x04, 0x01, 'a', 0x24, 0x80, 0x0c, 0x01, 'b', 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x07, 0x00, 0x00}, 21);
    Require(stack->num == 2 && Element(stack, 0).type == 4);
    String(stack, 0, 4, "ab");
    String(stack, 1, 2, "\x07");
    Release(stack);
    stack = Decode({0x30, 0x08, 0x24, 0x03, 0x04, 0x01, 'a', 0x02, 0x01, 0x05}, 10);
    Require(stack->num == 1 && Element(stack, 0).type == 4);
    String(stack, 0, 4, "a\x05");
    Release(stack);
    Require(allocations.empty());
    Rejects({0x30, 0x80, 0x02, 0x01, 0x05}, {137});
    Rejects({0x30, 0x80, 0x30, 0x80, 0x05, 0x00, 0x00, 0x00}, {137});
    Rejects({0x30, 0x80, 0x24, 0x80, 0x04, 0x01, 'a'}, {137, 58});
    Rejects({0x30, 0x02, 0x00, 0x00}, {159});
    Rejects({0x30, 0x80, 0x24, 0x80, 0x24, 0x80, 0x24, 0x80, 0x24, 0x80, 0x24, 0x80, 0x24, 0x80, 0x24, 0x80}, {197, 58});
}

void Errors() {
    Rejects({}, {102, 58});
    Rejects({0x30}, {102, 58});
    Rejects({0x31, 0x00}, {168, 58});
    Rejects({0xb0, 0x00}, {168, 58});
    Rejects({0x30, 0x01}, {155, 58});
    Rejects({0x30, 0xff, 0x00}, {102, 58});
    Rejects({0x10, 0x80}, {102, 58});
    Rejects({0x30, 0x84, 0x01}, {102, 58});
    Rejects(Sequence({0x02}), {102, 58, 58});
    Rejects(Sequence({0x02, 0x02, 0x01}), {155, 58, 58});
    Rejects(Sequence({0x1f, 0x88, 0x80, 0x80, 0x80, 0x00, 0x00}), {155, 58, 58});
    Rejects(Sequence({0x05, 0x01, 0x00}), {144, 58});
    Rejects(Sequence({0x01, 0x02, 0x00, 0x00}), {106, 58});
    Rejects(Sequence({0x10, 0x00}), {156, 58});
    Rejects(Sequence({0x11, 0x00}), {156, 58});
    Rejects(Sequence({0x22, 0x00}), {231, 58});
    Rejects(Sequence({0x25, 0x00}), {231, 58});
    Rejects(Sequence({0x06, 0x00}), {216, 58});
    Rejects(Sequence({0x06, 0x02, 0x80, 0x01}), {216, 58});
    Rejects(Sequence({0x06, 0x02, 0x2a, 0x86}), {216, 58});
    Rejects(Sequence({0x03, 0x00}), {152, 58});
    Rejects(Sequence({0x03, 0x02, 0x08, 0xff}), {220, 58});
    Rejects(Sequence({0x02, 0x00}), {62, 58});
    Rejects(Sequence({0x02, 0x02, 0x00, 0x01}), {62, 58});
    Rejects(Sequence({0x0a, 0x02, 0xff, 0x80}), {62, 58});
    Rejects(Sequence({0x1e, 0x01, 'a'}), {214, 58});
    Rejects(Sequence({0x1c, 0x02, 'a', 'b'}), {215, 58});
    Rejects(Sequence({0x17, 0x0d, '2', '3', '0', '2', '2', '9', '2', '3', '5', '9', '5', '9', 'Z'}), {132, 58});
    Rejects(Sequence({0x17, 0x0b, '2', '4', '0', '2', '2', '9', '2', '3', '5', '9', 'Z'}), {132, 58});
    Rejects(Sequence({0x18, 0x0d, '2', '4', '0', '2', '2', '9', '2', '3', '5', '9', '5', '9', 'Z'}), {132, 58});
    Rejects(Sequence({0x02, 0x01, 0x05, 0x05, 0x01, 0x00}), {144, 58});
}

void Arguments() {
    const Bytes encoding = Sequence({0x02, 0x01, 0x05});
    const unsigned char* cursor = encoding.data();
    ERR_clear_error_nid_postfix();
    Require(d2i_ASN1_SEQUENCE_ANY_nid_postfix(nullptr, &cursor, -1) == nullptr && cursor == encoding.data());
    Require(d2i_ASN1_SEQUENCE_ANY_nid_postfix(nullptr, nullptr, -1) == nullptr);
    ExpectErrors({});
    bool threw = false;
    try { d2i_ASN1_SEQUENCE_ANY_nid_postfix(nullptr, nullptr, 0); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    const unsigned char* missing = nullptr;
    threw = false;
    try { d2i_ASN1_SEQUENCE_ANY_nid_postfix(nullptr, &missing, 1); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    Require(d2i_ASN1_SEQUENCE_ANY_nid_postfix(nullptr, &missing, 0) == nullptr && missing == nullptr);
    ExpectErrors({102, 58});

    LibreSslStack* output = nullptr;
    auto* first = d2i_ASN1_SEQUENCE_ANY_nid_postfix(&output, &cursor, static_cast<std::int64_t>(encoding.size()));
    Require(first != nullptr && output == first && cursor == encoding.data() + encoding.size());
    const auto held = allocations.size();
    cursor = encoding.data();
    auto* second = d2i_ASN1_SEQUENCE_ANY_nid_postfix(&output, &cursor, static_cast<std::int64_t>(encoding.size()));
    Require(second != nullptr && output == second && allocations.size() == held);
    const Bytes wrong{0x31, 0x00};
    cursor = wrong.data();
    Require(d2i_ASN1_SEQUENCE_ANY_nid_postfix(&output, &cursor, 2) == nullptr && output == nullptr);
    ExpectErrors({168, 58});
    Require(allocations.empty());

    output = sk_new_nid_postfix(nullptr);
    Require(sk_push_nid_postfix(output, nullptr) == 1);
    cursor = wrong.data();
    Require(d2i_ASN1_SEQUENCE_ANY_nid_postfix(&output, &cursor, 2) == nullptr && output == nullptr);
    ExpectErrors({168, 58});
    Require(allocations.empty());
}

void AllocationFailures() {
    const Bytes encoding = Sequence({0x02, 0x01, 0x05, 0x06, 0x01, 0x2a, 0x04, 0x01, 'a', 0x03, 0x02, 0x00, 0x01});
    const std::array<std::initializer_list<int>, 15> expected{{
        {65}, {65},
        {65, 58}, {58}, {65, 65, 58},
        {65, 58}, {58}, {65, 58},
        {65, 58}, {65, 65, 58}, {65, 58},
        {65, 58}, {58}, {65, 65, 58},
        {65},
    }};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        ERR_clear_error_nid_postfix();
        failAfter = static_cast<int>(index);
        const unsigned char* cursor = encoding.data();
        Require(d2i_ASN1_SEQUENCE_ANY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(encoding.size())) == nullptr);
        Require(failAfter == -1 && cursor == encoding.data());
        ExpectErrors(expected[index]);
        Require(allocations.empty());
    }
    failAfter = static_cast<int>(expected.size());
    Release(Decode(encoding, encoding.size()));
    Require(failAfter == 0);
    failAfter = -1;
    Require(allocations.empty());
}

} // namespace

int main() {
    std::array<void*, 10> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch), reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Values();
    Indefinite();
    Errors();
    Arguments();
    AllocationFailures();
    Require(allocations.empty());
    std::puts("LibreSSL3 ASN.1 sequence tests passed");
}
