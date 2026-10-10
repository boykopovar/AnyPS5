#include "SceTypes.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
Pthread APS5_VABI pthread_self_nid_postfix(void);
int APS5_VABI pthread_rename_np_nid_postfix(Pthread thread, const char* name);
int APS5_VABI pthread_getname_np_nid_postfix(Pthread thread, char* name);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
}

static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EFAULT = 14;

static void Require(bool value) { if (!value) std::abort(); }

static std::array<unsigned char, 64> Read(Pthread thread, int expected) {
    std::array<unsigned char, 64> buffer{};
    buffer.fill(0xAA);
    Require(pthread_getname_np_nid_postfix(thread, reinterpret_cast<char*>(buffer.data())) == expected);
    return buffer;
}

static void RequireName(Pthread thread, const std::string& name) {
    const auto buffer = Read(thread, 0);
    Require(std::memcmp(buffer.data(), name.c_str(), name.size() + 1) == 0);
    for (std::size_t index = name.size() + 1; index < buffer.size(); ++index) Require(buffer[index] == 0xAA);
}

static void* APS5_VABI Unnamed(void*) {
    RequireName(pthread_self_nid_postfix(), "");
    return nullptr;
}

int main() {
    const Pthread self = pthread_self_nid_postfix();
    for (const char* name : {"", "A", "AnyPS5Probe", "0123456789012345678901234567890"}) {
        Require(pthread_rename_np_nid_postfix(self, name) == 0);
        RequireName(self, name);
    }
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, Unnamed, nullptr, nullptr) == 0);
    Require(scePthreadJoin(worker, nullptr) == 0);

    const auto untouched = Read(nullptr, GUEST_ESRCH);
    for (unsigned char byte : untouched) Require(byte == 0xAA);
    Require(pthread_getname_np_nid_postfix(self, nullptr) == GUEST_EFAULT);
}
