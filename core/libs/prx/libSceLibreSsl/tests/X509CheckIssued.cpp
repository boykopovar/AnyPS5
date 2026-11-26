#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
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

struct Extension {
    void* object;
    int critical;
    int padding;
    void* value;
};

LibreSslAsn1Object* NewObject(int nid) {
    auto* object = static_cast<LibreSslAsn1Object*>(calloc_nid_postfix(1, sizeof(LibreSslAsn1Object)));
    Require(object != nullptr, "object");
    object->nid = nid;
    return object;
}

Asn1String* NewValue(std::initializer_list<unsigned char> bytes) {
    auto* value = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    Require(value != nullptr, "value");
    value->length = static_cast<int>(bytes.size());
    value->type = 4;
    value->data = static_cast<unsigned char*>(malloc_nid_postfix(bytes.size() ? bytes.size() : 1));
    Require(value->data != nullptr, "value data");
    std::memcpy(value->data, bytes.begin(), bytes.size());
    return value;
}

LibreSslStack* NewStack(std::initializer_list<void*> elements) {
    auto* stack = static_cast<LibreSslStack*>(calloc_nid_postfix(1, sizeof(LibreSslStack)));
    Require(stack != nullptr, "stack");
    stack->num = static_cast<int>(elements.size());
    stack->numAlloc = static_cast<int>(elements.size());
    stack->data = static_cast<char**>(calloc_nid_postfix(elements.size() ? elements.size() : 1, sizeof(char*)));
    Require(stack->data != nullptr, "stack data");
    int index = 0;
    for (void* element : elements) stack->data[index++] = static_cast<char*>(element);
    return stack;
}

LibreSslX509Name* NewName(int nid, const char* text) {
    auto* object = NewObject(nid);
    const std::size_t length = std::strlen(text);
    std::vector<unsigned char> bytes(text, text + length);
    auto* value = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    Require(value != nullptr, "name value");
    value->length = static_cast<int>(length);
    value->type = 4;
    value->data = static_cast<unsigned char*>(malloc_nid_postfix(length ? length : 1));
    Require(value->data != nullptr, "name bytes");
    std::memcpy(value->data, bytes.data(), length);
    auto* entry = static_cast<LibreSslX509NameEntry*>(calloc_nid_postfix(1, sizeof(LibreSslX509NameEntry)));
    Require(entry != nullptr, "entry");
    entry->object = object;
    entry->value = value;
    auto* name = static_cast<LibreSslX509Name*>(calloc_nid_postfix(1, sizeof(LibreSslX509Name)));
    Require(name != nullptr, "name");
    name->entries = NewStack({entry});
    return name;
}

Extension* NewExtension(int nid, std::initializer_list<unsigned char> content) {
    auto* extension = static_cast<Extension*>(calloc_nid_postfix(1, sizeof(Extension)));
    Require(extension != nullptr, "extension");
    extension->object = NewObject(nid);
    extension->value = NewValue(content);
    return extension;
}

LibreSslX509* NewCertificate(LibreSslX509Name* subject, LibreSslX509Name* issuer,
                             std::initializer_list<Extension*> extensions = {}, const char* serial = nullptr) {
    auto* info = static_cast<LibreSslX509CertInfo*>(calloc_nid_postfix(1, sizeof(LibreSslX509CertInfo)));
    Require(info != nullptr, "certificate info");
    info->subject = subject;
    info->issuer = issuer;
    if (serial) info->serialNumber = NewValue({static_cast<unsigned char>(serial[0])});
    if (extensions.size() != 0) {
        auto* stack = static_cast<LibreSslStack*>(calloc_nid_postfix(1, sizeof(LibreSslStack)));
        Require(stack != nullptr, "extension stack");
        stack->data = static_cast<char**>(calloc_nid_postfix(extensions.size(), sizeof(char*)));
        Require(stack->data != nullptr, "extension data");

        stack->num = static_cast<int>(extensions.size());
        stack->numAlloc = static_cast<int>(extensions.size());
        int index = 0;
        for (Extension* extension : extensions) stack->data[index++] = reinterpret_cast<char*>(extension);
        info->extensions = stack;
    }
    auto* certificate = static_cast<LibreSslX509*>(calloc_nid_postfix(1, sizeof(LibreSslX509)));
    Require(certificate != nullptr, "certificate");
    certificate->certificateInfo = info;
    certificate->references = 1;
    return certificate;
}

