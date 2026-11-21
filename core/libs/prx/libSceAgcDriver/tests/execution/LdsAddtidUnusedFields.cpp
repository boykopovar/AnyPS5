#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Checked = 11;
alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 185> Wave32Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0x7e160280u, 0x7e180280u,
    0x7e1a0280u, 0x7e1c0280u, 0x7e1e0280u, 0x7e200280u, 0x7e220280u, 0x7e240280u, 0x7e260280u, 0x7e280280u,
    0x7e2a0280u, 0x7e2c0280u, 0x7e2e0280u, 0x7e300280u, 0x7e320280u, 0x34640082u, 0x4a686480u, 0x386a68ffu,
    0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000080u, 0x386a68ffu, 0xa0000000u, 0xd8340000u,
    0x00003534u, 0x4a6864ffu, 0x00000100u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu,
    0x00000180u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000200u, 0x386a68ffu,
    0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000280u, 0x386a68ffu, 0xa0000000u, 0xd8340000u,
    0x00003534u, 0x4a6864ffu, 0x00000300u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu,
    0x00000380u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000400u, 0x386a68ffu,
    0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000480u, 0x386a68ffu, 0xa0000000u, 0xd8340000u,
    0x00003534u, 0x4a6864ffu, 0x00000500u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu,
    0x00000580u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000600u, 0x386a68ffu,
    0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000680u, 0x386a68ffu, 0xa0000000u, 0xd8340000u,
    0x00003534u, 0x4a6864ffu, 0x00000700u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu,
    0x00000780u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a5064ffu, 0x00000300u, 0x4a5264ffu,
    0x00000380u, 0x385400ffu, 0xd0000000u, 0x385600f5u, 0x385800ffu, 0xe0000000u, 0xbf8cc07fu, 0xbefc03ffu,
    0x00000100u, 0xdac40000u, 0x0a050406u, 0xdac40000u, 0x0b2a2928u, 0xdac40040u, 0x0c072829u, 0xbf8cc07fu,
    0xbefc03ffu, 0x00000200u, 0xdac00000u, 0x002a2b28u, 0xbf8cc07fu, 0xbefc03ffu, 0x00000500u, 0xdac00000u,
    0x002b2c00u, 0xbf8cc07fu, 0xbefc03ffu, 0x00000600u, 0xdac00000u, 0x00042a29u, 0xbf8cc07fu, 0x4a6c64ffu,
    0x00000200u, 0xd8d80000u, 0x0d000036u, 0xd8d80000u, 0x0e000028u, 0xd8d80000u, 0x0f000029u, 0x4a6c64ffu,
    0x00000500u, 0xd8d80000u, 0x10000036u, 0xd8d80000u, 0x11000032u, 0x4a6c64ffu, 0x00000600u, 0xd8d80000u,
    0x12000036u, 0xbf8cc07fu, 0xbefc03ffu, 0x00000600u, 0xdac40000u, 0x13000000u, 0xbf8cc07fu, 0x7e28027cu,
    0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 137> Wave64Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e140280u, 0x7e160280u, 0x7e180280u,
    0x7e1a0280u, 0x7e1c0280u, 0x7e1e0280u, 0x7e200280u, 0x7e220280u, 0x7e240280u, 0x7e260280u, 0x7e280280u,
    0x7e2a0280u, 0x7e2c0280u, 0x7e2e0280u, 0x7e300280u, 0x7e320280u, 0x34640082u, 0x4a686480u, 0x386a68ffu,
    0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000100u, 0x386a68ffu, 0xa0000000u, 0xd8340000u,
    0x00003534u, 0x4a6864ffu, 0x00000200u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu,
    0x00000300u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000400u, 0x386a68ffu,
    0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu, 0x00000500u, 0x386a68ffu, 0xa0000000u, 0xd8340000u,
    0x00003534u, 0x4a6864ffu, 0x00000600u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a6864ffu,
    0x00000700u, 0x386a68ffu, 0xa0000000u, 0xd8340000u, 0x00003534u, 0x4a5064ffu, 0x00000300u, 0x4a5264ffu,
    0x00000380u, 0x385400ffu, 0xd0000000u, 0x385600f5u, 0x385800ffu, 0xe0000000u, 0xbf8cc07fu, 0xbefc03ffu,
    0x00000100u, 0xdac40000u, 0x0a050406u, 0xdac40000u, 0x0b2a2928u, 0xdac40040u, 0x0c072829u, 0xbf8cc07fu,
    0xbefc03ffu, 0x00000200u, 0xdac00000u, 0x002a2b28u, 0xbf8cc07fu, 0xbefc03ffu, 0x00000500u, 0xdac00000u,
    0x002b2c00u, 0xbf8cc07fu, 0xbefc03ffu, 0x00000600u, 0xdac00000u, 0x00042a29u, 0xbf8cc07fu, 0x4a6c64ffu,
    0x00000200u, 0xd8d80000u, 0x0d000036u, 0xd8d80000u, 0x0e000028u, 0xd8d80000u, 0x0f000029u, 0x4a6c64ffu,
    0x00000500u, 0xd8d80000u, 0x10000036u, 0xd8d80000u, 0x11000032u, 0x4a6c64ffu, 0x00000600u, 0xd8d80000u,
    0x12000036u, 0xbf8cc07fu, 0xbefc03ffu, 0x00000600u, 0xdac40000u, 0x13000000u, 0xbf8cc07fu, 0x7e28027cu,
    0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u,
    0xbf810000u,
};

