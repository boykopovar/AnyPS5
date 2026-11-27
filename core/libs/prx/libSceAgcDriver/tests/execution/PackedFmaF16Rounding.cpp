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
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};
alignas(256) constexpr std::array<std::uint32_t, 16> Code{
    0x34020084, 0x34060084, 0xe0381000, 0x80000401, 0xbf8c3f70,
    0xcc0e400a, 0x1c1a0b04,
    0xcc0e490b, 0xdc1a0b04,
    0xcc0ec00c, 0x1c1a0b04,
    0x7e1a0306, 0x781a0b04,
    0xe0781000, 0x80010a03, 0xbf810000,
};

struct Pinned {
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
    std::array<std::uint32_t, 4> expected;
};

constexpr std::array<Pinned, 16> PinnedRows{{
    {0x800ad199u, 0x3a68f332u, 0x02e17aebu, {0x02d97c00u, 0x02d97c00u, 0x02d87bffu, 0x02d87bffu}},
    {0x01903bffu, 0x3e780401u, 0x01577bfeu, {0x03de7bfeu, 0x03de7bffu, 0x03dd7bfeu, 0x03dd7bfeu}},
    {0x26d26079u, 0x19cc6a37u, 0x815838d3u, {0x03997c00u, 0x039a7c00u, 0x03997bffu, 0x03997bffu}},
    {0x7bfe0112u, 0x8001436eu, 0xfbff803fu, {0xfbff03bbu, 0xfbff03bbu, 0xfc0003bau, 0xfbff03bau}},
    {0xe854820fu, 0x1362b617u, 0x7a3902e4u, {0x7a3903adu, 0x7a3903adu, 0x7a3803acu, 0x7a3803acu}},
    {0x809b80b5u, 0xc60a31b5u, 0x86d8857eu, {0x8330859eu, 0x832f859eu, 0x8330859fu, 0x832f859eu}},
    {0x3a224acbu, 0xc30a3068u, 0x3aed4151u, {0xbf544487u, 0xbf544488u, 0xbf554487u, 0xbf544487u}},
    {0xe4df465au, 0x3c003820u, 0x591edbcdu, {0xe43bdbb3u, 0xe43bdbb2u, 0xe43cdbb3u, 0xe43bdbb2u}},
    {0x37589ac0u, 0x13c9c845u, 0x8450290fu, {0x0e122c55u, 0x0e122c55u, 0x0e112c54u, 0x0e112c54u}},
    {0xb0573cddu, 0x79823c00u, 0x19ef18d8u, {0xedfa3cdfu, 0xedf93ce0u, 0xedfa3cdfu, 0xedf93cdfu}},
    {0xdc8fcb18u, 0x2140b0adu, 0xc217daa8u, {0xc609da97u, 0xc609da97u, 0xc60ada98u, 0xc609da97u}},
    {0x0af305bfu, 0x21cb4264u, 0x2b4287d9u, {0x2b420942u, 0x2b430942u, 0x2b420941u, 0x2b420941u}},
    {0xe3cc1400u, 0x31610400u, 0x08de3c00u, {0xd93e3c00u, 0xd93e3c01u, 0xd93f3c00u, 0xd93e3c00u}},
    {0x3ed78b37u, 0x84d6f082u, 0x041d81f2u, {0x84284011u, 0x84274011u, 0x84284010u, 0x84274010u}},
    {0xdbc6de15u, 0xa4711be9u, 0x222481aau, {0x4454be04u, 0x4454be03u, 0x4453be04u, 0x4453be03u}},
    {0x43b70584u, 0xbf083d81u, 0x833804bcu, {0xc6c80a29u, 0xc6c70a2au, 0xc6c80a29u, 0xc6c70a29u}},
}};

