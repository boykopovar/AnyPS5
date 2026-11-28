#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Upper = 0xabcd0000u;
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};
alignas(256) constexpr std::array<std::uint32_t, 23> Code{
    0x34020084, 0x34060084, 0xe0381000, 0x80000401,
    0x7e1402ff, Upper, 0x7e1602ff, Upper, 0x7e1802ff, Upper,
    0xbf8c3f70,
    0xcc21000a, 0x041a0b04,
    0xcc21400b, 0x1c1a0b04,
    0xcc21480c, 0x0c1a0b04,
    0x7e1a0307,
    0xcc22700d, 0x141a0b04,
    0xe0781000, 0x80010a03, 0xbf810000,
};

struct Pinned {
    std::array<std::uint32_t, 4> words;
    std::array<std::array<std::uint32_t, 4>, 4> expected;
};

constexpr std::array<Pinned, 16> PinnedRows{{
    {{0x43176f85u, 0x01a60200u, 0xb6080054u, 0x609782b4u}, {{{0x8000u, 0x0000u, 0x0000u, 0xb608u}, {0x8000u, 0x0000u, 0x0000u, 0xb608u}, {0x8000u, 0x0000u, 0x0000u, 0xb608u}, {0x8000u, 0x0000u, 0x0000u, 0xb608u}}}},
    {{0x4706e4e4u, 0x037901ffu, 0x33c00000u, 0x800b83ffu}, {{{0x0000u, 0x0000u, 0x0000u, 0x33c0u}, {0x0000u, 0x0000u, 0x0000u, 0x33c0u}, {0x0000u, 0x8000u, 0x0000u, 0x33c0u}, {0x0000u, 0x0000u, 0x0000u, 0x33c0u}}}},
    {{0xb61ba7a8u, 0x34e52be8u, 0x0400813eu, 0xb32f7bd3u}, {{{0x8000u, 0x9791u, 0x8000u, 0x0000u}, {0x8000u, 0x9791u, 0x8000u, 0x0000u}, {0x8000u, 0x9792u, 0x8000u, 0x0000u}, {0x8000u, 0x9791u, 0x8000u, 0x0000u}}}},
    {{0x36cd19e1u, 0x37cb94e7u, 0x841d0000u, 0x7f800000u}, {{{0x0000u, 0x8000u, 0x0000u, 0x8000u}, {0x0000u, 0x8000u, 0x0000u, 0x8000u}, {0x0000u, 0x8000u, 0x0000u, 0x8000u}, {0x0000u, 0x8000u, 0x0000u, 0x8000u}}}},
    {{0xbc070395u, 0x3769e315u, 0x80000000u, 0x9326f88eu}, {{{0x8000u, 0x0000u, 0x8000u, 0x9bd1u}, {0x8000u, 0x0000u, 0x8000u, 0x9bd0u}, {0x8000u, 0x8000u, 0x8000u, 0x9bd1u}, {0x8000u, 0x0000u, 0x8000u, 0x9bd0u}}}},
    {{0x3b572724u, 0x3a3e3635u, 0xb5a6039du, 0x80018472u}, {{{0x0000u, 0x218au, 0x1174u, 0xb59cu}, {0x0000u, 0x218bu, 0x1175u, 0xb59bu}, {0x0000u, 0x218au, 0x1174u, 0xb59cu}, {0x0000u, 0x218au, 0x1174u, 0xb59bu}}}},
    {{0x3b49935bu, 0xb7b92796u, 0x00000000u, 0x8001aae9u}, {{{0x8000u, 0x8000u, 0x8000u, 0x9615u}, {0x8000u, 0x8000u, 0x8000u, 0x9614u}, {0x8000u, 0x8000u, 0x8000u, 0x9615u}, {0x8000u, 0x8000u, 0x8000u, 0x9614u}}}},
    {{0xb644e2adu, 0x356a04feu, 0x83c50200u, 0xb36fa591u}, {{{0x8000u, 0xac2au, 0x8000u, 0x8000u}, {0x8000u, 0xac2au, 0x8000u, 0x8000u}, {0x8000u, 0xac2bu, 0x8000u, 0x8000u}, {0x8000u, 0xac2au, 0x8000u, 0x8000u}}}},
    {{0x3f183315u, 0xb6d48afdu, 0x80000000u, 0xa9fd0200u}, {{{0x8000u, 0x8000u, 0x8000u, 0xb40fu}, {0x8000u, 0x8000u, 0x8000u, 0xb40fu}, {0x8000u, 0x8000u, 0x8000u, 0xb410u}, {0x8000u, 0x8000u, 0x8000u, 0xb40fu}}}},
    {{0xbfa21460u, 0x351d8fe8u, 0x00000000u, 0xffc12345u}, {{{0x8000u, 0x8000u, 0x8000u, 0xb67au}, {0x8000u, 0x8000u, 0x8000u, 0xb679u}, {0x8000u, 0x8000u, 0x8000u, 0xb67au}, {0x8000u, 0x8000u, 0x8000u, 0xb679u}}}},
    {{0xbcf38049u, 0xb790da19u, 0x00000000u, 0xb47e50fdu}, {{{0x0000u, 0x0000u, 0x0000u, 0x2331u}, {0x0000u, 0x0000u, 0x0000u, 0x2332u}, {0x0000u, 0x0000u, 0x0000u, 0x2331u}, {0x0000u, 0x0000u, 0x0000u, 0x2331u}}}},
    {{0x401986fbu, 0x35a82f08u, 0x80000000u, 0x33800000u}, {{{0x0000u, 0x8000u, 0x0000u, 0x3ac9u}, {0x0000u, 0x8000u, 0x0000u, 0x3ac9u}, {0x0000u, 0x8000u, 0x0000u, 0x3ac8u}, {0x0000u, 0x8000u, 0x0000u, 0x3ac8u}}}},
    {{0x39b9857bu, 0xb8512d61u, 0x357903e5u, 0xc33b0499u}, {{{0x0000u, 0x8000u, 0x8000u, 0x3578u}, {0x0000u, 0x8000u, 0x8000u, 0x3579u}, {0x0000u, 0x8000u, 0x8000u, 0x3578u}, {0x0000u, 0x8000u, 0x8000u, 0x3578u}}}},
    {{0xbe0f9019u, 0xb6b92981u, 0x80000000u, 0x38dca7d4u}, {{{0x0000u, 0x8000u, 0x0000u, 0x2b8au}, {0x0000u, 0x8000u, 0x0000u, 0x2b8bu}, {0x0000u, 0x8000u, 0x0000u, 0x2b8au}, {0x0000u, 0x8000u, 0x0000u, 0x2b8au}}}},
    {{0x4044851bu, 0xb5c6abbau, 0x00000000u, 0x7fa00000u}, {{{0x8000u, 0x0000u, 0x8000u, 0xbc6fu}, {0x8000u, 0x0000u, 0x8000u, 0xbc6eu}, {0x8000u, 0x0000u, 0x8000u, 0xbc6fu}, {0x8000u, 0x0000u, 0x8000u, 0xbc6eu}}}},
    {{0xbd3a1428u, 0x37b00535u, 0x00000000u, 0x83bffa59u}, {{{0x8000u, 0x0000u, 0x8000u, 0xa596u}, {0x8000u, 0x0000u, 0x8000u, 0xa596u}, {0x8000u, 0x0000u, 0x8000u, 0xa597u}, {0x8000u, 0x0000u, 0x8000u, 0xa596u}}}},
}};

