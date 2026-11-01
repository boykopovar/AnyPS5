#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <unordered_map>

static_assert(sizeof(LibreSslAsn1Object) == 40);
static_assert(offsetof(LibreSslAsn1Object, shortName) == 0);
static_assert(offsetof(LibreSslAsn1Object, longName) == 8);
static_assert(offsetof(LibreSslAsn1Object, nid) == 16);
static_assert(offsetof(LibreSslAsn1Object, length) == 20);
static_assert(offsetof(LibreSslAsn1Object, data) == 24);
static_assert(offsetof(LibreSslAsn1Object, flags) == 32);

namespace {

std::unordered_map<void*, std::size_t> allocations;
int failAfter = -1;
int attempts = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "OBJ_txt2obj check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (attempts++ == failAfter) return nullptr;
    void* pointer = std::malloc(size);
    Require(pointer != nullptr);
    allocations.emplace(pointer, size);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI AllocateZeroed_nid_no_patch(std::size_t count, std::size_t size) {
    Require(count == 1 && size == sizeof(LibreSslAsn1Object));
    void* pointer = Allocate_nid_no_patch(size);
    if (pointer) std::memset(pointer, 0, size);
    return pointer;
}
void* APS5_VABI UnexpectedReallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Release(LibreSslAsn1Object* object) {
    Require(object && object->flags == 9 && object->nid == 0);
    free_nid_postfix(const_cast<unsigned char*>(object->data));
    free_nid_postfix(object);
}

void Check(const char* text, std::initializer_list<unsigned char> expected) {
    for (const int noName : {0, 1, -1}) {
        auto* object = OBJ_txt2obj_nid_postfix(text, noName);
        Require(object && !object->shortName && !object->longName);
        Require(object->length == static_cast<int>(expected.size()));
        Require(std::equal(expected.begin(), expected.end(), object->data));
        Require(allocations.size() == 2);
        Release(object);
        Require(allocations.empty());
    }
}

void Numeric() {
    Check("0.0", {0});
    Check("1.39", {79});
    Check("2.0", {80});
    Check("2.999.3", {0x88, 0x37, 3});
    Check("1.2.840.113549.1.1.1", {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 1});
    Check("1 2 840 113549 1 1 1", {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 1, 1, 1});
    Check("01.02.03", {42, 3});
    Check("2.5.4.3", {85, 4, 3});
    Check("1.3.101.112", {43, 101, 112});
    Check("2.16.0.127.128.16383.16384", {96, 0, 127, 0x81, 0, 0xff, 0x7f, 0x81, 0x80, 0});
    Check("2.18446744073709551535", {0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});
    Check("2.16.18446744073709551615", {96, 0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});
    Check("2.16.18446744073709551616", {96, 0});
    Check("2.18446744073709551616", {80});
    Check("1.2.184467440737095516161", {42, 1});
    Check("1.2\0.3", {42});
    for (const char* text : {"", "0", "1.", "1..2", "1  2", "1.2.", "1 2 ", "1 2.3", "1.2 3", ".1.2", " 1.2", "1\t2",
        "3.0", "0.40", "1.40", "00.2", "1.00", "1.2.00", "-1.2", "1.-2", "1.+2", "1.a", "1.2x", "2.18446744073709551536",
        "2.16.184467440737095516159", "CN", "commonName", "unknown-name"}) {
        Require(OBJ_txt2obj_nid_postfix(text, 1) == nullptr);
        Require(allocations.empty());
    }
    std::string text = "1.2";
    for (int index = 0; index < 1024; ++index) text += ".1";
    auto* object = OBJ_txt2obj_nid_postfix(text.c_str(), 1);
    Require(object && object->length == 1025 && object->data[0] == 42);
    Require(std::all_of(object->data + 1, object->data + object->length, [](unsigned char value) { return value == 1; }));
    Release(object);
}

void NameLookupUnavailable() {
    for (const char* text : {"CN", "commonName", "Ed25519", "NULL", "not-a-known-object", ""}) {
        ERR_clear_error_nid_postfix();
        Require(OBJ_txt2obj_nid_postfix("1", 1) == nullptr);
        const int before = attempts;
        bool threw = false;
        try { OBJ_txt2obj_nid_postfix(text, 0); }
        catch (const std::runtime_error& error) {
            threw = std::string(error.what()).find("not implemented") != std::string::npos;
        }
        Require(threw && attempts == before && allocations.empty());
        Require(ERR_get_error_nid_postfix() == 0x0dfff08au);
        Require(ERR_get_error_nid_postfix() == 0);
    }
    for (int noName : {0, 1, -1}) {
        bool threw = false;
        try { OBJ_txt2obj_nid_postfix(nullptr, noName); }
        catch (const std::invalid_argument&) { threw = true; }
        Require(threw);
    }
}

void Errors() {
    struct Rejection { const char* text; int reason; };
    for (const Rejection rejection : {Rejection{"", 187}, {"1", 138}, {"1.", 187}, {"1.2a", 130},
        {"a.2", 131}, {"1 2.3", 131}, {"3.1", 122}, {"1.40", 147},
        {"2.18446744073709551536", 147}, {"2.1.184467440737095516159", 155},
        {"3.a", 130}, {"3.", 187}, {"3.00", 187}, {"1.40a", 130}, {"1.40.00", 147},
        {"1.2.184467440737095516160", 187}}) {
        ERR_clear_error_nid_postfix();
        Require(OBJ_txt2obj_nid_postfix(rejection.text, 1) == nullptr);
        Require(ERR_get_error_nid_postfix() == (0x0dfff000u | static_cast<unsigned>(rejection.reason)));
        Require(ERR_get_error_nid_postfix() == 0);
    }
    Require(OBJ_txt2obj_nid_postfix("1", 1) == nullptr);
    auto* object = OBJ_txt2obj_nid_postfix("1.2.3", 1);
    Require(object != nullptr);
    Require(ERR_get_error_nid_postfix() == 0x0dfff08au);
    Release(object);
}

void AllocationFailures() {
    for (const int allocation : {0, 1}) {
        failAfter = allocation;
        attempts = 0;
        ERR_clear_error_nid_postfix();
        Require(OBJ_txt2obj_nid_postfix("1.2.3", 1) == nullptr);
        Require(ERR_get_error_nid_postfix() == (allocation == 1 ? 0x0dfff041u : 0u));
        Require(attempts == allocation + 1 && allocations.empty());
    }
    failAfter = -1;
    auto* first = OBJ_txt2obj_nid_postfix("2.5.4.3", 1);
    auto* second = OBJ_txt2obj_nid_postfix("2.5.4.3", 1);
    Require(first != second && first->data != second->data && allocations.size() == 4);
    Release(second);
    Release(first);
    Require(allocations.empty());
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(AllocateZeroed_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedReallocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Numeric();
    NameLookupUnavailable();
    Errors();
    AllocationFailures();
    std::puts("OBJ_txt2obj tests passed");
}
