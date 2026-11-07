#include <stddef.h>
#include <stdint.h>

char *getenv(const char *name);
void exit(int status);
int puts(const char *text);
int64_t time(int64_t *timer);
int sceVideoOutOpen(int userId, int busType, int index, const void *param);
void sceVideoOutSetBufferAttribute2(void *attribute, uint64_t pixelFormat, uint32_t tilingMode, uint32_t width, uint32_t height, uint64_t option, uint32_t dccControl, uint64_t dccClear);
int sceVideoOutRegisterBuffers2(int handle, int setIndex, int bufferIndexStart, const void *buffers, int bufferNum, const void *attribute, int category, void *option);
int sceVideoOutSubmitFlip(int handle, int index, int flipMode, int64_t flipArg);
int sceVideoOutWaitVblank(int handle);
int scePadInit(void);
int scePadOpen(int userId, int type, int index, const void *param);
int scePadRead(int handle, void *data, int count);
int sceAudioOutOpen(int userId, int type, int index, uint32_t len, uint32_t freq, uint32_t param);
int sceAudioOutOutput(int handle, const void *ptr);

enum {
    SquareWidth = 320, SquareHeight = 180, SquareSize = 28, SquareFramesDefault = 180,
    PlayerX = 200, PlayerY = 90, PlayerSize = 12,
    CoinSize = 8
};

static _Alignas(65536) uint8_t square_pixels[262144];

struct SquareBuffers {
    const void *data;
    const void *metadata;
    const void *reserved0;
    const void *reserved1;
};

static int square_frames(const char *text) {
    int value = 0;
    if (text == 0 || *text == 0) return SquareFramesDefault;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (*text - '0');
        ++text;
    }
    return value > 0 ? value : SquareFramesDefault;
}

static int square_enabled(const char *text) {
    if (text == 0 || *text == 0 || *text == '0') return 0;
    return 1;
}

static void fill(uint8_t *pixels, int originX, int originY, int wide, int high, uint8_t blue, uint8_t green, uint8_t red) {
    int y;
    for (y = 0; y < high; ++y) {
        int x;
        int row = originY + y;
        if (row < 0 || row >= SquareHeight) continue;
        for (x = 0; x < wide; ++x) {
            int column = originX + x;
            uint8_t *pixel;
            if (column < 0 || column >= SquareWidth) continue;
            pixel = pixels + ((size_t)row * SquareWidth + (size_t)column) * 4u;
            pixel[0] = blue;
            pixel[1] = green;
            pixel[2] = red;
            pixel[3] = 0xff;
        }
    }
}