constexpr std::array<std::array<std::uint32_t, 4>, 240> MeasuredRows{{
    {0x8b2e82cfu, 0xc8a20401u, 0xb83085c6u, 0x35e8955du}, {0x3e205feeu, 0xba2cd14au, 0x00000000u, 0xb76b6e93u}, {0xc358175cu, 0x3350a290u, 0x5b682a06u, 0x127904a9u},
    {0x140081e6u, 0x02000002u, 0x1400869eu, 0x38800000u}, {0xbb02f5d2u, 0x47800000u, 0x47800000u, 0xc19fc8eeu}, {0x8400328du, 0x38db00d8u, 0x3a0e0294u, 0x807fffffu},
    {0xb3095d2au, 0x477ff000u, 0x01518000u, 0x852e01dau}, {0x378a91c6u, 0x33c00000u, 0x93ec811fu, 0x477ff000u}, {0xbb61ca19u, 0xb7b35110u, 0x00000000u, 0xf3d03bffu},
    {0x70bd8193u, 0x7f800001u, 0xbbdbf16bu, 0x381260e5u}, {0xba99161cu, 0xbccb7143u, 0x80000000u, 0x387ff000u}, {0x029c7806u, 0x87b049b1u, 0x0001b15bu, 0x03490002u},
    {0x02d7fbffu, 0x85108003u, 0x39981d36u, 0x7fa00000u}, {0x3739cb82u, 0xd2803c01u, 0x70e5c908u, 0x05fc1400u}, {0x68ea4050u, 0x806809dbu, 0xbe555c0au, 0x3edcf3b6u},
    {0x346dda42u, 0x7bff07ffu, 0xb8268de1u, 0xbfd8d432u}, {0xb4534788u, 0x3f800000u, 0xb43ed80cu, 0x3c01841cu}, {0x387fffffu, 0xbb82d0fbu, 0xb742e1f7u, 0x86ec0002u},
    {0x36837035u, 0x61f55314u, 0xbd07bda4u, 0x823285dcu}, {0x330e1d75u, 0x01ff0000u, 0x2fcaea15u, 0x7fc00000u}, {0x33c934d6u, 0x08080127u, 0x0b2e01ffu, 0x8386873cu},
    {0x3a11a42fu, 0x7fc00000u, 0x477ff000u, 0xb66a9787u}, {0xb9b865bdu, 0x3de709edu, 0xb86945b9u, 0x39eb6124u}, {0xc3ca9fbfu, 0xd37b3800u, 0xb9ef833fu, 0xaf8863c3u},
    {0x3c014000u, 0x38000000u, 0xbf800000u, 0x00000001u}, {0xbdc03038u, 0xb8f4f877u, 0x00000000u, 0x1243006au}, {0x3c289314u, 0xb70587aau, 0x80000000u, 0xb7cb1db6u},
    {0x00007c00u, 0xb3948729u, 0xb1c3b126u, 0x00800001u}, {0x807200a9u, 0x3580a2d4u, 0x2ba6e875u, 0x7c01892bu}, {0x34fd70a7u, 0x2ff8d9f8u, 0x32e7f13cu, 0x3c010000u},
    {0xf91bbc00u, 0xc3cc1733u, 0x33c00000u, 0x852883ffu}, {0x32f842f7u, 0x3bcd7099u, 0x83fc03b8u, 0x81037c01u}, {0x7f800001u, 0x786883e9u, 0x03c80001u, 0x379bf296u},
    {0x00d77378u, 0xb63cfb6bu, 0xb3c21ad4u, 0x8051d314u}, {0xffc12345u, 0x81be532bu, 0x387ff000u, 0x058201ffu}, {0x47800000u, 0x04763bffu, 0x020002abu, 0xb527fe4fu},
    {0xa05607ffu, 0x36670488u, 0x35b9c693u, 0xebff0099u}, {0x84008387u, 0xb438c030u, 0x58e73120u, 0x00010400u}, {0xb327bb14u, 0x41b6d249u, 0x048d8694u, 0xc1c21763u},
    {0x477ff000u, 0xb7f6c40cu, 0x331fb280u, 0x7f800000u}, {0xbf194bb2u, 0xb4f617d8u, 0x00000000u, 0x337014fau}, {0x3fc37511u, 0x34510925u, 0x80000000u, 0x402c428au},
    {0xc774deadu, 0xd28a0002u, 0xb9143c45u, 0x7fa00000u}, {0xb3c4f81fu, 0x855982d0u, 0x387fe000u, 0xafdf9372u}, {0x40461a6fu, 0xb22e57d0u, 0x80000000u, 0x840a0822u},
    {0x84d4044eu, 0x387fe000u, 0x339142c6u, 0xbe8347cau}, {0x7fc00000u, 0xb2a7be50u, 0x3477a9f1u, 0xba4c825au}, {0x387ff000u, 0x8400dbd6u, 0x7c01d228u, 0x3c01807au},
    {0xc62d47f2u, 0xfbff2000u, 0x3ba47764u, 0xb3800000u}, {0xb9b1a3dcu, 0x3dac777du, 0x80000000u, 0x3f0635beu}, {0x81864bb7u, 0x6374c8a1u, 0xb8071789u, 0x348d87a1u},
    {0xbee9848eu, 0x3624e3c5u, 0x80000000u, 0x39d1c92bu}, {0xb59985b7u, 0x532a057cu, 0x01ecfe01u, 0x5016075du}, {0x02bcfe01u, 0x8001e1f2u, 0x06f82000u, 0x7f800001u},
    {0xff800000u, 0xbf0f5dfcu, 0x3723f675u, 0x7c018422u}, {0x1152ca45u, 0xb2c5711eu, 0xb734034eu, 0x37f02716u}, {0x00000001u, 0xfbffc80au, 0x36fe4783u, 0x3bff0468u},
    {0xb8cb591bu, 0x37311b65u, 0x82b971e3u, 0x07f57bffu}, {0x3800861au, 0xb1c5a551u, 0xc5bb6956u, 0x7fa00000u}, {0x533a81c9u, 0xc83041b6u, 0x067e5809u, 0x877ca03fu},
    {0x3ce9bc36u, 0xb9b84048u, 0xb661a5e5u, 0x33583dd1u}, {0xbe90dec7u, 0xb7d60106u, 0x36395844u, 0x03ff0401u}, {0x375f7ac6u, 0x06ea4000u, 0x3f800000u, 0xb505b78du},
    {0x3397a5e5u, 0xb3dc3800u, 0xc12f8f11u, 0x7300077au}, {0xb8cef498u, 0x349b4cffu, 0xe86f07ffu, 0x8001fe01u}, {0xbd237baeu, 0x35a831f1u, 0x80000000u, 0xa0e128e0u},
    {0x477ff000u, 0xb717f48au, 0x387fe000u, 0xc3a48001u}, {0xc30981bfu, 0xbdf6c1f6u, 0x80012bd1u, 0x387fffffu}, {0x7f800001u, 0x01ff5a23u, 0xbc00fbadu, 0x7bff90d5u},
    {0xb77eb17fu, 0xa9a660bau, 0x81116065u, 0x84002000u}, {0x9845c840u, 0x81691325u, 0x00017c01u, 0xba1285c5u}, {0x384eeca3u, 0x3826a28du, 0x3c007d23u, 0x0001832cu},
    {0xb34ff631u, 0x7fa00000u, 0x055cd9fbu, 0xb28e776au}, {0x7c008014u, 0x38000000u, 0x9b27fe01u, 0x323cd9f6u}, {0x3f7df501u, 0x3577ba2fu, 0x80000000u, 0xc2eb8c08u},
    {0x35657b37u, 0x3d84977du, 0x01ff8697u, 0xca0068e3u}, {0x3787cb56u, 0x07fd03ffu, 0x8440875fu, 0x36447cf2u}, {0x01dca058u, 0xc5a987e3u, 0xbca4f955u, 0x35917aceu},
    {0x5bf21400u, 0x7fa00000u, 0x39f2ae8bu, 0xe3e8a102u}, {0x3f992932u, 0xb83daaf4u, 0x37d2dd18u, 0xb2c899c1u}, {0xba19b626u, 0xbd9cfae7u, 0x80000000u, 0x00023800u},
    {0x80df01ffu, 0x04118337u, 0x000105c9u, 0x320b1337u}, {0x4019f21cu, 0x4282b3f4u, 0x3c017bffu, 0x387fe000u}, {0x36b648a9u, 0x477fe000u, 0x731d30d7u, 0xb5962840u},
    {0xc0466faau, 0xb32af002u, 0xb3f7bd39u, 0x85d206aau}, {0xfe010002u, 0xb8c13a8cu, 0x477fe000u, 0x7f800001u}, {0x3553cc25u, 0xbf800000u, 0x7c000001u, 0x387fe000u},
    {0x3b8ac782u, 0x3bc0c6c2u, 0x37bac6a4u, 0x7fc00000u}, {0x800107ffu, 0x382b66a2u, 0x34ac0e72u, 0x33c00000u}, {0xb9954729u, 0x33800000u, 0xb844caa4u, 0x00800001u},
    {0x7e00837bu, 0xb206c9fau, 0x3c0183ffu, 0x3f800000u}, {0x7e00ea86u, 0x7c003800u, 0xba030d7bu, 0x387fffffu}, {0x3fb96da9u, 0xb48c8aa0u, 0x343db67du, 0x000052c5u},
    {0x835c2041u, 0xb4a35b09u, 0xb405d412u, 0xbf800000u}, {0x3f2971a3u, 0xb6ec032eu, 0x369a1173u, 0x87313800u}, {0x18c38001u, 0x7f7fffffu, 0x7f800001u, 0xb5f22198u},
    {0x00000000u, 0x84000000u, 0x430a0200u, 0x8305c948u}, {0x7f800000u, 0xb404338fu, 0xb8fcc145u, 0x229e10bdu}, {0xbd793c62u, 0x3af26430u, 0x80000000u, 0x381d740eu},
    {0xbe4c51a7u, 0xb979ae96u, 0x80000000u, 0x35b2ee74u}, {0xbb5ed090u, 0x053abb40u, 0x00000001u, 0x80699a8au}, {0x80cd822eu, 0xff800000u, 0x86f6bc00u, 0x353ad0d0u},
    {0x3b92fa02u, 0x381fe97au, 0xb3e96897u, 0x9bc202bbu}, {0xbb40a6b3u, 0x84172000u, 0x860fc3a9u, 0xb793ac95u}, {0x3ff3a7f4u, 0xb29f776au, 0x80000000u, 0x04a484f1u},
    {0x370f7c72u, 0xb9a8960au, 0x370a58a7u, 0xb3800000u}, {0x39619f61u, 0xfb67984eu, 0x359e6bc4u, 0x390297e6u}, {0x387fe000u, 0x3c007bffu, 0xb3429df2u, 0x47800000u},
    {0x3e631961u, 0xb72e944au, 0xb65f8a79u, 0x823002c7u}, {0xc096876du, 0xb340e64du, 0xc401443fu, 0x7bff01c4u}, {0xbe4c039cu, 0x3494ea95u, 0x00000000u, 0xffc12345u},
    {0xbe22d89cu, 0xb840992du, 0x00000000u, 0x454b18ceu}, {0x7e00fe01u, 0x332638f4u, 0x4301c54cu, 0x71aa04b5u}, {0xbdd69810u, 0xbad23567u, 0x00000000u, 0x3edd7a3au},
    {0x020004d6u, 0x370ea14cu, 0x387ff000u, 0xb93c188bu}, {0x33800000u, 0x807fffffu, 0x7c013865u, 0x037b9af0u}, {0x398c1b4fu, 0xb5c3b62cu, 0x3e795a62u, 0x7fc00000u},
    {0x8a0f06afu, 0x032c0000u, 0x36b6a049u, 0x91f74000u}, {0xb6a8a297u, 0xa82e05dau, 0x00000000u, 0xc37122c8u}, {0xffc12345u, 0xb0537bf1u, 0x377fff62u, 0x7bff6101u},
    {0xc019043eu, 0x849a3800u, 0xbf800000u, 0x000186bau}, {0x3c98906bu, 0x38668549u, 0x00000000u, 0x8b6c83f3u}, {0xb93637a0u, 0x1400d1c4u, 0x32dc0dd3u, 0xc6f185a7u},
    {0xc059aeacu, 0x37aa41aeu, 0xb7ef3994u, 0x00800000u}, {0xba69d2a9u, 0x00800000u, 0x37f3c18bu, 0x0221012au}, {0x39b68fcau, 0x3958eb00u, 0xb28bf9bau, 0xb922f5c2u},
    {0xb705c3a9u, 0x33800000u, 0x00028838u, 0xb7bf6a1cu}, {0x4471eb72u, 0x63e511ccu, 0x03841400u, 0xff800000u}, {0xb87ff3c1u, 0x20007296u, 0x07ff0200u, 0x409f8154u},
    {0x3e45a8f6u, 0x07698095u, 0x020004e3u, 0xbc1a25f4u}, {0x37c1e811u, 0x4000bc00u, 0x41df9082u, 0xfe0107d9u}, {0x3c010363u, 0x33c00000u, 0x3f800000u, 0x387fe000u},
    {0x87ef1400u, 0x38000000u, 0xc246f1e2u, 0x7fa00000u}, {0x43130aaau, 0x3632d325u, 0x42139884u, 0x3971871bu}, {0x33c00000u, 0xaffc5879u, 0xb3c91583u, 0x477ff000u},
    {0x3a5c473du, 0x00000000u, 0x3752129fu, 0x830bcb12u}, {0x38000000u, 0x8223630bu, 0xb8f5a396u, 0x073284fdu}, {0x14000200u, 0xb76ac5b4u, 0x47800000u, 0x00800000u},
    {0x7f800001u, 0x38000000u, 0x3f8351a9u, 0x7fc00000u}, {0xb3d604b1u, 0x39cb7d30u, 0xfc003c00u, 0xbbe2835bu}, {0x401130a4u, 0xb42f3560u, 0xb43ad1f0u, 0x477ff000u},
    {0x00000001u, 0x00800001u, 0x3f800000u, 0x7f7fffffu}, {0xa240023cu, 0xbd8c14d7u, 0xb9a371bfu, 0x1ac30744u}, {0x3b32a6ccu, 0x3c47b5d9u, 0x37f9e4e5u, 0xe1df878du},
    {0xb811ba0du, 0xc18b7189u, 0xb50210d9u, 0x04d0e3bfu}, {0x38800000u, 0x7c00b218u, 0x3666737cu, 0xb6ee5f37u}, {0x83ff237au, 0xb304ad97u, 0xb5783417u, 0x7fc00000u},
    {0x05493c00u, 0x344f057bu, 0x0314fc00u, 0x8722835du}, {0xb6db10ecu, 0x840001f5u, 0xb3d3ea86u, 0x060e8000u}, {0x38cb6e96u, 0x4607e63fu, 0x387ff000u, 0xb37c2eebu},
    {0x383b78b5u, 0x04cc2adeu, 0x052e8626u, 0x382d1bd8u}, {0x7fc00000u, 0xb1fabfcdu, 0x345b8765u, 0xb3800000u}, {0x387fe000u, 0x3479a45eu, 0x00800001u, 0x82fb038du},
    {0x04847c01u, 0x38a6b492u, 0xb0140aecu, 0x3ca93887u}, {0x8a893c00u, 0xc74d5636u, 0x00000001u, 0x4779248fu}, {0x4af01001u, 0xb4d4d6c0u, 0x5363fc00u, 0x00020002u},
    {0xbe93f61cu, 0x34c8ce9fu, 0xb416c15fu, 0x3781b3abu}, {0x00000001u, 0x7f7fffffu, 0x83ca8046u, 0x7fc00000u}, {0x98c3063au, 0x90964000u, 0xb3939520u, 0x477fe000u},
    {0x35d2fc87u, 0xb82b3929u, 0xb8d807f6u, 0xb891ab30u}, {0x7c008408u, 0xb6f5e6a2u, 0x07fdfa6du, 0xc584fe92u}, {0x399e6b6bu, 0xbc0a27cbu, 0x00000000u, 0x3f800000u},
    {0x02e42b43u, 0x331ef3cau, 0x31d10f7eu, 0xc0a9e294u}, {0x30e932efu, 0x00000000u, 0x3a0f3013u, 0x01730212u}, {0xe9e330bcu, 0x86023b12u, 0x9349076bu, 0xb5636297u},
    {0xbc1b534bu, 0x36b413eau, 0x32b2cf17u, 0x3520d9d0u}, {0xfe017bffu, 0x056c4000u, 0xc3177583u, 0xb92c0340u}, {0x854f8529u, 0xc800efd2u, 0xe30503ffu, 0xb61bbe94u},
    {0x3993f4cbu, 0x53ab836au, 0x31556998u, 0x2f4f6232u}, {0x03514028u, 0x7c013c00u, 0xc7183eadu, 0x8000871bu}, {0x90283c01u, 0xb282c4a9u, 0x05b303b3u, 0xb5d718f6u},
    {0xfc007d23u, 0xb3149736u, 0x625d382fu, 0x3bff3c00u}, {0xb3666c48u, 0x38800000u, 0xb72c26a9u, 0x00800000u}, {0xbea2ed92u, 0xb5c1920bu, 0x34f1267fu, 0x3767c09eu},
    {0x0352fc00u, 0xb569512au, 0x477fe000u, 0xb72e2506u}, {0x3b20542du, 0xb97f759bu, 0xb4ce5a51u, 0x056f7c01u}, {0x3d822f2bu, 0x35ebc96cu, 0x32c25de5u, 0xb5e8f4f9u},
    {0x3f800000u, 0x090e4a5fu, 0xb39eb1a0u, 0x439a1a7du}, {0x308adb6cu, 0x443def42u, 0xb4c81740u, 0xb475bcceu}, {0x333c03c1u, 0xb5d5d03eu, 0x395d018au, 0x3c0081c2u},
    {0x3c9a36b5u, 0xb98b5db7u, 0x80000000u, 0x39830620u}, {0x3bff006au, 0x83183c01u, 0x04013800u, 0xbe18c458u}, {0x3720a296u, 0xf2a60124u, 0x387ff000u, 0x079c8719u},
    {0x3c1a949fu, 0xbc4e526cu, 0x38ca31f8u, 0x13948009u}, {0x83a62a54u, 0xfae57c00u, 0xbd789e69u, 0x3a5f973eu}, {0x3ff94a02u, 0xb4b38368u, 0x342c84b1u, 0x428d039cu},
    {0xc28e0ce9u, 0xb944dd80u, 0x00800001u, 0x35910624u}, {0x18bf7a7au, 0x355d7bbcu, 0xfbff8865u, 0x3c0107bfu}, {0xbe67a982u, 0x39b88d39u, 0xb8121402u, 0x4543e3d9u},
    {0x39ef68abu, 0x00008338u, 0xd14587d2u, 0xfbff207fu}, {0x43ee125bu, 0xbe31a86fu, 0x34f0fa48u, 0xb4d87c52u}, {0xb4c4e255u, 0x33800000u, 0xff800000u, 0xb2b10d18u},
    {0x04038171u, 0xb354aa0bu, 0xc64a2b7du, 0x736a00acu}, {0x040187b8u, 0xb2bc2fb2u, 0x38800000u, 0xfe0191b3u}, {0x80018024u, 0x9b82fe01u, 0x38000401u, 0x04c03c01u},
    {0xbc8982bau, 0x39cc9473u, 0x040f39b4u, 0x86d60210u}, {0xbfdbd38fu, 0xb649b863u, 0x00000000u, 0x412b913fu}, {0x387fe000u, 0x3f800000u, 0x00800001u, 0x825f076eu},
    {0x3bb733e7u, 0x3bbdf3e3u, 0x80000000u, 0x000f84fdu}, {0xb81ea01eu, 0x3395a17bu, 0x7c018250u, 0x3c01026eu}, {0xfe012000u, 0x387ff000u, 0x83250689u, 0x01e07a61u},
    {0x3a36ecedu, 0x34a7d118u, 0xb8bcb800u, 0x7f800001u}, {0xb25eab41u, 0xfe010400u, 0x44b15a22u, 0x84008001u}, {0x36298708u, 0x3301f235u, 0xbfc3cbd8u, 0x07ff7c00u},
    {0x03a40001u, 0x110c8480u, 0xb3786076u, 0xb64acb61u}, {0x32c30000u, 0xb4cfb0b9u, 0x83b98787u, 0x7fa00000u}, {0x7fa00000u, 0x7a889145u, 0xc2c8f984u, 0xb6ee8612u},
    {0x00800001u, 0x33b6f5efu, 0x37572c0cu, 0x39922a0fu}, {0x343e4847u, 0x03ff83afu, 0x3b179041u, 0x869d0631u}, {0xb7d83247u, 0x387b8baeu, 0xb72ec325u, 0x8001fc00u},
    {0x310783ffu, 0xe0707c00u, 0x80000000u, 0xb1007371u}, {0xb6032364u, 0xb1850a20u, 0xb4e6feafu, 0x3f800000u}, {0xbd5cc36cu, 0x4644b4bbu, 0x04010401u, 0x7c00aa79u},
    {0x7c01281fu, 0x00800001u, 0x33cbfbb7u, 0xb9ae552bu}, {0x515afc00u, 0x3765be04u, 0x017b3a43u, 0x35c7b411u}, {0x7fc00000u, 0x3841d2e6u, 0xb3800000u, 0xb63f234du},
    {0x30de063eu, 0x80000000u, 0xfbffb0dbu, 0xb8af0fc8u}, {0x80e007f3u, 0x3e1f7305u, 0xba522c56u, 0x37e6e873u}, {0x810d3bffu, 0x38e88dc1u, 0x79dc7e00u, 0xb4786429u},
    {0x80000000u, 0xc1887919u, 0xff800000u, 0x348f8e75u}, {0xbedf4fb6u, 0xb9f1a9bbu, 0x80000000u, 0x37ff4e98u}, {0xbdcc405fu, 0xb91b9635u, 0x80000000u, 0x311001ffu},
    {0x81957a91u, 0x041c5a4cu, 0xffc12345u, 0xc1fe84adu}, {0x11a38347u, 0xff800000u, 0x823f04feu, 0x029280cau}, {0x41fe843fu, 0xc393cd09u, 0xe9600277u, 0x7f7fffffu},
    {0xbc8e5577u, 0xba39f2e9u, 0x80000000u, 0xff800000u}, {0x395f129cu, 0x328b2ae9u, 0x81f6fe01u, 0xbaa20200u}, {0xb5b9eab0u, 0x6aa30721u, 0x90623800u, 0x359f3266u},
    {0x3a0403e0u, 0x3bff7c01u, 0xb13d01ffu, 0xb7842068u}, {0x3ac92365u, 0x3ad6fae4u, 0x00000000u, 0xffc12345u}, {0xf0032000u, 0xb3800000u, 0xb3800000u, 0xfbff8001u},
    {0xb7eb712au, 0x387fe000u, 0x06a903ffu, 0x33c00000u}, {0xda07a945u, 0x82578317u, 0x84c73c01u, 0x387fe000u}, {0x03643c01u, 0x6364cae5u, 0xba6c9743u, 0x33c5d6ceu},
    {0xb18997fcu, 0xb87485aeu, 0xb6e6202du, 0x3547c052u}, {0x3d3dfe13u, 0x37f3ee49u, 0x80000000u, 0xb41b64a4u}, {0xfbff7bdfu, 0xb658516au, 0x001152b2u, 0x3c012000u},
    {0x3e2bc00du, 0xb9a24499u, 0x372ccda8u, 0xc59feef4u}, {0xba29ba4eu, 0xbb51c0d3u, 0x36572d7eu, 0x461594b7u}, {0x00000001u, 0x387fffffu, 0x0323bc00u, 0x050e0423u},
}};