constexpr std::uint32_t Rows[MaxThreads][Inputs] = {
    {0x3c6da5d7u, 0x4da4f9fcu, 0x1a6916c7u, 0xb8a1abcdu},
    {0x656412a9u, 0x7a97c643u, 0x27ac435au, 0x1710cf53u},
    {0x11072231u, 0x0512bd13u, 0x66ceab36u, 0x8ca59966u},
    {0xeaff1a09u, 0x4a14876au, 0xccea71ffu, 0xfd724452u},
    {0xc3e1b258u, 0x0f1099c6u, 0x38d048ecu, 0x8534f457u},
    {0x8963dc6eu, 0x5c3902b3u, 0x46d4ac7au, 0xc79d6793u},
    {0x2c33be0au, 0xd3addccbu, 0x1b2ed40eu, 0x43000de0u},
    {0x36e2f24bu, 0xf165c8ceu, 0xed6f0b09u, 0x06905269u},
    {0xd4341aadu, 0xa4042bb3u, 0xce80c4b0u, 0x42a00403u},
    {0xccea2645u, 0x459142deu, 0x3184ff27u, 0x2a318785u},
    {0x4f5253a0u, 0x4a25e466u, 0xa0817910u, 0xde08caa1u},
    {0xbb5d7385u, 0xf5ff0c03u, 0xdaca3c06u, 0xd93936e1u},
    {0xe14b0190u, 0x5f552773u, 0x16332acau, 0xd8441b56u},
    {0x9b191bf4u, 0x56600224u, 0xabf4a07cu, 0x634f806fu},
    {0x81862fc9u, 0x3fb62d2cu, 0x2d83a823u, 0x3f508249u},
    {0x793d0e45u, 0x47adec26u, 0x16df6486u, 0xf1cfd992u},
    {0xef412ed6u, 0xd160c5d0u, 0xdd905ecfu, 0xf1347e0cu},
    {0x8c320f89u, 0xd7288ff6u, 0x4cdce7a6u, 0x01d89a02u},
    {0xe8abb93fu, 0x4abcb06au, 0x9286a175u, 0xb474c7e8u},
    {0xe1df8af9u, 0x4fcfa583u, 0xd9196adau, 0xc3e4a892u},
    {0x8224b122u, 0x31f3b923u, 0x69f85e31u, 0x6c79a3deu},
    {0x995253fdu, 0x49c7b59bu, 0x6e58d5cau, 0x738d243au},
    {0x294c4ea3u, 0x3bb4a570u, 0x4e1bcb38u, 0x4278c261u},
    {0xd0060cc5u, 0xcc21ce88u, 0x0b11ad28u, 0x14c15c91u},
    {0x0bdbc23au, 0x7671863cu, 0xa05885acu, 0xff5a52f1u},
    {0x47ca7883u, 0x84d4cd1fu, 0x88dcf943u, 0xa5e333cbu},
    {0x78a330a1u, 0xb36cc9aau, 0x57c49391u, 0x2522d538u},
    {0xff4dab10u, 0xac7cc4a4u, 0x32111ac1u, 0x11021c9eu},
    {0x69ace913u, 0xe9dd38b8u, 0x33e238b4u, 0xa2909cb6u},
    {0xa1f65507u, 0x70ef55b1u, 0x46bbe9e8u, 0x2f0733c8u},
    {0x5b17b966u, 0x6f98bca3u, 0xbf37a2beu, 0x96b98b5fu},
    {0x52175b7au, 0xa26a25c8u, 0x8efbc170u, 0x32decd6bu},
    {0xe78131c1u, 0x52d32377u, 0x19d9c9ccu, 0xd6e4a515u},
    {0x0fc5b043u, 0xb54a2302u, 0x3a973000u, 0x4708d989u},
    {0xc3e1ac3bu, 0x950b16ffu, 0x9d8cf4d4u, 0xdcb285f8u},
    {0x3cc75f3eu, 0x1f44ebd1u, 0x54c0ce68u, 0xef40af2eu},
    {0x2d733dcdu, 0x4a7a0305u, 0x758240dfu, 0x0692b534u},
    {0x0af5ac87u, 0x5b69dc23u, 0xb281b888u, 0x1525f363u},
    {0xe55b85ddu, 0xf6e7d078u, 0xf469aef8u, 0x4922b9ccu},
    {0xbc20f626u, 0xacdac615u, 0xf5b9e1f5u, 0x53be4721u},
    {0x04a7f007u, 0x52a3b181u, 0x49fbac36u, 0x52595dafu},
    {0xf74c3816u, 0x272515cdu, 0xc676176au, 0xa6e46653u},
    {0x6911c9ddu, 0xdc82f252u, 0xf17ca82cu, 0xde97faf0u},
    {0x9ed9c621u, 0xae17584au, 0xd163b764u, 0x13e7d611u},
    {0x4b1cef39u, 0x9e2387a5u, 0x31009624u, 0xe4e2aafdu},
    {0x71b34e47u, 0x4ac034cfu, 0x22ed9387u, 0x40031ad6u},
    {0x61b2cebau, 0x994b9717u, 0xf6396ae3u, 0x28adf9c6u},
    {0x54d08ce6u, 0x92b607d5u, 0x0267ce8cu, 0x5d02db43u},
    {0x0b77d43au, 0x746ccfcdu, 0x2b68beefu, 0x5d7d255fu},
    {0xc8a38e7bu, 0xcde9d231u, 0x5cdb7f4cu, 0xff478895u},
    {0x4a53583bu, 0x924aecbeu, 0x18dbb0c1u, 0x7077b81du},
    {0xfb53e13du, 0x350d775du, 0x6c8826ecu, 0xeaa1b295u},
    {0x353a09cfu, 0x1d1615adu, 0x0f30e005u, 0x0fecf10eu},
    {0x0e263730u, 0xbcb3fd50u, 0x2b2df98du, 0x9874f882u},
    {0xad448abdu, 0xf87a7976u, 0x264ed787u, 0x9b44baf5u},
    {0x0a77ec0cu, 0x8bd5bb71u, 0x7d9c649au, 0x952989c1u},
    {0x3fc24ec0u, 0x52496e1eu, 0xff651b90u, 0x091b5ffbu},
    {0x1f4d4cc5u, 0xd5b8c255u, 0x877c606fu, 0x4afbfae4u},
    {0xc6173d94u, 0xff353728u, 0x68c946b0u, 0xa6d00e34u},
    {0xfcd71d42u, 0x33445533u, 0x7a4e9ba3u, 0x33a760e1u},
    {0x3deaadddu, 0x70562073u, 0x691e8e3bu, 0x7defb12bu},
    {0x0970425bu, 0x38146a2fu, 0x6bd5231fu, 0x71895aa3u},
    {0x3fa3549bu, 0xa5be57d9u, 0xe3a9312cu, 0x6d86b88du},
    {0xd4bf8115u, 0x37396957u, 0x7fa2ebbcu, 0x300dc4c2u},
};

