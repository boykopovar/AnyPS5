#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_USERCONTEXT_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_USERCONTEXT_HPP

#include <cstddef>
#include <cstdint>

struct UserMcontext {
    std::uint64_t onstack;
    std::uint64_t rdi;
    std::uint64_t rsi;
    std::uint64_t rdx;
    std::uint64_t rcx;
    std::uint64_t r8;
    std::uint64_t r9;
    std::uint64_t rax;
    std::uint64_t rbx;
    std::uint64_t rbp;
    std::uint64_t r10;
    std::uint64_t r11;
    std::uint64_t r12;
    std::uint64_t r13;
    std::uint64_t r14;
    std::uint64_t r15;
    std::uint32_t trapno;
    std::uint16_t fs;
    std::uint16_t gs;
    std::uint64_t addr;
    std::uint32_t flags;
    std::uint16_t es;
    std::uint16_t ds;
    std::uint64_t err;
    std::uint64_t rip;
    std::uint64_t cs;
    std::uint64_t rflags;
    std::uint64_t rsp;
    std::uint64_t ss;
    std::uint64_t len;
    std::uint64_t fpformat;
    std::uint64_t ownedfp;
    alignas(16) std::uint64_t fpstate[64];
    std::uint64_t fsbase;
    std::uint64_t gsbase;
    std::uint64_t xfpustate;
    std::uint64_t xfpustateLength;
    std::uint64_t spare[4];
};

struct UserUcontext {
    std::uint32_t sigmask[4];
    UserMcontext mcontext;
    UserUcontext* link;
    void* stackPointer;
    std::uint64_t stackSize;
    std::int32_t stackFlags;
    std::int32_t stackAlign;
    std::int32_t flags;
    std::int32_t spare[4];
};

static_assert(offsetof(UserMcontext, rsp) == 0xb8 && offsetof(UserMcontext, len) == 0xc8);
static_assert(offsetof(UserMcontext, fpstate) == 0xe0 && offsetof(UserMcontext, fsbase) == 0x2e0);
static_assert(sizeof(UserMcontext) == 0x320);
static_assert(offsetof(UserUcontext, mcontext) == 0x10);
static_assert(offsetof(UserUcontext, link) == 0x330);
static_assert(offsetof(UserUcontext, stackPointer) == 0x338);
static_assert(offsetof(UserUcontext, stackSize) == 0x340);
static_assert(offsetof(UserUcontext, flags) == 0x350);
static_assert(sizeof(UserUcontext) == 0x370);

extern "C" int SigreturnContext_nid_no_patch(const UserUcontext* context);

#endif
