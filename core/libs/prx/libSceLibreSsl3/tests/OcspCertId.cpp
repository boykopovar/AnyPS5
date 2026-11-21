#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;
std::vector<void*> released;
std::vector<void*> expected;

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "OCSP_CERTID_free check failed at line %u: %s\n", location.line(), message);
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::calloc(1, size);
    Require(pointer != nullptr, "allocation");
    allocations.emplace(pointer, size);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1, "release must own a live allocation");
    released.push_back(pointer);
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    return Allocate_nid_no_patch(count * size);
}

void* APS5_VABI UnexpectedReallocate_nid_no_patch(void*, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { std::abort(); }

template<class T> T* Owned() {
    return static_cast<T*>(Allocate_nid_no_patch(sizeof(T)));
}

Asn1String* String(int type) {
    auto* string = Owned<Asn1String>();
    auto* data = Owned<unsigned char>();
    *data = 7;
    *string = {1, type, data, 0};
    expected.push_back(data);
    expected.push_back(string);
    return string;
}

Asn1Object_nid_no_patch staticOid{};
LibreSslAsn1Object staticParameterOid{};

void* AlgorithmOid(bool dynamic) {
    if (!dynamic) return &staticOid;
    auto* object = Owned<Asn1Object_nid_no_patch>();
    object->flags = 1 | 4 | 8;
    auto* shortName = Owned<char>();
    auto* longName = Owned<char>();
    auto* data = Owned<unsigned char>();
    object->shortName = shortName;
    object->longName = longName;
    object->data = data;
    object->length = 1;
    expected.insert(expected.end(), {shortName, longName, data, object});
    return object;
}

Asn1Type* Parameter(int kind, bool dynamic) {
    if (kind == 0) return nullptr;
    auto* parameter = Owned<Asn1Type>();
    if (kind == 1) {
        parameter->type = 5;
    } else if (kind == 2) {
        parameter->type = 6;
        if (dynamic) {
            auto* object = Owned<LibreSslAsn1Object>();
            auto* data = Owned<unsigned char>();
            *object = {nullptr, nullptr, 0, 1, data, 1 | 8};
            parameter->value.pointer = object;
            expected.insert(expected.end(), {data, object});
        } else {
            parameter->value.pointer = &staticParameterOid;
        }
    } else {
        parameter->type = 4;
        parameter->value.pointer = String(4);
    }
    expected.push_back(parameter);
    return parameter;
}

void Check(unsigned mask, int parameterKind, bool dynamic) {
    expected.clear();
    released.clear();
    auto* id = Owned<OCSP_CERTID>();
    if (mask & 8) id->serialNumber = String(2);
    if (mask & 4) id->issuerKeyHash = String(4);
    if (mask & 2) id->issuerNameHash = String(4);
    if (mask & 1) {
        auto* algorithm = Owned<LibreSsl3X509Algor>();
        algorithm->parameter = reinterpret_cast<LibreSsl3Asn1Type*>(Parameter(parameterKind, dynamic));
        algorithm->algorithm = AlgorithmOid(dynamic);
        expected.push_back(algorithm);
        id->hashAlgorithm = algorithm;
    }
    expected.push_back(id);
    OCSP_CERTID_free_nid_postfix(id);
    Require(allocations.empty(), "outstanding allocations");
    Require(released == expected, "reverse SEQUENCE field release order");
}

} // namespace

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedReallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    OCSP_CERTID_free_nid_postfix(nullptr);
    Require(allocations.empty() && released.empty(), "null is a no-op");
    for (unsigned mask = 0; mask < 16; ++mask)
        for (int parameter = 0; parameter < 4; ++parameter)
            for (bool dynamic : {false, true}) Check(mask, parameter, dynamic);
    std::puts("OCSP_CERTID_free: 128 ownership cases and null passed");
}
