#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

long allocations = 0;

void* APS5_VABI Allocate(std::size_t size) { void* p = std::malloc(size ? size : 1); if (p) ++allocations; return p; }
void APS5_VABI Release(void* pointer) { if (pointer) --allocations; std::free(pointer); }
void* APS5_VABI Callocate(std::size_t count, std::size_t size) { void* p = std::calloc(count ? count : 1, size ? size : 1); if (p) ++allocations; return p; }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { if (!pointer) return Allocate(size); return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release),
        reinterpret_cast<void*>(Callocate), reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align),
        reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

const std::vector<unsigned char> Expected = {
    0x30, 0x15, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x04, 0x00, 0x01, 0x02, 0x03};

void Run() {
    const long baseline = allocations;
    unsigned char oid[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
    LibreSslAsn1Object object{};
    object.length = static_cast<int>(sizeof(oid));
    object.data = oid;
    LibreSslAsn1TypeValue parameter{};
    parameter.type = 5;
    LibreSslX509Algor algor{};
    algor.algorithm = &object;
    algor.parameter = &parameter;
    unsigned char bitsValue[] = {0x01, 0x02, 0x03};
    Asn1String bits{};
    bits.length = 3;
    bits.type = 3;
    bits.data = bitsValue;
    LibreSslX509Pubkey key{&algor, &bits, nullptr};

    Require(i2d_X509_PUBKEY_nid_postfix(&key, nullptr) == static_cast<int>(Expected.size()), "encoded length");
    unsigned char* allocated = nullptr;
    Require(i2d_X509_PUBKEY_nid_postfix(&key, &allocated) == static_cast<int>(Expected.size()), "allocated encoding length");
    Require(allocated != nullptr, "allocated encoding");
    Require(std::memcmp(allocated, Expected.data(), Expected.size()) == 0, "allocated encoding matches");
    free_nid_postfix(allocated);

    std::array<unsigned char, 32> buffer{};
    unsigned char* cursor = buffer.data();
    Require(i2d_X509_PUBKEY_nid_postfix(&key, &cursor) == static_cast<int>(Expected.size()), "buffer encoding length");
    Require(cursor == buffer.data() + Expected.size(), "the output pointer advances");
    Require(std::memcmp(buffer.data(), Expected.data(), Expected.size()) == 0, "buffer encoding matches");

    Require(i2d_X509_PUBKEY_nid_postfix(nullptr, nullptr) == 0, "a null key has no encoding");
    LibreSslAsn1Object empty{};
    LibreSslX509Algor emptyAlgor{};
    emptyAlgor.algorithm = &empty;
    LibreSslX509Pubkey emptyKey{&emptyAlgor, &bits, nullptr};
    bool threw = false;
    try {
        i2d_X509_PUBKEY_nid_postfix(&emptyKey, nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "an algorithm without an object throws");
    parameter.type = 99;
    threw = false;
    try {
        i2d_X509_PUBKEY_nid_postfix(&key, nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "an unsupported parameter throws");
    parameter.type = 5;

    LibreSslAsn1TypeValue integerParameter{};
    integerParameter.type = 2;
    unsigned char integerBytes[] = {0x80};
    Asn1String integerValue{1, 2, integerBytes, 0};
    integerParameter.value.pointer = &integerValue;
    algor.parameter = &integerParameter;
    const std::vector<unsigned char> expectedInteger = {
        0x30, 0x17, 0x30, 0x0f, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
        0x01, 0x01, 0x01, 0x02, 0x02, 0x00, 0x80, 0x03, 0x04, 0x00, 0x01, 0x02, 0x03};
    std::array<unsigned char, 32> integerBuffer{};
    unsigned char* integerCursor = integerBuffer.data();
    Require(i2d_X509_PUBKEY_nid_postfix(&key, &integerCursor) == static_cast<int>(expectedInteger.size()),
            "an integer parameter is encoded");
    Require(std::memcmp(integerBuffer.data(), expectedInteger.data(), expectedInteger.size()) == 0,
            "an integer parameter keeps its sign byte");

    LibreSslAsn1TypeValue booleanParameter{};
    booleanParameter.type = 1;
    booleanParameter.value.pointer = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    algor.parameter = &booleanParameter;
    const std::vector<unsigned char> expectedBoolean = {
        0x30, 0x13, 0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
        0x01, 0x01, 0x01, 0x03, 0x04, 0x00, 0x01, 0x02, 0x03};
    std::array<unsigned char, 32> booleanBuffer{};
    unsigned char* booleanCursor = booleanBuffer.data();
    Require(i2d_X509_PUBKEY_nid_postfix(&key, &booleanCursor) == static_cast<int>(expectedBoolean.size()),
            "an absent boolean parameter is omitted");
    Require(std::memcmp(booleanBuffer.data(), expectedBoolean.data(), expectedBoolean.size()) == 0,
            "the omitted boolean changes the DER");
    algor.parameter = &parameter;
    Require(allocations == baseline, "the heap returns to its baseline");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "i2d_X509_PUBKEY threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl i2d_X509_PUBKEY tests passed");
    return 0;
}
