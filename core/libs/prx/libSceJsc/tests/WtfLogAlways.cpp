#include "prx/libSceJsc/include/WtfAssertionsExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTFLogAlwaysV check failed at line %d\n", line);
        std::exit(1);
    }
}

#define Require(value) Check((value), __LINE__)

constexpr std::uint32_t NoGeneralRegisters = 48;
constexpr std::uint32_t NoVectorRegisters = 176;

struct Arguments {
    std::uint64_t slots[4]{};
    unsigned char registers[176]{};

    VaList List() {
        return VaList{NoGeneralRegisters, NoVectorRegisters, slots, registers};
    }
};

void Log(const char* format, Arguments arguments = {}) {
    VaList list = arguments.List();
    WTFLogAlwaysV(format, &list);
}

int PrintToStandardError() {
    Arguments numbers;
    numbers.slots[0] = 41;
    numbers.slots[1] = reinterpret_cast<std::uint64_t>("text");
    Log("first %d %s", numbers);
    Log("second\n");
    Log("");
    Arguments name;
    name.slots[0] = reinterpret_cast<std::uint64_t>("third");
    name.slots[1] = static_cast<std::uint64_t>(-7);
    Log("%s %d\n", name);
    const std::string longFormat = std::string(5000, 'x') + "!";
    Log(longFormat.c_str());
    Log("100%% done");
    Log("end");
    fflush_nid_postfix(&_Stderr_nid_postfix);
    return 0;
}

void TestNullFormat() {
    Arguments arguments;
    VaList list = arguments.List();
    bool thrown = false;
    try {
        WTFLogAlwaysV(nullptr, &list);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
}

}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "stderr") == 0) return PrintToStandardError();
    TestNullFormat();
    return 0;
}
