#ifdef __APPLE__

#include <pthread.h>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct GuestTlsDescriptor {
    std::uint64_t version;
    std::uint64_t templateAddress;
    std::uint64_t templateSize;
    std::uint64_t blockSize;
    std::uint64_t alignment;
    std::uint64_t key;
};

struct TlsIndex {
    const GuestTlsDescriptor* module;
    std::uint64_t offset;
};

constexpr std::size_t ThreadControlBlockBytes = 0x100;
constexpr std::size_t AllocationSlot = ThreadControlBlockBytes - sizeof(void*);

[[noreturn]] void Fail(const char* reason) {
    std::fprintf(stderr, "guest TLS: %s\n", reason);
    std::abort();
}

void Release(void* threadPointer) {
    std::free(*reinterpret_cast<void**>(static_cast<std::uint8_t*>(threadPointer) + AllocationSlot));
}

}

extern "C" {

std::uint8_t Aps5GuestTlsAvx_nid_no_patch = [] {
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx") ? 1 : 0;
}();

void Aps5GuestTlsRegister_nid_no_patch(GuestTlsDescriptor* descriptor) {
    if (descriptor == nullptr || descriptor->version != 1) Fail("unknown TLS descriptor");
    if (!std::has_single_bit(descriptor->alignment) || descriptor->blockSize % descriptor->alignment != 0 || descriptor->templateSize > descriptor->blockSize)
        Fail("invalid TLS descriptor layout");
    pthread_key_t key;
    if (pthread_key_create(&key, Release) != 0) Fail("cannot create a thread pointer key");
    descriptor->key = key;
}

void* Aps5GuestTlsAllocateThreadPointer_nid_no_patch(const GuestTlsDescriptor* descriptor) {
    if (descriptor == nullptr || descriptor->version != 1) Fail("unknown TLS descriptor");
    void* base = nullptr;
    const auto alignment = std::max<std::size_t>(descriptor->alignment, 16);
    if (posix_memalign(&base, alignment, descriptor->blockSize + ThreadControlBlockBytes) != 0) Fail("cannot allocate a TLS block");
    auto* bytes = static_cast<std::uint8_t*>(base);
    if (descriptor->templateSize != 0)
        std::memcpy(bytes, reinterpret_cast<const void*>(descriptor->templateAddress), descriptor->templateSize);
    std::memset(bytes + descriptor->templateSize, 0, descriptor->blockSize - descriptor->templateSize + ThreadControlBlockBytes);
    auto* threadPointer = bytes + descriptor->blockSize;
    *reinterpret_cast<void**>(threadPointer) = threadPointer;
    *reinterpret_cast<void**>(threadPointer + AllocationSlot) = base;
    if (pthread_setspecific(static_cast<pthread_key_t>(descriptor->key), threadPointer) != 0) Fail("cannot store the thread pointer");
    return threadPointer;
}

void* Aps5GuestTlsGetAddr_nid_no_patch(const TlsIndex* index) {
    const auto* descriptor = index->module;
    auto* threadPointer = static_cast<std::uint8_t*>(pthread_getspecific(static_cast<pthread_key_t>(descriptor->key)));
    if (threadPointer == nullptr) threadPointer = static_cast<std::uint8_t*>(Aps5GuestTlsAllocateThreadPointer_nid_no_patch(descriptor));
    return threadPointer - descriptor->blockSize + index->offset;
}

}

asm(R"(
    .text
    .globl _Aps5GuestTlsAllocate_nid_no_patch
_Aps5GuestTlsAllocate_nid_no_patch:
    push %rbp
    mov %rsp, %rbp
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    and $-64, %rsp
    sub $512, %rsp
    mov %rax, %rdi
    testb $1, _Aps5GuestTlsAvx_nid_no_patch(%rip)
    jz 1f
    vmovdqu %ymm0, 0(%rsp)
    vmovdqu %ymm1, 32(%rsp)
    vmovdqu %ymm2, 64(%rsp)
    vmovdqu %ymm3, 96(%rsp)
    vmovdqu %ymm4, 128(%rsp)
    vmovdqu %ymm5, 160(%rsp)
    vmovdqu %ymm6, 192(%rsp)
    vmovdqu %ymm7, 224(%rsp)
    vmovdqu %ymm8, 256(%rsp)
    vmovdqu %ymm9, 288(%rsp)
    vmovdqu %ymm10, 320(%rsp)
    vmovdqu %ymm11, 352(%rsp)
    vmovdqu %ymm12, 384(%rsp)
    vmovdqu %ymm13, 416(%rsp)
    vmovdqu %ymm14, 448(%rsp)
    vmovdqu %ymm15, 480(%rsp)
    call _Aps5GuestTlsAllocateThreadPointer_nid_no_patch
    vmovdqu 0(%rsp), %ymm0
    vmovdqu 32(%rsp), %ymm1
    vmovdqu 64(%rsp), %ymm2
    vmovdqu 96(%rsp), %ymm3
    vmovdqu 128(%rsp), %ymm4
    vmovdqu 160(%rsp), %ymm5
    vmovdqu 192(%rsp), %ymm6
    vmovdqu 224(%rsp), %ymm7
    vmovdqu 256(%rsp), %ymm8
    vmovdqu 288(%rsp), %ymm9
    vmovdqu 320(%rsp), %ymm10
    vmovdqu 352(%rsp), %ymm11
    vmovdqu 384(%rsp), %ymm12
    vmovdqu 416(%rsp), %ymm13
    vmovdqu 448(%rsp), %ymm14
    vmovdqu 480(%rsp), %ymm15
    jmp 2f
1:
    movdqu %xmm0, 0(%rsp)
    movdqu %xmm1, 32(%rsp)
    movdqu %xmm2, 64(%rsp)
    movdqu %xmm3, 96(%rsp)
    movdqu %xmm4, 128(%rsp)
    movdqu %xmm5, 160(%rsp)
    movdqu %xmm6, 192(%rsp)
    movdqu %xmm7, 224(%rsp)
    movdqu %xmm8, 256(%rsp)
    movdqu %xmm9, 288(%rsp)
    movdqu %xmm10, 320(%rsp)
    movdqu %xmm11, 352(%rsp)
    movdqu %xmm12, 384(%rsp)
    movdqu %xmm13, 416(%rsp)
    movdqu %xmm14, 448(%rsp)
    movdqu %xmm15, 480(%rsp)
    call _Aps5GuestTlsAllocateThreadPointer_nid_no_patch
    movdqu 0(%rsp), %xmm0
    movdqu 32(%rsp), %xmm1
    movdqu 64(%rsp), %xmm2
    movdqu 96(%rsp), %xmm3
    movdqu 128(%rsp), %xmm4
    movdqu 160(%rsp), %xmm5
    movdqu 192(%rsp), %xmm6
    movdqu 224(%rsp), %xmm7
    movdqu 256(%rsp), %xmm8
    movdqu 288(%rsp), %xmm9
    movdqu 320(%rsp), %xmm10
    movdqu 352(%rsp), %xmm11
    movdqu 384(%rsp), %xmm12
    movdqu 416(%rsp), %xmm13
    movdqu 448(%rsp), %xmm14
    movdqu 480(%rsp), %xmm15
2:
    lea -64(%rbp), %rsp
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbp
    ret
)");

#endif
