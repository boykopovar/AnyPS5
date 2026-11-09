#include <gtest/gtest.h>
#include <signal.h>
#include <errno.h>

TEST(GuestKernelSigactionUncatchable, SigkillUncatchable) {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    int ret = sigaction(SIGKILL, &sa, nullptr);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(errno, EINVAL);
}

TEST(GuestKernelSigactionUncatchable, SigstopUncatchable) {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    int ret = sigaction(SIGSTOP, &sa, nullptr);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(errno, EINVAL);
}
