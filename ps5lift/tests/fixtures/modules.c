#include "host.h"

typedef long i64;

extern long module_counter;
long module_value(long x);
long (*module_function(void))(long);
long module_tls(void);
long* module_local_address(void);
long module_table_call(int index, long x);
long module_absolute_address(void);
[[gnu::returns_twice]] int module_setjmp(long* buffer) __asm__("gNQ1V2vfXDE#guest");
_Noreturn void module_longjmp(long* buffer, int value) __asm__("lKEN2IebgJ0#guest");

struct program_header {
    unsigned int type;
    unsigned int flags;
    unsigned long offset;
    unsigned long address;
    unsigned long physical_address;
    unsigned long file_size;
    unsigned long memory_size;
    unsigned long alignment;
};

struct dl_phdr_info {
    unsigned long dlpi_addr;
    const char* dlpi_name;
    const struct program_header* dlpi_phdr;
    unsigned short dlpi_phnum;
    unsigned long long dlpi_adds;
    unsigned long long dlpi_subs;
    unsigned long dlpi_tls_modid;
    void* dlpi_tls_data;
};

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info*, unsigned long, void*), void* data);

static struct image_search {
    long executables;
    long main_found;
    long module_found;
    long module_tls_found;
} search;

static __thread long executable_local = 100;

static unsigned long length_of(const char* text) {
    unsigned long length = 0;
    while (text[length]) ++length;
    return length;
}

static void print(const char* text) {
    host_write(text, length_of(text));
}

static void print_signed(i64 value) {
    char buffer[24];
    int position = 23;
    buffer[position] = 0;
    const int negative = value < 0;
    unsigned long magnitude = negative ? 0 - (unsigned long)value : (unsigned long)value;
    do {
        buffer[--position] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);
    if (negative) buffer[--position] = '-';
    print(buffer + position);
}

static void line(const char* name, i64 value) {
    print(name);
    print(" = ");
    print_signed(value);
    print("\n");
}

static int ends_with(const char* text, const char* suffix) {
    const unsigned long text_length = length_of(text);
    const unsigned long suffix_length = length_of(suffix);
    if (suffix_length > text_length) return 0;
    for (unsigned long index = 0; index < suffix_length; ++index) {
        if (text[text_length - suffix_length + index] != suffix[index]) return 0;
    }
    return 1;
}

static int contains(const struct dl_phdr_info* image, const void* address) {
    for (unsigned short index = 0; index < image->dlpi_phnum; ++index) {
        const struct program_header* header = &image->dlpi_phdr[index];
        if (header->type == 1 && (unsigned long)address - (image->dlpi_addr + header->address) < header->memory_size) return 1;
    }
    return 0;
}

int main(void);

static int visit_image(struct dl_phdr_info* image, unsigned long size, void* data) {
    struct image_search* search = data;
    if (size < sizeof(*image)) return -1;
    if (image->dlpi_name[0] == 0) {
        ++search->executables;
        search->main_found += contains(image, (const void*)main);
    } else if (ends_with(image->dlpi_name, "libguestmodule.so")) {
        search->module_found += contains(image, (const void*)module_value);
        search->module_tls_found += image->dlpi_tls_modid != 0 && image->dlpi_tls_data == module_local_address();
    }
    return 0;
}

static int stop_at_first(struct dl_phdr_info* image, unsigned long size, void* data) {
    (void)image;
    (void)size;
    (void)data;
    return 5;
}

int main(void) {
    line("module counter", module_counter);
    line("module value(5)", module_value(5));
    line("through a module pointer", module_function()(3));
    module_counter = 100;
    line("after writing its data", module_value(1));
    line("module table", module_table_call(0, 3) * 10000 + module_table_call(1, 3) * 100 + module_table_call(2, 0));
    line("module absolute symbol", module_absolute_address());
    line("module tls", module_tls());
    line("module tls again", module_tls());
    static long jump_buffer[16];
    static volatile int jumps;
    const int jumped = module_setjmp(jump_buffer);
    if (jumped < 3) {
        ++jumps;
        module_longjmp(jump_buffer, jumped + 1);
    }
    line("setjmp and longjmp a module exports", jumped * 10 + jumps);
    executable_local += module_tls();
    line("executable tls", executable_local);
    line("iteration result", dl_iterate_phdr(visit_image, &search));
    line("executable images", search.executables);
    line("main in the executable", search.main_found);
    line("module_value in its module", search.module_found);
    line("module tls block", search.module_tls_found);
    line("iteration stops with", dl_iterate_phdr(stop_at_first, 0));
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
