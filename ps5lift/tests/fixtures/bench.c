#include "host.h"

#ifndef BENCH_SCALE
#define BENCH_SCALE 16
#endif

typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned char u8;

void* memcpy(void* destination, const void* source, u64 length);
void* memset(void* destination, int value, u64 length);

#ifdef PS5LIFT_GUEST
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
#endif

enum {
    StreamCount = BENCH_SCALE << 20,
    ChaseCount = BENCH_SCALE << 16,
    RecordCount = BENCH_SCALE << 12,
    TableBits = 16,
    TableSize = 1 << TableBits,
    SortCount = BENCH_SCALE << 12,
    DispatchCount = 1 << 16,
    DispatchCalls = BENCH_SCALE << 16
};

struct Record {
    u64 key;
    u64 payload[7];
};

static u32 stream[StreamCount];
static u32 chase[ChaseCount];
static struct Record records[RecordCount];
static struct Record copies[RecordCount];
static u64 table_keys[TableSize];
static u64 table_values[TableSize];
static u32 sortable[SortCount];

static u64 rng_state = 0x9e3779b97f4a7c15ul;

static u64 next_random(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static u64 length_of(const char* text) {
    u64 length = 0;
    while (text[length]) ++length;
    return length;
}

static void print(const char* text) {
    host_write(text, length_of(text));
}

static void print_unsigned(u64 value) {
    char buffer[32];
    int position = 31;
    buffer[position] = 0;
    do {
        buffer[--position] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    print(buffer + position);
}

static void report(const char* name, u64 checksum, u64 nanoseconds) {
    print("result ");
    print(name);
    print(" ");
    print_unsigned(checksum);
    print("\ntime ");
    print(name);
    print(" ");
    print_unsigned(nanoseconds);
    print("\n");
}

[[gnu::noinline]] static u64 bench_stream(void) {
    for (u32 i = 0; i < StreamCount; ++i) stream[i] = i * 2654435761u;
    u64 sum = 0;
    for (int pass = 0; pass < 4; ++pass) {
        for (u32 i = 0; i < StreamCount; ++i) sum += stream[i] ^ (u32)pass;
    }
    return sum;
}

[[gnu::noinline]] static u64 bench_chase(void) {
    for (u32 i = 0; i < ChaseCount; ++i) chase[i] = i;
    for (u32 i = ChaseCount - 1; i > 0; --i) {
        u32 j = (u32)(next_random() % i);
        u32 swap = chase[i];
        chase[i] = chase[j];
        chase[j] = swap;
    }
    u32 position = 0;
    u64 sum = 0;
    for (u32 step = 0; step < ChaseCount * 4u; ++step) {
        position = chase[position];
        sum += position;
    }
    return sum;
}

[[gnu::noinline]] static u64 bench_copy(void) {
    for (u32 i = 0; i < RecordCount; ++i) {
        records[i].key = next_random();
        for (int j = 0; j < 7; ++j) records[i].payload[j] = records[i].key + (u64)j;
    }
    u64 sum = 0;
    for (int pass = 0; pass < 16; ++pass) {
        for (u32 i = 0; i < RecordCount; ++i) {
            u32 source = (i * 7919u + (u32)pass) % RecordCount;
            copies[i] = records[source];
            copies[i].payload[pass % 7] += (u64)pass;
        }
        for (u32 i = 0; i < RecordCount; i += 64) sum += copies[i].payload[pass % 7];
    }
    return sum;
}

[[gnu::noinline]] static u64 bench_hash(void) {
    memset(table_keys, 0, sizeof(table_keys));
    u64 inserted = 0;
    for (u32 i = 0; i < TableSize / 2; ++i) {
        u64 key = next_random() | 1;
        u64 slot = (key * 0x9e3779b97f4a7c15ul) >> (64 - TableBits);
        while (table_keys[slot] != 0 && table_keys[slot] != key) slot = (slot + 1) & (TableSize - 1);
        if (table_keys[slot] == 0) ++inserted;
        table_keys[slot] = key;
        table_values[slot] = i;
    }
    u64 sum = inserted;
    for (u32 round = 0; round < (u32)BENCH_SCALE * 8u; ++round) {
        for (u32 i = 0; i < TableSize; i += 3) {
            u64 key = table_keys[i] ? table_keys[i] : (u64)i * 2 + 1;
            u64 slot = (key * 0x9e3779b97f4a7c15ul) >> (64 - TableBits);
            while (table_keys[slot] != 0 && table_keys[slot] != key) slot = (slot + 1) & (TableSize - 1);
            sum += table_keys[slot] == key ? table_values[slot] : 1;
        }
    }
    return sum;
}

static void sift_down(u32* values, u32 start, u32 count) {
    u32 root = start;
    while (root * 2 + 1 < count) {
        u32 child = root * 2 + 1;
        if (child + 1 < count && values[child] < values[child + 1]) ++child;
        if (values[root] >= values[child]) return;
        u32 swap = values[root];
        values[root] = values[child];
        values[child] = swap;
        root = child;
    }
}

[[gnu::noinline]] static u64 bench_sort(void) {
    u64 sum = 0;
    for (int pass = 0; pass < 4; ++pass) {
        for (u32 i = 0; i < SortCount; ++i) sortable[i] = (u32)next_random();
        for (u32 start = SortCount / 2; start-- > 0;) sift_down(sortable, start, SortCount);
        for (u32 end = SortCount - 1; end > 0; --end) {
            u32 swap = sortable[0];
            sortable[0] = sortable[end];
            sortable[end] = swap;
            sift_down(sortable, 0, end);
        }
        for (u32 i = 0; i < SortCount; i += 97) sum = sum * 31 + sortable[i];
    }
    return sum;
}

#define HEX1(apply, p) apply(p##0) apply(p##1) apply(p##2) apply(p##3) apply(p##4) apply(p##5) apply(p##6) apply(p##7) apply(p##8) apply(p##9) apply(p##a) apply(p##b) apply(p##c) apply(p##d) apply(p##e) apply(p##f)
#define HEX2(apply, p) HEX1(apply, p##0) HEX1(apply, p##1) HEX1(apply, p##2) HEX1(apply, p##3) HEX1(apply, p##4) HEX1(apply, p##5) HEX1(apply, p##6) HEX1(apply, p##7) \
    HEX1(apply, p##8) HEX1(apply, p##9) HEX1(apply, p##a) HEX1(apply, p##b) HEX1(apply, p##c) HEX1(apply, p##d) HEX1(apply, p##e) HEX1(apply, p##f)
#define HEX3(apply, p) HEX2(apply, p##0) HEX2(apply, p##1) HEX2(apply, p##2) HEX2(apply, p##3) HEX2(apply, p##4) HEX2(apply, p##5) HEX2(apply, p##6) HEX2(apply, p##7) \
    HEX2(apply, p##8) HEX2(apply, p##9) HEX2(apply, p##a) HEX2(apply, p##b) HEX2(apply, p##c) HEX2(apply, p##d) HEX2(apply, p##e) HEX2(apply, p##f)
#define HEX4(apply) HEX3(apply, 0) HEX3(apply, 1) HEX3(apply, 2) HEX3(apply, 3) HEX3(apply, 4) HEX3(apply, 5) HEX3(apply, 6) HEX3(apply, 7) \
    HEX3(apply, 8) HEX3(apply, 9) HEX3(apply, a) HEX3(apply, b) HEX3(apply, c) HEX3(apply, d) HEX3(apply, e) HEX3(apply, f)
#define DISPATCH_TARGET(id) static u64 dispatch_##id(u64 value) { return value * 0x9e3779b97f4a7c15ul + 0x##id; }
#define DISPATCH_ENTRY(id) dispatch_##id,
HEX4(DISPATCH_TARGET)
static u64 (*const dispatch_targets[DispatchCount])(u64) = {HEX4(DISPATCH_ENTRY)};

[[gnu::noinline]] static u64 bench_dispatch(void) {
    u64 value = 1;
    u64 index = 0x2545f4914f6cdd1dul;
    for (u32 i = 0; i < DispatchCalls; ++i) {
        index = index * 6364136223846793005ul + 1442695040888963407ul;
        value = dispatch_targets[(index >> 48) & (DispatchCount - 1)](value);
    }
    return value;
}

static void run(const char* name, u64 (*kernel)(void)) {
    u64 begin = host_ticks();
    u64 checksum = kernel();
    report(name, checksum, host_ticks() - begin);
}

int main(void) {
    run("stream", bench_stream);
    run("chase", bench_chase);
    run("copy", bench_copy);
    run("hash", bench_hash);
    run("sort", bench_sort);
    run("dispatch", bench_dispatch);
    return 0;
}

#ifdef PS5LIFT_GUEST
__asm__(".globl _start\n"
        "_start:\n"
        "    xor %ebp, %ebp\n"
        "    and $-16, %rsp\n"
        "    call main\n"
        "    mov %eax, %edi\n"
        "    call host_exit\n"
        "    ud2\n");
#endif
