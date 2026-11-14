#include <dlfcn.h>

#include <cstdio>
#include <cstring>
#include <string>

struct SharedExport {
    const char* name;
    const char* nid;
};

static const SharedExport SharedExports[] = {
    {"RSA_new", "kADEz2eAE7c"},
    {"d2i_RSA_PUBKEY", "PFxpQ6lkQP0"},
    {"EVP_PKEY_get1_RSA", "5RBpaKVkWRQ"},
    {"PEM_read_RSAPrivateKey", "psMmvmdPKds"},
};

static bool EndsWith(const char* value, const char* suffix) {
    const std::size_t valueLength = std::strlen(value);
    const std::size_t suffixLength = std::strlen(suffix);
    return valueLength >= suffixLength && std::strcmp(value + valueLength - suffixLength, suffix) == 0;
}

static const char* DefiningFile(void* handle, const char* nid) {
    void* address = dlsym(handle, nid);
    if (address == nullptr) return nullptr;
    Dl_info info{};
    if (dladdr(address, &info) == 0 || info.dli_fname == nullptr) return nullptr;
    return info.dli_fname;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <patched libraries directory>\n", argv[0]);
        return 2;
    }
    const std::string directory = argv[1];
    void* libreSsl3 = dlopen((directory + "/libSceLibreSsl3.prx").c_str(), RTLD_NOW | RTLD_LOCAL);
    void* libreSsl = dlopen((directory + "/libSceLibreSsl.prx").c_str(), RTLD_NOW | RTLD_LOCAL);
    if (libreSsl3 == nullptr || libreSsl == nullptr) {
        std::fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    int failed = 0;
    for (const auto& entry : SharedExports) {
        const char* own = DefiningFile(libreSsl, entry.nid);
        if (own == nullptr || !EndsWith(own, "/libSceLibreSsl.prx")) {
            std::fprintf(stderr, "%s (%s) is not exported by libSceLibreSsl\n", entry.name, entry.nid);
            ++failed;
        }
        const char* bound = DefiningFile(libreSsl3, entry.nid);
        if (bound == nullptr || !EndsWith(bound, "/libSceLibreSsl3.prx")) {
            std::fprintf(stderr, "%s (%s) imported from libSceLibreSsl3 binds to %s\n", entry.name, entry.nid, bound ? bound : "nothing");
            ++failed;
        }
    }
    return failed == 0 ? 0 : 1;
}
