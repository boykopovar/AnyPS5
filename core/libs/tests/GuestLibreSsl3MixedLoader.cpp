#include <dlfcn.h>
#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) return 2;
    const auto directory = std::filesystem::canonical(argv[1]);
    const std::string order = argv[2];
    if (order != "l3-first" && order != "legacy-first") return 2;
    const auto first = directory / (order == "legacy-first" ? "libSceLibreSsl.prx" : "libSceLibreSsl3.prx");
    if (dlopen(first.c_str(), RTLD_NOW | RTLD_LOCAL) == nullptr) {
        std::fprintf(stderr, "%s\n", dlerror());
        return 3;
    }
    const auto path = directory / "guest_libressl3_mixed_evp_consumer.prx";
    auto* consumer = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (consumer == nullptr) {
        std::fprintf(stderr, "%s\n", dlerror());
        return 4;
    }
    auto* entry = dlsym(consumer, "LibreSsl3MixedEvp_nid_no_patch");
    Dl_info info{};
    if (entry == nullptr || dladdr(entry, &info) == 0 || std::filesystem::canonical(info.dli_fname) != path) return 5;
    return reinterpret_cast<int (*)(const char*, const char*)>(entry)(directory.c_str(), argc == 4 ? argv[3] : nullptr);
}
