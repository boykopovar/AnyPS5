#include "prx/libc/include/Api.hpp"
#include <nid/NidCompute.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class Module {
#ifdef _WIN32
    HMODULE handle;
#else
    void* handle;
#endif
public:
    explicit Module(const std::filesystem::path& path) {
#ifdef _WIN32
        handle = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!handle) std::fprintf(stderr, "LoadLibraryExW failed: %lu\n", GetLastError());
#else
        handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) std::fprintf(stderr, "%s\n", dlerror());
#endif
        Require(handle != nullptr);
    }
    ~Module() {
#ifdef _WIN32
        Require(FreeLibrary(handle) != 0);
#else
        Require(dlclose(handle) == 0);
#endif
    }
    template<typename TFunction> TFunction Resolve(const std::string& name) {
#ifdef _WIN32
        const auto symbol = GetProcAddress(handle, name.c_str());
#else
        const auto symbol = dlsym(handle, name.c_str());
#endif
        Require(symbol != nullptr);
        static_assert(sizeof(TFunction) == sizeof(symbol));
        TFunction function;
        std::memcpy(&function, &symbol, sizeof(function));
        return function;
    }
};

struct Answer { long long numerator, denominator, quotient, remainder; };
static constexpr Answer small[] = {
        {0LL, 1LL, 0LL, 0LL},
        {0LL, 2LL, 0LL, 0LL},
        {0LL, -3LL, 0LL, 0LL},
        {0LL, 17LL, 0LL, 0LL},
        {1LL, 1LL, 1LL, 0LL},
        {1LL, 2LL, 0LL, 1LL},
        {1LL, -3LL, 0LL, 1LL},
        {1LL, 17LL, 0LL, 1LL},
        {-1LL, 1LL, -1LL, 0LL},
        {-1LL, 2LL, 0LL, -1LL},
        {-1LL, -3LL, 0LL, -1LL},
        {-1LL, 17LL, 0LL, -1LL},
        {7LL, 1LL, 7LL, 0LL},
        {7LL, 2LL, 3LL, 1LL},
        {7LL, -3LL, -2LL, 1LL},
        {7LL, 17LL, 0LL, 7LL},
        {-7LL, 1LL, -7LL, 0LL},
        {-7LL, 2LL, -3LL, -1LL},
        {-7LL, -3LL, 2LL, -1LL},
        {-7LL, 17LL, 0LL, -7LL},
        {2147483647LL, 1LL, 2147483647LL, 0LL},
        {2147483647LL, 2LL, 1073741823LL, 1LL},
        {2147483647LL, -3LL, -715827882LL, 1LL},
        {2147483647LL, 17LL, 126322567LL, 8LL},
        {-2147483648LL, 1LL, -2147483648LL, 0LL},
        {-2147483648LL, 2LL, -1073741824LL, 0LL},
        {-2147483648LL, -3LL, 715827882LL, -2LL},
        {-2147483648LL, 17LL, -126322567LL, -9LL}
};
static constexpr Answer wide[] = {
        {0LL, 1LL, 0LL, 0LL},
        {0LL, 2LL, 0LL, 0LL},
        {0LL, -3LL, 0LL, 0LL},
        {0LL, 17LL, 0LL, 0LL},
        {1LL, 1LL, 1LL, 0LL},
        {1LL, 2LL, 0LL, 1LL},
        {1LL, -3LL, 0LL, 1LL},
        {1LL, 17LL, 0LL, 1LL},
        {-1LL, 1LL, -1LL, 0LL},
        {-1LL, 2LL, 0LL, -1LL},
        {-1LL, -3LL, 0LL, -1LL},
        {-1LL, 17LL, 0LL, -1LL},
        {7LL, 1LL, 7LL, 0LL},
        {7LL, 2LL, 3LL, 1LL},
        {7LL, -3LL, -2LL, 1LL},
        {7LL, 17LL, 0LL, 7LL},
        {-7LL, 1LL, -7LL, 0LL},
        {-7LL, 2LL, -3LL, -1LL},
        {-7LL, -3LL, 2LL, -1LL},
        {-7LL, 17LL, 0LL, -7LL},
        {2147483647LL, 1LL, 2147483647LL, 0LL},
        {2147483647LL, 2LL, 1073741823LL, 1LL},
        {2147483647LL, -3LL, -715827882LL, 1LL},
        {2147483647LL, 17LL, 126322567LL, 8LL},
        {-2147483648LL, 1LL, -2147483648LL, 0LL},
        {-2147483648LL, 2LL, -1073741824LL, 0LL},
        {-2147483648LL, -3LL, 715827882LL, -2LL},
        {-2147483648LL, 17LL, -126322567LL, -9LL},
        {1099511627809LL, 2LL, 549755813904LL, 1LL},
        {1099511627809LL, -7LL, -157073089687LL, 0LL},
        {1099511627809LL, 4294967297LL, 255LL, 4294967074LL},
        {-1099511627809LL, 2LL, -549755813904LL, -1LL},
        {-1099511627809LL, -7LL, 157073089687LL, 0LL},
        {-1099511627809LL, 4294967297LL, -255LL, -4294967074LL},
        {9223372036854775807LL, 2LL, 4611686018427387903LL, 1LL},
        {9223372036854775807LL, -7LL, -1317624576693539401LL, 0LL},
        {9223372036854775807LL, 4294967297LL, 2147483647LL, 2147483648LL},
        {-9223372036854775807LL, 2LL, -4611686018427387903LL, -1LL},
        {-9223372036854775807LL, -7LL, 1317624576693539401LL, 0LL},
        {-9223372036854775807LL, 4294967297LL, -2147483647LL, -2147483648LL}
};

template<typename TFunction, std::size_t TSize>
static void Check(TFunction function, const Answer (&answers)[TSize]) {
    for (const auto& answer : answers) {
        using Result = decltype(function(answer.numerator, answer.denominator));
        struct GuardedResult { std::uint64_t before; Result value; std::uint64_t after; };
        GuardedResult result{0x123456789abcdef0ull, {}, 0xfedcba9876543210ull};
        result.value = function(answer.numerator, answer.denominator);
        Require(result.value.quot == answer.quotient && result.value.rem == answer.remainder);
        Require(result.before == 0x123456789abcdef0ull && result.after == 0xfedcba9876543210ull);
    }
}

int main(int argc, char** argv) {
    Require(argc == 3);
    for (int index = 1; index <= 2; ++index) {
        Module module(std::filesystem::absolute(argv[index]));
        const auto name = [&](const char* symbol) { return index == 1 ? std::string(symbol) + "_nid_postfix" : Nid::ComputeNid(symbol, "libc"); };
        Check(module.Resolve<decltype(&div_nid_postfix)>(name("div")), small);
        Check(module.Resolve<decltype(&ldiv_nid_postfix)>(name("ldiv")), wide);
        Check(module.Resolve<decltype(&lldiv_nid_postfix)>(name("lldiv")), wide);
        Check(module.Resolve<decltype(&imaxdiv_nid_postfix)>(name("imaxdiv")), wide);
    }
}
