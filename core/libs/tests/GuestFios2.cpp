#include <gtest/gtest.h>
#include "libSceFios2/Api.hpp"

TEST(GuestFios2Test, SceFiosOpCancelInvalidEventId) {
    int invalidEventId = -1;
    int result = sceFiosOpCancel(invalidEventId);
    EXPECT_NE(result, 0);
}

TEST(GuestFios2Test, SceFiosOpCancelZeroEventId) {
    int zeroEventId = 0;
    int result = sceFiosOpCancel(zeroEventId);
    EXPECT_NE(result, 0);
}
