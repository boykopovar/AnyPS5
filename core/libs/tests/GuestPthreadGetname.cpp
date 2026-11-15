#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EFAULT = 14;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCE_KERNEL_ERROR_EFAULT = static_cast<int>(0x8002000e);
static constexpr int SCE_KERNEL_ERROR_ENAMETOOLONG = static_cast<int>(0x8002003f);
static constexpr int SCE_KERNEL_ERROR_ESRCH = static_cast<int>(0x80020003);

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

static void* APS5_VABI MaximumName(void*) {
    RequireName(pthread_self_nid_postfix(), std::string(31, 'N'));
    return nullptr;
}

static void CheckUnusableBuffers(Pthread self, Pthread joined) {
    constexpr std::size_t Page = 0x4000;
    auto* memory = static_cast<char*>(mmap_nid_postfix(nullptr, 3 * Page, 3, 0x1002, -1, 0));
    Require(memory != reinterpret_cast<char*>(static_cast<std::uintptr_t>(-1)));
    char* const end = memory + Page;
    char* const readOnly = memory + 2 * Page;
    Require(mprotect_nid_postfix(end, Page, 0) == 0);
    Require(mprotect_nid_postfix(readOnly, Page, 1) == 0);
    Require(pthread_rename_np_nid_postfix(self, "kept") == 0);
    char* const bad[] = {reinterpret_cast<char*>(0x10), end, readOnly, end - 4};
    for (char* buffer : bad) {
        Require(pthread_getname_np_nid_postfix(self, buffer) == GUEST_EFAULT);
        Require(scePthreadGetname(self, buffer) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadGetname(joined, buffer) == SCE_KERNEL_ERROR_EFAULT);
        Require(pthread_getname_np_nid_postfix(nullptr, buffer) == GUEST_ESRCH);
        Require(pthread_getname_np_nid_postfix(joined, buffer) == GUEST_ESRCH);
        Require(scePthreadGetname(reinterpret_cast<Pthread>(0x10), buffer) == SCE_KERNEL_ERROR_ESRCH);
        Require(scePthreadGetname(nullptr, buffer) == SCE_KERNEL_ERROR_EINVAL);
    }
    std::memset(memory, 'Z', Page);
    char* const exact = end - 5;
    Require(pthread_getname_np_nid_postfix(self, exact) == 0 && std::memcmp(exact, "kept", 5) == 0);
    std::memset(memory, 'Z', Page);
    Require(scePthreadGetname(self, exact) == 0 && std::memcmp(exact, "kept", 5) == 0);
    Require(exact[-1] == 'Z');
    Require(scePthreadGetname(self, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(mprotect_nid_postfix(end, Page, 3) == 0);
    Require(mprotect_nid_postfix(readOnly, Page, 3) == 0);
    Require(munmap_nid_postfix(memory, 3 * Page) == 0);
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

    Pthread invalidName = reinterpret_cast<Pthread>(static_cast<std::uintptr_t>(1));
    Require(scePthreadCreate(&invalidName, nullptr, ExitNow, nullptr, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(0x10))) == SCE_KERNEL_ERROR_EFAULT);
    Require(invalidName == nullptr);
    invalidName = reinterpret_cast<Pthread>(static_cast<std::uintptr_t>(1));
    const std::string tooLongName(32, 'N');
    Require(scePthreadCreate(&invalidName, nullptr, ExitNow, nullptr, tooLongName.c_str()) == SCE_KERNEL_ERROR_ENAMETOOLONG);
    Require(invalidName == nullptr);
    const std::string maximumName(31, 'N');
    Pthread named = nullptr;
    Require(scePthreadCreate(&named, nullptr, MaximumName, nullptr, maximumName.c_str()) == 0);
    Require(scePthreadJoin(named, nullptr) == 0);

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
    CheckUnusableBuffers(self, joined);
}
