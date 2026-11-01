extern "C" int printf(const char* format, ...);
extern "C" [[noreturn]] void exit(int code);

namespace {

struct Tracker {
    const char* name;
    explicit Tracker(const char* label) : name(label) {}
    ~Tracker() { printf("destroy %s\n", name); }
};

struct Base {
    int code;
    explicit Base(int value) : code(value) {}
    virtual ~Base() {}
};

struct Derived : Base {
    explicit Derived(int value) : Base(value) {}
};

struct Plain {
    long first;
    long second;
};

struct Other {};

[[gnu::noinline]] void thrower(int kind) {
    Tracker tracker("thrower");
    if (kind == 0) throw Derived(7);
    if (kind == 1) throw Plain{11, 13};
    if (kind == 2) throw Other{};
    printf("thrower returns\n");
}

[[gnu::noinline]] void middle(int kind) {
    Tracker tracker("middle");
    thrower(kind);
    printf("middle returns\n");
}

[[gnu::noinline]] int run(int kind) {
    try {
        middle(kind);
    } catch (const Base& error) {
        printf("caught base %d\n", error.code);
        return 1;
    } catch (Plain plain) {
        printf("caught plain %ld %ld\n", plain.first, plain.second);
        return 2;
    } catch (...) {
        printf("caught other\n");
        return 3;
    }
    return 0;
}

[[gnu::noinline]] int rethrow() {
    try {
        try {
            thrower(0);
        } catch (const Derived& error) {
            printf("inner %d\n", error.code);
            throw;
        }
    } catch (const Base& error) {
        printf("outer %d\n", error.code);
        return error.code;
    }
    return 0;
}

[[gnu::noinline]] int recurse(int depth) {
    Tracker tracker(depth % 2 ? "odd" : "even");
    if (depth == 0) throw Plain{depth, 99};
    return recurse(depth - 1) + 1;
}

}

int main() {
    for (int kind = 0; kind < 4; ++kind) printf("run %d -> %d\n", kind, run(kind));
    printf("rethrow -> %d\n", rethrow());
    try {
        recurse(4);
    } catch (const Plain& plain) {
        printf("recursion %ld %ld\n", plain.first, plain.second);
    }
    return 0;
}

extern "C" [[noreturn]] void start_main() {
    exit(main());
}

__asm__(".globl _start\n"
        "_start:\n"
        "    xor %ebp, %ebp\n"
        "    and $-16, %rsp\n"
        "    call start_main\n"
        "    ud2\n");
