struct CleanupCounter { int* count; };

static void Leave(struct CleanupCounter* counter) { ++*counter->count; }

void CallWithCleanup(void (*function)(void), int* count) {
    struct CleanupCounter counter __attribute__((cleanup(Leave))) = {count};
    function();
}

void CallAroundCleanup(void (*before)(void), void (*within)(void), int* count) {
    before();
    struct CleanupCounter counter __attribute__((cleanup(Leave))) = {count};
    within();
}
