#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

constexpr int PosixEinval = 22;
constexpr std::size_t PageSize = 0x4000;
constexpr int ProtRead = 1;
constexpr int ProtWrite = 2;
constexpr int ProtReadWrite = ProtRead | ProtWrite;
constexpr int MapPrivate = 0x2;
constexpr int MapAnon = 0x1000;
constexpr int AdviceNormal = 0;
constexpr int AdviceDontNeed = 4;
constexpr int AdviceProtect = 10;
constexpr std::uintptr_t UserAddressEnd = 0x800000000000;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestMadviseBounds() {
    *__error_nid_postfix() = 0;
    void* mapped = mmap_nid_postfix(nullptr, PageSize * 4, ProtReadWrite, MapPrivate | MapAnon, -1, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));

    for (int advice = 0; advice <= 10; ++advice) {
        Require(madvise_nid_postfix(mapped, PageSize * 4, advice) == 0);
    }

    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(mapped, PageSize, -1) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(mapped, PageSize, 11) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(mapped, PageSize, 100) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(reinterpret_cast<void*>(UserAddressEnd + 1), PageSize, AdviceNormal) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(mapped, std::numeric_limits<std::size_t>::max(), AdviceNormal) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    Require(madvise_nid_postfix(mapped, std::numeric_limits<std::size_t>::max(), AdviceProtect) == 0);
    Require(madvise_nid_postfix(reinterpret_cast<void*>(UserAddressEnd + 1), PageSize, AdviceProtect) == 0);

    Require(madvise_nid_postfix(mapped, 0, AdviceNormal) == 0);
    *__error_nid_postfix() = 0;
    Require(madvise_nid_postfix(mapped, 0, 11) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    auto* unaligned = static_cast<char*>(mapped) + 128;
    Require(madvise_nid_postfix(unaligned, PageSize - 128, AdviceDontNeed) == 0);

    Require(munmap_nid_postfix(mapped, PageSize * 4) == 0);
}

void TestPosixMadviseBounds() {
    *__error_nid_postfix() = 77;
    void* mapped = mmap_nid_postfix(nullptr, PageSize * 4, ProtReadWrite, MapPrivate | MapAnon, -1, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));

    for (int advice = 0; advice <= 10; ++advice) {
        Require(posix_madvise_nid_postfix(mapped, PageSize * 4, advice) == 0);
    }
    Require(*__error_nid_postfix() == 77);

    Require(posix_madvise_nid_postfix(mapped, PageSize, -1) == PosixEinval);
    Require(posix_madvise_nid_postfix(mapped, PageSize, 11) == PosixEinval);
    Require(posix_madvise_nid_postfix(mapped, PageSize, 255) == PosixEinval);
    Require(*__error_nid_postfix() == 77);

    Require(posix_madvise_nid_postfix(nullptr, PageSize, AdviceNormal) == PosixEinval);
    Require(posix_madvise_nid_postfix(reinterpret_cast<void*>(0x1000), PageSize, AdviceNormal) == PosixEinval);
    Require(posix_madvise_nid_postfix(reinterpret_cast<void*>(PageSize - 1), 1, AdviceNormal) == PosixEinval);
    Require(posix_madvise_nid_postfix(reinterpret_cast<void*>(UserAddressEnd + 1), PageSize, AdviceNormal) == PosixEinval);
    Require(posix_madvise_nid_postfix(mapped, std::numeric_limits<std::size_t>::max(), AdviceNormal) == PosixEinval);

    Require(posix_madvise_nid_postfix(nullptr, PageSize, AdviceProtect) == 0);
    Require(posix_madvise_nid_postfix(reinterpret_cast<void*>(UserAddressEnd + 1), PageSize, AdviceProtect) == 0);
    Require(posix_madvise_nid_postfix(mapped, std::numeric_limits<std::size_t>::max(), AdviceProtect) == 0);

    Require(posix_madvise_nid_postfix(mapped, 0, AdviceNormal) == 0);
    Require(posix_madvise_nid_postfix(mapped, 0, 11) == PosixEinval);
    Require(posix_madvise_nid_postfix(nullptr, 0, AdviceNormal) == PosixEinval);
    Require(*__error_nid_postfix() == 77);

    auto* unaligned = static_cast<char*>(mapped) + 64;
    Require(posix_madvise_nid_postfix(unaligned, PageSize - 64, AdviceDontNeed) == 0);

    Require(munmap_nid_postfix(mapped, PageSize * 4) == 0);
}

void TestMprotectBoundsAndAlignment() {
    Require(mprotect_nid_postfix(nullptr, 0, ProtReadWrite) == 0);
    Require(sceKernelMprotect(nullptr, 0, ProtReadWrite) == 0);

    *__error_nid_postfix() = 0;
    Require(mprotect_nid_postfix(nullptr, PageSize, ProtRead) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    void* mapped = mmap_nid_postfix(nullptr, PageSize * 4, ProtReadWrite, MapPrivate | MapAnon, -1, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));

    auto* bytes = static_cast<char*>(mapped);
    bytes[0] = 0x42;
    bytes[PageSize * 4 - 1] = 0x24;
    Require(bytes[0] == 0x42);
    Require(bytes[PageSize * 4 - 1] == 0x24);

    Require(mprotect_nid_postfix(mapped, 0, ProtRead) == 0);
    Require(sceKernelMprotect(mapped, 0, ProtRead) == 0);

    *__error_nid_postfix() = 0;
    Require(mprotect_nid_postfix(mapped, std::numeric_limits<std::size_t>::max(), ProtRead) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(mprotect_nid_postfix(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - PageSize + 1), PageSize * 2, ProtRead) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    Require(sceKernelMprotect(mapped, PageSize * 4, ProtRead) == 0);
    Require(bytes[0] == 0x42);

    Require(mprotect_nid_postfix(mapped, PageSize * 4, ProtReadWrite) == 0);
    bytes[0] = 0x55;
    Require(bytes[0] == 0x55);

    Require(sceKernelMprotect(mapped, PageSize, 0x8000 | ProtReadWrite) == 0);
    Require(mprotect_nid_postfix(mapped, PageSize, 0x10000 | ProtReadWrite) == 0);

    Require(mprotect_nid_postfix(bytes + 1, PageSize - 2, ProtRead) == 0);
    Require(sceKernelMprotect(bytes + 1, PageSize - 2, ProtReadWrite) == 0);
    bytes[1] = 0x77;
    Require(bytes[1] == 0x77);

    Require(munmap_nid_postfix(mapped, PageSize * 4) == 0);
}

} // namespace

int main() {
    TestMadviseBounds();
    TestPosixMadviseBounds();
    TestMprotectBoundsAndAlignment();
    return 0;
}
