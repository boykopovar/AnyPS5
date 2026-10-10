#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadGetname(Pthread thread, char* name);
int APS5_VABI pthread_rename_np_nid_postfix(Pthread thread, const char* name);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

static void CheckStoredName(Pthread thread, const std::string& expected) {
    std::array<unsigned char, 64> buffer;
    buffer.fill(0xaa);
    Require(scePthreadGetname(thread, reinterpret_cast<char*>(buffer.data())) == 0);
    Require(std::memcmp(buffer.data(), expected.c_str(), expected.size() + 1) == 0);
    for (std::size_t i = expected.size() + 1; i < buffer.size(); ++i) Require(buffer[i] == 0xaa);
}

static void CheckThreadRename(Pthread thread) {
    const int savedError = *__error_nid_postfix();
    const std::string names[] = {"", "A", "AnyPS5Probe", "0123456789012345678901234567890"};
    for (const auto& name : names) {
        *__error_nid_postfix() = 77;
        Require(pthread_rename_np_nid_postfix(thread, name.c_str()) == 0);
        Require(*__error_nid_postfix() == 77);
        CheckStoredName(thread, name);
    }
    for (const std::size_t length : {32u, 40u, 1024u}) {
        const std::string name(length, 'x');
        *__error_nid_postfix() = 77;
        const int error = pthread_rename_np_nid_postfix(thread, name.c_str());
        if (error != 63) {
            std::fprintf(stderr, "pthread_rename_np: %zu-byte name returned %d; expected ENAMETOOLONG=63\n", length, error);
            std::abort();
        }
        Require(*__error_nid_postfix() == 77);
        CheckStoredName(thread, names[3]);
    }
    Require(pthread_rename_np_nid_postfix(thread, "renamed") == 0);
    CheckStoredName(thread, "renamed");
    *__error_nid_postfix() = savedError;
}

static void* APS5_VABI Worker(void*) {
    const Pthread self = scePthreadSelf();
    Require(self != nullptr);
    CheckThreadRename(self);
    return nullptr;
}

int main() {
    const Pthread self = scePthreadSelf();
    Require(self != nullptr);
    CheckThreadRename(self);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, nullptr, "rename-test") == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
}
