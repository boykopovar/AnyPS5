#include <common/Logger.hpp>
#include <gtest/gtest.h>
#include <libSceAudioIn.h>

TEST(GuestAudioInReadBounds, InputNullBuffer) {
    int32_t handle = -1;
    int ret = sceAudioInInput(handle, nullptr);
    EXPECT_NE(ret, 0);
}

TEST(GuestAudioInReadBounds, InputInvalidHandle) {
    float buffer[256];
    int ret = sceAudioInInput(9999, buffer);
    EXPECT_NE(ret, 0);
}

#endif
