#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define REGION 0x400000000ull
#define CODE_BASE (REGION + 0x10000)
#define DATA_BASE (REGION + 0x40000)
#define DATA_SIZE 0x1000
#define STACK_BASE (REGION + 0x80000)
#define STACK_SIZE 0x1000
#define SENTINEL (REGION + 0xF000)

extern unsigned char oracle_page[0x1000];

static void *map_at(uint64_t address, size_t size) {
    void *result = mmap((void *)address, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (result != (void *)address) {
        fprintf(stderr, "oracle: cannot map %#llx\n", (unsigned long long)address);
        exit(2);
    }
    return result;
}

static void read_exact(FILE *file, void *buffer, size_t size) {
    if (fread(buffer, 1, size, file) != size) {
        fprintf(stderr, "oracle: truncated input\n");
        exit(2);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: oracle <input> <output>\n");
        return 2;
    }
    map_at(REGION, 0x100000);
    unsigned char *code = (unsigned char *)CODE_BASE;
    unsigned char *data = (unsigned char *)DATA_BASE;
    unsigned char *stack = (unsigned char *)STACK_BASE;
    unsigned char *page = (unsigned char *)SENTINEL;
    FILE *input = fopen(argv[1], "rb");
    FILE *output = fopen(argv[2], "wb");
    if (!input || !output) {
        perror("oracle");
        return 2;
    }
    uint32_t length;
    while (fread(&length, 1, 4, input) == 4) {
        memcpy(page, oracle_page, 0x1000);
        memset(code, 0xcc, 0x10000);
        read_exact(input, code, length);
        read_exact(input, page + 0x500, 0x80);
        read_exact(input, page + 0x440, 8);
        read_exact(input, page + 0x600, 0x200);
        read_exact(input, data, DATA_SIZE);
        read_exact(input, stack, STACK_SIZE);
        uint64_t code_address = CODE_BASE;
        memcpy(page + 0x438, &code_address, 8);
        ((void (*)(void))(page + 0x200))();
        fwrite(page + 0x500, 1, 0x80, output);
        fwrite(page + 0x448, 1, 8, output);
        fwrite(page + 0x600, 1, 0x200, output);
        fwrite(data, 1, DATA_SIZE, output);
        fwrite(stack, 1, STACK_SIZE, output);
    }
    fclose(output);
    return 0;
}