using Wide = unsigned __int128;

int Width(Wide value) {
    int width = 0;
    while (value != 0u) {
        ++width;
        value >>= 1u;
    }
    return width;
}

struct Source {
    bool negative = false;
    bool nan = false;
    bool infinite = false;
    std::uint64_t significand = 0u;
    int exponent = 0;
    std::uint32_t nanHalf = 0u;
};

Source Half(std::uint32_t bits, std::uint32_t denorm16) {
    const std::uint32_t magnitude = bits & 0x7fffu;
    Source source;
    source.negative = (bits & 0x8000u) != 0u;
    source.nan = magnitude > 0x7c00u;
    source.infinite = magnitude == 0x7c00u;
    source.nanHalf = bits | 0x200u;
    if (source.nan || source.infinite) return source;
    if (magnitude < 0x400u && (denorm16 & 1u) == 0u) return source;
    source.significand = (magnitude & 0x3ffu) | (magnitude >= 0x400u ? 0x400u : 0u);
    source.exponent = std::max(1, static_cast<int>(magnitude >> 10u)) - 25;
    return source;
}

Source Single(std::uint32_t bits, std::uint32_t denorm32) {
    const std::uint32_t magnitude = bits & 0x7fffffffu;
    Source source;
    source.negative = (bits & 0x80000000u) != 0u;
    source.nan = magnitude > 0x7f800000u;
    source.infinite = magnitude == 0x7f800000u;
    source.nanHalf = ((bits >> 16u) & 0x8000u) | 0x7e00u | ((bits >> 13u) & 0x3ffu);
    if (source.nan || source.infinite) return source;
    if (magnitude < 0x800000u && (denorm32 & 1u) == 0u) return source;
    source.significand = (magnitude & 0x7fffffu) | (magnitude >= 0x800000u ? 0x800000u : 0u);
    source.exponent = std::max(1, static_cast<int>(magnitude >> 23u)) - 150;
    return source;
}

