#include <stdexcept>
#include <cstdio>
#include <cstring>

extern "C" void NotImplemented_nid_no_patch(const char*);

static int destroyed;
struct Guard { ~Guard() { ++destroyed; } };
__attribute__((noinline)) static void ThrowNested() {
    Guard guard;
    throw std::runtime_error("native own unwind");
}
__attribute__((noinline)) static void Rethrow() {
    Guard guard;
    try { ThrowNested(); }
    catch (const std::runtime_error&) { throw; }
}
int main() {
    try {
        Rethrow();
        return 1;
    } catch (const std::runtime_error& error) {
        if (std::strcmp(error.what(), "native own unwind") || destroyed != 2) return 2;
    }
    try { NotImplemented_nid_no_patch("cross DLL"); }
    catch (const std::runtime_error& error) {
        if (std::strcmp(error.what(), "cross DLL not implemented")) return 3;
        std::puts("Own Windows exceptions: catch, rethrow, destructors, cross DLL passed");
        return 0;
    }
    return 4;
}
