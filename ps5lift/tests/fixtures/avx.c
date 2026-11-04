#include "host.h"

typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned char u8;
typedef long long v2 [[gnu::vector_size(16)]];
typedef long long v4 [[gnu::vector_size(32)]];

void* memcpy(void* destination, const void* source, u64 length) {
    u8* to = destination;
    const u8* from = source;
    while (length--) *to++ = *from++;
    return destination;
}

void* memset(void* destination, int value, u64 length) {
    u8* to = destination;
    while (length--) *to++ = (u8)value;
    return destination;
}

static u64 length_of(const char* text) {
    u64 length = 0;
    while (text[length]) ++length;
    return length;
}

static void print(const char* text) {
    host_write(text, length_of(text));
}

static void print_hex(u64 value) {
    static const char digits[] = "0123456789abcdef";
    char buffer[17];
    for (int i = 0; i < 16; ++i) buffer[i] = digits[(value >> (60 - 4 * i)) & 0xf];
    buffer[16] = 0;
    print(buffer);
}

static void line(const char* name, u64 value) {
    print(name);
    print(" ");
    print_hex(value);
    print("\n");
}

static u64 seed = 0x853c49e6748fea9bul;

static u64 next(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}

static void mix(u64* hash, const void* data, u64 size) {
    const u8* bytes = data;
    for (u64 i = 0; i < size; ++i) *hash = (*hash ^ bytes[i]) * 0x100000001b3ul;
}

static void fill(u8* buffer, u64 size) {
    for (u64 i = 0; i < size; i += 8) {
        const u64 value = next();
        memcpy(buffer + i, &value, 8);
    }
}

static v2 random_vector128(void) {
    v2 value;
    for (int i = 0; i < 2; ++i) value[i] = (long long)next();
    return value;
}

static v4 random_vector256(void) {
    v4 value;
    for (int i = 0; i < 4; ++i) value[i] = (long long)next();
    return value;
}

static v2 load_ps128(const void* address, v2 mask) {
    v2 result;
    __asm__ volatile("vmaskmovps (%[p]), %[m], %[r]" : [r] "=x"(result) : [p] "r"(address), [m] "x"(mask) : "memory");
    return result;
}

static void store_ps128(void* address, v2 mask, v2 value) {
    __asm__ volatile("vmaskmovps %[v], %[m], (%[p])" : : [p] "r"(address), [m] "x"(mask), [v] "x"(value) : "memory");
}

static v4 load_ps256(const void* address, v4 mask) {
    v4 result;
    __asm__ volatile("vmaskmovps (%[p]), %[m], %[r]" : [r] "=x"(result) : [p] "r"(address), [m] "x"(mask) : "memory");
    return result;
}

static void store_ps256(void* address, v4 mask, v4 value) {
    __asm__ volatile("vmaskmovps %[v], %[m], (%[p])" : : [p] "r"(address), [m] "x"(mask), [v] "x"(value) : "memory");
}

static v2 load_pd128(const void* address, v2 mask) {
    v2 result;
    __asm__ volatile("vmaskmovpd (%[p]), %[m], %[r]" : [r] "=x"(result) : [p] "r"(address), [m] "x"(mask) : "memory");
    return result;
}

static void store_pd128(void* address, v2 mask, v2 value) {
    __asm__ volatile("vmaskmovpd %[v], %[m], (%[p])" : : [p] "r"(address), [m] "x"(mask), [v] "x"(value) : "memory");
}

static v4 load_pd256(const void* address, v4 mask) {
    v4 result;
    __asm__ volatile("vmaskmovpd (%[p]), %[m], %[r]" : [r] "=x"(result) : [p] "r"(address), [m] "x"(mask) : "memory");
    return result;
}

static void store_pd256(void* address, v4 mask, v4 value) {
    __asm__ volatile("vmaskmovpd %[v], %[m], (%[p])" : : [p] "r"(address), [m] "x"(mask), [v] "x"(value) : "memory");
}

