#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstring>
#include <string>

extern "C" {
Pthread APS5_VABI pthread_self_nid_postfix(void);
int APS5_VABI pthread_rename_np_nid_postfix(Pthread thread, const char* name);
int APS5_VABI pthread_getname_np_nid_postfix(Pthread thread, char* name);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int GUEST_ESRCH = 3;
constexpr int GUEST_EFAULT = 14;
constexpr unsigned char poison = 0xAA;

using NameBuffer = std::array<unsigned char, 64>;

struct NameRead {
    int result = -1;
    NameBuffer buffer{};
};

NameRead ReadName(Pthread thread) {
    NameRead read;
    read.buffer.fill(poison);
    read.result = pthread_getname_np_nid_postfix(thread, reinterpret_cast<char*>(read.buffer.data()));
    return read;
}

void RequireName(const NameRead& read, const std::string& name) {
    RequireEqual(read.result, 0, "getname result for '" + name + "'");
    Require(std::memcmp(read.buffer.data(), name.c_str(), name.size() + 1) == 0, "name bytes for '" + name + "'");
    for (std::size_t index = name.size() + 1; index < read.buffer.size(); ++index) {
        RequireEqual(read.buffer[index], poison, "byte " + std::to_string(index) + " after '" + name + "'");
    }
}

class MainThreadNameRestore {
public:
    MainThreadNameRestore() {
        const NameRead read = ReadName(pthread_self_nid_postfix());
        if (read.result == 0) original.assign(reinterpret_cast<const char*>(read.buffer.data()));
    }
    ~MainThreadNameRestore() { pthread_rename_np_nid_postfix(pthread_self_nid_postfix(), original.c_str()); }
    MainThreadNameRestore(const MainThreadNameRestore&) = delete;
    MainThreadNameRestore& operator=(const MainThreadNameRestore&) = delete;

private:
    std::string original;
};

void* APS5_VABI ReadOwnName(void* arg) {
    *static_cast<NameRead*>(arg) = ReadName(pthread_self_nid_postfix());
    return nullptr;
}

const Case renamed{"PthreadGetname_AfterRename_ReturnsNameWithoutWritingPastTerminator", [] {
    const MainThreadNameRestore restore;
    const Pthread self = pthread_self_nid_postfix();
    for (const char* name : {"", "A", "AnyPS5Probe", "0123456789012345678901234567890"}) {
        RequireEqual(pthread_rename_np_nid_postfix(self, name), 0, std::string("rename to '") + name + "'");
        RequireName(ReadName(self), name);
    }
}};

const Case unnamedThread{"PthreadGetname_ThreadCreatedWithoutName_ReturnsEmptyName", [] {
    NameRead read;
    Pthread worker = nullptr;
    RequireEqual(scePthreadCreate(&worker, nullptr, ReadOwnName, &read, nullptr), 0, "create thread");
    RequireEqual(scePthreadJoin(worker, nullptr), 0, "join thread");
    RequireName(read, "");
}};

const Case nullThread{"PthreadGetname_NullThread_FailsWithEsrchAndLeavesBuffer", [] {
    const NameRead read = ReadName(nullptr);
    RequireEqual(read.result, GUEST_ESRCH, "getname result");
    for (std::size_t index = 0; index < read.buffer.size(); ++index) {
        RequireEqual(read.buffer[index], poison, "byte " + std::to_string(index));
    }
}};

const Case nullBuffer{"PthreadGetname_NullBuffer_FailsWithEfault", [] {
    RequireEqual(pthread_getname_np_nid_postfix(pthread_self_nid_postfix(), nullptr), GUEST_EFAULT, "getname result");
}};

} // namespace
