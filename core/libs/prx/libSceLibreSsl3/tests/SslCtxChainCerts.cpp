#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

bool failAllocation = false;
bool throwOnRealloc = false;
std::size_t activeAllocations = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_CTX chain certs check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failAllocation) return nullptr;
    ++activeAllocations;
    return std::malloc(size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) {
        --activeAllocations;
        std::free(pointer);
    }
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (failAllocation) return nullptr;
    ++activeAllocations;
    return std::calloc(count, size);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (throwOnRealloc) throw std::bad_alloc();
    if (failAllocation) return nullptr;
    if (!pointer) ++activeAllocations;
    return std::realloc(pointer, size);
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned allocation");
}

void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned reallocation");
}

int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected posix_memalign");
}

void TestNullArguments() {
    LibreSslCtx context{};
    LibreSsl3X509 cert{};
    cert.references = 1;

    Require(SSL_CTX_add0_chain_cert_nid_postfix(nullptr, &cert) == 0);
    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, nullptr) == 0);
    Require(context.extraCerts == nullptr);
    Require(cert.references == 1);

    Require(SSL_CTX_clear_chain_certs_nid_postfix(nullptr) == 0);
}

void TestAddAndClear() {
    LibreSslCtx context{};

    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);

    auto* cert1 = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
    cert1->certInfo = nullptr;
    cert1->sigAlg = nullptr;
    cert1->signature = nullptr;
    cert1->valid = 1;
    cert1->references = 1;

    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, cert1) == 1);
    auto* stack = static_cast<LibreSslStack*>(context.extraCerts);
    Require(stack != nullptr);
    Require(stack->num == 1);
    Require(stack->data[0] == reinterpret_cast<char*>(cert1));
    Require(cert1->references == 1);

    auto* cert2 = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
    cert2->certInfo = nullptr;
    cert2->sigAlg = nullptr;
    cert2->signature = nullptr;
    cert2->valid = 1;
    cert2->references = 3;

    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, cert2) == 1);
    Require(context.extraCerts == stack);
    Require(stack->num == 2);
    Require(stack->data[1] == reinterpret_cast<char*>(cert2));
    Require(cert2->references == 3);

    std::array<LibreSsl3X509*, 6> additional{};
    for (std::size_t i = 0; i < additional.size(); ++i) {
        additional[i] = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
        additional[i]->certInfo = nullptr;
        additional[i]->sigAlg = nullptr;
        additional[i]->signature = nullptr;
        additional[i]->valid = 1;
        additional[i]->references = 1;
        Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, additional[i]) == 1);
    }
    stack = static_cast<LibreSslStack*>(context.extraCerts);
    Require(stack->num == 8);

    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);
    Require(cert2->references == 2);
    Free_nid_no_patch(cert2);

    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);
}

void TestAllocationFailure() {
    LibreSslCtx context{};
    auto* cert = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
    cert->certInfo = nullptr;
    cert->sigAlg = nullptr;
    cert->signature = nullptr;
    cert->valid = 1;
    cert->references = 1;

    failAllocation = true;
    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, cert) == 0);
    Require(context.extraCerts == nullptr);
    Require(cert->references == 1);
    failAllocation = false;

    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, cert) == 1);

    std::array<LibreSsl3X509*, 2> preFill{};
    for (std::size_t i = 0; i < preFill.size(); ++i) {
        preFill[i] = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
        preFill[i]->certInfo = nullptr;
        preFill[i]->sigAlg = nullptr;
        preFill[i]->signature = nullptr;
        preFill[i]->valid = 1;
        preFill[i]->references = 1;
        Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, preFill[i]) == 1);
    }
    auto* stack = static_cast<LibreSslStack*>(context.extraCerts);
    Require(stack->num == 3);

    auto* failingCert = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
    failingCert->certInfo = nullptr;
    failingCert->sigAlg = nullptr;
    failingCert->signature = nullptr;
    failingCert->valid = 1;
    failingCert->references = 1;

    failAllocation = true;
    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, failingCert) == 0);
    Require(context.extraCerts == stack);
    Require(stack->num == 3);
    Require(failingCert->references == 1);
    failAllocation = false;

    Free_nid_no_patch(failingCert);

    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);
}

