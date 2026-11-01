#include "prx/libSceSystemService/include/SystemService.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int invalidArgument = static_cast<int>(0x80A10003u);
constexpr unsigned char untouched = 0x5A;

void Require(bool value) { if (!value) std::abort(); }

SystemServicePSButtonEvent* Output(unsigned char* buffer) {
    return reinterpret_cast<SystemServicePSButtonEvent*>(buffer);
}

void FillEvent(SystemServiceEvent& event, std::int32_t eventType) {
    event.event_type = eventType;
    for (std::size_t i = 0; i < sizeof(event.data); ++i) event.data[i] = static_cast<std::uint8_t>(0x11 + i);
}

void RequireUntouched(const unsigned char* buffer, std::size_t from, std::size_t size) {
    for (std::size_t i = from; i < size; ++i) Require(buffer[i] == untouched);
}

}

int main() {
    static SystemServiceEvent event;
    alignas(8) unsigned char buffer[32];
    FillEvent(event, 0x20000001);
    std::memset(buffer, untouched, sizeof(buffer));
    Require(sceSystemServiceGetPSButtonEvent(nullptr, Output(buffer)) == invalidArgument);
    Require(sceSystemServiceGetPSButtonEvent(&event, nullptr) == invalidArgument);
    RequireUntouched(buffer, 0, sizeof(buffer));
    for (std::int32_t eventType : {0, 1, 0x10000001, 0x20000000, 0x20000003, 0x30000001, -1}) {
        FillEvent(event, eventType);
        Require(sceSystemServiceGetPSButtonEvent(&event, Output(buffer)) == invalidArgument);
        RequireUntouched(buffer, 0, sizeof(buffer));
    }
    for (std::int32_t eventType : {0x20000001, 0x20000002}) {
        FillEvent(event, eventType);
        std::memset(buffer, untouched, sizeof(buffer));
        Require(sceSystemServiceGetPSButtonEvent(&event, Output(buffer)) == 0);
        std::int32_t outputType = 0;
        std::memcpy(&outputType, buffer, sizeof(outputType));
        Require(outputType == eventType);
        Require(std::memcmp(buffer + 4, event.data + 8, 4) == 0);
        Require(std::memcmp(buffer + 8, event.data, 8) == 0);
        RequireUntouched(buffer, 16, sizeof(buffer));
    }
    return 0;
}
