#include "prx/libSceNKWebKit/include/InspectorConsoleMessageExports.hpp"
#include "prx/libSceNKWebKit/src/InspectorConsoleMessageAbi.hpp"

#include <stdexcept>
#include <string>

namespace {

using NKWebKit::InspectorAbi::ConsoleMessage;
using NKWebKit::InspectorAbi::HandleSlot;
using NKWebKit::InspectorAbi::ScriptArguments;

template <typename T>
T* Require(T* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + ": null object");
    return self;
}

const ConsoleMessage* Message(const void* self, const char* function) {
    return Require(static_cast<const ConsoleMessage*>(self), function);
}

const ScriptArguments* Arguments(const void* self, const char* function) {
    return Require(static_cast<const ScriptArguments*>(self), function);
}

void* GlobalObjectFromSlot(HandleSlot slot) {
    return slot && *slot ? reinterpret_cast<void*>(static_cast<std::uintptr_t>(*slot)) : nullptr;
}

}

extern "C" {

std::uint8_t APS5_VABI _ZNK9Inspector14ConsoleMessage6sourceEv(const void* self) {
    return Message(self, __func__)->source;
}

std::uint8_t APS5_VABI _ZNK9Inspector14ConsoleMessage4typeEv(const void* self) {
    return Message(self, __func__)->type;
}

std::uint8_t APS5_VABI _ZNK9Inspector14ConsoleMessage5levelEv(const void* self) {
    return Message(self, __func__)->level;
}

void APS5_VABI _ZN9Inspector14ConsoleMessage14incrementCountEv(void* self) {
    ++Require(static_cast<ConsoleMessage*>(self), __func__)->repeatCount;
}

unsigned APS5_VABI _ZNK9Inspector14ConsoleMessage13argumentCountEv(const void* self) {
    const auto* message = Message(self, __func__);
    return message->arguments ? message->arguments->arguments.size : 0;
}

void* APS5_VABI _ZNK9Inspector14ConsoleMessage12globalObjectEv(const void* self) {
    const auto* message = Message(self, __func__);
    if (message->arguments) return GlobalObjectFromSlot(message->arguments->globalObject);
    return GlobalObjectFromSlot(message->globalObject);
}

void* APS5_VABI _ZNK9Inspector15ScriptArguments12globalObjectEv(const void* self) {
    return GlobalObjectFromSlot(Arguments(self, __func__)->globalObject);
}

std::uint64_t APS5_VABI _ZNK9Inspector15ScriptArguments10argumentAtEm(const void* self, std::size_t index) {
    const auto* arguments = Arguments(self, __func__);
    if (index >= arguments->arguments.size) throw std::out_of_range(std::string(__func__) + ": index out of range");
    const HandleSlot slot = arguments->arguments.buffer[index];
    return slot ? *slot : 0;
}

}
