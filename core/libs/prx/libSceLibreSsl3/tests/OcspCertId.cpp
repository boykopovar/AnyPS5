#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

std::unordered_map<void*, std::size_t> allocations;
std::vector<void*> released;
std::vector<void*> expected;

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "OCSP_CERTID_free check failed at line %u: %s\n", location.line(), message);
    std::abort();
}

template<typename TAction>
void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, "expected std::invalid_argument", location);
}

class ReadOnlyPage {
public:
    explicit ReadOnlyPage(const void* initial, std::size_t size) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(_page != nullptr, "VirtualAlloc");
        if (initial && size > 0) std::memcpy(_page, initial, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(_page, _size, PAGE_READONLY, &oldProtect) != 0, "VirtualProtect");
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        _page = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(_page != MAP_FAILED, "mmap");
        if (initial && size > 0) std::memcpy(_page, initial, size);
        Require(mprotect(_page, _size, PROT_READ) == 0, "mprotect");
#endif
    }

    ~ReadOnlyPage() {
        if (!_page) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    template<typename T> T* As() const { return static_cast<T*>(_page); }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

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

void CheckUnreadable() {
    expected.clear();
    released.clear();
    auto* const unmapped = reinterpret_cast<OCSP_CERTID*>(std::uintptr_t{1});
    errno = EDOM;
    RequireInvalidArgument([&] { OCSP_CERTID_free_nid_postfix(unmapped); });
    Require(errno == EDOM, "errno preserved on unreadable cert id");
    Require(allocations.empty() && released.empty(), "no allocations touched on unreadable cert id");

    Asn1String* serial = String(2);
    OCSP_CERTID prototype{};
    prototype.hashAlgorithm = reinterpret_cast<LibreSsl3X509Algor*>(std::uintptr_t{1});
    prototype.serialNumber = serial;
    const ReadOnlyPage page(&prototype, sizeof(prototype));
    errno = EDOM;
    RequireInvalidArgument([&] { OCSP_CERTID_free_nid_postfix(page.As<OCSP_CERTID>()); });
    Require(errno == EDOM, "errno preserved on unreadable hash algorithm");
    Require(released.empty() && allocations.size() == 2, "fields not freed when hash algorithm is unreadable");
    ASN1_INTEGER_free_nid_postfix(serial);
    Require(allocations.empty(), "cleanup serial");
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
    CheckUnreadable();
    for (unsigned mask = 0; mask < 16; ++mask)
        for (int parameter = 0; parameter < 4; ++parameter)
            for (bool dynamic : {false, true}) Check(mask, parameter, dynamic);
    std::puts("OCSP_CERTID_free: 128 ownership cases, unreadable, and null passed");
}
