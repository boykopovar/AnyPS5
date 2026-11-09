#include <common.h>
#include <libkernel.h>
#include <sys/mman.h>
#include <errno.h>

TEST(GuestKernelMlockLimits, AlignmentAndLengthValidation) {
    size_t pageSize = sysconf(_SC_PAGESIZE);
    void *map = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    ASSERT_NE(map, MAP_FAILED);

    char *unalignedPtr = static_cast<char*>(map) + 1;
    int retMlock = mlock(unalignedPtr, pageSize);
    EXPECT_EQ(retMlock, -1);
    EXPECT_EQ(errno, EINVAL);

    int retMunlock = munlock(unalignedPtr, pageSize);
    EXPECT_EQ(retMunlock, -1);
    EXPECT_EQ(errno, EINVAL);

    int retMlockZero = mlock(map, 0);
    EXPECT_EQ(retMlockZero, -1);
    EXPECT_EQ(errno, EINVAL);

    int retMunlockZero = munlock(map, 0);
    EXPECT_EQ(retMunlockZero, -1);
    EXPECT_EQ(errno, EINVAL);

    munmap(map, pageSize * 2);
}