void TestReleaseWithOwnedSubAllocations() {
    LibreSslCtx context{};
    const auto baseline = activeAllocations;

    auto* cert = static_cast<LibreSsl3X509*>(Callocate_nid_no_patch(1, sizeof(LibreSsl3X509)));
    cert->valid = 1;
    cert->references = 1;

    auto* certInfo = static_cast<LibreSsl3CertificateInfo*>(Callocate_nid_no_patch(1, sizeof(LibreSsl3CertificateInfo)));
    auto* version = static_cast<Asn1String*>(Callocate_nid_no_patch(1, sizeof(Asn1String)));
    version->data = static_cast<unsigned char*>(Allocate_nid_no_patch(4));
    certInfo->version = version;

    auto* serialNumber = static_cast<Asn1String*>(Callocate_nid_no_patch(1, sizeof(Asn1String)));
    serialNumber->data = static_cast<unsigned char*>(Allocate_nid_no_patch(8));
    certInfo->serialNumber = serialNumber;

    cert->certInfo = reinterpret_cast<LibreSsl3X509CertInfo*>(certInfo);

    auto* sigAlg = static_cast<LibreSsl3X509Algor*>(Callocate_nid_no_patch(1, sizeof(LibreSsl3X509Algor)));
    auto* algObject = static_cast<Asn1Object_nid_no_patch*>(Callocate_nid_no_patch(1, sizeof(Asn1Object_nid_no_patch)));
    algObject->flags = 0x08;
    algObject->data = static_cast<unsigned char*>(Allocate_nid_no_patch(8));
    sigAlg->algorithm = algObject;
    cert->sigAlg = sigAlg;

    auto* signature = static_cast<Asn1String*>(Callocate_nid_no_patch(1, sizeof(Asn1String)));
    signature->data = static_cast<unsigned char*>(Allocate_nid_no_patch(32));
    cert->signature = signature;

    Require(activeAllocations > baseline);
    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, cert) == 1);
    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);
    Require(activeAllocations == baseline);

    auto* multiRefCert = static_cast<LibreSsl3X509*>(Callocate_nid_no_patch(1, sizeof(LibreSsl3X509)));
    multiRefCert->valid = 1;
    multiRefCert->references = 2;

    auto* multiInfo = static_cast<LibreSsl3CertificateInfo*>(Callocate_nid_no_patch(1, sizeof(LibreSsl3CertificateInfo)));
    auto* multiVersion = static_cast<Asn1String*>(Callocate_nid_no_patch(1, sizeof(Asn1String)));
    multiVersion->data = static_cast<unsigned char*>(Allocate_nid_no_patch(4));
    multiInfo->version = multiVersion;
    multiRefCert->certInfo = reinterpret_cast<LibreSsl3X509CertInfo*>(multiInfo);

    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, multiRefCert) == 1);
    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);
    Require(multiRefCert->references == 1);
    Require(activeAllocations > baseline);

    FreeCertificate_nid_no_patch(multiRefCert);
    Require(activeAllocations == baseline);
}

void TestPushExceptionHandling() {
    LibreSslCtx context{};
    auto* cert = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
    cert->certInfo = nullptr;
    cert->sigAlg = nullptr;
    cert->signature = nullptr;
    cert->valid = 1;
    cert->references = 1;

    LibreSslStack badStack{};
    badStack.num = -1;
    badStack.numAlloc = 4;
    context.extraCerts = &badStack;
    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, cert) == 0);
    Require(context.extraCerts == &badStack);
    Require(cert->references == 1);
    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 0);
    Require(context.extraCerts == &badStack);

    badStack.num = 10;
    badStack.numAlloc = 4;
    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 0);
    Require(context.extraCerts == &badStack);

    badStack.num = 2;
    badStack.numAlloc = 4;
    badStack.data = nullptr;
    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 0);
    Require(context.extraCerts == &badStack);
    context.extraCerts = nullptr;

    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, cert) == 1);
    auto* stack = static_cast<LibreSslStack*>(context.extraCerts);
    Require(stack != nullptr);
    Require(stack->num == 1);

    std::array<LibreSsl3X509*, 2> preFill{};
    for (std::size_t i = 0; i < preFill.size(); ++i) {
        preFill[i] = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
        preFill[i]->certInfo = nullptr;
        preFill[i]->sigAlg = nullptr;
        preFill[i]->signature = nullptr;
        preFill[i]->valid = 1;
        preFill[i]->references = 1;
        Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, preFill[i]) == 1);
    }
    Require(stack->num == 3);

    auto* failingCert = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
    failingCert->certInfo = nullptr;
    failingCert->sigAlg = nullptr;
    failingCert->signature = nullptr;
    failingCert->valid = 1;
    failingCert->references = 1;

    throwOnRealloc = true;
    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, failingCert) == 0);
    throwOnRealloc = false;
    Require(context.extraCerts == stack);
    Require(stack->num == 3);
    Require(failingCert->references == 1);

    Free_nid_no_patch(failingCert);
    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);
}

