#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <string>

static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EFAULT = 14;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);

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

static void* APS5_VABI ExitNow(void*) {
    return nullptr;
}

int main() {
    const Pthread self = pthread_self_nid_postfix();
    for (const char* name : {"", "A", "AnyPS5Probe", "0123456789012345678901234567890"}) {
        Require(pthread_rename_np_nid_postfix(self, name) == 0);
        RequireName(self, name);
    }
    Require(scePthreadRename(self, "SceRenamed") == 0);
    RequireName(self, "SceRenamed");
    Require(scePthreadRename(self, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRename(nullptr, "Unused") == SCE_KERNEL_ERROR_EINVAL);
    RequireName(self, "SceRenamed");
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, Unnamed, nullptr, nullptr) == 0);
    Require(scePthreadJoin(worker, nullptr) == 0);

    // A joined thread answers pthread_getname_np with ESRCH (PS5 firmware 13.60) and leaves the
    // output buffer untouched, so the posix wrapper must not read the retained object's name.
    Pthread joined = nullptr;
    Require(scePthreadCreate(&joined, nullptr, ExitNow, nullptr, "JoinedWorker") == 0);
    Require(scePthreadJoin(joined, nullptr) == 0);
    const auto afterJoin = Read(joined, GUEST_ESRCH);
    for (unsigned char byte : afterJoin) Require(byte == 0xAA);

    // The SCE sibling has no measured joined-thread behaviour on the console; it still reads the
    // retained object's name, which only pins today's behaviour of scePthreadGetname.
    std::array<char, 64> sibling{};
    sibling.fill(0x5A);
    Require(scePthreadGetname(joined, sibling.data()) == 0);
    Require(std::memcmp(sibling.data(), "JoinedWorker", sizeof("JoinedWorker")) == 0);

    const auto untouched = Read(nullptr, GUEST_ESRCH);
    for (unsigned char byte : untouched) Require(byte == 0xAA);
    Require(pthread_getname_np_nid_postfix(self, nullptr) == GUEST_EFAULT);
}
