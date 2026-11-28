// Minimal real x86-64 compiler fixture. Do not hand-edit the machine-code bytes
// in GuestCpuTests.cpp; regenerate them from this source when changing the fixture.
// Build: clang -target x86_64-unknown-linux-gnu -O1 -fno-stack-protector
//        -fno-pic -c compiler_fixture.c -o compiler_fixture.o
// Inspect: objdump -d compiler_fixture.o
// Observed .text bytes (Clang x86-64): 8d 47 02 c3
__attribute__((noinline))
int guest_add_two(int value) {
    return value + 2;
}

// M16 compiler-generated integration fixtures. Clang 17 x86-64 -O1
// (same build command above). The test byte arrays contain these functions'
// instruction streams with a five-byte "mov edi, immediate" setup prefix.
// guest_sum_to_n (first 26 bytes):
// 85 ff 7e 13 8d 47 ff 8d 4f fe 48 0f af c8 48 d1 e9
// 8d 04 79 ff c8 c3 31 c0 c3
// guest_bitmix (17 bytes):
// 8d 04 bd 00 00 00 00 c1 ef 03 09 c7 40 0f b6 c7 c3
__attribute__((noinline))
int guest_sum_to_n(int n) {
    int sum = 0;
    for (int i = 1; i <= n; ++i) sum += i;
    return sum;
}

__attribute__((noinline))
unsigned guest_bitmix(unsigned x) {
    return ((x << 2) | (x >> 3)) & 255u;
}
