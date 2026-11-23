#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

struct Extension {
    Asn1Object_nid_no_patch* object;
    std::int32_t critical;
    std::int32_t padding;
    Asn1String* value;
};

static_assert(sizeof(Extension) == 24);

std::unordered_map<void*, std::size_t> allocations;
std::size_t allocationAttempts = 0;
std::size_t failOnAllocationAttempt = 0;

void Require(bool condition, std::source_location location = std::source_location::current());

bool ShouldFailAllocation() {
    ++allocationAttempts;
    return failOnAllocationAttempt != 0 && allocationAttempts == failOnAllocationAttempt;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (ShouldFailAllocation()) return nullptr;
    void* result = std::malloc(size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (ShouldFailAllocation()) return nullptr;
    void* result = std::calloc(count, size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    if (ShouldFailAllocation()) return nullptr;
    Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Require(bool condition, std::source_location location) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509 extension check failed at line %u\n", location.line());
    std::abort();
}

Asn1Object_nid_no_patch* MakeObject() {
    auto* object = static_cast<Asn1Object_nid_no_patch*>(Callocate_nid_no_patch(1, sizeof(Asn1Object_nid_no_patch)));
    auto* shortName = static_cast<char*>(Allocate_nid_no_patch(3));
    auto* longName = static_cast<char*>(Allocate_nid_no_patch(5));
    auto* data = static_cast<unsigned char*>(Allocate_nid_no_patch(3));
    std::memcpy(shortName, "SN", 3);
    std::memcpy(longName, "Long", 5);
    data[0] = 0x55; data[1] = 0x1d; data[2] = 0x13;
    object->shortName = shortName;
    object->longName = longName;
    object->nid = 0;
    object->length = 3;
    object->data = data;
    object->flags = 0x01 | 0x04 | 0x08;
    return object;
}

Asn1String* MakeValue(unsigned char byte) {
    auto* value = static_cast<Asn1String*>(Callocate_nid_no_patch(1, sizeof(Asn1String)));
    auto* data = static_cast<unsigned char*>(Allocate_nid_no_patch(2));
    data[0] = byte;
    data[1] = 0x42;
    value->length = 2;
    value->type = 4;
    value->data = data;
    value->flags = 0;
    return value;
}

Extension* MakeExtension(unsigned char byte) {
    auto* extension = static_cast<Extension*>(Callocate_nid_no_patch(1, sizeof(Extension)));
    extension->object = MakeObject();
    extension->critical = 1;
    extension->value = MakeValue(byte);
    return extension;
}

Extension* MakeBorrowedExtension(unsigned char* data) {
    auto* extension = static_cast<Extension*>(Callocate_nid_no_patch(1, sizeof(Extension)));
    extension->object = MakeObject();
    extension->critical = 1;
    extension->value = static_cast<Asn1String*>(Callocate_nid_no_patch(1, sizeof(Asn1String)));
    extension->value->length = 2;
    extension->value->type = 4;
    extension->value->data = data;
    extension->value->flags = 0x10;
    return extension;
}

Extension* MakeStaticExtension(unsigned char* data) {
    static Asn1Object_nid_no_patch object{};
    auto* extension = static_cast<Extension*>(Callocate_nid_no_patch(1, sizeof(Extension)));
    object.shortName = "keyUsage";
    object.longName = "X509v3 Key Usage";
    object.nid = 83;
    object.length = 3;
    object.data = data;
    extension->object = &object;
    extension->critical = 0;
    extension->value = MakeValue(9);
    return extension;
}

void FreeCertificateExtensions(LibreSsl3ExtensionsX509* x509) {
    const LibreSsl3Stack* const stackView = X509_get0_extensions_nid_postfix(x509);
    if (!stackView) return;
    auto* const stack = const_cast<LibreSslStack*>(reinterpret_cast<const LibreSslStack*>(stackView));
    for (int index = 0; index < stack->num; ++index) {
        X509_EXTENSION_free_nid_postfix(stack->data[index]);
    }
    sk_free_nid_postfix(stack);
    const_cast<LibreSsl3ExtensionsCertInfo*>(x509->certInfo)->extensions = nullptr;
}

void PopulateAtCapacity(LibreSsl3ExtensionsX509* x509) {
    constexpr std::array<unsigned char, 3> bytes{0x11, 0x22, 0x33};
    for (unsigned char byte : bytes) {
        Extension* extension = MakeExtension(byte);
        Require(X509_add_ext_nid_postfix(x509, extension, -1) == 1);
        X509_EXTENSION_free_nid_postfix(extension);
    }
    const LibreSsl3Stack* const stack = X509_get0_extensions_nid_postfix(x509);
    Require(X509_get_ext_count_nid_postfix(x509) == 3);
    Require(stack != nullptr && stack->numAlloc == 4);
}

void TestEmptyStackAllocationRollback() {
    Extension* source = MakeExtension(0x5a);
    LibreSsl3ExtensionsCertInfo probeCert{};
    LibreSsl3ExtensionsX509 probeX509{};
    probeX509.certInfo = &probeCert;
    allocationAttempts = 0;
    failOnAllocationAttempt = 0;
    Require(X509_add_ext_nid_postfix(&probeX509, source, -1) == 1);
    const std::size_t totalAllocations = allocationAttempts;
    Require(totalAllocations > 0);
    FreeCertificateExtensions(&probeX509);

    for (std::size_t failIndex = 1; failIndex <= totalAllocations; ++failIndex) {
        LibreSsl3ExtensionsCertInfo cert{};
        LibreSsl3ExtensionsX509 x509{};
        x509.certInfo = &cert;
        const std::size_t baselineLive = allocations.size();
        Require(X509_get_ext_count_nid_postfix(&x509) == 0);
        Require(X509_get0_extensions_nid_postfix(&x509) == nullptr);

        allocationAttempts = 0;
        failOnAllocationAttempt = failIndex;
        Require(X509_add_ext_nid_postfix(&x509, source, -1) == 0);
        failOnAllocationAttempt = 0;

        Require(X509_get_ext_count_nid_postfix(&x509) == 0);
        Require(X509_get0_extensions_nid_postfix(&x509) == nullptr);
        Require(allocations.size() == baselineLive);
        Require(source->object != nullptr && source->value != nullptr && source->value->data[0] == 0x5a);

        Require(X509_add_ext_nid_postfix(&x509, source, -1) == 1);
        Require(X509_get_ext_count_nid_postfix(&x509) == 1);
        const LibreSsl3Stack* const stack = X509_get0_extensions_nid_postfix(&x509);
        Require(stack != nullptr && stack->num == 1 && stack->data[0] != nullptr);
        const auto* const added = reinterpret_cast<const Extension*>(stack->data[0]);
        Require(added != source && added->value->data[0] == 0x5a);
        FreeCertificateExtensions(&x509);
        Require(allocations.size() == baselineLive);
    }

    X509_EXTENSION_free_nid_postfix(source);
    Require(allocations.empty());
}

void TestGrowthAllocationRollback() {
    Extension* source = MakeExtension(0x7c);
    LibreSsl3ExtensionsCertInfo probeCert{};
    LibreSsl3ExtensionsX509 probeX509{};
    probeX509.certInfo = &probeCert;
    PopulateAtCapacity(&probeX509);
    allocationAttempts = 0;
    failOnAllocationAttempt = 0;
    Require(X509_add_ext_nid_postfix(&probeX509, source, 1) == 1);
    const std::size_t totalAllocations = allocationAttempts;
    Require(totalAllocations > 0);
    Require(X509_get_ext_count_nid_postfix(&probeX509) == 4);
    Require(X509_get0_extensions_nid_postfix(&probeX509)->numAlloc == 8);
    FreeCertificateExtensions(&probeX509);

    LibreSsl3ExtensionsCertInfo cert{};
    LibreSsl3ExtensionsX509 x509{};
    x509.certInfo = &cert;
    PopulateAtCapacity(&x509);

    for (std::size_t failIndex = 1; failIndex <= totalAllocations; ++failIndex) {
        const int countBefore = X509_get_ext_count_nid_postfix(&x509);
        const LibreSsl3Stack* const stackBefore = X509_get0_extensions_nid_postfix(&x509);
        Require(countBefore == 3 && stackBefore != nullptr && stackBefore->numAlloc == 4);
        char** const dataBefore = stackBefore->data;
        Require(allocations.contains(dataBefore));
        const std::array<void*, 3> elementsBefore{stackBefore->data[0], stackBefore->data[1], stackBefore->data[2]};
        const std::size_t baselineLive = allocations.size();

        allocationAttempts = 0;
        failOnAllocationAttempt = failIndex;
        Require(X509_add_ext_nid_postfix(&x509, source, 1) == 0);
        failOnAllocationAttempt = 0;

        Require(X509_get_ext_count_nid_postfix(&x509) == countBefore);
        Require(X509_get0_extensions_nid_postfix(&x509) == stackBefore);
        Require(stackBefore->data == dataBefore);
        Require(stackBefore->numAlloc == 4);
        Require(allocations.contains(dataBefore));
        for (int index = 0; index < 3; ++index) Require(stackBefore->data[index] == elementsBefore[index]);
        Require(reinterpret_cast<const Extension*>(stackBefore->data[0])->value->data[0] == 0x11);
        Require(reinterpret_cast<const Extension*>(stackBefore->data[1])->value->data[0] == 0x22);
        Require(reinterpret_cast<const Extension*>(stackBefore->data[2])->value->data[0] == 0x33);
        Require(allocations.size() == baselineLive);
        Require(source->object != nullptr && source->value != nullptr && source->value->data[0] == 0x7c);
    }

    const LibreSsl3Stack* const stackBefore = X509_get0_extensions_nid_postfix(&x509);
    const std::array<void*, 3> elementsBefore{stackBefore->data[0], stackBefore->data[1], stackBefore->data[2]};
    Require(X509_add_ext_nid_postfix(&x509, source, 1) == 1);
    Require(X509_get_ext_count_nid_postfix(&x509) == 4);
    const LibreSsl3Stack* const stackAfter = X509_get0_extensions_nid_postfix(&x509);
    Require(stackAfter == stackBefore && stackAfter->numAlloc == 8);
    Require(stackAfter->data[0] == elementsBefore[0]);
    Require(reinterpret_cast<const Extension*>(stackAfter->data[1]) != source && stackAfter->data[1] != elementsBefore[0] &&
            stackAfter->data[1] != elementsBefore[1] && stackAfter->data[1] != elementsBefore[2]);
    Require(reinterpret_cast<const Extension*>(stackAfter->data[1])->value->data[0] == 0x7c);
    Require(stackAfter->data[2] == elementsBefore[1]);
    Require(stackAfter->data[3] == elementsBefore[2]);

    FreeCertificateExtensions(&x509);
    X509_EXTENSION_free_nid_postfix(source);
    Require(allocations.empty());
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    LibreSsl3ExtensionsCertInfo certificate{};
    LibreSsl3ExtensionsX509 x509{};
    x509.certInfo = &certificate;

    Extension* first = MakeExtension(1);
    Extension* second = MakeExtension(2);
    Require(X509_add_ext_nid_postfix(&x509, first, -1) == 1);
    Require(certificate.extensions != nullptr);
    auto* stack = reinterpret_cast<LibreSslStack*>(const_cast<LibreSsl3Stack*>(certificate.extensions));
    Require(stack->num == 1);
    auto* added = reinterpret_cast<Extension*>(stack->data[0]);
    Require(added != first && added->object != first->object && added->value != first->value);
    Require(added->critical == 1 && added->object->length == 3 && added->value->length == 2);
    first->value->data[0] = 0x7f;
    Require(added->value->data[0] == 1);

    Require(X509_add_ext_nid_postfix(&x509, second, 0) == 1);
    Require(stack->num == 2);
    Require(reinterpret_cast<Extension*>(stack->data[0])->value->data[0] == 2);
    Require(reinterpret_cast<Extension*>(stack->data[1])->value->data[0] == 1);

    std::array<unsigned char, 2> borrowedData{0x33, 0x44};
    Extension* borrowed = MakeBorrowedExtension(borrowedData.data());
    Require(X509_add_ext_nid_postfix(&x509, borrowed, 1) == 1);
    Require(stack->num == 3);
    auto* borrowedCopy = reinterpret_cast<Extension*>(stack->data[1]);
    Require(borrowedCopy->value->data != nullptr && borrowedCopy->value->data != borrowedData.data());
    Require(borrowedCopy->value->data[0] == 0x33 && borrowedCopy->value->data[1] == 0x44);
    Require((borrowedCopy->value->flags & 0x10) == 0);

    static unsigned char staticOid[] = {0x55, 0x1d, 0x0f};
    Extension* staticExtension = MakeStaticExtension(staticOid);
    Require(X509_add_ext_nid_postfix(&x509, staticExtension, -1) == 1);
    Require(stack->num == 4);
    auto* staticCopy = reinterpret_cast<Extension*>(stack->data[3]);
    Require(staticCopy->object->nid == 83 && staticCopy->object->length == 3);
    Require(staticCopy->object->data != staticOid);
    Require(std::memcmp(staticCopy->object->data, staticOid, 3) == 0);
    Require((staticCopy->object->flags & 0x08) != 0);

    X509_EXTENSION_free_nid_postfix(first);
    X509_EXTENSION_free_nid_postfix(second);
    X509_EXTENSION_free_nid_postfix(borrowed);
    X509_EXTENSION_free_nid_postfix(staticExtension);
    X509_EXTENSION_free_nid_postfix(nullptr);
    Require(X509_add_ext_nid_postfix(nullptr, nullptr, 0) == 0);
    Require(X509_add_ext_nid_postfix(&x509, nullptr, 0) == 0);

    X509_EXTENSION_free_nid_postfix(stack->data[0]);
    X509_EXTENSION_free_nid_postfix(stack->data[1]);
    X509_EXTENSION_free_nid_postfix(stack->data[2]);
    X509_EXTENSION_free_nid_postfix(stack->data[3]);
    sk_free_nid_postfix(stack);
    Require(allocations.empty());

    TestEmptyStackAllocationRollback();
    TestGrowthAllocationRollback();
    std::puts("libSceLibreSsl3 X509 extension stack tests passed");
}
