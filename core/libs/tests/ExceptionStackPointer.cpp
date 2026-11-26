#include <cstdio>

extern "C" int ExceptionStackPointerFrame(void (*)(int, int, int, int, int, int, int, int, int, int));

[[gnu::noinline]] void ThrowWithStackArguments(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    if (a + b + c + d + e + f + g + h + i + j == 0) return;
    throw 42;
}

int main() {
    const int result = ExceptionStackPointerFrame(ThrowWithStackArguments);
    if (result != 1) {
        std::printf("exception stack pointer tests failed: the handler ran with the wrong stack pointer (%d)\n", result);
        return 1;
    }
    std::puts("exception stack pointer tests passed");
    return 0;
}
