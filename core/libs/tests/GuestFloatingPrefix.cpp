#include "prx/libc/include/general/VabiMacros.hpp"
#include <nid/NidCompute.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

template<typename TChar>
using Prefix = int (APS5_VABI*)(const TChar**, TChar**);

struct Case {
    const char* text;
    int code;
    std::size_t cursor;
    bool writesEnd;
};

constexpr std::array Cases{
    Case{"", 1, 0, false}, Case{"12.5e4", 1, 0, false}, Case{".", 1, 0, false},
    Case{"-", 9, 1, false}, Case{"+", 1, 1, false}, Case{"  -12", 9, 3, false},
    Case{"\t\r\n\v\f +0x1p2", 2, 9, false}, Case{"0x1", 2, 2, false},
    Case{"-0X.8p2", 10, 3, false}, Case{"0x", 1, 0, false}, Case{"0x.", 1, 0, false},
    Case{"0xg", 1, 0, false}, Case{"0x.1", 2, 2, false}, Case{"0x..1", 1, 0, false},
    Case{"0x 1", 1, 0, false}, Case{"  -0xg", 9, 3, false},
    Case{"n", 0, 0, true}, Case{"na", 0, 0, true}, Case{"nax", 0, 0, true},
    Case{"  -Na", 0, 0, true}, Case{"nan", 4, 3, true}, Case{"-NaN", 4, 4, true},
    Case{"nan()", 4, 5, true}, Case{"nan(a_Z09)", 4, 10, true},
    Case{"nan(payload", 4, 3, true}, Case{"nan(a-b)", 4, 3, true},
    Case{"nan(a(b))", 4, 3, true}, Case{"nan()tail", 4, 5, true},
    Case{"i", 0, 0, true}, Case{"in", 0, 0, true}, Case{"inx", 0, 0, true},
    Case{" -iN", 0, 0, true}, Case{"inf", 3, 3, true}, Case{"-INF", 11, 4, true},
    Case{"infinity", 3, 8, true}, Case{"+InFiNiTy!", 3, 9, true},
    Case{"infi", 3, 3, true}, Case{"infin", 3, 3, true}, Case{"infinit", 3, 3, true},
    Case{"infinite", 3, 3, true}, Case{"  -infinity", 11, 11, true},
    Case{"not-a-number", 0, 0, true}, Case{"garbage", 1, 0, false},
};

template<typename TChar>
static std::vector<TChar> Text(const char* source) {
    std::vector<TChar> result;
    do result.push_back(static_cast<unsigned char>(*source)); while (*source++);
    return result;
}

template<typename TChar>
static void CheckOne(Prefix<TChar> function, const TChar* text, const Case& test) {
    constexpr std::uint64_t guard = 0xd3c4b5a69788796a;
    TChar sentinel{};
    struct Outputs {
        std::uint64_t before;
        const TChar* cursor;
        std::uint64_t middle;
        TChar* end;
        std::uint64_t after;
    };
    static_assert(offsetof(Outputs, cursor) == 8 && offsetof(Outputs, middle) == 16);
    static_assert(offsetof(Outputs, end) == 24 && offsetof(Outputs, after) == 32);
    Outputs outputs{guard, text, guard, &sentinel, guard};
    const int code = function(&outputs.cursor, &outputs.end);
    if (code != test.code || outputs.cursor != text + test.cursor ||
        outputs.end != (test.writesEnd ? text + test.cursor : &sentinel)) {
        std::fprintf(stderr, "prefix '%s': code %d expected %d, cursor %td expected %zu\n",
            test.text, code, test.code, outputs.cursor - text, test.cursor);
        std::exit(1);
    }
    Require(outputs.before == guard && outputs.middle == guard && outputs.after == guard, "output bounds");
    const TChar* cursor = text;
    Require(function(&cursor, nullptr) == test.code && cursor == text + test.cursor, "optional end pointer");
}

