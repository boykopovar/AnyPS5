extern "C" void host_print_long(long value);

namespace {

struct Counter {
    long *count;
    ~Counter() { *count += 1; }
};

}  // namespace

void thrower(long value) {
    if (value > 0) throw value;
}

long middle(long value, long *cleanups) {
    Counter counter{cleanups};
    thrower(value);
    return 1;
}

extern "C" long run_exceptions(long value) {
    long cleanups = 0;
    try {
        middle(value, &cleanups);
    } catch (...) {
        return 100 + cleanups;
    }
    return 200 + cleanups;
}
