#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <new>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 128;
constexpr std::uint32_t SharedWords = 16;
constexpr std::size_t InputWords = Threads * Inputs + SharedWords;
constexpr std::size_t OutputWords = Threads * Results;
constexpr std::size_t BlockBytes = 65536;
std::uint32_t* Input = nullptr;
std::uint32_t* Output = nullptr;

alignas(256) constexpr std::array<std::uint32_t, 162> Wave64Code{
    0x34020084, 0x34060089, 0xe0381000, 0x80000401, 0xbf8c3f70, 0xbe880300, 0xbe890301, 0xbe8a03ff,
    0x00000400, 0xbe8b03ff, 0x31016fac, 0xbe8c0304, 0xbe8d0305, 0xbe8e03ff, 0x00004000, 0xbe8f03ff,
    0x31016fac, 0xbe980383, 0xe0781100, 0x80010403, 0xe0781110, 0x80010403, 0xe0781120, 0x80010403,
    0xe0781130, 0x80010403, 0xe07411f8, 0x80010403, 0xbf8c3f70, 0x36a60aff, 0x000007ff, 0xd5480054,
    0x022d1705, 0x36aa0cff, 0x000007fe, 0xd5480056, 0x022d1706, 0x36ae0eff, 0x000007fc, 0xd5480058,
    0x022d1707, 0x36acacc2, 0x36b0b0c4, 0x7e140306, 0x7e160306, 0x7e180306, 0x7e1a0306, 0x7e1c0306,
    0x7e1e0306, 0x7e200306, 0x7e220306, 0x7e240306, 0x7e260306, 0x7e280306, 0x7e2a0306, 0x2ca00e9e,
    0x7d8aa080, 0xbea8246a, 0xe0201000, 0x80020a53, 0xe0241000, 0x80020b54, 0xe0281000, 0x80020c55,
    0xe02c1000, 0x80020d56, 0xe0341000, 0x80020e57, 0xe03c1000, 0x80021058, 0xe0201005, 0x18021301,
    0xe024100f, 0x80021401, 0xe02c100b, 0x18021501, 0xbefe04c1, 0x36b2088f, 0x4ab2b303, 0xd548005a,
    0x020d0904, 0xd746005a, 0x040d035a, 0xd548005b, 0x02050f04, 0xd746005b, 0x040d055b, 0x2ca00c9e,
    0x7d8aa080, 0xbea8246a, 0xe0601100, 0x80010659, 0xe0681110, 0x8001075a, 0xbefe04c1, 0xe07c1120,
    0x8001055b, 0xe060112d, 0x18010703, 0xe0601130, 0x80030403, 0xe0681132, 0x80030503, 0xe07c1134,
    0x80030503, 0xe07c11f8, 0x80030503, 0xbf8c3f70, 0xe030d100, 0x80011603, 0xe030d104, 0x80011703,
    0xe030d108, 0x80011803, 0xe030d10c, 0x80011903, 0xe030d110, 0x80011a03, 0xe030d114, 0x80011b03,
    0xe030d118, 0x80011c03, 0xe030d11c, 0x80011d03, 0xe030d120, 0x80011e03, 0xe030d124, 0x80011f03,
    0xe030d128, 0x80012003, 0xe030d12c, 0x80012103, 0xe030d130, 0x80012203, 0xe030d134, 0x80012303,
    0xe030d138, 0x80012403, 0xe030d13c, 0x80012503, 0xe030d1f8, 0x80012603, 0xe030d1fc, 0x80012703,
    0xbf8c3f70, 0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xe0781020, 0x80011203, 0xe0781030,
    0x80011603, 0xe0781040, 0x80011a03, 0xe0781050, 0x80011e03, 0xe0781060, 0x80012203, 0xe0781070,
    0x80012603, 0xbf810000,
};