std::uint32_t Mix(const Source& a, const Source& b, const Source& c, std::uint32_t rounding, std::uint32_t denorm16, bool overflow) {
    const bool aZero = !a.nan && !a.infinite && a.significand == 0u;
    const bool bZero = !b.nan && !b.infinite && b.significand == 0u;
    if ((a.infinite && bZero) || (b.infinite && aZero)) return 0xfe00u;
    for (const Source* source : {&a, &b, &c}) {
        if (source->nan) return source->nanHalf;
    }
    const bool productNegative = a.negative != b.negative;
    if (a.infinite || b.infinite) return c.infinite && c.negative != productNegative ? 0xfe00u : (productNegative ? 0xfc00u : 0x7c00u);
    if (c.infinite) return c.negative ? 0xfc00u : 0x7c00u;
    const std::uint64_t productSignificand = a.significand * b.significand;
    const int productExponent = a.exponent + b.exponent;
    if (productSignificand == 0u && c.significand == 0u) {
        if (aZero || bZero) {
            if (productNegative == c.negative) return productNegative ? 0x8000u : 0u;
        }
        return rounding == 2u ? 0x8000u : 0u;
    }
    int top = -100000;
    if (productSignificand != 0u) top = std::max(top, productExponent + Width(productSignificand));
    if (c.significand != 0u) top = std::max(top, c.exponent + Width(c.significand));
    const int scale = top - 120;
    const auto aligned = [&](std::uint64_t significand, int exponent) -> Wide {
        if (significand == 0u) return 0u;
        if (exponent >= scale) return (static_cast<Wide>(significand) << (exponent - scale)) << 1u;
        const int shift = scale - exponent;
        if (shift >= 64) return 1u;
        const std::uint64_t kept = significand >> shift;
        const bool sticky = (significand & ((std::uint64_t{1} << shift) - 1u)) != 0u;
        return (static_cast<Wide>(kept) << 1u) | (sticky ? 1u : 0u);
    };
    const Wide product = aligned(productSignificand, productExponent);
    const Wide addend = aligned(c.significand, c.exponent);
    Wide total = 0u;
    bool negative = false;
    if (productNegative == c.negative) {
        total = product + addend;
        negative = productNegative;
    } else if (product >= addend) {
        total = product - addend;
        negative = productNegative;
    } else {
        total = addend - product;
        negative = c.negative;
    }
    if (total == 0u) return rounding == 2u ? 0x8000u : 0u;
    const int power = scale - 1;
    const std::uint32_t sign = negative ? 0x8000u : 0u;
    const auto rounded = [&](int shift) {
        if (shift <= 0) return total;
        if (shift >= 127) return Wide{(rounding == 1u && !negative) || (rounding == 2u && negative) ? 1u : 0u};
        const Wide value = total >> shift;
        const Wide remainder = total - (value << shift);
        const Wide halfway = Wide{1} << (shift - 1);
        const bool increment = rounding == 0u ? remainder > halfway || (remainder == halfway && (value & 1u) != 0u) : remainder != 0u && ((rounding == 1u && !negative) || (rounding == 2u && negative));
        return value + (increment ? 1u : 0u);
    };
    const int precisionShift = std::max(0, Width(total) - 11);
    if ((denorm16 & 2u) == 0u && Width(rounded(precisionShift)) - 1 + power + precisionShift < -14) return sign;
    const int shift = std::max(precisionShift, -24 - power);
    const Wide value = rounded(shift);
    const int valueScale = power + std::max(0, shift);
    if (value == 0u) return sign;
    const int valueTop = Width(value) - 1;
    const int resultExponent = valueTop + valueScale;
    if (resultExponent > 15) {
        const bool infinity = !overflow && (rounding == 0u || (rounding == 1u && !negative) || (rounding == 2u && negative));
        return sign | (infinity ? 0x7c00u : 0x7bffu);
    }
    if (resultExponent < -14) return sign | static_cast<std::uint32_t>(value << (valueScale + 24));
    const Wide fraction = valueTop > 10 ? value >> (valueTop - 10) : value << (10 - valueTop);
    return sign | (static_cast<std::uint32_t>(resultExponent + 15) << 10u) | (static_cast<std::uint32_t>(fraction) & 0x3ffu);
}

