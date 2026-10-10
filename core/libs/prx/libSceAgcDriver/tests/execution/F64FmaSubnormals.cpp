#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 8;
constexpr std::uint32_t Results = 2;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 12> Code{
    0x34020085, 0x34060083, 0xe0381000, 0x80000401, 0xe0381010, 0x80000801, 0xbf8c3f70, 0xd54c000c,
    0x04220d04, 0xe0741000, 0x80010c03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> FmasCode{
    0x34020085, 0x34060083, 0xe0381000, 0x80000401, 0xe0381010, 0x80000801, 0xbf8c3f70, 0xbeea03c1,
    0xd570000c, 0x04220d04, 0xe0741000, 0x80010c03, 0xbf810000,
};

constexpr std::uint64_t Rows[Threads][4] = {
    {0xaf909f1570e40fb2ull, 0x10697a78dd1df49bull, 0x8000000000000000ull, 0x800d3be6095b3463ull},
    {0x1e50511fd30f5d67ull, 0x24c92a1b12df5283ull, 0x8329a9b24976c10bull, 0x0006cc7690f01937ull},
    {0x98cd9ffcf3ed94edull, 0x2a51579178c2f7a9ull, 0x03300e10060c0a21ull, 0x000826b2393bc091ull},
    {0x2c67bd24a078a9e3ull, 0x169c3293f5b93146ull, 0x8314eb0558ce14d3ull, 0x8002b781181390c7ull},
    {0xbcca08e86f6c243dull, 0x0647cf3a0cd21b23ull, 0x03235effea3a0f40ull, 0x00053433dc65a9cbull},
    {0x97cb9cf8c8a2fbb6ull, 0xab2cb31ef6264b43ull, 0x8308c3ea76c1ebe5ull, 0x00014c7d2a0da8cbull},
    {0x0946591c0410303cull, 0xba059dbfbeb6d4adull, 0x80099e28c52b6c3dull, 0x835e314afa5d1a07ull},
    {0xcee37832c306ee31ull, 0x80017e95cd7d98dfull, 0x8c168438ac98afa4ull, 0x0ecd18c0555dceefull},
    {0x800e8b91a326e01dull, 0xd020fb9f53a2df2cull, 0x8cb514f5ff6d74b0ull, 0x103ee09fedab559aull},
    {0x401fac71cda29715ull, 0x8008000020000000ull, 0x002fac724c545e4full, 0x0000000000000007ull},
    {0x2e4c000000000000ull, 0x11aeb6db849700ffull, 0x800d70000a021070ull, 0x8000000000000000ull},
    {0x41d0000000000008ull, 0x0000000040000002ull, 0x80761e2b2ef2fce7ull, 0x0084f0ea6986819dull},
    {0xfc21916e447cf802ull, 0x8004eec859174483ull, 0x3be2fe907aa80074ull, 0x3c26da145be5eaacull},
    {0x35d175fd70782c6cull, 0x0c6033e7239c8fe0ull, 0x0005cb65548dabd1ull, 0x0241aea1c6a7627dull},
    {0xb4fb1dc3f1ce6211ull, 0x8b80000000000100ull, 0x800000000b9b5f36ull, 0x008b1dc3f1b72d04ull},
    {0x0008000000000000ull, 0x8004000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull},
    {0x0008000000000000ull, 0x0004000000000000ull, 0x0170000000000000ull, 0x0170000000000000ull},
    {0x4004000000000000ull, 0x0000000000000001ull, 0x0000000000000000ull, 0x0000000000000002ull},
    {0x3fe0000000000000ull, 0x0000000000000001ull, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x3fe0000000000001ull, 0x0000000000000001ull, 0x0000000000000000ull, 0x0000000000000001ull},
    {0x3fefffffffffffffull, 0x0010000000000000ull, 0x0000000000000000ull, 0x0010000000000000ull},
    {0x1e60000000000000ull, 0x1e60000000000000ull, 0x8000000000000001ull, 0x0000000000000000ull},
    {0x3ff0000000000001ull, 0x0018000000000001ull, 0x8010000000000000ull, 0x0008000000000003ull},
    {0x3ff8000000000000ull, 0x4000000000000000ull, 0x3fd0000000000000ull, 0x400a000000000000ull},
    {0x3ff0000000000001ull, 0x3ff0000000000001ull, 0xbff0000000000000ull, 0x3cc0000000000000ull},
    {0xffefffffffffffffull, 0x3ff0000000000000ull, 0x000fffffffffffffull, 0xffefffffffffffffull},
    {0x5fefffffffffffffull, 0x5fefffffffffffffull, 0x0000000000000000ull, 0x7feffffffffffffeull},
    {0x000fffffffffffffull, 0x4000000000000000ull, 0x8000000000000001ull, 0x001ffffffffffffdull},
    {0x8000000000000003ull, 0x7fe8000000000000ull, 0x3ff0000000000000ull, 0x3fefffffffffffeeull},
    {0x1a75555555555555ull, 0x000aaaaaaaaaaaabull, 0x0000000000000000ull, 0x0000000000000000ull},
    {0x0173000000000000ull, 0x3e17000000000000ull, 0x80001b5000000000ull, 0x0000000000000000ull},
    {0xa000000000000001ull, 0x1fffffffffffffffull, 0x0010000000000000ull, 0x8000000000000000ull},
};

constexpr std::uint64_t FmasRows[][4] = {
    {0xcee37832c306ee31ull, 0x80017e95cd7d98dfull, 0x8c168438ac98afa4ull, 0x06cd18c0555dceefull},
    {0x800e8b91a326e01dull, 0xd020fb9f53a2df2cull, 0x8cb514f5ff6d74b0ull, 0x083ee09fedab559aull},
    {0xfc21916e447cf802ull, 0x8004eec859174483ull, 0x3be2fe907aa80074ull, 0x3426da145be5eaacull},
    {0x6456b4d74e778407ull, 0x0000000000000112ull, 0x20280ac49e7f906cull, 0x19b84d8e7e014d9full},
    {0x3b87a9e17b60d8beull, 0x0c5959dcff86b48aull, 0x026239792b5377a2ull, 0x0004afcb04199627ull},
    {0xaf25fa06e8714b4bull, 0x18d2b6bb683e8122ull, 0x02a53f820f7c716bull, 0x800cda229eb9a09dull},
    {0xb99c5fe31c736fbbull, 0x8e5264264dcfc027ull, 0x8022aa8efefbe52cull, 0x0008275eae4b3d41ull},
    {0xc6fdfbc0001ad5dfull, 0x00ffcab17e745bd1ull, 0x82e0e3a0b85211a8ull, 0x800ee4e6bd8a79e5ull},
    {0xc1d1a6cfa76ad3b9ull, 0x862ba38c1492703full, 0x017130645c1851d5ull, 0x000f3ef62c247005ull},
    {0xada41786a4b824bcull, 0x1a554baa4a83fe63ull, 0x0134fb8e603406d4ull, 0x800d5ef26b6f7a29ull},
    {0xb81b70578b6c14b9ull, 0x8fcdd6775e710cc1ull, 0x829c666c28b85110ull, 0x0006656d3ea3b537ull},
    {0x2c256baa0ef1c91bull, 0x9baa7695febd5f53ull, 0x82d3dfc5a191618bull, 0x800236db72e217c3ull},
    {0x355013dab21e2539ull, 0x1296e6732f928ca7ull, 0x82f4e48c5e9c126cull, 0x0005c0b776cad66bull},
    {0xc458817a2d2ea918ull, 0x03a473fd45d66f08ull, 0x03240e0c4f7cfcc4ull, 0x800fa9bfa078e4f1ull},
    {0x33291df738c3f676ull, 0x146a0194f3933c51ull, 0x0120115306c4e7b2ull, 0x000028d32d6f3f07ull},
    {0xb1d797d703ad9014ull, 0x9613cb63b23f1b8bull, 0x0244b2cb24e6fb3aull, 0x00074c0e33ad5c9dull},
    {0x4656b4266997b7f2ull, 0x819765c0b9bfcad0ull, 0x03c9a52d2b9f1094ull, 0x80084cd678e92eefull},
    {0x4513191709caec8cull, 0x02d3b5ddc4ac6b05ull, 0x826384dfaf4b9e81ull, 0x0005e1b7f963c7dbull},
    {0xba6ec9ec81f6efb7ull, 0x8d785fa355446f4eull, 0x0279c121fe09c0aaull, 0x0005dcdd7f8c7cd3ull},
    {0xbe667b0282fcc03cull, 0x897ce8a400290c2cull, 0x0378775e473d7a50ull, 0x000513c44ed72187ull},
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

template <std::size_t Words, std::size_t Count>
void Run(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, Words>& program, const std::uint64_t (&rows)[Count][4]) {
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < Count; ++tid) {
        for (std::uint32_t operand = 0; operand < 3; ++operand) {
            Input[tid * Inputs + operand * 2] = static_cast<std::uint32_t>(rows[tid][operand]);
            Input[tid * Inputs + operand * 2 + 1] = static_cast<std::uint32_t>(rows[tid][operand] >> 32u);
        }
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(program);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

template <std::size_t Count>
void Check(const char* name, const std::uint64_t (&rows)[Count][4]) {
    for (std::uint32_t tid = 0; tid < Count; ++tid) {
        const std::uint64_t actual = Output[tid * Results] | (static_cast<std::uint64_t>(Output[tid * Results + 1]) << 32u);
        Require(actual == rows[tid][3], std::string(name) + ": lane " + std::to_string(tid) + " (" + Hex(rows[tid][0]) + ", " + Hex(rows[tid][1]) + ", " + Hex(rows[tid][2]) + ") is " + Hex(actual) + ", expected " + Hex(rows[tid][3]));
    }
}

const Testing::Case fmaSubnormals{"F64FmaSubnormals_Fma_MatchesReferenceBits", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, Code, Rows);
    Check("v_fma_f64", Rows);
}};

const Testing::Case divFmasSubnormals{"F64FmaSubnormals_DivFmasWithVccSet_MatchesReferenceBits", [] {
    auto& device = SharedVulkanTestDevice();
    Run(device, FmasCode, FmasRows);
    Check("v_div_fmas_f64 with VCC set", FmasRows);
}};

} // namespace
