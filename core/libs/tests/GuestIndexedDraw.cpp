#include "SceTypes.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcDcbSetIndexCount(CommandBuffer*, std::uint32_t);
std::uint32_t APS5_VABI sceAgcDcbSetIndexCountGetSize();
std::uint32_t* APS5_VABI sceAgcDcbDrawIndex(CommandBuffer*, std::uint32_t, const volatile void*, std::uint64_t);
std::uint32_t APS5_VABI sceAgcDcbDrawIndexGetSize();
}
static void Require(bool condition) { if (!condition) std::abort(); }
template<typename TAction> static void Reject(TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected);
}
int main() {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data()+words.size(), words.data(), words.data()+words.size(), nullptr, nullptr, 0};
    auto* sizePacket = sceAgcDcbSetIndexCount(&buffer, 3);
    Require(sizePacket == words.data() && words[0] == 0xc0001300u && words[1] == 3);
    Require(sceAgcDcbSetIndexCountGetSize() == 8);
    AgcDriver::QueueState queue;
    AgcDriver::Pm4::Validate({sizePacket,2},0);
    AgcDriver::Pm4::Execute({sizePacket,2},queue);
    Require(queue.indexBufferSize == 3);
    alignas(4) std::array<std::uint32_t,3> indices{2,0,1};
    auto* draw = sceAgcDcbDrawIndex(&buffer,3,indices.data(),0);
    Require(draw == words.data()+2 && draw[0] == 0xc0042700u && draw[1] == 3 && draw[4] == 3 && draw[5] == 0);
    Require(sceAgcDcbDrawIndexGetSize() == 24);
    Require(AgcDriver::Pm4::AccessesMemory(draw[0]));
    Require(AgcDriver::Pm4::DrawOpcode((draw[0] >> 8u) & 0xffu));
    queue.indexBase = 1;
    for (unsigned type=0;type<3;++type) {
        queue.indexType=type;
        const auto result=AgcDriver::Pm4::ResolveDraw({draw,6},queue);
        Require(result.indexAddress == reinterpret_cast<std::uintptr_t>(indices.data()));
        Require(result.indexCount == 3 && result.indexSize == (type==0?2:type==1?4:1) && result.indexed);
    }
    Require(queue.indexBase == 1);
    queue.indexType = 1;
    queue.indexBase = reinterpret_cast<std::uintptr_t>(indices.data());
    const std::array<std::uint32_t,5> offsetDraw{0xc0033500u,2,1,2,0};
    const auto offsetResult = AgcDriver::Pm4::ResolveDraw(offsetDraw,queue);
    Require(offsetResult.indexAddress == queue.indexBase+4 && offsetResult.indexCount == 2);
    draw[5] = 0x20;
    Require(AgcDriver::Pm4::ResolveDraw({draw,6},queue).flags == 0x20);
    draw[5] = 0;
    draw[1] = 0; draw[4] = 0;
    Require(AgcDriver::Pm4::ResolveDraw({draw,6},queue).indexCount == 0);
    draw[1] = 3; draw[4] = 3;
    draw[4]=4;
    Reject([&]{AgcDriver::Pm4::Validate({draw,6},0);});
    draw[4]=3;
    draw[5]=2;
    Reject([&]{AgcDriver::Pm4::Validate({draw,6},0);});
    draw[5]=0;
    Reject([&]{AgcDriver::Pm4::Validate({draw,5},0);});
    Reject([&]{AgcDriver::Pm4::Validate({draw,6},1);});
    draw[3]=0x10000;
    Reject([&]{AgcDriver::Pm4::Validate({draw,6},0);});
    draw[2]=1; draw[3]=0; queue.indexType=1;
    Reject([&]{AgcDriver::Pm4::ResolveDraw({draw,6},queue);});
    auto* saved=buffer.cursor_up;
    Reject([&]{sceAgcDcbDrawIndex(&buffer,3,nullptr,0);});
    Reject([&]{sceAgcDcbDrawIndex(&buffer,3,indices.data(),1ull<<40);});
    Require(buffer.cursor_up==saved);
    buffer.cursor_down=buffer.cursor_up+5;
    Reject([&]{sceAgcDcbDrawIndex(&buffer,3,indices.data(),0);});
    Require(buffer.cursor_up==saved);
    return 0;
}