std::array<std::uint32_t, 4> Expected(const std::array<std::uint32_t, 4>& words, std::uint32_t rounding, std::uint32_t denorm16, std::uint32_t denorm32, bool overflow) {
    const auto single = [&](std::uint32_t word) { return Single(word, denorm32); };
    const auto low = [&](std::uint32_t word) { return Half(word & 0xffffu, denorm16); };
    const auto high = [&](std::uint32_t word) { return Half(word >> 16u, denorm16); };
    return {
        Upper | Mix(single(words[0]), single(words[1]), single(words[2]), rounding, denorm16, overflow),
        Upper | Mix(low(words[0]), low(words[1]), low(words[2]), rounding, denorm16, overflow),
        Upper | Mix(high(words[0]), single(words[1]), low(words[2]), rounding, denorm16, overflow),
        (Mix(single(words[0]), high(words[1]), high(words[2]), rounding, denorm16, overflow) << 16u) | (words[3] & 0xffffu),
    };
}

std::array<std::uint32_t, 4> RowAt(std::size_t index) {
    if (index < PinnedRows.size()) return PinnedRows[index].words;
    return MeasuredRows[index - PinnedRows.size()];
}

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

}

int main(int argc, char** argv) {
    try {
        const auto wave = argc > 1 ? static_cast<std::uint32_t>(std::stoul(argv[1])) : 32u;
        const auto selectedRounding = argc > 2 ? static_cast<std::uint32_t>(std::stoul(argv[2])) : 0u;
        const bool selectedInteger = argc > 3 && std::stoul(argv[3]) != 0u;
        for (const auto& row : PinnedRows) {
            for (std::uint32_t rounding = 0u; rounding < 4u; ++rounding) {
                const auto expected = Expected(row.words, rounding, 0u, 0u, false);
                for (std::uint32_t op = 0u; op < 4u; ++op) {
                    const std::uint32_t half = op == 3u ? expected[op] >> 16u : expected[op] & 0xffffu;
                    Require(half == row.expected[rounding][op], "fma mix f16 flushed denormals: the reference differs from a measured row");
                }
            }
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint32_t> userData(8);
        const auto input = Descriptor(Input.data(), sizeof(Input));
        const auto output = Descriptor(Output.data(), sizeof(Output));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const std::span<const std::uint32_t> code(Code);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        constexpr std::size_t RowCount = PinnedRows.size() + MeasuredRows.size();
        std::uint64_t compared = 0u;
        auto target = device->Target();
        std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
        if (selectedInteger) std::erase(capabilities, 10u);
        target.supportedCapabilities = capabilities;
        target.featureDigestSealed = false;
        for (std::uint32_t denorm16 = 0u; denorm16 < 4u; ++denorm16) {
            for (const std::uint32_t denorm32 : {0u, 3u}) {
                for (const bool ieee : {false, true}) {
                    for (const bool overflow : {false, true}) {
                        ShaderRecompiler::RecompileRequest request{
                            {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
                            {wave, 0, userData, compute, std::nullopt, std::nullopt, memory}, target, {0, 0, 0, 128}
                        };
                        request.useCache = false;
                        request.context.floatMode = ShaderRecompiler::ShaderFloatMode{(denorm16 << 6u) | (denorm32 << 4u) | (selectedRounding << 2u), true, ieee, overflow};
                        const auto result = ShaderRecompiler::Recompile(request);
                        for (std::size_t first = 0u; first < RowCount; first += Threads) {
                            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                                const auto row = RowAt((first + tid) % RowCount);
                                std::copy(row.begin(), row.end(), Input.begin() + tid * 4u);
                            }
                            Output.fill(0xdeadbeefu);
                            device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
                            device->WaitIdle();
                            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                                const std::array<std::uint32_t, 4> words{Input[tid * 4u], Input[tid * 4u + 1u], Input[tid * 4u + 2u], Input[tid * 4u + 3u]};
                                const auto expected = Expected(words, selectedRounding, denorm16, denorm32, overflow);
                                for (std::uint32_t op = 0; op < 4u; ++op) {
                                    if (Output[tid * 4u + op] != expected[op]) {
                                        std::printf("integer=%u round=%u denorm16=%u denorm32=%u ieee=%u overflow=%u words=%08x %08x %08x %08x op=%u actual=%08x expected=%08x\n", selectedInteger, selectedRounding, denorm16, denorm32, ieee, overflow, words[0], words[1], words[2], words[3], op, Output[tid * 4u + op], expected[op]);
                                        throw std::runtime_error("fma mix f16 flushed denormals mismatch");
                                    }
                                    ++compared;
                                }
                            }
                        }
                    }
                }
            }
        }
        std::cout << "fma mix f16 flushed denormals: " << compared << " words passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
