
extern "C" {

void host_print_long(long value);
long host_add(long a, long b);
double host_scale(double value, long factor);
long host_apply(long (*function)(long), long value);
long host_save(unsigned long *buffer);
void host_restore(unsigned long *buffer, long value);
long guest_setjmp(unsigned long *buffer);
void guest_longjmp(unsigned long *buffer, long value);
long tls_initial_bump();

thread_local long tls_general = 5;

long classify(int value) {
    switch (value) {
    case 0: return 3;
    case 1: return 14;
    case 2: return 15;
    case 3: return 92;
    case 4: return 65;
    case 5: return 35;
    case 6: return 89;
    case 7: return 79;
    default: return -1;
    }
}

}

namespace {

long initialized;

struct Initializer {
    Initializer() { initialized = host_add(600, 400); }
};

Initializer initializer;

long twice(long value) { return value * 2; }
long thrice(long value) { return value * 3; }

struct Shape {
    long (*area)(const Shape *);
    long width;
    long height;
};

long rectangle(const Shape *shape) { return shape->width * shape->height; }
long triangle(const Shape *shape) { return shape->width * shape->height / 2; }

unsigned long jump_buffer[8];
unsigned long guest_buffer[8];

}

extern "C" {

long total_area(const Shape *shapes, int count) {
    long sum = 0;
    for (int index = 0; index < count; index++) sum += shapes[index].area(&shapes[index]);
    return sum;
}

void descend(int depth) {
    if (depth == 0) host_restore(jump_buffer, 77);
    descend(depth - 1);
    host_print_long(-1);
}

void climb(int depth) {
    if (depth == 0) guest_longjmp(guest_buffer, 33);
    climb(depth - 1);
    host_print_long(-3);
}

long run_switch() {
    long sum = 0;
    for (int value = -1; value < 9; value++) sum = sum * 3 + classify(value);
    return sum;
}

long run_imports(long value) {
    host_print_long(value);
    return host_add(value, 5) + static_cast<long>(host_scale(1.5, value));
}

long run_callback(long value) {
    long (*functions[2])(long) = {twice, thrice};
    return host_apply(functions[value & 1], value);
}

long run_vtable() {
    Shape shapes[3];
    shapes[0].area = rectangle; shapes[0].width = 3; shapes[0].height = 4;
    shapes[1].area = triangle; shapes[1].width = 6; shapes[1].height = 5;
    shapes[2].area = rectangle; shapes[2].width = 2; shapes[2].height = 2;
    return total_area(shapes, 3);
}

long run_tls() {
    tls_general += 1;
    return tls_general * 100 + tls_initial_bump();
}

long run_initialized() {
    return initialized;
}

long run_nonlocal() {
    long result = host_save(jump_buffer);
    if (result == 0) {
        descend(5);
        return -2;
    }
    return result;
}

long run_guest_longjmp() {
    volatile long rounds = 0;
    long result = guest_setjmp(guest_buffer);
    rounds = rounds + 1;
    if (result == 0) {
        climb(4);
        return -4;
    }
    return result * 10 + rounds;
}

}