constexpr std::array<std::array<std::uint32_t, 3>, 240> MeasuredRows{{
    {0x45015eb1u, 0x39be3c00u, 0x4b5e0838u}, {0x4ac3489bu, 0x3c003319u, 0xcac2dbddu}, {0xbb737d23u, 0xc62b8400u, 0xc3550001u}, {0x512135d5u, 0x70ba10a3u, 0xf245047bu},
    {0x00b59c8eu, 0xbf5b3c00u, 0x02761c8du}, {0x07ff0524u, 0x7bff3af8u, 0xfe018213u}, {0x3800b0e6u, 0x03ff3c00u, 0xfbff30e5u}, {0xb5f90164u, 0x0bb3381au, 0x0435028fu},
    {0xb0f3817au, 0x3c00474du, 0x20c082afu}, {0xbd859221u, 0x0b9b318bu, 0x02f0893eu}, {0x834ad031u, 0xc4e525c7u, 0x00e0cbf5u}, {0x8330e807u, 0x42083c00u, 0x02336809u},
    {0x31bd5b1cu, 0x3c00a39bu, 0x487e40b7u}, {0x7c003c3fu, 0xbc00076au, 0xfc00018du}, {0x3243070eu, 0x3c003ddfu, 0x39ef06b8u}, {0x423f3aa5u, 0x69ec47d3u, 0x60dc59a1u},
    {0x483f0116u, 0x379a3ac5u, 0xf8d3049bu}, {0x251315feu, 0x3c003c00u, 0x734195feu}, {0x203801abu, 0x00a0e106u, 0xcb9c70b0u}, {0x59f96a6bu, 0xa91a73b2u, 0xf2547988u},
    {0x00208400u, 0xbd383800u, 0x07513e00u}, {0x0400a1b6u, 0x4000aafdu, 0x5bffa0cbu}, {0xa2268181u, 0x290f4404u, 0x5b0f05e8u}, {0x892a43e3u, 0xab863d36u, 0xd21300edu},
    {0x333ba58eu, 0x3c001c70u, 0xb33d81efu}, {0x00ddbe28u, 0xb4e2c33eu, 0x80cefa4du}, {0xc0c1e99du, 0x3c005949u, 0x7baf3189u}, {0xfc0038e0u, 0x7c0186a7u, 0x3800845bu},
    {0xbabd6da5u, 0x8f9e3c00u, 0x01dd596bu}, {0xa92eaaf0u, 0xf8e3196cu, 0x58910027u}, {0x826d291fu, 0x3e9a94e3u, 0x84c2007du}, {0x835e0001u, 0x36bafc00u, 0x078b8000u},
    {0xc39e4533u, 0xb455340au, 0x1a273924u}, {0x8797b635u, 0xb5b88afcu, 0x81fd818bu}, {0x26bab63cu, 0xa2288c8cu, 0x0114828au}, {0x4d9fd237u, 0x281dea56u, 0x3be908beu},
    {0x20006a90u, 0x03ffc9edu, 0x00017364u}, {0x80903556u, 0x4b193c00u, 0x6b635810u}, {0x51e03fd2u, 0x2daf86feu, 0x31868404u}, {0xaf4091f3u, 0x90283c00u, 0x803011f4u},
    {0x14008001u, 0x7bfe7e00u, 0x14000200u}, {0xbc00965au, 0x01ff3c00u, 0x84001659u}, {0x59900001u, 0x7ae17e00u, 0x20803e00u}, {0x812183ffu, 0x31bb3e00u, 0x07260401u},
    {0x8a8e3d2fu, 0x33233edfu, 0x81020032u}, {0xb7a482d4u, 0x102bbd5fu, 0x036704b4u}, {0x20003e00u, 0x00008000u, 0x3bff7c00u}, {0x57885979u, 0x3c002797u, 0xd789a1cbu},
    {0x85f0825bu, 0x3fe4c590u, 0x8378029cu}, {0xf9adae1du, 0x3c003c00u, 0x79ab2e1du}, {0x0001bb53u, 0x0001bc56u, 0x020041d5u}, {0x31aebc49u, 0x3c00be55u, 0x00629bedu},
    {0x03365bf7u, 0xbe111fa2u, 0x850842b1u}, {0x60350001u, 0x3c000001u, 0x5a957e00u}, {0xa1872d51u, 0xa7211294u, 0x01150318u}, {0x49c12bc0u, 0x827902a6u, 0xa3ee930fu},
    {0x2f84a479u, 0x1aff1850u, 0x80280377u}, {0x87028a7eu, 0x43a2cbc8u, 0x039a5976u}, {0xf7668001u, 0x3c0003ffu, 0x783c3c01u}, {0x5bff3f24u, 0xbc0084c3u, 0x7c010453u},
    {0xfbff3348u, 0x7c008c2cu, 0x5bff82e5u}, {0x3306b988u, 0x959d0d13u, 0x831b0153u}, {0xb97a6822u, 0x0a993c00u, 0x042ce821u}, {0x4ad404c6u, 0x3c004234u, 0xcad582d7u},
    {0x02bad9d0u, 0xc25273f7u, 0x0612bb33u}, {0x6c7f134du, 0x3c003c00u, 0x08e9934du}, {0x39238400u, 0xd1a283ffu, 0xa8368001u}, {0xcbd72c04u, 0x3c001af2u, 0x23860229u},
    {0x5d0a1a52u, 0x1d463c00u, 0xe0809a50u}, {0xafb22e7du, 0x3c0094bcu, 0x103581f3u}, {0x4a09a34fu, 0x3c009d18u, 0x3b2983c3u}, {0x01ff25cdu, 0x20003c00u, 0x07ff50c7u},
    {0x3f5ccd55u, 0x0714acbcu, 0x047bf8a4u}, {0x0001ac2fu, 0x80009938u, 0x7c01803au}, {0x0654e970u, 0xb59d70ebu, 0x02bd81dbu}, {0x44c9e16bu, 0xb5943c00u, 0x0ad8616du},
    {0x87c53e54u, 0xb40a041fu, 0x07d38436u}, {0x80ee049cu, 0x33613a57u, 0x809e837du}, {0xbe033b35u, 0xc1553d44u, 0x22125a1eu}, {0xfc00cde2u, 0x07ff3c00u, 0x7c004de3u},
    {0xb9be4932u, 0x84233c00u, 0x01f9c932u}, {0x81c93aadu, 0xba670b55u, 0x0393839du}, {0x85b65565u, 0xb8cca82bu, 0x830aa225u}, {0x0200bd46u, 0x000007bau, 0x00028291u},
    {0xbe178400u, 0x04b17d23u, 0x828c0400u}, {0xb017ec1cu, 0x81203c00u, 0x1af339b7u}, {0xd63f85f1u, 0x26093b6cu, 0x708d077au}, {0x7bfeb1b8u, 0x7e003c00u, 0x3c011931u},
    {0xaf401953u, 0x91fc49fau, 0x005f42efu}, {0xb4cabca4u, 0x0cdf063cu, 0x039801a7u}, {0x29c6121bu, 0x71830b32u, 0x62eef9f1u}, {0xb4045345u, 0x0d052addu, 0x04214151u},
    {0xb3c320e9u, 0x9320102cu, 0x80fa80c1u}, {0xea965a79u, 0x31e1dad1u, 0xd3dce222u}, {0x26b5ccddu, 0x3c002967u, 0xa6b718cdu}, {0x0401fc00u, 0x5bff7c00u, 0x80002000u},
    {0x0f81bc00u, 0x3c002000u, 0x588e1400u}, {0xcdf34000u, 0xb38c3bffu, 0xe1803c00u}, {0x874f05efu, 0x3c003e5cu, 0x5b22824cu}, {0x60cc20acu, 0x23749ec2u, 0x10fd8143u},
    {0x1aab87a9u, 0x3c00b4d9u, 0x9aad03e5u}, {0x20002d12u, 0x80013c00u, 0x38005ac6u}, {0xadd1026du, 0x3c003374u, 0x2dcf06c4u}, {0x85920400u, 0xb7a23c00u, 0x02828000u},
    {0x7bfed005u, 0x3e002dc9u, 0x3c007a6bu}, {0xeed7fbffu, 0x3c003c00u, 0x6ed97c01u}, {0x362b5878u, 0x3c003121u, 0xb62c1b1fu}, {0xc19c38d6u, 0xd3c53c00u, 0x2a9e39feu},
    {0x8233341eu, 0x61130c91u, 0xf8ee803au}, {0x327d8001u, 0x15863bffu, 0x827883ffu}, {0x07860257u, 0xbb36e0cbu, 0x02ebda97u}, {0x0198855cu, 0xb88ec550u, 0x861f0235u},
    {0xcae811a1u, 0x29135bacu, 0xb2222b2bu}, {0x36ad8071u, 0x8e777a83u, 0x807ab316u}, {0x03a001ffu, 0x39327e00u, 0x801383ffu}, {0xb725a39du, 0x92979d3au, 0x805201c3u},
    {0x26067e00u, 0x204203ffu, 0x02de0401u}, {0xbf7abccfu, 0x859d4357u, 0x8298f806u}, {0x05750400u, 0xc1fd8400u, 0x84508400u}, {0xbab1e0f4u, 0xdbbc3c00u, 0x21f360f3u},
    {0x4c913c01u, 0xad328400u, 0xe1e80400u}, {0x012b87c3u, 0x393eb9f5u, 0x008c85dau}, {0x46d20002u, 0xb4615bffu, 0x28f801ffu}, {0x3800c594u, 0x7e003c00u, 0x7c014593u},
    {0xd1652d37u, 0x08da1a64u, 0x6390028cu}, {0x2d44d148u, 0x981ea784u, 0x02b2eb92u}, {0x36ca275eu, 0x92341a88u, 0x8291010cu}, {0x3e0041a1u, 0x03ff99d1u, 0x7d23229cu},
    {0x59c93b94u, 0x1cf54062u, 0x4089b089u}, {0xd33b3995u, 0xac5b4319u, 0x4239d83du}, {0xc8ee43f0u, 0x88b935d6u, 0x28802251u}, {0x07ff03f0u, 0x7bfe466cu, 0x7e000484u},
    {0x80d3fbffu, 0x397e3c00u, 0x044e5bffu}, {0x433d076au, 0xb0ab3ef7u, 0x9b818713u}, {0x2be6ed22u, 0x1d713c00u, 0x810d3aedu}, {0xd5391128u, 0x207c70acu, 0x83283be1u},
    {0xe35e0002u, 0x3c00fe01u, 0x635c8400u}, {0x0394d62du, 0x698d2126u, 0x898ee15au}, {0x728d0266u, 0xa0453f27u, 0x43d0067fu}, {0x3ba4f10eu, 0x895f3c00u, 0x03ef7ad9u},
    {0x218cbd02u, 0x3c008a8fu, 0x4976020bu}, {0x4bb14f56u, 0x3c00b2e2u, 0xcbb03380u}, {0x4e14bc00u, 0x322b83ffu, 0x90d40000u}, {0x4becd011u, 0x49e8a9fdu, 0x19fc5a0cu},
    {0x1515499du, 0x3c0030d1u, 0x5bb9582du}, {0x2840a27eu, 0x0849a20bu, 0x80c60289u}, {0x3a6c811cu, 0x3c00ab47u, 0x793420a8u}, {0x5bffcc8au, 0x7d233c00u, 0x5bff4c89u},
    {0x0507d6c7u, 0x3e40a05bu, 0x07759adfu}, {0x331fe962u, 0x88ba3c00u, 0x99c96960u}, {0x32625bffu, 0x20ca1400u, 0x924c83ffu}, {0x838074e0u, 0xb0f63c00u, 0x03890393u},
    {0xc92e8085u, 0xb2fa323au, 0x808d85c6u}, {0x5a3df18du, 0x3c003c00u, 0xda3c718cu}, {0x31840ab4u, 0x8f293c00u, 0x00d032e3u}, {0x2d1cbce1u, 0x12543dd2u, 0x818bd2b9u},
    {0x96b139eeu, 0x3c0047ffu, 0x16b21a21u}, {0xb3adfa75u, 0x8ee0d9dbu, 0x02acc0b1u}, {0xb0cc2f4du, 0x92c999f4u, 0x0235808au}, {0x4a6f59cfu, 0x11693c00u, 0x824a60e4u},
    {0x05a6b630u, 0x31cf9025u, 0x8729039bu}, {0xa5f683ffu, 0xa0d80400u, 0x81a64000u}, {0x000256beu, 0x2000233au, 0x8400a299u}, {0xa79fb575u, 0x3c000d00u, 0x22118280u},
    {0xb29ef603u, 0xf25f3c00u, 0x19f1215bu}, {0xb195054au, 0x8f523a03u, 0x820704fau}, {0xa3018034u, 0xa9bc35e8u, 0xd06d05ccu}, {0x0400c9b0u, 0x7bfe3c00u, 0x3800516eu},
    {0x5ade7c00u, 0x3c000001u, 0xdae03c00u}, {0xbe7ac83du, 0x0460b11du, 0x834b31d1u}, {0x7c00ba07u, 0x5bff89a1u, 0x40008193u}, {0x2dee01ffu, 0x18230400u, 0x83c2fe01u},
    {0xda09b9e3u, 0x1e3b44c8u, 0x92666a34u}, {0x389f861eu, 0xbffb3c00u, 0x79ff061eu}, {0xdc1d5971u, 0x998a39deu, 0x4bf7b352u}, {0x2b2903c6u, 0x3c00b4dfu, 0xab2b8290u},
    {0x8d4a6122u, 0x3c0061a6u, 0x0d4cda7cu}, {0xc827bbb5u, 0x3c000b4eu, 0x482800fbu}, {0x68d80001u, 0x715c7d23u, 0x9ae183ffu}, {0x5a6783a9u, 0xb3d2a33du, 0x1b97728fu},
    {0x7c000619u, 0x7c01b746u, 0x83ff006au}, {0xaeadfb53u, 0x3c008a7eu, 0x2eab3984u}, {0xa18e0001u, 0xa53a1400u, 0x03060400u}, {0x80014f97u, 0x40003c00u, 0x8400cf98u},
    {0xa6643bd7u, 0x1fb08be1u, 0x80dc028eu}, {0x01ffd450u, 0xfbffa507u, 0x3bff2b78u}, {0x80001b97u, 0x4000d3e3u, 0x02007af9u}, {0xbbc8843du, 0x86e1b8ecu, 0x043a0467u},
    {0x21218232u, 0x22b943d6u, 0x820d03d9u}, {0x80018677u, 0x7bfeb1c5u, 0x7d23821fu}, {0xb2f92f81u, 0x95641732u, 0x027c842eu}, {0xc697bff1u, 0xb8218abau, 0x791b8122u},
    {0x85538263u, 0x3085343eu, 0x031e0158u}, {0x4efc06d9u, 0xa94bc459u, 0xf0cf8267u}, {0x3d0ea572u, 0xb9393c00u, 0x18a728c2u}, {0xfbff908cu, 0x80012ba8u, 0x7bff59f6u},
    {0x49be1256u, 0x3c001150u, 0xc9bcfa81u}, {0x894bd10fu, 0x082d3c00u, 0xf31b7b79u}, {0x00425b48u, 0x1bf8c90fu, 0xaa0612c8u}, {0x55211861u, 0x3c003c00u, 0x32499860u},
    {0x2998a8b5u, 0x035f9bffu, 0x49610023u}, {0x9bfcb149u, 0x3c003c00u, 0x192461a5u}, {0xba8f0a5eu, 0x0beb607du, 0x81afe3c0u}, {0x80965d56u, 0xbbc9a3e9u, 0x870c4845u},
    {0x6a8bd725u, 0x3c00284fu, 0xea8cc8ffu}, {0x07701400u, 0x40aafc00u, 0x833d5bffu}, {0xcbdaf1d9u, 0xd0b500f5u, 0xe2d1b82fu}, {0x20004ca3u, 0x01ff29fau, 0x0002c19eu},
    {0x408d4d87u, 0xbc7b2dfdu, 0xab7eb8d8u}, {0xabba01ffu, 0x3c0007ffu, 0x795b8000u}, {0x7d236a7eu, 0x40006a59u, 0x7e00f92bu}, {0x1a705760u, 0xb181a4beu, 0x815ea3b9u},
    {0x71b2d995u, 0x3c00f85fu, 0xf1b491ddu}, {0x72d65274u, 0xd33a3c00u, 0x091fd276u}, {0x7d238327u, 0x3c01c3dfu, 0x1400852du}, {0x80018400u, 0x03ff0200u, 0x7c0007ffu},
    {0xb15c053eu, 0x3c00459au, 0x315e0627u}, {0xb82eda48u, 0x0fdb3c00u, 0x811d5a48u}, {0xc7a7aad7u, 0xb5bf3c00u, 0xbb1272b3u}, {0x33243bffu, 0x10bf8000u, 0x01fe01ffu},
    {0xa5a6fc00u, 0x20ad3800u, 0x8067fbffu}, {0x2b59b4c8u, 0x9c22919eu, 0x01128010u}, {0x8585ac07u, 0xb7b21806u, 0x01600409u}, {0x02ce4102u, 0x3eab58f4u, 0x0328c100u},
    {0x3bff426bu, 0xbc00bc33u, 0x3c01f049u}, {0x3e008401u, 0x7e00c576u, 0x03ff0293u}, {0x06703e00u, 0xbe277bffu, 0x833b03ffu}, {0x8501bc9cu, 0xb6a23e24u, 0x845d79d2u},
    {0xbd92d262u, 0x0532dbecu, 0x8105989cu}, {0xa07f00c6u, 0x1d9bb9adu, 0x82ba00b5u}, {0x2760b49fu, 0x3c0092d6u, 0x7a3b8423u}, {0xa8bc03ffu, 0x1ebf0000u, 0x01703c00u},
    {0xb5975c87u, 0x11d02207u, 0x0024705eu}, {0xd6c48361u, 0x250ee3dfu, 0x825bc032u}, {0x5125049fu, 0xc2b8bfdeu, 0x185f04b1u}, {0x3c01b2f8u, 0xfe01103bu, 0xfe0180bbu},
    {0x80ffab2eu, 0xb5791860u, 0x83e800cdu}, {0x8abe7e00u, 0x3c008400u, 0x31d007ffu}, {0x2000bbffu, 0x3e008c90u, 0x01ff0322u}, {0x06e8c21fu, 0xb670b93au, 0x007b72f6u},
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

std::uint32_t Fma(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t rounding, std::uint32_t denorm, bool ieee, bool overflow) {
    const auto mag = [](std::uint32_t value) { return value & 0x7fffu; };
    const auto flush = [&](std::uint32_t value) { return mag(value) < 0x400u ? value & 0x8000u : value; };
    if ((denorm & 1u) == 0u) { a = flush(a); b = flush(b); c = flush(c); }
    const auto nan = [&](std::uint32_t value) { return mag(value) > 0x7c00u; };
    const auto infinite = [&](std::uint32_t value) { return mag(value) == 0x7c00u; };
    if ((infinite(a) && mag(b) == 0u) || (infinite(b) && mag(a) == 0u)) return 0xfe00u;
    for (const auto value : {a, b, c}) {
        if (nan(value)) return value | (ieee ? 0x200u : 0u);
    }
    const auto productSign = (a ^ b) & 0x8000u;
    if (infinite(a) || infinite(b)) return infinite(c) && (c & 0x8000u) != productSign ? 0xfe00u : productSign | 0x7c00u;
    if (infinite(c)) return c;
    const auto exponent = [&](std::uint32_t value) { return std::max(1, static_cast<int>(mag(value) >> 10u)) - 25; };
    const auto significand = [&](std::uint32_t value) { return static_cast<std::uint64_t>((value & 0x3ffu) | (mag(value) >= 0x400u ? 0x400u : 0u)); };
    const int productPower = exponent(a) + exponent(b);
    const int addendPower = exponent(c);
    const int power = std::min(productPower, addendPower);
    const Wide product = static_cast<Wide>(significand(a) * significand(b)) << (productPower - power);
    const Wide addend = static_cast<Wide>(significand(c)) << (addendPower - power);
    const bool productNegative = productSign != 0u;
    const bool addendNegative = (c & 0x8000u) != 0u;
    Wide total = 0u;
    bool negative = false;
    if (productNegative == addendNegative) {
        total = product + addend;
        negative = productNegative;
    } else if (product >= addend) {
        total = product - addend;
        negative = productNegative;
    } else {
        total = addend - product;
        negative = addendNegative;
    }
    if (total == 0u) {
        if (product == 0u && productSign == (c & 0x8000u)) return productSign;
        return rounding == 2u ? 0x8000u : 0u;
    }
    const std::uint32_t sign = negative ? 0x8000u : 0u;
    const auto rounded = [&](int shift) {
        if (shift <= 0) return total;
        const Wide value = total >> shift;
        const Wide remainder = total - (value << shift);
        const Wide halfway = Wide{1} << (shift - 1);
        const bool increment = rounding == 0u ? remainder > halfway || (remainder == halfway && (value & 1u) != 0u) : remainder != 0u && ((rounding == 1u && !negative) || (rounding == 2u && negative));
        return value + (increment ? 1u : 0u);
    };
    const int precisionShift = std::max(0, Width(total) - 11);
    if ((denorm & 2u) == 0u && Width(rounded(precisionShift)) - 1 + power + precisionShift < -14) return sign;
    const int shift = std::max(precisionShift, -24 - power);
    const Wide value = rounded(shift);
    const int scale = power + std::max(0, shift);
    if (value == 0u) return sign;
    const int top = Width(value) - 1;
    const int resultExponent = top + scale;
    if (resultExponent > 15) {
        const bool infinity = !overflow && (rounding == 0u || (rounding == 1u && !negative) || (rounding == 2u && negative));
        return sign | (infinity ? 0x7c00u : 0x7bffu);
    }
    if (resultExponent < -14) return sign | static_cast<std::uint32_t>(value << (scale + 24));
    const Wide fraction = top > 10 ? value >> (top - 10) : value << (10 - top);
    return sign | (static_cast<std::uint32_t>(resultExponent + 15) << 10u) | (static_cast<std::uint32_t>(fraction) & 0x3ffu);
}

std::uint32_t Clamp(std::uint32_t bits, bool dx10) {
    if ((bits & 0x7fffu) > 0x7c00u) return dx10 ? 0u : bits;
    if ((bits & 0x8000u) != 0u) return 0u;
    return bits > 0x3c00u ? 0x3c00u : bits;
}

std::array<std::uint32_t, 3> RowAt(std::size_t index) {
    if (index < PinnedRows.size()) return {PinnedRows[index].a, PinnedRows[index].b, PinnedRows[index].c};
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
                std::uint32_t expected = 0u;
                for (std::uint32_t half = 0u; half < 2u; ++half) expected |= Fma((row.a >> (half * 16u)) & 0xffffu, (row.b >> (half * 16u)) & 0xffffu, (row.c >> (half * 16u)) & 0xffffu, rounding, 3u, false, false) << (half * 16u);
                Require(expected == row.expected[rounding], "packed fma f16 rounding: the reference differs from a measured row");
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
        for (std::uint32_t denorm = 0u; denorm < 4u; ++denorm) {
            for (const bool ieee : {false, true}) {
                for (const bool dx10 : {false, true}) {
                    for (const bool overflow : {false, true}) {
                        ShaderRecompiler::RecompileRequest request{
                            {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
                            {wave, 0, userData, compute, std::nullopt, std::nullopt, memory}, target, {0, 0, 0, 128}
                        };
                        request.useCache = false;
                        request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0x30u | (selectedRounding << 2u) | (denorm << 6u), dx10, ieee, overflow};
                        const auto result = ShaderRecompiler::Recompile(request);
                        for (std::size_t first = 0u; first < RowCount; first += Threads) {
                            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                                const auto row = RowAt((first + tid) % RowCount);
                                Input[tid * 4u] = row[0];
                                Input[tid * 4u + 1u] = row[1];
                                Input[tid * 4u + 2u] = row[2];
                                Input[tid * 4u + 3u] = 0u;
                            }
                            Output.fill(0xdeadbeefu);
                            device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
                            device->WaitIdle();
                            for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                                for (std::uint32_t op = 0; op < 4u; ++op) {
                                    std::uint32_t expected = 0u;
                                    for (std::uint32_t half = 0; half < 2u; ++half) {
                                        auto a = (Input[tid * 4u] >> (op == 1u && half == 0u ? 16u : half * 16u)) & 0xffffu;
                                        auto b = (Input[tid * 4u + 1u] >> (half * 16u)) & 0xffffu;
                                        auto c = (Input[tid * 4u + 2u] >> (half * 16u)) & 0xffffu;
                                        if (op == 1u) {
                                            if (half == 0u) {
                                                b ^= 0x8000u;
                                                c ^= 0x8000u;
                                            } else {
                                                a ^= 0x8000u;
                                            }
                                        }
                                        auto bits = Fma(a, b, c, selectedRounding, denorm, ieee, overflow);
                                        if (op == 2u) bits = Clamp(bits, dx10);
                                        expected |= bits << (half * 16u);
                                    }
                                    if (Output[tid * 4u + op] != expected) {
                                        std::printf("integer=%u round=%u denorm=%u ieee=%u dx10=%u overflow=%u a=%08x b=%08x c=%08x op=%u actual=%08x expected=%08x\n", selectedInteger, selectedRounding, denorm, ieee, dx10, overflow, Input[tid * 4u], Input[tid * 4u + 1u], Input[tid * 4u + 2u], op, Output[tid * 4u + op], expected);
                                        throw std::runtime_error("packed fma f16 rounding mismatch");
                                    }
                                    ++compared;
                                }
                            }
                        }
                    }
                }
            }
        }
        std::cout << "packed fma f16 rounding: " << compared << " words passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
