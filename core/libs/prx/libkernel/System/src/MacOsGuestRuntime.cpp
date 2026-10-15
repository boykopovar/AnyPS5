#ifdef __APPLE__

#include "prx/libc/include/HostMainThread.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <vector>

namespace {

struct GuestTlsDescriptor {
    std::uint64_t version;
    std::uint64_t image;
    std::uint64_t imageBytes;
    std::uint64_t blockBytes;
    std::uint64_t alignment;
    std::uint64_t key;
};
static_assert(offsetof(GuestTlsDescriptor, key) == 0x28);

struct GuestTlsIndex {
    const GuestTlsDescriptor* descriptor;
    std::uint64_t offset;
};

constexpr std::size_t ControlBlockBytes = 0x40;
constexpr std::size_t StackGuardSlot = 0x28;
constexpr std::size_t AllocationSlot = 0x38;
constexpr std::size_t GuestMainStackBytes = std::size_t{64} << 20;

struct GuestStart {
    void* entry;
    std::vector<std::uint64_t> parameters;
};

[[noreturn]] void Fail(const char* message) {
    const auto written = ::write(STDERR_FILENO, message, std::strlen(message));
    (void)written;
    std::abort();
}

std::uint64_t StackGuard() {
    static const std::uint64_t guard = (static_cast<std::uint64_t>(arc4random()) << 32) | arc4random();
    return guard;
}

void FreeBlock(void* threadPointer) {
    std::uint64_t allocation;
    std::memcpy(&allocation, static_cast<std::uint8_t*>(threadPointer) + AllocationSlot, sizeof(allocation));
    std::free(reinterpret_cast<void*>(allocation));
}

void Store(std::uint8_t* at, std::uint64_t value) {
    std::memcpy(at, &value, sizeof(value));
}

std::uintptr_t AllocateBlock(const GuestTlsDescriptor* descriptor) {
    if (descriptor->imageBytes > descriptor->blockBytes) Fail("guest TLS: template larger than its block\n");
    std::size_t alignment = 16;
    while (alignment < descriptor->alignment) alignment <<= 1;
    void* allocation = nullptr;
    if (posix_memalign(&allocation, alignment, descriptor->blockBytes + ControlBlockBytes) != 0) Fail("guest TLS: block allocation failed\n");
    auto* block = static_cast<std::uint8_t*>(allocation);
    if (descriptor->imageBytes != 0) std::memcpy(block, reinterpret_cast<const void*>(descriptor->image), descriptor->imageBytes);
    std::memset(block + descriptor->imageBytes, 0, descriptor->blockBytes - descriptor->imageBytes + ControlBlockBytes);
    auto* threadPointer = block + descriptor->blockBytes;
    Store(threadPointer, reinterpret_cast<std::uint64_t>(threadPointer));
    Store(threadPointer + StackGuardSlot, StackGuard());
    Store(threadPointer + AllocationSlot, reinterpret_cast<std::uint64_t>(allocation));
    if (pthread_setspecific(static_cast<pthread_key_t>(descriptor->key), threadPointer) != 0) Fail("guest TLS: pthread_setspecific failed\n");
    return reinterpret_cast<std::uintptr_t>(threadPointer);
}

}

extern "C" {

std::uintptr_t Aps5GuestTlsAllocateBody_nid_no_patch(const GuestTlsDescriptor* descriptor) {
    return AllocateBlock(descriptor);
}

void Aps5GuestTlsRegister_nid_no_patch(GuestTlsDescriptor* descriptor) {
    pthread_key_t key;
    if (pthread_key_create(&key, FreeBlock) != 0) Fail("guest TLS: pthread_key_create failed\n");
    descriptor->key = key;
}

void* APS5_VABI Aps5GuestTlsGetAddr_nid_no_patch(const GuestTlsIndex* index) {
    const auto* descriptor = index->descriptor;
    auto threadPointer = reinterpret_cast<std::uintptr_t>(pthread_getspecific(static_cast<pthread_key_t>(descriptor->key)));
    if (threadPointer == 0) threadPointer = AllocateBlock(descriptor);
    return reinterpret_cast<void*>(threadPointer - descriptor->blockBytes + index->offset);
}

[[noreturn]] void Aps5StartGuest_nid_no_patch(void* entry, const std::uint64_t* parameters) {
    const auto argumentCount = parameters[0];
    std::size_t words = 1 + argumentCount + 1;
    while (parameters[words] != 0) ++words;
    ++words;
    auto* start = new GuestStart{entry, std::vector<std::uint64_t>(parameters, parameters + words)};
    start->parameters.push_back(0);
    start->parameters.push_back(0);
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0 || pthread_attr_setstacksize(&attributes, GuestMainStackBytes) != 0) Fail("guest start: thread attributes failed\n");
    pthread_t thread;
    const auto run = [](void* context) -> void* {
        auto* start = static_cast<GuestStart*>(context);
        using GuestEntry = int APS5_VABI (*)(std::uint64_t*, void (*)());
        const int result = reinterpret_cast<GuestEntry>(start->entry)(start->parameters.data(), nullptr);
        std::exit(result);
    };
    if (pthread_create(&thread, &attributes, run, start) != 0) Fail("guest start: pthread_create failed\n");
    pthread_attr_destroy(&attributes);
    HostMainThreadServe_nid_no_patch();
}

}

