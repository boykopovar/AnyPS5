#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <unistd.h>

static void ReportErrno(const char* name) {
    std::printf("%s: errno %d %s\n", name, errno, std::strerror(errno));
}

int main() {
    const long pageSize = ::getpagesize();
    const auto page = static_cast<std::size_t>(pageSize > 0 ? pageSize : 4096);
    const char* inserted = std::getenv("DYLD_INSERT_LIBRARIES");
    std::printf("dyld_insert_libraries: %s\n", inserted == nullptr ? "unset" : inserted);
#if defined(__x86_64__)
    std::printf("arch: x86_64\n");
#elif defined(__aarch64__)
    std::printf("arch: arm64\n");
#else
    std::printf("arch: other\n");
#endif

    void* rwx = ::mmap(nullptr, page, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (rwx == MAP_FAILED) ReportErrno("mmap_rwx");
    else {
        std::printf("mmap_rwx: ok\n");
        ::munmap(rwx, page);
    }

    void* rw = ::mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (rw == MAP_FAILED) ReportErrno("mmap_rw");
    else {
        std::printf("mmap_rw: ok\n");
        if (::mprotect(rw, page, PROT_READ | PROT_EXEC) != 0) ReportErrno("mprotect_rx");
        else std::printf("mprotect_rx: ok\n");
        ::munmap(rw, page);
    }

    void* writable = ::mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (writable == MAP_FAILED) ReportErrno("calltrace_mmap");
    else {
        mach_vm_address_t executable = 0;
        vm_prot_t current = VM_PROT_NONE;
        vm_prot_t maximum = VM_PROT_NONE;
        const kern_return_t remapped = ::mach_vm_remap(mach_task_self(), &executable, page, 0, VM_FLAGS_ANYWHERE,
            mach_task_self(), reinterpret_cast<mach_vm_address_t>(writable), false, &current, &maximum, VM_INHERIT_NONE);
        if (remapped != KERN_SUCCESS) std::printf("mach_vm_remap: kern %d\n", remapped);
        else {
            const kern_return_t protectedPages = ::mach_vm_protect(mach_task_self(), executable, page, false, VM_PROT_READ | VM_PROT_EXECUTE);
            if (protectedPages != KERN_SUCCESS) std::printf("mach_vm_protect_rx: kern %d\n", protectedPages);
            else std::printf("mach_vm_protect_rx: ok\n");
            ::mach_vm_deallocate(mach_task_self(), executable, page);
        }
        ::munmap(writable, page);
    }
    return 0;
}