static v2 load_d128(const void* address, v2 mask) {
    v2 result;
    __asm__ volatile("vpmaskmovd (%[p]), %[m], %[r]" : [r] "=x"(result) : [p] "r"(address), [m] "x"(mask) : "memory");
    return result;
}

static void store_d128(void* address, v2 mask, v2 value) {
    __asm__ volatile("vpmaskmovd %[v], %[m], (%[p])" : : [p] "r"(address), [m] "x"(mask), [v] "x"(value) : "memory");
}

static v4 load_d256(const void* address, v4 mask) {
    v4 result;
    __asm__ volatile("vpmaskmovd (%[p]), %[m], %[r]" : [r] "=x"(result) : [p] "r"(address), [m] "x"(mask) : "memory");
    return result;
}

static void store_d256(void* address, v4 mask, v4 value) {
    __asm__ volatile("vpmaskmovd %[v], %[m], (%[p])" : : [p] "r"(address), [m] "x"(mask), [v] "x"(value) : "memory");
}

static v2 load_q128(const void* address, v2 mask) {
    v2 result;
    __asm__ volatile("vpmaskmovq (%[p]), %[m], %[r]" : [r] "=x"(result) : [p] "r"(address), [m] "x"(mask) : "memory");
    return result;
}

static void store_q128(void* address, v2 mask, v2 value) {
    __asm__ volatile("vpmaskmovq %[v], %[m], (%[p])" : : [p] "r"(address), [m] "x"(mask), [v] "x"(value) : "memory");
}

static v4 load_q256(const void* address, v4 mask) {
    v4 result;
    __asm__ volatile("vpmaskmovq (%[p]), %[m], %[r]" : [r] "=x"(result) : [p] "r"(address), [m] "x"(mask) : "memory");
    return result;
}

static void store_q256(void* address, v4 mask, v4 value) {
    __asm__ volatile("vpmaskmovq %[v], %[m], (%[p])" : : [p] "r"(address), [m] "x"(mask), [v] "x"(value) : "memory");
}

static v2 permilps128(v2 value, v2 selectors) {
    v2 result;
    __asm__("vpermilps %[s], %[v], %[r]" : [r] "=x"(result) : [v] "x"(value), [s] "x"(selectors));
    return result;
}

static v4 permilps256(v4 value, v4 selectors) {
    v4 result;
    __asm__("vpermilps %[s], %[v], %[r]" : [r] "=x"(result) : [v] "x"(value), [s] "x"(selectors));
    return result;
}

static v2 permilpd128(v2 value, v2 selectors) {
    v2 result;
    __asm__("vpermilpd %[s], %[v], %[r]" : [r] "=x"(result) : [v] "x"(value), [s] "x"(selectors));
    return result;
}

static v4 permilpd256(v4 value, v4 selectors) {
    v4 result;
    __asm__("vpermilpd %[s], %[v], %[r]" : [r] "=x"(result) : [v] "x"(value), [s] "x"(selectors));
    return result;
}

static v2 minimum_position(v2 value) {
    v2 result;
    __asm__("phminposuw %[v], %[r]" : [r] "=x"(result) : [v] "x"(value));
    return result;
}

static v2 minimum_position_vex(v2 value) {
    v2 result;
    __asm__("vphminposuw %[v], %[r]" : [r] "=x"(result) : [v] "x"(value));
    return result;
}

#define BROADCAST(name, instruction, type)                                                 \
    static type name(v2 value) {                                                          \
        type result;                                                                      \
        __asm__(instruction " %[v], %[r]" : [r] "=x"(result) : [v] "x"(value)); \
        return result;                                                                    \
    }

