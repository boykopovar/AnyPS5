#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "usage: mimalloc_restart_tests off|on|once");
    const std::string_view mode = argv[1];
    Require(std::getenv("APS5_MIMALLOC_RESTARTED") == nullptr, "the restart marker stayed in the environment");
    using InHeap = bool (*)(const void*);
    const auto inHeap = reinterpret_cast<InHeap>(dlsym(RTLD_DEFAULT, "mi_is_in_heap_region"));
    if (mode != "on") {
        Require(inHeap == nullptr, "mimalloc was loaded without APS5_MIMALLOC or after a restart");
        return 0;
    }
    Require(inHeap != nullptr, "APS5_MIMALLOC did not load mimalloc");
    void* block = std::malloc(64);
    const auto object = std::make_unique<int>(1);
    const auto array = std::make_unique<int[]>(64);
    Require(inHeap(block), "malloc did not allocate through mimalloc");
    Require(inHeap(object.get()) && inHeap(array.get()), "operator new did not allocate through mimalloc");
    std::free(block);
    return 0;
}
