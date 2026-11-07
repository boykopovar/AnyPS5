#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
int destroyCalls = 0;
LibreSsl3Bio* destroyedBio = nullptr;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
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

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* result = std::calloc(count, size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (pointer) Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

int APS5_VABI ReadCallback(LibreSsl3Bio*, char*, int) { return 0; }
int APS5_VABI WriteCallback(LibreSsl3Bio*, const char*, int) { return 0; }
long APS5_VABI ControlCallback(LibreSsl3Bio*, int, long, void*) { return 0; }
int APS5_VABI CreateCallback(LibreSsl3Bio*) { return 1; }
int APS5_VABI DestroyCallback(LibreSsl3Bio* bio) {
    ++destroyCalls;
    destroyedBio = bio;
    return 1;
}

LibreSsl3Bio* NewBio() {
    auto* bio = static_cast<LibreSsl3Bio*>(calloc_nid_postfix(1, sizeof(LibreSsl3Bio)));
    Require(bio != nullptr);
    bio->references = 1;
    return bio;
}

void TestFlagsStateAndData() {
    auto* bio = NewBio();
    Require(BIO_get_shutdown_nid_postfix(bio) == 0);
    BIO_set_flags_nid_postfix(bio, 0x0f);
    Require(bio->flags == 0x0f);
    BIO_clear_flags_nid_postfix(bio, 0x05);
    Require(bio->flags == 0x0a);
    BIO_set_init_nid_postfix(bio, 1);
    Require(bio->initialized == 1);
    BIO_set_shutdown_nid_postfix(bio, 1);
    Require(BIO_get_shutdown_nid_postfix(bio) == 1);
    BIO_set_shutdown_nid_postfix(bio, 0);
    Require(BIO_get_shutdown_nid_postfix(bio) == 0);

    Require(BIO_get_data_nid_postfix(bio) == nullptr);
    int marker = 0;
    BIO_set_data_nid_postfix(bio, &marker);
    Require(BIO_get_data_nid_postfix(bio) == &marker);
    BIO_set_data_nid_postfix(bio, nullptr);
    Require(BIO_get_data_nid_postfix(bio) == nullptr);
    BIO_vfree_nid_postfix(bio);
}

void TestMethodTable() {
    static const char name[] = "test method";
    auto* method = BIO_meth_new_nid_postfix(0x401, name);
    Require(method != nullptr && method->type == 0x401 && method->name == name);
    Require(method->write == nullptr && method->read == nullptr && method->ctrl == nullptr && method->create == nullptr && method->destroy == nullptr);
    Require(BIO_meth_set_write_nid_postfix(method, reinterpret_cast<void*>(WriteCallback)) == 1 && method->write == reinterpret_cast<void*>(WriteCallback));
    Require(BIO_meth_set_read_nid_postfix(method, reinterpret_cast<void*>(ReadCallback)) == 1 && method->read == reinterpret_cast<void*>(ReadCallback));
    Require(BIO_meth_set_ctrl_nid_postfix(method, reinterpret_cast<void*>(ControlCallback)) == 1 && method->ctrl == reinterpret_cast<void*>(ControlCallback));
    Require(BIO_meth_set_create_nid_postfix(method, reinterpret_cast<void*>(CreateCallback)) == 1 && method->create == reinterpret_cast<void*>(CreateCallback));
    Require(BIO_meth_set_destroy_nid_postfix(method, reinterpret_cast<void*>(DestroyCallback)) == 1 && method->destroy == reinterpret_cast<void*>(DestroyCallback));
    BIO_meth_free_nid_postfix(method);
    BIO_meth_free_nid_postfix(nullptr);
}

void TestVfreeReferencesAndDestroy() {
    BIO_vfree_nid_postfix(nullptr);

    auto* method = BIO_meth_new_nid_postfix(0x401, "destroy method");
    Require(BIO_meth_set_destroy_nid_postfix(method, reinterpret_cast<void*>(DestroyCallback)) == 1);
    auto* bio = NewBio();
    bio->method = method;
    bio->references = 2;
    destroyCalls = 0;
    BIO_vfree_nid_postfix(bio);
    Require(destroyCalls == 0 && bio->references == 1);
    BIO_vfree_nid_postfix(bio);
    Require(destroyCalls == 1 && destroyedBio == bio);
    BIO_meth_free_nid_postfix(method);

    auto* extended = NewBio();
    extended->exData = &destroyCalls;
    bool threw = false;
    try {
        BIO_vfree_nid_postfix(extended);
    } catch (...) {
        threw = true;
    }
    Require(threw);
    extended->exData = nullptr;
    BIO_vfree_nid_postfix(extended);
}

}  // namespace

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestFlagsStateAndData();
    TestMethodTable();
    TestVfreeReferencesAndDestroy();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 BIO tests passed");
}
