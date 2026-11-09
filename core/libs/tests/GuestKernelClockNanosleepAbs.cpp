#include <gtest/gtest.h>
#include <time.h>
#include <errno.h>

TEST(GuestKernelClockNanosleepAbs, InvalidClockId)
{
    struct timespec ts = {0, 0};
    int ret = clock_nanosleep(9999, TIMER_ABSTIME, &ts, nullptr);
    EXPECT_EQ(ret, EINVAL);
}

TEST(GuestKernelClockNanosleepAbs, NullTimespec)
{
    int ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, nullptr, nullptr);
    EXPECT_EQ(ret, EFAULT);
}

TEST(GuestKernelClockNanosleepAbs, InvalidNsec)
{
    struct timespec ts = {0, 2000000000};
    int ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, nullptr);
    EXPECT_EQ(ret, EINVAL);
}

TEST(GuestKernelClockNanosleepAbs, NegativeNsec)
{
    struct timespec ts = {0, -1};
    int ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, nullptr);
    EXPECT_EQ(ret, EINVAL);
}
