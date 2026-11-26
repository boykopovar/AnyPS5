#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using LibreSsl::Bio;
using LibreSsl::BioMethod;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    } catch (...) {
    }
    Require(false, message);
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size ? size : 1); }
void APS5_VABI Release(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate(std::size_t count, std::size_t size) { return std::calloc(count ? count : 1, size ? size : 1); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release),
        reinterpret_cast<void*>(Callocate), reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align),
        reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

std::string captured;
int destroyCount = 0;
bool failWrites = false;

int APS5_VABI CaptureWrite(Bio*, const char* source, int size) {
    if (failWrites) return 0;
    captured.append(source, static_cast<std::size_t>(size));
    return size;
}

int APS5_VABI CountDestroy(Bio*) {
    ++destroyCount;
    return 1;
}

int APS5_VABI InitializeCapture(Bio* bio) {
    bio->initialized = 1;
    return 1;
}

const BioMethod CaptureMethod{0, "capture", CaptureWrite, nullptr, nullptr, nullptr, nullptr, InitializeCapture, CountDestroy, nullptr};

void BioPrintf() {
    captured.clear();
    Bio* bio = BIO_new_nid_postfix(&CaptureMethod);
    Require(bio != nullptr, "capture bio");
    Require(BIO_printf_nid_postfix(bio, "value %d", 7) == 7, "formatted length");
    Require(captured == "value 7", "formatted text");
    captured.clear();
    const std::string longText(1500, 'x');
    Require(BIO_printf_nid_postfix(bio, "%s", longText.c_str()) == 1500, "long formatted length");
    Require(captured == longText, "long formatted text");
    RequireInvalidArgument([&] { BIO_printf_nid_postfix(nullptr, "%d", 1); }, "null bio");
    RequireInvalidArgument([&] { BIO_printf_nid_postfix(bio, nullptr); }, "null format");
    failWrites = true;
    Require(BIO_printf_nid_postfix(bio, "%d", 1) == -1, "a failed write reports -1");
    failWrites = false;
    Require(BIO_free_nid_postfix(bio) == 1, "capture bio freed");
}

void SetBio() {
    const LibreSslMethod* method = TLS_client_method_nid_postfix();
    Require(method != nullptr, "TLS client method");
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(method);
    Require(ctx != nullptr, "context");
    LibreSslSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(ssl != nullptr, "ssl");
    Bio* readBio = BIO_new_nid_postfix(&CaptureMethod);
    Bio* writeBio = BIO_new_nid_postfix(&CaptureMethod);
    Require(readBio != nullptr && writeBio != nullptr, "bios");
    SSL_set_bio_nid_postfix(ssl, readBio, writeBio);
    Require(ssl->rbio == readBio && ssl->wbio == writeBio, "the bios are stored");
    Bio* shared = BIO_new_nid_postfix(&CaptureMethod);
    Require(shared != nullptr, "shared bio");
    const int destroyedBefore = destroyCount;
    SSL_set_bio_nid_postfix(ssl, shared, shared);
    Require(ssl->rbio == shared && ssl->wbio == shared, "one bio is both directions");
    Require(destroyCount == destroyedBefore + 2, "the replaced bios are freed once each");
    SSL_set_bio_nid_postfix(ssl, shared, shared);
    Require(destroyCount == destroyedBefore + 2, "the same bios are not freed again");
    RequireInvalidArgument([&] { SSL_set_bio_nid_postfix(nullptr, shared, shared); }, "null ssl");
    SSL_free_nid_postfix(ssl);
    Require(destroyCount == destroyedBefore + 3, "SSL_free frees the shared bio once");
    SSL_CTX_free_nid_postfix(ctx);
    Require(destroyCount == destroyedBefore + 3, "the context frees no bio");
}

}

int main() {
    RegisterHeap();
    try {
        BioPrintf();
        SetBio();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "BIO and SSL_set_bio threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl BIO_printf and SSL_set_bio tests passed");
    return 0;
}