static int overlaps(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void paint(uint8_t *pixels, int originX, int originY, int playerX, int playerY, int coinA, int coinB) {
    fill(pixels, 0, 0, SquareWidth, SquareHeight, 0x20, 0x18, 0x10);
    if (coinA) fill(pixels, 16, 150, CoinSize, CoinSize, 0x30, 0xd0, 0xf0);
    if (coinB) fill(pixels, 260, 16, CoinSize, CoinSize, 0x30, 0xd0, 0xf0);
    fill(pixels, originX, originY, SquareSize, SquareSize, 0x40, 0xe0, 0x40);
    fill(pixels, playerX, playerY, PlayerSize, PlayerSize, 0xf0, 0x60, 0x20);
}

static void tone(int16_t *samples) {
    int index;
    for (index = 0; index < 256; ++index) {
        int16_t sample = (index & 16) != 0 ? 4000 : -4000;
        samples[index * 2] = sample;
        samples[index * 2 + 1] = sample;
    }
}

void guest_start(void *stack, void *unused) {
    uint8_t *pixels = square_pixels;
    struct SquareBuffers buffers;
    unsigned char attribute[128];
    unsigned char padData[256];
    int16_t samples[512];
    int frames;
    int video;
    int pad;
    int audio;
    int x = 40;
    int y = 30;
    int dx = 3;
    int dy = 2;
    int playerX = PlayerX;
    int playerY = PlayerY;
    int coinA = 1;
    int coinB = 1;
    int frame;
    int index;
    (void)stack;
    (void)unused;
    frames = square_frames(getenv("APS5_SQUARE_FRAMES"));
    int interactive = square_enabled(getenv("APS5_SQUARE_INTERACTIVE"));
    int64_t started = -1;
    int cap = interactive ? 7200 : frames;
    buffers.data = pixels;
    buffers.metadata = 0;
    buffers.reserved0 = 0;
    buffers.reserved1 = 0;
    for (index = 0; index < 128; ++index) attribute[index] = 0;
    for (index = 0; index < 256; ++index) padData[index] = 0;
    video = sceVideoOutOpen(255, 0, 0, 0);
    if (video < 0) {
        puts("square video open failed");
        exit(1);
    }
    sceVideoOutSetBufferAttribute2(attribute, 0x8000000000000000ull, 1, SquareWidth, SquareHeight, 0, 0, 0);
    if (sceVideoOutRegisterBuffers2(video, 0, 0, &buffers, 1, attribute, 0, 0) != 0) {
        puts("square register failed");
        exit(1);
    }
    pad = scePadInit() == 0 ? scePadOpen(255, 0, 0, 0) : -1;
    audio = sceAudioOutOpen(255, 0, 0, 256, 48000, 1);
    int quit = 0;
    for (frame = 0; frame < cap && !quit; ++frame) {
        if (interactive && pad >= 0 && scePadRead(pad, padData, 1) > 0) {
            int stickX = padData[4];
            int stickY = padData[5];
            uint32_t buttons = (uint32_t)padData[0] | ((uint32_t)padData[1] << 8) | ((uint32_t)padData[2] << 16) | ((uint32_t)padData[3] << 24);
            if (stickX < 100) dx = -3;
            else if (stickX > 156) dx = 3;
            if (stickY < 100) dy = -2;
            else if (stickY > 156) dy = 2;
            if (stickX < 100) playerX -= 3;
            else if (stickX > 156) playerX += 3;
            if (stickY < 100) playerY -= 2;
            else if (stickY > 156) playerY += 2;
            if (playerX < 0) playerX = 0;
            if (playerY < 0) playerY = 0;
            if (playerX > SquareWidth - PlayerSize) playerX = SquareWidth - PlayerSize;
            if (playerY > SquareHeight - PlayerSize) playerY = SquareHeight - PlayerSize;
            if ((buttons & 0x4000u) != 0) {
                coinA = 0;
                coinB = 0;
            }
            if ((buttons & 0x0008u) != 0) quit = 1;
        }
        if (coinA && overlaps(x, y, SquareSize, SquareSize, 16, 150, CoinSize, CoinSize)) coinA = 0;
        if (coinB && overlaps(x, y, SquareSize, SquareSize, 260, 16, CoinSize, CoinSize)) coinB = 0;
        paint(pixels, x, y, playerX, playerY, coinA, coinB);
        if (sceVideoOutSubmitFlip(video, 0, 1, frame) != 0) {
            puts("square flip failed");
            exit(1);
        }
        sceVideoOutWaitVblank(video);
        if (audio > 0 && (frame % 15) == 0) {
            tone(samples);
            sceAudioOutOutput(audio, samples);
        }
        x += dx;
        y += dy;
        if (x < 0 || x > SquareWidth - SquareSize) {
            dx = -dx;
            x += dx;
        }
        if (y < 0 || y > SquareHeight - SquareSize) {
            dy = -dy;
            y += dy;
        }
        if (interactive && started < 0) started = time(0);
        if (interactive && started >= 0) {
            int64_t now = time(0);
            if (now >= started && now - started >= 30) quit = 1;
        }
        if (interactive && started < 0 && frame + 1 >= 1800) quit = 1;
    }
    puts("square presented");
    exit(0);
}
