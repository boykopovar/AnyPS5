#include "libatrac9.h"
#include <stdlib.h>

static int ChannelCount(int sampleRateIndex, int channelConfigIndex)
{
    unsigned char config[ATRAC9_CONFIG_DATA_SIZE] = {0xFE, (unsigned char)(sampleRateIndex << 4 | channelConfigIndex << 1), 0x0B, 0xE0};
    Atrac9CodecInfo info = {0};
    void* decoder = Atrac9GetHandle();
    if (decoder == NULL) exit(EXIT_FAILURE);
    int channels = -1;
    if (Atrac9InitDecoder(decoder, config) == 0 && Atrac9GetCodecInfo(decoder, &info) == 0) channels = info.channels;
    Atrac9ReleaseHandle(decoder);
    return channels;
}

int main(void)
{
    static const int channels[8] = {1, 2, 2, 6, 8, 4, -1, -1};
    for (int sampleRateIndex = 0; sampleRateIndex < 16; ++sampleRateIndex)
    {
        for (int channelConfigIndex = 0; channelConfigIndex < 8; ++channelConfigIndex)
        {
            if (ChannelCount(sampleRateIndex, channelConfigIndex) != channels[channelConfigIndex]) return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
