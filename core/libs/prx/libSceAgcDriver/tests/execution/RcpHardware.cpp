#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::array<std::array<std::uint32_t, 2>, 256> Measured{{
    {0x000000u, 0x3f800000u}, {0x002922u, 0x3f7fadd6u}, {0x00a1a9u, 0x3f7ebe44u}, {0x010c47u, 0x3f7debceu},
    {0x013df5u, 0x3f7d8a32u}, {0x01a87fu, 0x3f7cb9ddu}, {0x024f43u, 0x3f7b766fu}, {0x02d76du, 0x3f7a70beu},
    {0x031690u, 0x3f79f820u}, {0x033035u, 0x3f79c742u}, {0x03adabu, 0x3f78d946u}, {0x04435bu, 0x3f77bfa4u},
    {0x04f3a1u, 0x3f76772au}, {0x057a96u, 0x3f757df8u}, {0x05c6afu, 0x3f74f253u}, {0x06342au, 0x3f742a80u},
    {0x06bf6fu, 0x3f732e24u}, {0x07709au, 0x3f71f00au}, {0x07eb02u, 0x3f711627u}, {0x086273u, 0x3f704304u},
    {0x09362bu, 0x3f6ed04au}, {0x099950u, 0x3f6e2438u}, {0x09f75cu, 0x3f6d81e2u}, {0x0a097cu, 0x3f6d62b3u},
    {0x0ab7e3u, 0x3f6c3840u}, {0x0b1a69u, 0x3f6b90f0u}, {0x0b93a5u, 0x3f6ac454u}, {0x0becd7u, 0x3f6a2eadu},
    {0x0bf0b4u, 0x3f6a2836u}, {0x0c5c7fu, 0x3f697463u}, {0x0c9164u, 0x3f691c8au}, {0x0cb1e2u, 0x3f68e6b5u},
    {0x0d5f89u, 0x3f67c8a0u}, {0x0dd27au, 0x3f670cc6u}, {0x0e09e4u, 0x3f66b2a2u}, {0x0ead02u, 0x3f65aae2u},
    {0x0ed904u, 0x3f656421u}, {0x0f17a3u, 0x3f64ffbeu}, {0x0f21ddu, 0x3f64ef61u}, {0x0f2bcdu, 0x3f64df7du},
    {0x0f4205u, 0x3f64bbfeu}, {0x0f8808u, 0x3f644c6cu}, {0x0faddcu, 0x3f641050u}, {0x0fd630u, 0x3f63d05fu},
    {0x1012f0u, 0x3f637050u}, {0x108fe7u, 0x3f62abb4u}, {0x10a3d6u, 0x3f628c77u}, {0x1184b9u, 0x3f612e5au},
    {0x119a72u, 0x3f610cc2u}, {0x11e20bu, 0x3f609e4fu}, {0x123f59u, 0x3f600f00u}, {0x128b2fu, 0x3f5f9b0du},
    {0x12bd4au, 0x3f5f4eb3u}, {0x12f18du, 0x3f5eff48u}, {0x135d16u, 0x3f5e5c8du}, {0x13deefu, 0x3f5d994bu},
    {0x140ceeu, 0x3f5d5472u}, {0x14a0f9u, 0x3f5c77fdu}, {0x14f473u, 0x3f5bfc70u}, {0x15051au, 0x3f5be3dau},
    {0x153e7cu, 0x3f5b8f4fu}, {0x15b949u, 0x3f5adb3au}, {0x1600a3u, 0x3f5a7320u}, {0x1676ebu, 0x3f59c766u},
    {0x172abbu, 0x3f58c45au}, {0x1738f7u, 0x3f58aff3u}, {0x17a05au, 0x3f581c33u}, {0x17f5e8u, 0x3f57a288u},
    {0x1818e8u, 0x3f5770e9u}, {0x185f1au, 0x3f570da8u}, {0x18f135u, 0x3f564038u}, {0x194f2fu, 0x3f55bce2u},
    {0x1a61dbu, 0x3f54409cu}, {0x1a63e2u, 0x3f543dd2u}, {0x1af098u, 0x3f537d12u}, {0x1bd958u, 0x3f52413au},
    {0x1cb224u, 0x3f511e54u}, {0x1deb7au, 0x3f4f7f68u}, {0x1df9fdu, 0x3f4f6c59u}, {0x1e27a1u, 0x3f4f307du},
    {0x1e398fu, 0x3f4f1903u}, {0x1ef9d8u, 0x3f4e1e85u}, {0x1fb17cu, 0x3f4d317eu}, {0x1fec12u, 0x3f4ce652u},
    {0x202036u, 0x3f4ca39au}, {0x20b671u, 0x3f4be450u}, {0x211c70u, 0x3f4b633bu}, {0x21b455u, 0x3f4aa42eu},
    {0x2217beu, 0x3f4a27e7u}, {0x22c947u, 0x3f494b6eu}, {0x230d97u, 0x3f48f719u}, {0x23a75fu, 0x3f483a41u},
    {0x244f6fu, 0x3f476d74u}, {0x24ede6u, 0x3f46add8u}, {0x250d33u, 0x3f46882au}, {0x254b0cu, 0x3f463de2u},
    {0x263ba6u, 0x3f451ef3u}, {0x269e0du, 0x3f44aa89u}, {0x26a2c0u, 0x3f44a4fdu}, {0x26bb7du, 0x3f4487d0u},
    {0x26e875u, 0x3f4452ddu}, {0x2744e3u, 0x3f43e660u}, {0x281972u, 0x3f42eeaau}, {0x29b882u, 0x3f4111f2u},
    {0x2a3af4u, 0x3f407e00u}, {0x2a96fbu, 0x3f401628u}, {0x2afa95u, 0x3f3fa642u}, {0x2b0537u, 0x3f3f9a58u},
    {0x2baf6au, 0x3f3edc66u}, {0x2c7557u, 0x3f3e015au}, {0x2d1c9au, 0x3f3d49c5u}, {0x2d711au, 0x3f3ced8cu},
    {0x2e0531u, 0x3f3c4cc6u}, {0x2e4415u, 0x3f3c08d1u}, {0x2e8e7du, 0x3f3bb8aau}, {0x2eae05u, 0x3f3b96c8u},
    {0x2fa54eu, 0x3f3a8eaeu}, {0x301850u, 0x3f3a14d7u}, {0x30d16au, 0x3f39520au}, {0x31f5efu, 0x3f38216cu},
    {0x32c6ecu, 0x3f374a2du}, {0x33d0ceu, 0x3f363b28u}, {0x34b9b5u, 0x3f355051u}, {0x34c9ebu, 0x3f35400eu},
    {0x364b9du, 0x3f33c092u}, {0x36f675u, 0x3f3318bau}, {0x376cbfu, 0x3f32a53au}, {0x37dc76u, 0x3f3238afu},
    {0x387a34u, 0x3f31a04au}, {0x3898d1u, 0x3f3182d5u}, {0x392630u, 0x3f30fb4bu}, {0x398406u, 0x3f30a1c6u},
    {0x3aa8b5u, 0x3f2f8cd0u}, {0x3b1287u, 0x3f2f2983u}, {0x3b6186u, 0x3f2edfabu}, {0x3bbbe9u, 0x3f2e8b79u},
    {0x3bfb1eu, 0x3f2e50c8u}, {0x3cf38cu, 0x3f2d6b98u}, {0x3d9c17u, 0x3f2cd172u}, {0x3e7d1bu, 0x3f2c054du},
    {0x3e875fu, 0x3f2bfc08u}, {0x3f63afu, 0x3f2b360fu}, {0x3f98e2u, 0x3f2b0685u}, {0x3fa7d9u, 0x3f2af92au},
    {0x407b49u, 0x3f2a3d5au}, {0x4174afu, 0x3f2961e2u}, {0x427df9u, 0x3f287ad8u}, {0x43435cu, 0x3f27d089u},
    {0x437bcbu, 0x3f27a016u}, {0x44985eu, 0x3f26ad72u}, {0x451abdu, 0x3f263f34u}, {0x457d42u, 0x3f25ec44u},
    {0x472077u, 0x3f248ef6u}, {0x4724dau, 0x3f248b56u}, {0x47469au, 0x3f246f78u}, {0x482c9cu, 0x3f23b287u},
    {0x486824u, 0x3f2381e6u}, {0x48db40u, 0x3f232432u}, {0x499523u, 0x3f228dc2u}, {0x49b64au, 0x3f22730au},
    {0x49dea9u, 0x3f22528du}, {0x4a23d5u, 0x3f221b02u}, {0x4b14f9u, 0x3f215a86u}, {0x4c25fcu, 0x3f2082beu},
    {0x4cbd87u, 0x3f200befu}, {0x4cdd20u, 0x3f1ff340u}, {0x4dd422u, 0x3f1f334cu}, {0x4ef8aau, 0x3f1e5249u},
    {0x4f20f3u, 0x3f1e337eu}, {0x4f426du, 0x3f1e19f1u}, {0x5051c1u, 0x3f1d4c05u}, {0x506bf2u, 0x3f1d3841u},
    {0x507313u, 0x3f1d32e0u}, {0x519088u, 0x3f1c5cc0u}, {0x520935u, 0x3f1c02eau}, {0x52e6b4u, 0x3f1b5f10u},
    {0x536d35u, 0x3f1afc38u}, {0x54a6d8u, 0x3f1a17a2u}, {0x55c663u, 0x3f19485eu}, {0x56ed5bu, 0x3f187600u},
    {0x571242u, 0x3f185bd8u}, {0x5790f8u, 0x3f180249u}, {0x57ee05u, 0x3f17c0c8u}, {0x588a7eu, 0x3f175320u},
    {0x58d556u, 0x3f171ee4u}, {0x59a54au, 0x3f168e80u}, {0x59f999u, 0x3f165444u}, {0x5affb2u, 0x3f15a05bu},
    {0x5b855cu, 0x3f15453fu}, {0x5bd86du, 0x3f150cd9u}, {0x5c90a9u, 0x3f149059u}, {0x5cd546u, 0x3f146230u},
    {0x5d9dc9u, 0x3f13dbf0u}, {0x5e14d9u, 0x3f138caau}, {0x5e8766u, 0x3f1340b6u}, {0x5f41cau, 0x3f12c5c6u},
    {0x5f5572u, 0x3f12b8dcu}, {0x60fd9bu, 0x3f11a440u}, {0x616499u, 0x3f1161b4u}, {0x625d1cu, 0x3f10c218u},
    {0x62c33au, 0x3f1080e8u}, {0x63c0e3u, 0x3f0fdff7u}, {0x641547u, 0x3f0faabcu}, {0x64b8d7u, 0x3f0f43feu},
    {0x651327u, 0x3f0f0b83u}, {0x658cdau, 0x3f0ebfadu}, {0x65dc9fu, 0x3f0e8e23u}, {0x662b59u, 0x3f0e5d60u},
    {0x66d228u, 0x3f0df67eu}, {0x670f27u, 0x3f0dd104u}, {0x67e4c4u, 0x3f0d4e61u}, {0x6916adu, 0x3f0c94eeu},
    {0x6a50dfu, 0x3f0bd86cu}, {0x6a7a5au, 0x3f0bbfaeu}, {0x6b0a18u, 0x3f0b6a37u}, {0x6b0d54u, 0x3f0b684cu},
    {0x6b4013u, 0x3f0b4a3au}, {0x6b4cb2u, 0x3f0b42c1u}, {0x6bd361u, 0x3f0af338u}, {0x6bf46cu, 0x3f0adfc3u},
    {0x6cad4au, 0x3f0a734au}, {0x6cb93du, 0x3f0a6c4cu}, {0x6d76b0u, 0x3f09fdddu}, {0x6dc676u, 0x3f09cf92u},
    {0x6e36aau, 0x3f098ea8u}, {0x6f0367u, 0x3f0918d3u}, {0x6fa5cau, 0x3f08bbedu}, {0x70c958u, 0x3f08165eu},
    {0x71e6c2u, 0x3f0775ccu}, {0x721583u, 0x3f075ba3u}, {0x72e6ccu, 0x3f06e703u}, {0x72fdf2u, 0x3f06da29u},
    {0x73596du, 0x3f06a778u}, {0x7403e4u, 0x3f064966u}, {0x744916u, 0x3f06235cu}, {0x74c9dfu, 0x3f05dccau},
    {0x74e69au, 0x3f05cd16u}, {0x756dbau, 0x3f05836cu}, {0x7631a9u, 0x3f05192au}, {0x7731afu, 0x3f048f50u},
    {0x7774a2u, 0x3f046b72u}, {0x791a7cu, 0x3f038b32u}, {0x795e82u, 0x3f036751u}, {0x7aa2c1u, 0x3f02bd52u},
    {0x7c2684u, 0x3f01f444u}, {0x7c64ebu, 0x3f01d422u}, {0x7d2cafu, 0x3f016db2u}, {0x7d959fu, 0x3f013822u},
    {0x7e62aau, 0x3f00cffbu}, {0x7ebff2u, 0x3f00a0d0u}, {0x7eecf4u, 0x3f008a1au}, {0x7f1505u, 0x3f0075eau},
    {0x7f1b10u, 0x3f0072dfu}, {0x7f2614u, 0x3f006d53u}, {0x7ff486u, 0x3f0005beu}, {0x7fffffu, 0x3f000001u},
}};
constexpr std::array<std::uint32_t, 6> Exponents{1u, 60u, 127u, 128u, 200u, 252u};
constexpr std::uint32_t Cases = static_cast<std::uint32_t>(Measured.size() * Exponents.size() * 2u);
constexpr std::uint32_t Batches = Cases / Threads;
alignas(256) std::array<std::uint32_t, Threads> Input{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 9> Code{
    0x34020082, 0xe0301000, 0x80000201, 0xbf8c3f70, 0x7e065502, 0xe0701000, 0x80010301, 0xbf8c3f70,
    0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Sample(std::uint32_t index) {
    const auto& measured = Measured[index % Measured.size()];
    const auto exponent = Exponents[(index / Measured.size()) % Exponents.size()];
    const auto sign = (index / static_cast<std::uint32_t>(Measured.size() * Exponents.size())) << 31u;
    return sign | exponent << 23u | measured[0];
}

std::uint32_t Expected(std::uint32_t index) {
    const auto& measured = Measured[index % Measured.size()];
    const auto exponent = Exponents[(index / Measured.size()) % Exponents.size()];
    const auto sign = (index / static_cast<std::uint32_t>(Measured.size() * Exponents.size())) << 31u;
    return sign | ((measured[1] >> 23u) + 127u - exponent) << 23u | (measured[1] & 0x7fffffu);
}
}

int main() {
    try {
        if (std::getenv("APS5_EXACT_RCP") == nullptr) return VulkanTestSkipped;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint32_t> userData(8, 0u);
        const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
        const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const std::span<const std::uint32_t> code(Code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            device->Target(),
            {0, 0, 0, 128}
        };
        request.useCache = false;
        request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false};
        const auto result = ShaderRecompiler::Recompile(request);
        for (std::uint32_t batch = 0; batch < Batches; ++batch) {
            for (std::uint32_t tid = 0; tid < Threads; ++tid) Input[tid] = Sample(batch * Threads + tid);
            Output.fill(0xdeadbeefu);
            device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
            device->WaitIdle();
            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                const auto wanted = Expected(batch * Threads + tid);
                Require(Output[tid] == wanted, std::string("v_rcp_f32 of ") + Hex(Input[tid]) + " is " + Hex(Output[tid]) + ", expected the RDNA 2 value " + Hex(wanted));
            }
        }
        std::puts("rcp hardware tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
