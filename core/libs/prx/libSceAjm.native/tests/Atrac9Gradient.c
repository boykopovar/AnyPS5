#include "unpack.c"
#include <stdlib.h>

static void WriteBits(unsigned char* data, int* position, int value, int width)
{
    for (int bit = width - 1; bit >= 0; --bit)
    {
        int offset = (*position)++;
        data[offset / 8] |= (unsigned char)(((value >> bit) & 1) << (7 - offset % 8));
    }
}

static At9Status GradientStatus(int endUnit)
{
    unsigned char data[4] = {0};
    int position = 0;
    WriteBits(data, &position, 0, 2);
    WriteBits(data, &position, 0, 6);
    WriteBits(data, &position, endUnit - 1, 6);
    WriteBits(data, &position, 0, 5);
    WriteBits(data, &position, 0, 5);
    WriteBits(data, &position, 0, 4);

    Block block = {0};
    BitReaderCxt reader;
    InitBitReaderCxt(&reader, data, sizeof(data));
    return ReadGradientParams(&block, &reader);
}

int main(void)
{
    if (GradientStatus(31) != ERR_SUCCESS) return EXIT_FAILURE;
    if (GradientStatus(32) != ERR_UNPACK_GRAD_END_UNIT_OOB) return EXIT_FAILURE;
    if (GradientStatus(47) != ERR_UNPACK_GRAD_END_UNIT_OOB) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
