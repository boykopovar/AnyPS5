#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

using Setter = int (APS5_VABI *)(void*, const void*);

struct Field {
    const char* symbol;
    std::size_t offset;
    std::size_t width;
};

constexpr Field Fields[] = {
    {"+oqyRiwxW18", 0x14, 4},
    {"63mPNKdUOZY", 0x78, 4},
    {"SQMjAiGtsiI", 0x70, 4},
    {"FySvwNKJp2U", 0x3c, 4},
    {"slvvHT5LSEw", 0x44, 4},
    {"SyilEZ5n3wU", 0x14, 4},
    {"lFoCvh0UOew", 0x91, 1},
    {"8C36kDPJiS0", 0x119, 1},
    {"fqF2T3M8uxk", 0x74, 4},
    {"puesPxZFDaQ", 0x74, 4},
    {"bCCKHTeKetw", 0x18, 4},
    {"GzqrUd4brhA", 0x18, 4},
    {"8O-6RHUTD7k", 0x38, 1},
    {"guy3X-PEbqk", 0x10, 4},
    {"uzgnlW3QnTo", 0x10, 4},
    {"tF4iLLzeCTc", 0x1d, 1},
    {"CWfrxh5cD90", 0xe9, 1},
};

using Model = std::array<std::uint8_t, 0x140>;

bool Check(Setter setter, const Field& field, Model& model, const void* argument) {
    auto expected = model;
    int value = 0;
    std::memcpy(&value, argument, field.width);
    std::memcpy(expected.data() + field.offset, argument, field.width);
    expected[field.offset + field.width] = 1;
    return setter(model.data(), argument) == value && model == expected;
}

bool CheckField(Setter setter, const Field& field) {
    Model model{};
    for (const auto fill : {0, 0xcc, 0xff}) {
        for (const std::uint32_t value : {0u, 1u, 3u, 0xffu, 0x12345678u, 0x80000000u, 0xffffffffu}) {
            model.fill(static_cast<std::uint8_t>(fill));
            if (!Check(setter, field, model, &value)) return false;
        }
    }
    for (const auto at : {field.offset - 1, field.offset, field.offset + 1, field.offset + field.width}) {
        for (std::size_t i = 0; i < model.size(); ++i) model[i] = static_cast<std::uint8_t>(i * 37 + 17);
        if (!Check(setter, field, model, model.data() + at)) return false;
    }
    return true;
}

}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
#ifdef _WIN32
    const auto module = LoadLibraryA(argv[1]);
#else
    const auto module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    if (!module) return 1;
    for (const auto& field : Fields) {
#ifdef _WIN32
        const auto setter = reinterpret_cast<Setter>(GetProcAddress(module, field.symbol));
#else
        const auto setter = reinterpret_cast<Setter>(dlsym(module, field.symbol));
#endif
        if (!setter || !CheckField(setter, field)) {
            std::fprintf(stderr, "session request field %s failed\n", field.symbol);
            return 1;
        }
    }
    return 0;
}