constexpr std::uint32_t Rows[64][4] = {
    {0x536e95dfu, 0x536fe5e0u, 0xd28fe56au, 0xf08db1fcu},
    {0x04826f45u, 0x049fc746u, 0x08fb4bf8u, 0x125fc41du},
    {0x80000001u, 0xb2cc53fcu, 0x7caefbfcu, 0xc92fdbfcu},
    {0xa93f32a9u, 0x00007fffu, 0x074206e2u, 0xd5de6e91u},
    {0x12345678u, 0x459ccb82u, 0x71dbadabu, 0xa0879a14u},
    {0xc045596eu, 0x00ff01fcu, 0x87ed8dfcu, 0x3f6fe27eu},
    {0x6ecc5cb6u, 0x6ecc5cb6u, 0xfc520fdeu, 0x810717e7u},
    {0x924f1693u, 0x0000007fu, 0x2a61adfdu, 0xf4f00c48u},
    {0x198f69a0u, 0xfb6fe870u, 0xb517d9fdu, 0xbd5ce1fdu},
    {0x9399468au, 0x99e237bfu, 0x6ad546e6u, 0x7d513542u},
    {0xfff2776cu, 0xfc2a621cu, 0xbfce753fu, 0xc488e594u},
    {0x811b6c22u, 0x00007fffu, 0x60e75e3cu, 0xf86e2c6cu},
    {0xce05cb84u, 0x5b11485au, 0x790577fdu, 0x7d79f3c4u},
    {0x99b9355bu, 0x066ff6ccu, 0x6c4ff23fu, 0xdcbc85feu},
    {0x8232b967u, 0x8232b967u, 0x47a0e739u, 0x6c3f4bc5u},
    {0xfe8c34bdu, 0x2b8bb388u, 0x1a85f953u, 0x4303ee91u},
    {0x000000ffu, 0x7fffffffu, 0x2ed0d50au, 0x54dc77a7u},
    {0x00007fffu, 0x000ff0ffu, 0x39cff032u, 0x310ff5fcu},
    {0x1f1950d9u, 0xffffffffu, 0x0e6fd1c9u, 0xd4041791u},
    {0x00000001u, 0x00000001u, 0xa1e99e64u, 0x0a19deacu},
    {0x0e532388u, 0xd28fee69u, 0xb123fdfdu, 0xa78e89fdu},
    {0x80000000u, 0x000ff801u, 0xcb4b21ffu, 0x7b2ffe58u},
    {0x7fffffffu, 0x1a6589fdu, 0xe22fe806u, 0xdaef49fdu},
    {0x00000001u, 0x3bbcaa3au, 0xaba08b3du, 0x80631136u},
    {0xe2a77a78u, 0xe2a779fdu, 0x9f2fe8f7u, 0x6a3e71fdu},
    {0x00000001u, 0x000ff801u, 0x242ffb1fu, 0x024ffee3u},
    {0x7fffffffu, 0x7ffffffeu, 0x8853d3bfu, 0xb1fd721bu},
    {0xff00ff00u, 0x00007fffu, 0x4894991du, 0xdd42f005u},
    {0xffffffffu, 0x00000000u, 0xc69f64a3u, 0x47f32e2eu},
    {0xb4f88561u, 0xecc6592du, 0x14321c1cu, 0x4636ff26u},
    {0x00010000u, 0xb22f4004u, 0x0c8b001fu, 0x364d2b66u},
    {0x8f9fa5ecu, 0x946fa280u, 0x0e9505f4u, 0x870fa76du},
    {0x00000080u, 0x57d10b4du, 0x0f7b8468u, 0xad8d899eu},
    {0xf4823356u, 0x00008000u, 0x142cfcf9u, 0x5c0a21d7u},
    {0xd94e6bb3u, 0xd94e6bb3u, 0x732951cdu, 0xbf9cf610u},
    {0x12345678u, 0x13afe032u, 0x6e0b75fcu, 0x012fe591u},
    {0xd14e11a6u, 0x00007fffu, 0xa217908cu, 0xe80b3d6eu},
    {0xab1ff0d5u, 0xab1ff0d5u, 0x0b897562u, 0x8cf72913u},
    {0x82f94d41u, 0x2dafc382u, 0xbcd18df8u, 0x7b4cd5f8u},
    {0x89abcdefu, 0x89abcdefu, 0x62fba05du, 0xbccfa6b8u},
    {0x321439f8u, 0x00000003u, 0x3631cb82u, 0xc93a3d1bu},
    {0x4e84d753u, 0x54a9b365u, 0x56ee8a7bu, 0xf702a20au},
    {0xffffffffu, 0xff00ff00u, 0x36938222u, 0x41145208u},
    {0x00000080u, 0x00000081u, 0xd4d05190u, 0x43d09980u},
    {0x00008000u, 0x00000003u, 0x7ad78e73u, 0x967003cfu},
    {0x00000080u, 0xe8e0746eu, 0x4bbe399cu, 0x950ff203u},
    {0x9de1f1fcu, 0x7fffffffu, 0xe065fc6cu, 0xd2326b65u},
    {0x62cd61e2u, 0x12345678u, 0x6df9dc5cu, 0x9c8faa94u},
    {0x00ff00ffu, 0x00ff00feu, 0x704b8500u, 0x82de0143u},
    {0x053b0655u, 0x7fffffffu, 0x54a25ef4u, 0xe80c1562u},
    {0x99062011u, 0x1c2fe6d7u, 0x59a71dfcu, 0x5b3425fcu},
    {0x57ef669fu, 0x00000002u, 0xe1ff47dbu, 0xe840795eu},
    {0x230a2bc5u, 0x230a2bc4u, 0xb278ccf4u, 0x063f9962u},
    {0xff00ff00u, 0x000000ffu, 0xb83f90e9u, 0xa3640fdeu},
    {0xee86c75eu, 0x00ff00ffu, 0xff5ae0b0u, 0x6ca5bfdcu},
    {0x00000001u, 0x7c900a51u, 0x77bbc220u, 0x47229d30u},
    {0x000000ffu, 0xffffffffu, 0x3b082f86u, 0x9592116bu},
    {0xa9af3d85u, 0x938bfc16u, 0xf3cf3a11u, 0xb75e01dau},
    {0x12345678u, 0x47021cf1u, 0x59c67fb6u, 0xf2b52a63u},
    {0x89009332u, 0x89009332u, 0x3044bfafu, 0x5db890dau},
    {0x7ab0e583u, 0x1d432825u, 0x8b85be22u, 0x7ac0b14cu},
    {0x80000000u, 0x000001fcu, 0x5eafe75eu, 0x2b0fe583u},
    {0x00007fffu, 0x00007ffeu, 0x5d9057fau, 0xbd31e9d1u},
    {0x07939cf9u, 0x07939cf9u, 0xf7bc3c79u, 0xf438acb4u}
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Dispatch(AgcDriver::VulkanDevice& device, bool wait = true) {
    static std::optional<ShaderRecompiler::RecompileResult> compiled;
    const std::span<const std::uint32_t> code(Wave64Code);
    std::fill(Input, Input + InputWords, 0u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
    std::fill(Output, Output + OutputWords, 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input, (Threads * Inputs + SharedWords) * 4u);
    const auto output = BufferDescriptor(Output, Threads * Results * 4u);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {Threads, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    if (!compiled.has_value()) compiled = ShaderRecompiler::Recompile(request);
    device.Dispatch(*compiled, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    if (wait) device.WaitIdle();
}

AgcDriver::Graphics::LookupMemoCounts Counts() { return AgcDriver::Graphics::LookupMemoCounters(); }

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_LOOKUP_MEMO", "1");
    _putenv_s("APS5_VERIFY_PROOFS", "1");
    _putenv_s("APS5_NO_DISPATCH_RECIPE", "1");
#else
    setenv("APS5_LOOKUP_MEMO", "1", 1);
    setenv("APS5_VERIFY_PROOFS", "1", 1);
    setenv("APS5_NO_DISPATCH_RECIPE", "1", 1);
#endif
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto* block = static_cast<std::uint32_t*>(::operator new(2 * BlockBytes, std::align_val_t{BlockBytes}, std::nothrow));
        Require(block != nullptr, "lookup memo: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, 2 * BlockBytes, true, true);
        struct Release {
            std::uint32_t* block;
            ~Release() {
                GuestAllocations::Mutation().Remove(block);
                ::operator delete(block, std::align_val_t{BlockBytes});
            }
        } release{block};
        Input = block;
        Output = block + BlockBytes / 4;
        const auto results = [] { return std::vector<std::uint32_t>(Output, Output + OutputWords); };
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Dispatch(*device);
        const auto first = results();
        Dispatch(*device);
        const auto taken = Counts();
        if (taken.hits + taken.misses == 0) {
            std::puts("lookup memo: the dispatches did not reuse a template (no Revalidate): skipped");
            return VulkanTestSkipped;
        }
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Dispatch(*device, false);
        Dispatch(*device, false);
        device->WaitIdle();
        auto now = Counts();
        Require(now.hits == taken.hits + 1, "lookup memo: a repeat within the epoch was not answered by the memo");
        Require(results() == first, "lookup memo: a memo-proved dispatch wrote other results");
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Dispatch(*device, false);
        AgcDriver::Graphics::StorageTexture::BumpPendingSerial();
        auto before = Counts();
        Dispatch(*device, false);
        now = Counts();
        Require(now.hits == before.hits && now.misses == before.misses + 1, "lookup memo: a pending registry change was not seen");
        Dispatch(*device, false);
        device->WaitIdle();
        Require(Counts().hits == now.hits + 1, "lookup memo: the memo was not taken again after the registry change");
        Require(results() == first, "lookup memo: the results changed after the registry change");
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Dispatch(*device, false);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        before = Counts();
        Dispatch(*device);
        now = Counts();
        Require(now.hits == before.hits, "lookup memo: a new collect epoch was not seen");
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Dispatch(*device, false);
        {
            GuestAllocations::Mutation mutation;
        }
        before = Counts();
        Dispatch(*device);
        Require(Counts().hits == before.hits, "lookup memo: a registry mutation was not seen");
        Require(results() == first, "lookup memo: the results changed");
        std::printf("lookup memo: %llu hits, %llu misses\n", static_cast<unsigned long long>(Counts().hits), static_cast<unsigned long long>(Counts().misses));
        std::puts("lookup memo tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