BROADCAST(broadcast_ss128, "vbroadcastss", v2)
BROADCAST(broadcast_ss256, "vbroadcastss", v4)
BROADCAST(broadcast_sd256, "vbroadcastsd", v4)
BROADCAST(broadcast_b128, "vpbroadcastb", v2)
BROADCAST(broadcast_w256, "vpbroadcastw", v4)
BROADCAST(broadcast_d128, "vpbroadcastd", v2)
BROADCAST(broadcast_q256, "vpbroadcastq", v4)

static v2 small_words(void) {
    v2 value = random_vector128();
    for (int i = 0; i < 2; ++i) value[i] &= 0x0007000700070007ll;
    return value;
}

enum { Rounds = 4096, Kinds = 8, PermuteKinds = 6, BroadcastKinds = 7 };

int main(void) {
    static const char* const names[Kinds] = {"vmaskmovps 128", "vmaskmovps 256", "vmaskmovpd 128", "vmaskmovpd 256", "vpmaskmovd 128", "vpmaskmovd 256", "vpmaskmovq 128", "vpmaskmovq 256"};
    static u8 buffer[64];
    u64 hashes[Kinds];
    for (int i = 0; i < Kinds; ++i) hashes[i] = 0xcbf29ce484222325ul;
    for (int round = 0; round < Rounds; ++round) {
        {
            const v2 mask = random_vector128(), value = random_vector128();
            const unsigned offset = (unsigned)(next() % 32);
            fill(buffer, sizeof(buffer));
            const v2 loaded = load_ps128(buffer + offset, mask);
            mix(&hashes[0], &loaded, sizeof(loaded));
            store_ps128(buffer + offset, mask, value);
            mix(&hashes[0], buffer, sizeof(buffer));
        }
        {
            const v4 mask = random_vector256(), value = random_vector256();
            const unsigned offset = (unsigned)(next() % 32);
            fill(buffer, sizeof(buffer));
            const v4 loaded = load_ps256(buffer + offset, mask);
            mix(&hashes[1], &loaded, sizeof(loaded));
            store_ps256(buffer + offset, mask, value);
            mix(&hashes[1], buffer, sizeof(buffer));
        }
        {
            const v2 mask = random_vector128(), value = random_vector128();
            const unsigned offset = (unsigned)(next() % 32);
            fill(buffer, sizeof(buffer));
            const v2 loaded = load_pd128(buffer + offset, mask);
            mix(&hashes[2], &loaded, sizeof(loaded));
            store_pd128(buffer + offset, mask, value);
            mix(&hashes[2], buffer, sizeof(buffer));
        }
        {
            const v4 mask = random_vector256(), value = random_vector256();
            const unsigned offset = (unsigned)(next() % 32);
            fill(buffer, sizeof(buffer));
            const v4 loaded = load_pd256(buffer + offset, mask);
            mix(&hashes[3], &loaded, sizeof(loaded));
            store_pd256(buffer + offset, mask, value);
            mix(&hashes[3], buffer, sizeof(buffer));
        }
        {
            const v2 mask = random_vector128(), value = random_vector128();
            const unsigned offset = (unsigned)(next() % 32);
            fill(buffer, sizeof(buffer));
            const v2 loaded = load_d128(buffer + offset, mask);
            mix(&hashes[4], &loaded, sizeof(loaded));
            store_d128(buffer + offset, mask, value);
            mix(&hashes[4], buffer, sizeof(buffer));
        }
        {
            const v4 mask = random_vector256(), value = random_vector256();
            const unsigned offset = (unsigned)(next() % 32);
            fill(buffer, sizeof(buffer));
            const v4 loaded = load_d256(buffer + offset, mask);
            mix(&hashes[5], &loaded, sizeof(loaded));
            store_d256(buffer + offset, mask, value);
            mix(&hashes[5], buffer, sizeof(buffer));
        }
        {
            const v2 mask = random_vector128(), value = random_vector128();
            const unsigned offset = (unsigned)(next() % 32);
            fill(buffer, sizeof(buffer));
            const v2 loaded = load_q128(buffer + offset, mask);
            mix(&hashes[6], &loaded, sizeof(loaded));
            store_q128(buffer + offset, mask, value);
            mix(&hashes[6], buffer, sizeof(buffer));
        }
        {
            const v4 mask = random_vector256(), value = random_vector256();
            const unsigned offset = (unsigned)(next() % 32);
            fill(buffer, sizeof(buffer));
            const v4 loaded = load_q256(buffer + offset, mask);
            mix(&hashes[7], &loaded, sizeof(loaded));
            store_q256(buffer + offset, mask, value);
            mix(&hashes[7], buffer, sizeof(buffer));
        }
    }
    for (int i = 0; i < Kinds; ++i) line(names[i], hashes[i]);
    static const char* const permute_names[PermuteKinds] = {"vpermilps 128", "vpermilps 256", "vpermilpd 128", "vpermilpd 256", "phminposuw", "vphminposuw"};
    u64 permutes[PermuteKinds];
    for (int i = 0; i < PermuteKinds; ++i) permutes[i] = 0xcbf29ce484222325ul;
    for (int round = 0; round < Rounds; ++round) {
        const v2 narrow = random_vector128(), narrow_selectors = random_vector128();
        const v4 wide = random_vector256(), wide_selectors = random_vector256();
        const v2 words = (round & 1) ? small_words() : random_vector128();
        const v2 r0 = permilps128(narrow, narrow_selectors), r2 = permilpd128(narrow, narrow_selectors);
        const v4 r1 = permilps256(wide, wide_selectors), r3 = permilpd256(wide, wide_selectors);
        const v2 r4 = minimum_position(words), r5 = minimum_position_vex(words);
        mix(&permutes[0], &r0, sizeof(r0));
        mix(&permutes[1], &r1, sizeof(r1));
        mix(&permutes[2], &r2, sizeof(r2));
        mix(&permutes[3], &r3, sizeof(r3));
        mix(&permutes[4], &r4, sizeof(r4));
        mix(&permutes[5], &r5, sizeof(r5));
    }
    for (int i = 0; i < PermuteKinds; ++i) line(permute_names[i], permutes[i]);

    static const char* const broadcast_names[BroadcastKinds] = {"vbroadcastss register 128", "vbroadcastss register 256", "vbroadcastsd register 256",
                                                                "vpbroadcastb register 128", "vpbroadcastw register 256", "vpbroadcastd register 128",
                                                                "vpbroadcastq register 256"};
    u64 broadcasts[BroadcastKinds];
    for (int i = 0; i < BroadcastKinds; ++i) broadcasts[i] = 0xcbf29ce484222325ul;
    for (int round = 0; round < 64; ++round) {
        const v2 source = random_vector128();
        const v2 r0 = broadcast_ss128(source), r3 = broadcast_b128(source), r5 = broadcast_d128(source);
        const v4 r1 = broadcast_ss256(source), r2 = broadcast_sd256(source), r4 = broadcast_w256(source), r6 = broadcast_q256(source);
        mix(&broadcasts[0], &r0, sizeof(r0));
        mix(&broadcasts[1], &r1, sizeof(r1));
        mix(&broadcasts[2], &r2, sizeof(r2));
        mix(&broadcasts[3], &r3, sizeof(r3));
        mix(&broadcasts[4], &r4, sizeof(r4));
        mix(&broadcasts[5], &r5, sizeof(r5));
        mix(&broadcasts[6], &r6, sizeof(r6));
    }
    for (int i = 0; i < BroadcastKinds; ++i) line(broadcast_names[i], broadcasts[i]);
    return 0;
}

__asm__(".globl _start\n"
        "_start:\n"
        "    xor %ebp, %ebp\n"
        "    and $-16, %rsp\n"
        "    call main\n"
        "    mov %eax, %edi\n"
        "    call host_exit\n"
        "    ud2\n");
