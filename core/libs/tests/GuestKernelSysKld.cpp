#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int ErrorPermission = 1;
constexpr int ErrorNoSystemCall = 78;

struct FileStat {
    std::int32_t version;
    char name[1024];
    std::int32_t references;
    std::int32_t id;
    std::uint64_t address;
    std::uint64_t size;
    char path[1024];
};

struct SymbolLookup {
    std::int32_t version;
    const char* name;
    std::uint64_t value;
};

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

void RequireFailed(int result, int expected, const char* what) {
    Require(result == -1, what);
    Require(*__error_nid_postfix() == expected, what);
}

void RefusesTheFindAndTheWalk() {
    *__error_nid_postfix() = 71;
    RequireFailed(kldfind_nid_postfix(nullptr), ErrorNoSystemCall, "kldfind without a name");
    RequireFailed(kldfind_nid_postfix("/app0/sce_module/libkernel.prx"), ErrorNoSystemCall, "kldfind with a name");
    RequireFailed(kldfirstmod_nid_postfix(0), ErrorNoSystemCall, "kldfirstmod of the kernel");
    RequireFailed(kldfirstmod_nid_postfix(7), ErrorNoSystemCall, "kldfirstmod of a file id");
    RequireFailed(kldnext_nid_postfix(0), ErrorNoSystemCall, "kldnext from zero");
    RequireFailed(kldnext_nid_postfix(7), ErrorNoSystemCall, "kldnext from a file id");
}

void RefusesTheStatusAndTheSymbolLookup() {
    FileStat stat{};
    SymbolLookup symbol{};
    stat.version = static_cast<std::int32_t>(offsetof(FileStat, path));
    *__error_nid_postfix() = 71;
    RequireFailed(kldstat_nid_postfix(0, nullptr), ErrorNoSystemCall, "kldstat without a buffer");
    RequireFailed(kldstat_nid_postfix(0, &stat), ErrorNoSystemCall, "kldstat of the kernel");
    RequireFailed(kldstat_nid_postfix(-1, &stat), ErrorNoSystemCall, "kldstat of a negative file id");
    RequireFailed(kldsym_nid_postfix(0, 0, nullptr), ErrorNoSystemCall, "kldsym without a lookup");
    RequireFailed(kldsym_nid_postfix(0, 1, &symbol), ErrorNoSystemCall, "kldsym of the kernel");
}

void RefusesToLoadAndUnload() {
    *__error_nid_postfix() = 71;
    RequireFailed(kldload_nid_postfix("/app0/sce_module/libkernel.prx"), ErrorPermission, "kldload of a module path");
    RequireFailed(kldunload_nid_postfix(0), ErrorPermission, "kldunload of the kernel");
    RequireFailed(kldunload_nid_postfix(3), ErrorPermission, "kldunload of a file id");
}

}

int main() {
    RefusesTheFindAndTheWalk();
    RefusesTheStatusAndTheSymbolLookup();
    RefusesToLoadAndUnload();
    return 0;
}
