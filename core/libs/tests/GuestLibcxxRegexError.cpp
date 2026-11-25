#include "prx/libc/include/Api.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <typeinfo>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

struct Example {
    std::int32_t code;
    const char* text;
};

constexpr std::array Examples {
    Example{1, "invalid collating element"},
    Example{14, "invalid regex grammar"},
    Example{17, "did not consume"},
    Example{16, "Unknown error type"},
    Example{-1, "Unknown error type"},
};

using What = const char* (APS5_VABI*)(const LibcException::ExceptionObject*);

void Check(const Example& example) {
    LibcException::LibcxxRegexErrorObject exception{};
    _ZNSt3__111regex_errorC1ENS_15regex_constants10error_typeE_nid_postfix(&exception, example.code);
    Require(exception.code == example.code, "libc++ regex_error changed the guest error code");

    const auto* vtable = static_cast<const void* const*>(exception.base.vtable);
    const auto* type = static_cast<const std::type_info*>(vtable[-1]);
    Require(std::strcmp(type->name(), "NSt3__111regex_errorE") == 0, "libc++ regex_error uses the wrong RTTI");
    const auto* inherited = static_cast<const __cxxabiv1::__si_class_type_info*>(type);
    Require(inherited->__base_type && std::strcmp(inherited->__base_type->name(), "St13runtime_error") == 0,
            "libc++ regex_error must inherit from std::runtime_error");
    const auto what = reinterpret_cast<What>(vtable[2]);
    Require(std::strstr(what(&exception.base), example.text) != nullptr, "libc++ regex_error reports the wrong diagnostic");

    _ZNSt3__111regex_errorD1Ev_nid_postfix(&exception);
    Require(exception.base.message == nullptr, "libc++ regex_error left the message attached after destruction");
}

}

int main() {
    for (const auto& example : Examples) Check(example);
    std::puts("libc++ regex_error guest ABI checks passed");
    return 0;
}
