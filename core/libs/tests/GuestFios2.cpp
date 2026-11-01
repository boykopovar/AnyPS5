#include <gtest/gtest.h>
#include "prx/libSceFios2/include/Api.hpp"

TEST(GuestFios2, FileReadSyncNullBuffer) {
    int res = sceFiosFileReadSync();
    EXPECT_NE(res, 0);
}