void TestVerifyModeLayoutIndependence() {
    LibreSslCtx context{};
    context.verifyMode = 0x01;

    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);
    Require(context.verifyMode == 0x01);

    auto* cert = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
    cert->certInfo = nullptr;
    cert->sigAlg = nullptr;
    cert->signature = nullptr;
    cert->valid = 1;
    cert->references = 1;

    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, cert) == 1);
    Require(context.extraCerts != nullptr);
    Require(context.verifyMode == 0x01);

    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);
    Require(context.verifyMode == 0x01);
}

void TestNonpositiveReferences() {
    LibreSslCtx context{};
    const auto baseline = activeAllocations;

    auto* zeroRefCert = static_cast<LibreSsl3X509*>(Callocate_nid_no_patch(1, sizeof(LibreSsl3X509)));
    zeroRefCert->references = 0;

    auto* negRefCert = static_cast<LibreSsl3X509*>(Callocate_nid_no_patch(1, sizeof(LibreSsl3X509)));
    negRefCert->references = -1;

    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, zeroRefCert) == 1);
    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, negRefCert) == 1);
    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);

    Require(zeroRefCert->references == 0);
    Require(negRefCert->references == -1);

    Free_nid_no_patch(zeroRefCert);
    Free_nid_no_patch(negRefCert);
    Require(activeAllocations == baseline);
}

void TestClearDetachesBeforeRelease() {
    LibreSslCtx context{};
    const auto baseline = activeAllocations;

    auto* cert1 = static_cast<LibreSsl3X509*>(Allocate_nid_no_patch(sizeof(LibreSsl3X509)));
    cert1->certInfo = nullptr;
    cert1->sigAlg = nullptr;
    cert1->signature = nullptr;
    cert1->valid = 1;
    cert1->references = 1;

    Require(SSL_CTX_add0_chain_cert_nid_postfix(&context, cert1) == 1);
    auto* stack = static_cast<LibreSslStack*>(context.extraCerts);
    Require(stack != nullptr);

    char* badCert = reinterpret_cast<char*>(static_cast<std::uintptr_t>(0xDEADBEEF));
    stack->data[0] = badCert;

    bool threw = false;
    try {
        SSL_CTX_clear_chain_certs_nid_postfix(&context);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(context.extraCerts == nullptr);
    Require(SSL_CTX_clear_chain_certs_nid_postfix(&context) == 1);
    Require(context.extraCerts == nullptr);
    Free_nid_no_patch(cert1);
    Require(activeAllocations == baseline);
}

void TestAllocationTracking() {
    Require(activeAllocations == 0);
}

}

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch),
        reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)
    };
    ApplicationHeapRegister_nid_no_patch(api.data());

    TestNullArguments();
    TestAddAndClear();
    TestAllocationFailure();
    TestPushExceptionHandling();
    TestReleaseWithOwnedSubAllocations();
    TestVerifyModeLayoutIndependence();
    TestNonpositiveReferences();
    TestClearDetachesBeforeRelease();
    TestAllocationTracking();

    std::puts("libSceLibreSsl3 SSL_CTX chain certs tests passed");
    return 0;
}