asm(".text\n"
    APS5_ASM_FUNCTION("Aps5GuestTlsAllocate_nid_no_patch")
    "    push %rbp\n"
    "    mov %rsp, %rbp\n"
    "    push %rdi\n"
    "    push %rsi\n"
    "    push %rdx\n"
    "    push %rcx\n"
    "    push %r8\n"
    "    push %r9\n"
    "    push %r10\n"
    "    push %r11\n"
    "    sub $544, %rsp\n"
    "    and $-32, %rsp\n"
    "    vmovdqu %ymm0, 0(%rsp)\n"
    "    vmovdqu %ymm1, 32(%rsp)\n"
    "    vmovdqu %ymm2, 64(%rsp)\n"
    "    vmovdqu %ymm3, 96(%rsp)\n"
    "    vmovdqu %ymm4, 128(%rsp)\n"
    "    vmovdqu %ymm5, 160(%rsp)\n"
    "    vmovdqu %ymm6, 192(%rsp)\n"
    "    vmovdqu %ymm7, 224(%rsp)\n"
    "    vmovdqu %ymm8, 256(%rsp)\n"
    "    vmovdqu %ymm9, 288(%rsp)\n"
    "    vmovdqu %ymm10, 320(%rsp)\n"
    "    vmovdqu %ymm11, 352(%rsp)\n"
    "    vmovdqu %ymm12, 384(%rsp)\n"
    "    vmovdqu %ymm13, 416(%rsp)\n"
    "    vmovdqu %ymm14, 448(%rsp)\n"
    "    vmovdqu %ymm15, 480(%rsp)\n"
    "    stmxcsr 512(%rsp)\n"
    "    fnstcw 516(%rsp)\n"
    "    mov %rax, %rdi\n"
    "    call " APS5_ASM_SYMBOL("Aps5GuestTlsAllocateBody_nid_no_patch") "\n"
    "    fldcw 516(%rsp)\n"
    "    ldmxcsr 512(%rsp)\n"
    "    vmovdqu 0(%rsp), %ymm0\n"
    "    vmovdqu 32(%rsp), %ymm1\n"
    "    vmovdqu 64(%rsp), %ymm2\n"
    "    vmovdqu 96(%rsp), %ymm3\n"
    "    vmovdqu 128(%rsp), %ymm4\n"
    "    vmovdqu 160(%rsp), %ymm5\n"
    "    vmovdqu 192(%rsp), %ymm6\n"
    "    vmovdqu 224(%rsp), %ymm7\n"
    "    vmovdqu 256(%rsp), %ymm8\n"
    "    vmovdqu 288(%rsp), %ymm9\n"
    "    vmovdqu 320(%rsp), %ymm10\n"
    "    vmovdqu 352(%rsp), %ymm11\n"
    "    vmovdqu 384(%rsp), %ymm12\n"
    "    vmovdqu 416(%rsp), %ymm13\n"
    "    vmovdqu 448(%rsp), %ymm14\n"
    "    vmovdqu 480(%rsp), %ymm15\n"
    "    lea -64(%rbp), %rsp\n"
    "    pop %r11\n"
    "    pop %r10\n"
    "    pop %r9\n"
    "    pop %r8\n"
    "    pop %rcx\n"
    "    pop %rdx\n"
    "    pop %rsi\n"
    "    pop %rdi\n"
    "    pop %rbp\n"
    "    ret\n");

#endif