void FreeStack(LibreSslStack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

void FreeName(LibreSslX509Name* name) {
    if (!name) return;
    auto* stack = static_cast<LibreSslStack*>(name->entries);
    if (stack) {
        for (int index = 0; index < stack->num; ++index) {
            auto* entry = reinterpret_cast<LibreSslX509NameEntry*>(stack->data[index]);
            if (!entry) continue;
            auto* object = static_cast<LibreSslAsn1Object*>(entry->object);
            auto* value = static_cast<Asn1String*>(entry->value);
            if (value) {
                free_nid_postfix(value->data);
                free_nid_postfix(value);
            }
            free_nid_postfix(object);
            free_nid_postfix(entry);
        }
        FreeStack(stack);
    }
    free_nid_postfix(name);
}

void FreeCertificate(LibreSslX509* certificate) {
    if (!certificate) return;
    auto* info = static_cast<LibreSslX509CertInfo*>(certificate->certificateInfo);
    if (info) {
        FreeName(static_cast<LibreSslX509Name*>(info->subject));
        FreeName(static_cast<LibreSslX509Name*>(info->issuer));

        auto* serial = static_cast<Asn1String*>(info->serialNumber);
        if (serial) {
            free_nid_postfix(serial->data);
            free_nid_postfix(serial);
        }
        auto* stack = static_cast<LibreSslStack*>(info->extensions);
        if (stack) {
            for (int index = 0; index < stack->num; ++index) {
                auto* extension = reinterpret_cast<Extension*>(stack->data[index]);
                if (!extension) continue;
                auto* value = static_cast<Asn1String*>(extension->value);
                if (value) {
                    free_nid_postfix(value->data);
                    free_nid_postfix(value);
                }
                free_nid_postfix(extension->object);
                free_nid_postfix(extension);
            }
            FreeStack(stack);
        }
        free_nid_postfix(info);
    }
    free_nid_postfix(certificate);
}

void Run() {
    const long baseline = allocations;
    LibreSslX509* issuer = NewCertificate(NewName(13, "issuer"), NewName(13, "issuer"));
    LibreSslX509* subject = NewCertificate(NewName(13, "issuer"), NewName(13, "issuer"));
    Require(X509_check_issued_nid_postfix(issuer, subject) == 0, "matching names are ok");

    FreeCertificate(subject);
    subject = NewCertificate(NewName(13, "issuer"), NewName(13, "other"));
    Require(X509_check_issued_nid_postfix(issuer, subject) == 29, "a name mismatch is reported");
    FreeCertificate(subject);

    Extension* skid = NewExtension(82, {0x04, 0x02, 0x01, 0x02});
    FreeCertificate(issuer);
    issuer = NewCertificate(NewName(13, "issuer"), NewName(13, "issuer"), {skid});
    Extension* akid = NewExtension(90, {0x30, 0x04, 0x80, 0x02, 0x01, 0x02});
    subject = NewCertificate(NewName(13, "issuer"), NewName(13, "issuer"), {akid});
    Require(X509_check_issued_nid_postfix(issuer, subject) == 0, "matching key identifiers are ok");
    FreeCertificate(subject);
    subject = NewCertificate(NewName(13, "issuer"), NewName(13, "issuer"),
                             {NewExtension(90, {0x30, 0x04, 0x80, 0x02, 0x03, 0x04})});
    Require(X509_check_issued_nid_postfix(issuer, subject) == 30, "a key identifier mismatch is reported");
    FreeCertificate(subject);

    FreeCertificate(issuer);
    issuer = NewCertificate(NewName(13, "issuer"), NewName(13, "issuer"),
                            {NewExtension(83, {0x03, 0x02, 0x01, 0x04})});
    subject = NewCertificate(NewName(13, "issuer"), NewName(13, "issuer"));
    Require(X509_check_issued_nid_postfix(issuer, subject) == 0, "keyCertSign is accepted");
    FreeCertificate(issuer);
    issuer = NewCertificate(NewName(13, "issuer"), NewName(13, "issuer"),
                            {NewExtension(83, {0x03, 0x02, 0x01, 0x80})});
    Require(X509_check_issued_nid_postfix(issuer, subject) == 32, "a key without keyCertSign is rejected");
    FreeCertificate(subject);
    FreeCertificate(issuer);

    bool threw = false;
    try {
        X509_check_issued_nid_postfix(nullptr, nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "a null certificate throws");
    Require(allocations == baseline, "the heap returns to its baseline");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "X509_check_issued threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl X509_check_issued tests passed");
    return 0;
}