template<typename TChar>
static void Check(Prefix<TChar> function, unsigned payloadLimit = 256) {
    for (const auto& test : Cases) {
        auto text = Text<TChar>(test.text);
        const auto saved = text;
        CheckOne(function, text.data(), test);
        Require(text == saved, "input modified");
    }
    for (const char* word : {"nan", "inf", "infinity"}) {
        const std::size_t length = std::strlen(word);
        for (unsigned mask = 0; mask < (1u << length); ++mask) {
            std::string text = word;
            for (std::size_t index = 0; index < length; ++index)
                if (mask & (1u << index)) text[index] -= 'a' - 'A';
            for (const char* sign : {"", "+", "-"}) {
                const std::string input = " \t" + std::string(sign) + text;
                const int code = word[0] == 'n' ? 4 : 3 + (sign[0] == '-' ? 8 : 0);
                const Case test{input.c_str(), code, input.size(), true};
                const auto data = Text<TChar>(input.c_str());
                CheckOne(function, data.data(), test);
            }
        }
    }
    for (unsigned value = 0; value < payloadLimit; ++value) {
        const std::array<TChar, 7> payload{TChar{'n'}, TChar{'a'}, TChar{'n'}, TChar{'('}, static_cast<TChar>(value), TChar{')'}, TChar{}};
        const bool accepted = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '_';
        const std::size_t stop = accepted ? 6 : value == ')' ? 5 : 3;
        const std::string label = "payload byte " + std::to_string(value) + ", unit width " + std::to_string(sizeof(TChar));
        CheckOne(function, payload.data(), Case{label.c_str(), 4, stop, true});
    }
    if constexpr (sizeof(TChar) == 2) {
        const std::array<TChar, 4> high{static_cast<TChar>(0x100), TChar{'i'}, TChar{'n'}, TChar{}};
        CheckOne(function, high.data(), Case{"wide code unit", 1, 0, false});
    }
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(allocation != nullptr, "guard allocation");
    DWORD previous = 0;
    Require(VirtualProtect(allocation + page, page, PAGE_NOACCESS, &previous) != 0, "guard protection");
#else
    const std::size_t page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* allocation = static_cast<unsigned char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(allocation != MAP_FAILED, "guard allocation");
    Require(mprotect(allocation + page, page, PROT_NONE) == 0, "guard protection");
#endif
    for (const auto& test : Cases) {
        const auto source = Text<TChar>(test.text);
        const std::size_t bytes = source.size() * sizeof(TChar);
        auto* text = reinterpret_cast<TChar*>(allocation + page - bytes);
        std::memcpy(text, source.data(), bytes);
        CheckOne(function, text, test);
    }
#ifdef _WIN32
    Require(VirtualFree(allocation, 0, MEM_RELEASE) != 0, "guard release");
#else
    Require(munmap(allocation, page * 2) == 0, "guard release");
#endif
}

template<typename TChar>
static void CheckConcurrent(Prefix<TChar> function) {
    std::array<std::thread, 4> workers;
    for (auto& worker : workers) {
        worker = std::thread([function] {
            for (unsigned repeat = 0; repeat < 8; ++repeat) {
                for (const auto& test : Cases) {
                    const auto text = Text<TChar>(test.text);
                    CheckOne(function, text.data(), test);
                }
            }
        });
    }
    for (auto& worker : workers) worker.join();
}

int main(int argc, char** argv) {
    Require(argc == 4, "library directory, patched/unpatched, narrow/wide");
    const bool wide = std::string(argv[3]) == "wide";
    Require(wide || std::string(argv[3]) == "narrow", "width selection");
    const std::string name = wide ? "_WStopfx" : "_Stopfx";
    const std::string symbol = std::string(argv[2]) == "patched" ? Nid::ComputeNid(name, "") : name + "_nid_postfix";
    const auto path = std::filesystem::absolute(argv[1]) / "libc.prx";
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    const auto startup = GetModuleHandleW(L"libc.prx");
    const auto library = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    Require(library != nullptr, "load libc");
    std::array<wchar_t, 32768> loaded{};
    const auto length = GetModuleFileNameW(library, loaded.data(), static_cast<DWORD>(loaded.size()));
    Require(length != 0 && length < loaded.size() && std::filesystem::equivalent(path, std::filesystem::path(loaded.data())), "wrong libc loaded");
    if (startup) Require(startup == library, "startup libc differs");
    const auto function = reinterpret_cast<void*>(GetProcAddress(library, symbol.c_str()));
#else
    const auto library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    Require(library != nullptr, "load libc");
    const auto function = dlsym(library, symbol.c_str());
#endif
    if (!function) std::fprintf(stderr, "Missing %s (%s)\n", name.c_str(), symbol.c_str());
    Require(function != nullptr, "cannot exercise missing prefix parser");
    if (wide) {
        const auto prefix = reinterpret_cast<Prefix<char16_t>>(function);
        Check(prefix);
        CheckConcurrent(prefix);
    } else {
        const auto prefix = reinterpret_cast<Prefix<char>>(function);
        Check(prefix);
        CheckConcurrent(prefix);
    }
    std::printf("%s: prefix codes, pointer updates, input/output bounds and guarded reads passed\n", name.c_str());
    return 0;
}