constexpr const char* Names[Checked] = {
    "ds_read_addtid_b32 m0 0x100, data0 v4, data1 v5, addr v6",
    "ds_read_addtid_b32 m0 0x100, data0 v41, data1 v42, addr v40",
    "ds_read_addtid_b32 m0 0x100 offset 0x40, data0 v40, data1 v7, addr v41",
    "ds_write_addtid_b32 m0 0x200, data0 v43, data1 v42, addr v40",
    "LDS at v40 after the writes",
    "LDS at v41 after the writes",
    "ds_write_addtid_b32 m0 0x500, data0 v44, data1 v43, addr v0",
    "LDS at 4 * lane after the writes",
    "ds_write_addtid_b32 m0 0x600, data0 v42, data1 v4, addr v41",
    "ds_read_addtid_b32 m0 0x600, all fields v0",
    "m0",
};

std::uint32_t Expected(std::uint32_t lane, std::uint32_t index) {
    const std::array<std::uint32_t, Checked> values{
        0xa0000100u + lane * 4u, 0xa0000100u + lane * 4u, 0xa0000140u + lane * 4u, 0xc0000000u | lane, 0xa0000300u + lane * 4u, 0xa0000380u + lane * 4u,
        0xe0000000u | lane, 0xa0000000u + lane * 4u, 0xd0000000u | lane, 0xd0000000u | lane, 0x600u,
    };
    return values[index];
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const char* run) {
    for (std::uint32_t tid = 0; tid < MaxThreads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 2048u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        for (std::uint32_t index = 0; index < Checked; ++index) {
            const auto actual = Output[lane * Results + index];
            const auto expected = Expected(lane, index);
            Require(actual == expected, std::string("lds addtid unused fields ") + run + ": lane " + std::to_string(lane) + " " + Names[index] + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, the device's subgroups are narrower than a wave (%u lanes)\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        Run(*device, Wave32Code, 32, device->Target(), "wave32");
        Run(*device, Wave64Code, 64, device->Target(), "wave64");
        Run(*device, Wave64Code, 64, device->ComputeTarget(32), "wave64 split");
        std::puts("lds addtid unused field tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
