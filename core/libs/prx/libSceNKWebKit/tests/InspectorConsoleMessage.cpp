#include "prx/libSceNKWebKit/include/InspectorConsoleMessageExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Inspector::ConsoleMessage check failed at line %u\n", location.line());
        std::abort();
    }
}

template <typename T>
void Store(unsigned char* object, std::size_t offset, T value) {
    std::memcpy(object + offset, &value, sizeof(value));
}

template <typename T>
T Load(const unsigned char* object, std::size_t offset) {
    T value;
    std::memcpy(&value, object + offset, sizeof(value));
    return value;
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static_assert(std::is_same_v<decltype(&_ZNK9Inspector14ConsoleMessage6sourceEv), std::uint8_t (APS5_VABI *)(const void*)>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector14ConsoleMessage14incrementCountEv), void (APS5_VABI *)(void*)>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector14ConsoleMessage13argumentCountEv), unsigned (APS5_VABI *)(const void*)>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector15ScriptArguments10argumentAtEm), std::uint64_t (APS5_VABI *)(const void*, std::size_t)>);

}

int main() {
    alignas(16) unsigned char message[96] = {};
    alignas(16) unsigned char arguments[32] = {};
    alignas(16) unsigned char messageGlobal[16] = {};
    alignas(16) unsigned char argumentsGlobal[16] = {};

    message[0] = 4;
    message[1] = 9;
    message[2] = 3;
    Require(_ZNK9Inspector14ConsoleMessage6sourceEv(message) == 4);
    Require(_ZNK9Inspector14ConsoleMessage4typeEv(message) == 9);
    Require(_ZNK9Inspector14ConsoleMessage5levelEv(message) == 3);

    Store<unsigned>(message, 72, 1);
    _ZN9Inspector14ConsoleMessage14incrementCountEv(message);
    Require(Load<unsigned>(message, 72) == 2);
    _ZN9Inspector14ConsoleMessage14incrementCountEv(message);
    Require(Load<unsigned>(message, 72) == 3);
    Require(Load<unsigned>(message, 64) == 0);
    Require(Load<unsigned>(message, 68) == 0);

    Require(_ZNK9Inspector14ConsoleMessage13argumentCountEv(message) == 0);
    Require(_ZNK9Inspector14ConsoleMessage12globalObjectEv(message) == nullptr);

    std::uint64_t emptyValue = 0;
    Store<std::uint64_t*>(message, 56, &emptyValue);
    Require(_ZNK9Inspector14ConsoleMessage12globalObjectEv(message) == nullptr);

    std::uint64_t messageGlobalValue = reinterpret_cast<std::uintptr_t>(messageGlobal);
    Store<std::uint64_t*>(message, 56, &messageGlobalValue);
    Require(_ZNK9Inspector14ConsoleMessage12globalObjectEv(message) == messageGlobal);

    std::uint64_t argumentValues[3] = {0xfffe000000000007ull, 0, 0x0000000000000006ull};
    std::uint64_t* slots[4] = {&argumentValues[0], nullptr, &argumentValues[1], &argumentValues[2]};
    Store<unsigned>(arguments, 0, 1);
    Store<std::uint64_t**>(arguments, 16, slots);
    Store<unsigned>(arguments, 24, 8);
    Store<unsigned>(arguments, 28, 4);
    Store<unsigned char*>(message, 16, arguments);

    Require(_ZNK9Inspector14ConsoleMessage13argumentCountEv(message) == 4);
    Require(_ZNK9Inspector14ConsoleMessage12globalObjectEv(message) == nullptr);
    Require(_ZNK9Inspector15ScriptArguments12globalObjectEv(arguments) == nullptr);

    Store<std::uint64_t*>(arguments, 8, &emptyValue);
    Require(_ZNK9Inspector15ScriptArguments12globalObjectEv(arguments) == nullptr);
    Require(_ZNK9Inspector14ConsoleMessage12globalObjectEv(message) == nullptr);

    std::uint64_t argumentsGlobalValue = reinterpret_cast<std::uintptr_t>(argumentsGlobal);
    Store<std::uint64_t*>(arguments, 8, &argumentsGlobalValue);
    Require(_ZNK9Inspector15ScriptArguments12globalObjectEv(arguments) == argumentsGlobal);
    Require(_ZNK9Inspector14ConsoleMessage12globalObjectEv(message) == argumentsGlobal);

    Require(_ZNK9Inspector15ScriptArguments10argumentAtEm(arguments, 0) == 0xfffe000000000007ull);
    Require(_ZNK9Inspector15ScriptArguments10argumentAtEm(arguments, 1) == 0);
    Require(_ZNK9Inspector15ScriptArguments10argumentAtEm(arguments, 2) == 0);
    Require(_ZNK9Inspector15ScriptArguments10argumentAtEm(arguments, 3) == 6);
    Require(Throws([&] { _ZNK9Inspector15ScriptArguments10argumentAtEm(arguments, 4); }));
    Require(Load<unsigned>(arguments, 0) == 1);

    Store<unsigned>(arguments, 28, 0);
    Require(_ZNK9Inspector14ConsoleMessage13argumentCountEv(message) == 0);
    Require(Throws([&] { _ZNK9Inspector15ScriptArguments10argumentAtEm(arguments, 0); }));

    Require(Throws([] { _ZNK9Inspector14ConsoleMessage6sourceEv(nullptr); }));
    Require(Throws([] { _ZN9Inspector14ConsoleMessage14incrementCountEv(nullptr); }));
    Require(Throws([] { _ZNK9Inspector14ConsoleMessage12globalObjectEv(nullptr); }));
    Require(Throws([] { _ZNK9Inspector15ScriptArguments12globalObjectEv(nullptr); }));

    std::puts("Inspector::ConsoleMessage checks passed");
    return 0;
}
