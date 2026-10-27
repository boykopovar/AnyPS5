#include "prx/libSceAjm.native/libSceAjm.native.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>



namespace {

constexpr std::uint64_t Stream = 1ull << 47;
constexpr std::uint64_t Format = 1ull << 46;
constexpr std::uint64_t Multiple = 1ull << 12;
constexpr std::uint64_t Nondelay = 1ull << 33;
constexpr std::array<double, 5> Frequencies{311, 677, 1013, 1423, 1871};

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "AAC multichannel check failed at line %d\n", line);
        std::abort();
    }
}

#define CHECK(value) Require((value), __LINE__)

constexpr std::string_view Aac3 =
    "fff14cc02fdffcdc004c61766336312e332e31303000025ca65b69683d390b44995ade70e6f5adf1cb5532eaf28f9924"
    "93cac312c57496c5b6b11a6a9de6ae35c5b15a7ac9f7699415b0e652cfc0a0479da710cde5c8c3763e4113033b02890f"
    "c15124fd57b4f9d70b956538ef45ebdd87869d8e8d6cfad9f5b5663a363a35f9aaa52a94aa52a949a5268928928928c9"
    "44944944944944940940944944944944944940c0c0c0c0c8914b2cb2cb2cb2cb2c514514514514514514514511095069"
    "00bc4000276cb07666c0cdf5eff8ae7ad4a85c92b547724924911016561bcedd4b8ebd449b15dbded3988e8fe5f4b4ba"
    "244aa33ad6956d2b5a535ac26ae6ae6ae6c31c31919db0c70c70c709b09b0c64c649a49a49a499d9d85a46919d9e8747"
    "6761676767676769db1c3511b0737dd3dfd7e35af32f85c93b4e9e724892e2101f70b62618fb964aa255165a499ab3c9"
    "7d17e49489546732d456894a6b4c942784666732d04d34a1270e1c094b4d2830303030303220606366ea60ba6be0fff1"
    "4cc03c9ffc011a96daca5db4598f1654975659272fff8beff7eb87a6af57e3ffdbff6ea71c6b5ad7f7fffb5fe7f5bd75"
    "abd6bfa7ffd9ffbf135d6b5ad07f7286c8b9a69969bf85ae743ef9682a30585a9a75327681d7e76b7f3d46b7f3f39adf"
    "cfce6b9bf9ff3f38d737f3fe7fcc6bd6fe7fcff989baa1036743c478e079a6389b257d229fe914ff464453abe9f48a29"
    "e7fa7d228a79fe9f4e78a213a87d3e9f48a213ce3e9f4fa45114f3aadafb6c0e870938716133049c0909381212702424"
    "24852db2000400c03c13b64aedb22d38b23dd94ed9faf9e386d5deb5ffa6febceb5c6af5afffb99f7eb5ae357ad7ffda"
    "e7efedd715abd684fa0dee51451445bf37b4a0462e9edd90bc7cf7200624eee6e50665dc1a5060606060606777770606"
    "94181997770606360c0c0ceee0c0c0c0c0c0dbaf50cee243bc753c670f54e0d936c45fcbd5ac7f2fe5ac7f2fe5ae63f9"
    "7f2fe5ac1fcbf97f2d60fe5fcbf96b9bdd3a3952cb29b941818181818181818181818181a5ec95db64a7a43eec8cfd64"
    "bad2fff0ffbfb71a756a6b5fdffb7fd7eae3ee3ffc3fb7fdfeb407f1fdfff6f6eb8f3c6b8fb85a950463514506452804"
    "4e9a17878226117c9547397c82425dc18199cea02e1a8bd5115e387e0f3fc1e765510c953cff2e6fde9fde9fde97bf7a"
    "5ee0fff14cc027dffc011cf4195624311f4242d092defede35affa77cf3faffb7f5e38d5fdf9b5cb8f2e1c6a5c922480"
    "0c0f1f3638f9bc3c3703839393c7a4504ff15afdf5949dda0a69971414d38d05fd5517f35f1987bfbc2327c363dfdc86"
    "4cc61efee07c6600c9f7f703e33019c801f0d81ee407c3603a40ff19827dc81fe3304fb903fc3613ee41ff86c27dc83f"
    "f0d84fb901f0d84f6901f0d84fb9047c3613ee411f0d84fb910bd029cb1b077cff5ff6fbaf8f8deaf534d24971aad249"
    "2492440ffc666cbe3e3e1bddefefa07c7c363dfde13fc7c36efbfb90f8f809f7d00ff0d877df407fe1b13858d0887422"
    "1d5089babf7f5fa7aff1c75f79d385c4b91d37ad49722493ef03befef0bef9d5efa3407fe3e03befee1ff8f8327dfdc8"
    "ff1f007be803e3e00f7d007c7c01fc4f3607f03f8f8479bc462c279bc462c2e0fff14cc0243ffc00f8330be5d12b2aa9"
    "3f4fff0fff0fffe1ededd71c0915112220ec0fa3cf3cf3cf30e738d2c783cc79e79efa6ba28a28a28a28a28a28a28a28"
    "a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28"
    "a28a28a28a28fd747ea8a28a28a28a28a28a28a28a28a28a28211a0801e0000004295f4641d4015fdffedff1ffdffebc"
    "71abbd5c92449239e490888359d2a53cf3cf7399e79e79e79ed745145145145145145145145145145145145145145145"
    "143e95f5e82673df7f3fff6fe7ffc7febfaeae6492490f38888883e812f3cf3c94b0d1b05145145143cf3cf3cf6979ef"
    "dbcf7ef4bdfbfdfeff7fbfdfeff7fb79e03030303030303030303030303030303efff14cc0283ffc010030992e261a4d"
    "3155ed3f3ffc7fdbfffdf5f7357249044880ececececec475d35b1dae97a7c5252ed5b39f7535d20c0c0c0c0c0c0c0c0"
    "c0c6e5965965965965965965965965965965965965965965965965965965965965965965965965965965965965965965"
    "965965965965965965965965965965965211a0801fffc0004316b4a41043f6ffebfc7fe7ff7e35abbd5dc9249244a722"
    "49d0e295dc6e3c78fcbe44279e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e7"
    "9e79e79e79de8b3a540e8503a140e8903a273cf7dfcfffdaf9ffdfff2fd757724924911ea8952489007f5f5f5f53e109"
    "5ca114514514514514514514514514514514514514514514514514514514514514418181818181818303030303030303"
    "030303030303030303030303030303030338fff14cc0271ffc01002fa1362a2b1c83a73f5c73ade7fe3fd7ffe1f8f32b"
    "d5fc4a4dbaa449008220eeb738d8b8d8b3a6e362ce243dd1b703608187a683e1ae31ca7e2b85d7ab3ee5d076ec6e5561"
    "b14eb66aa94b3b3b3b3b3e38638635e35d35d35d35d35b3b3b3b3b3b3b3b3b3bcf3cf3c423410038070000842d887444"
    "1d09074221d100dfa7ffdbfe3ff7ffbf1c6b57c690924480e75dc481cf220c54514514501efdfeff6f3cf3cf3cf3cf3c"
    "f3cf3cf3cf3cf3dfbfdfeff7fbfdfeff7fbfdfeff7fbfdfeff7fbfdbcf3cf3cf3cf3cf3cf0079e79e79e79e79e79e79e"
    "79ed73981e931e1d17874de1d120744df7cf7effff6bf6ffdfff2ff4e35dc924935269eb08888d20c51a3468e38d6b5b"
    "e134d34c060606060606060606060606060606060606060734d33cd34cb9a69a67030303030303039780fff14cc02d7f"
    "fc0146341914c45ea4d52fed5f77cfeb1cc923711110f5841033b3d35d35d35ebdc6465a6c1ea9cef72f96a149e2f74c"
    "ced987b651d03dbc3dbc0e20e20e20e20e20e20e20e20e20e20e20e20e20e20e20606060606060606060606060606060"
    "606060606060606060606060606060606060606060606060606060606060606060606060606060606060606060606061"
    "08d0600fff03ffe993e97af7f7fc7172e4924448891088001e5b4d1e3c18181818181818181818181818181818181818"
    "18181818181818181818181818181934d34d34d34d34d34d34d34d34d34d34d34d34d34d34d34d306060606060606060"
    "606060606060606060606060606060606060606060606060606060606060606060606060606060606060606060606060"
    "606060606060606060606060606060606060606060606060606060606060606060606060606065312859d265666ff39f"
    "aebda4911225000007c9e79e79e4c33cf300000007";

constexpr std::string_view Aac4 =
    "fff14d00409ffcdc004c61766336312e332e31303000025ca55b69284d410b42e6dadaddeb5ad8d54c9907ca4924f2b1"
    "8962b0682cf54ed354ef3571ae2d8ae7ed53eed3282b61cca5ea699439da710cde5c8c3763e4113033b02890fc15124f"
    "d57b4f9d70b956538ec4dca76b53b1d1ad9f5b3eb6acc746c746bf3554a552954a552934a4d125125125125125125125"
    "030303030303030303031b366c191229659658a28a28a28a28a28a28a28a22129c505960e8cd795cf9ad56f8d4a96b92"
    "223ad2492488815552b796b9d35268898c373758cf5b5b6c9b693191122532d129454891030303030368606060637a58"
    "181b43220606366c181912206366cd8322448911b366e89b1af2b2b3c7afc6b5e65dda6ed247749224b88418e18e18e1"
    "357357360d86495d15d15cd5b618c98c94c94c98e13613613618e18c98c98c934934933b3b3b3b3b3b3b3b3a3b3b3b0b"
    "3b3b3a3a3b3b0b0b0b019229d6ea3a174c4773fc746a9a937170c9891019249d4840ed6ed5b6ad91854c953254c95a9a"
    "5a9a5a9a62d4ee159f75b6e1ef2fa8ff4acd1e0c6bb6013bb87e16549a6b5c9cb3c9dcc791646e594e2ae366b8d9ab33"
    "d1b1cd5f9abf3554a55293449a24a24a24a24a24a060606060606366c181912294d9b372cb2cb2cb2cb1451451451451"
    "45145145145145145145145145145145145145145145145145145145145145145145145cfff14d00505ffc011a96dac9"
    "5db447bb25dd94edfff7bfd3efd70f4d5eafc7ffdcffbfd75c71ad6b5fdfffe97fd7efe75d6af5afe9fff77fcfd75aeb"
    "5ad683fb942835145064dfc2d73a1f7cba738467aa3afced6fe7a8d6fe7e735bf9f9cd737f3fe7e71ae6fe7fcff98d7a"
    "dfcff9ff313754206ce8788f1c0a830106e257d229fe8c88a757d3e91453cff4fa4514f3fd3e9cf1442750fa7d3e9144"
    "279c7d3e9f48a229e755b5f6d6d731c38b099824e04849c0909381212124296d89db2576d94eec9649653b67ebe78b6d"
    "5deb5ff8dfd79d6b8d5eb5fff733efd6b5c6af5afffb5cfdfad6b8d5eb427d06f728a28a21bf37b4a0462e9edd8acd66"
    "f37794181893831b06cddc18181893b830303033b83030303033bbb830303030366eee0c0c0c0c0c0ceefbaf50cee243"
    "bc7539985ad846d6cfa738fa6be7007d3e9f460012121255dc24242424abbbb848484849577770909090909baeefee9d"
    "1ca372830303030303030303030303030303034bd92bb6c92f20d083b22bc871649794da5ffe1ff7f6e34ead4d6bfbff"
    "6ffadffd7ffc3fb7fdd9fdfff2eff7e3cf1ae3ee16a54118d45141914a0113a685e1e0898441212ee0c0cce750170d45"
    "ea88af1c2caa2195c06079019103220646e8191ba06292db512bb6c977647bb25dbffe93febedc3e7535af6fffcfd7fd"
    "fdb8f3ad6b5f3fffa7cffd7db8eb8d6b5ebffeaeffefedc75c6b8b195759c0c6a283585cb30dd4f9f7c68e12083094f4"
    "5ccf088b983cf4517cb99e7a28be5f279e645117cbe5ccf3cc8a22f97cbe4f3cf45145f2f97c9dfdb9d95f81c3abb049"
    "48c518ce3ea9c5ea8eb1fcb66b1fcbf96b98fe5fcbf96b987f2fe5fcb5cc3f97f2fe5af5b9b6560208a288a28a28a28a"
    "2e3145145170fff14d00249ffc011af38b3a120eb1efce2fe7c73cff1ff5fad752f52eeee5bcf5a92e49244077fe3e3e"
    "20cf9fdfddd0d8f7f7847c7c3633fbe8193e0c33fb90c9f0d8f7f7847c7c187bfb81f1f00193efee010bce29cafb077c"
    "ff5ff6fbaf8f8deaf4e2b4b971aa69249249203ff199b2f8f8f86f77bfbe81f1f0d8f7f784ff1f0da7dfdc0f8f809f7d"
    "01ff8cc1df7d13a500091f4241d08875421655fbfafd3d7f8e3afbce1ad5c92e3a2f5725c8924fb877dfde17df3abdf4"
    "680ffc7c04fbe807f84fbfb91fe3e0c3dfdc0f8f809f7f707f8f803e00f7d03f81fc7c23cde23161019279c58d7b7935"
    "cf29bf19ffe1ff3e7eee25eaeede4d25c92492027dfdfdc9fc9f0da5df7f7f705efa3dca1c9f198cefbe803e1b0efbe8"
    "0ffc660efbe80ffc661cfff14d0035bffc00fa330be5d1332aa93edffe1ffe1fffc3dbcf5d70108911107587d1e79e79"
    "e798739c6963c1e63cc79efa6ba28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a"
    "28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28fd747ea8a28a28a28a28a28a28a28a28a2"
    "8a282119c4295f4641d4011fdffedff1ffdffebc71abbd49224911cf26a11106b3a54a79e79ee7fa7d3e9f4fa6bd7451"
    "45145145145145145145145145145145145145145145143e17cda277cefbf9fffb7f3ffe3ff5fd4972492224912220ee"
    "81294bcf252c3471a5e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79"
    "e79e79e79e79e79e79e79e79e79e79e79e7bf7fbfdfef4bcf3cf3cf3cf3cf3dfbfdfeff7fbfdbda5e79e79e79e7bf7fb"
    "fdfeff7fbfdfeff7fbfdfeff7fbfdbcf3cf3cf3cf3c05e46325f5242d09555595953d7ffe6ffdfffca6ee5dc8f8b5a49"
    "3f1340181914b2cb2cb2ca0df2606fab3f4feb7d2df26f936869659659659659659659659659659659658a28a28a28a2"
    "8a28a28a28a2e0fff14d003d9ffc0102308be20aa4fcfff87ff87fffcf8f3d6ae5c89224480f97cbe5f2f94f3c294a47"
    "309d53abe53fcbe5f2f97ca28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a2"
    "8a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a22119c4325f52077edffd7f8ffcffefc6b577a97"
    "24924473c912480ddb830b366cd830303030303030303030303030303030303030303120606bc0c0d7af5ebd7af5ebd7"
    "9e93e873df3dfcfffdaf9ffdfff2fc692492489224444402c183060cb20654c30e181818181818181818181818181818"
    "181818181818181818181818181818181818181818181818181818181818181818181818181818181818181818181818"
    "181818181818181818181818181818181818181830303030303030303030303030303030303030303030303030303030"
    "303030303030303030303030303030303030303030303030303030303030303030303030300bc8c82f94aacacaccefed"
    "fffcbffc7ff5b97772e4b9249224a1f4e779e79e79e79e0f04861079e79e79e79e79e79e79e79e79e79e79e79e79e79e"
    "79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e"
    "79e79e79e79e79e79e79e79e79e79e79e48038fff14d003abffc01022f91364431134c420a27afb7fc7ffaf117a97248"
    "ea922750048484848484849c885cc8d82061e9a0ded0d712cb8964f1dd1212127024e04848484dd092b75e79e79e79e7"
    "9e79e79e79e79e79e79e79e79e79e79e78846710a5b1210c43a2013f3fff6ff8ffdffefc6af5c7164489122244838c1e"
    "91062a28a28a28a3e9f4fa3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cc79e79e79e7980079e79e798"
    "035ebd731af5ebd7af5ebd7ac03cf3cf3cf3cf7eff7fbfdfeff6f3cf3cf3cf3cf3dae7303d25fd881d0a0742e79e7bf7"
    "fffb5fb7fefff97dfa92492489a001f0f00628d1a3471c6b5ad6f2cb2cb2cb2cb2ca665033328199940c0cc818181999"
    "65940c0e5940c0c0c0c0c0c0c0c0c0c0c0c0c0c0e5033e330028ab1a61fffeab32b33733ffecffefe65dddc972492488"
    "880f7af5ebc3060cbc19af11a4639204eec22d53965d72b940303030b06160c0cbc185af0606160c2d7832f5ebc18181"
    "9783030b5ebc18197af5e0c2d7af5ebc185af5ebd783032f5ebd7af06060616bd7af06060606060616bc181818181818"
    "1818185af5ebd7af5e0c0c0c0c0c0c0c18185af5ebd7af5e0c0c0c0c0c0c0c0c2c18185af5ebd7830303030303030303"
    "0303030303030338fff14d00437ffc014a338b48bd12aaab2ab37f6fdf8b97248891222107887d1e79e79e79e79ec827"
    "54f3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cffafd754f3cf3cf3cf38606065ebd78303030303030"
    "30303030b5ebd783030303030303030303030303030b5e0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c2d7af060"
    "606060606060606060606060606060606060606060606108ce2ad3e86fd7bfe38b497244444488884203196d3478f064"
    "d31e0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c"
    "0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c"
    "0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c"
    "0c0c0c0c0c0c0c0ca82bfbb44accacfce7ebad4b92491128000ee83e8f3cf3cf2619e79e79e79e79e79e79e79e79e79f"
    "f5fa9e79e7fd7ea79e79e7fd7ebaa70348338be44168482552aaa95dfedfaf57772e244911110f10f97ca79e79e79e79"
    "e79cf39db783cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3"
    "cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf7eff7fbfdfeff7a5e79e79e79e79e03030303030"
    "303030303030303030303fdfeff7a5e79e7b80";

constexpr std::string_view Aac5 =
    "fff14d4042dffcdc004c61766336312e332e31303000025ca45ba9084d539b5e669ceb57546a953707ca4924906258ac"
    "1a0b3d53b4d53ba3b48e2d8ad3d64f7f766d4409547d4d32873b4e219bcb9186ec7c8226067605121f82a043e8dd6bcc"
    "5ad716c56cac4dca76b53b1d1ad9f5b3eb6acc746c746bf3554a552954a4d293449a24a24a24a24a24a24a24a0606063"
    "60c0c0c8914a6cd9b9659659659484a613d6664a275c677e5e353ad222e4888ed492492220ce5ae74d71ef154fa42630"
    "dcdd633d455c4e95cd5b618f6f5eaa7553236135735733b3b48c0c0c0c6f4b0303686440c0c6cd830322440c6cd9b064"
    "489122366cdd4a674e462b2b3c7afc6b5e65dda5cb4b8e69224f322401212134d284c2dd7af5eb830322a00c6cd9b060"
    "6448911b366cda2448a506366cdca8a5965965360c8914b29b372cb14514514514514514514514514514514514514514"
    "514514514514514514514514514514446530a24cea487a130a299dfedf7f6ea5dcb921e5249249f05809090909090909"
    "09090909094a12e7ebd7af5c181818181818d8303030303030303030303030303031b360c0c8914b2cb2cb2c51451451"
    "45145145145145145145145145151a63c241d3907c641028a5678fd2ff7e35a972ee3c52493c8b012121212121212121"
    "2121212121212138547115deb9b06452831b360c0c0c0c0c6c1818181819122941818181914514514514514514514514"
    "514514514517fff14d40615ffc011a96dac95db647bb25dd94edfff7bfd3efd70f4d5eaf3ffe97fdfebae38d6b5afeff"
    "ff4bfebf7f3aeb57ad7f4fffbbfe7ebad75ad6b41fdca141a8a28340abb0cf3dc4a7da174e708cf5475f9dadfcf51adf"
    "cfce6b7f3f39ae6fe7fcfce35cdfcff9ff31af5bf9ff3fe626ea840d9d0f11e381506020dc4afa453fd19114eafa7d22"
    "8a79fe9f48a29e7fa7d39e2884ea1f4fa7d22884f38fa7d3e914453ceab6bedadae638716133049c0909381212702424"
    "24852db13b64aedb29dd92c32ca76cfd7cf1c36aef5affd37f5e75ae357ad7ffdccfbf5ad71abd6bffed73f7eb5ae357"
    "ad09f41bdc8d145116fcded28118ba7b762b35bcacbca0cf2833b9b94199770694181818181819ddddc181a5060665dd"
    "c1818d8303033bb8303030303036ebd433b890ef1d4e6611a861b5b0df46187d35bc00fa7d1e07f2fe5ae63f97f2fe5a"
    "c1fcbf97f2d60fe5fcbf96b9bdd3a3952ca0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0d2f6bc93db644fd43ee57f56bd26d2"
    "fff0f8ffefc69d5a9ad7f7feddd29fffe47ff5fe2ea80ff9fef5bfff5e3cf1ae3ee16a54116145141914a0113a685e1e"
    "8402425dc181aff3d9ee397e384865510ca00303c80c881910323740c8dd234b6c45512bb6c977647bb25dbffe93febe"
    "dc3e7535af6fffcfd7fdfdb8f3ad6b5f3fffa7cffd7db8eb8d6b5ebffeaeffefedc75c6b8b195759c0c6a283585cb30d"
    "d4f9f7c68e12c606129e8b99e11173079e8a2f9733cf4517cbe4f3cc8a22f97cb99e7991445f2f97c9e79e8a28be5f2f"
    "93bfb73b2bf0387576092939ce319c7d538bd51d63f96cd63f97f2d731fcbf97f2d7ac3f97f2fe5ae61fcbf97f2d7adc"
    "db2b0104514451451451451718a28a289f2895db644f92daac89f2578eab25cea7ff1fff6bffd7eba7cf17abfd7ffe2f"
    "3c7d75e78d55d7ffc4e3ebaf3c7fc65f9fffb9fff6fffe1f5d79e35561ad78ec879b9959f799b3ef33623c3a94a42a54"
    "f3cf3aa754eae69e79e79fe53f34f3cff2e69fe53cfcd3cf88f0eb97795cbbca94a294a9e79e754ea9d53cf3cf3cf3cf"
    "3cf3cfdbb3f08ec54f389e79e79e79f8fff14d402c5ffc011af319c4bd090b4a41d1905efce55e78e79fe3febc7c65de"
    "a5ddddc79f0e25c9f172580306666666427c1f07c1f00c10264007777c1c0fe3e1b19fdf40c9f0619fdc864f86c7bfbc"
    "23e3e1b4fbfb90f8f800c9f7f723fc7c04fbe80327c660efbc207c3613ee442f30a72bebddf3fd7fdbeebe3e37abd38a"
    "d4972daa69249249207fe33365f1f1f0deef7f7d03e3e1b1efef09fe3e1b4fbfb90f8f80efbfb87fe33077dfde74afaf"
    "654f5ebf4f5fe38ebef389a5e92e3a12e25c89240efbfbc2fbe757be8d01ff8f809f7f707f8f809f7f784ff1f061efee"
    "0ff1f064fbfb8237984895f5ee726b9de5e7be7ff87fcf9fbce2eef51e91a4b9249240efbfbfb93ff1f0da53efefee0b"
    "dcbdca1c9f198cefbfb81f1983befa09ff8cc1df7d12257d7b738ef26bc55f8dfe9ff1d7fd7e3ae25e9ef772d7249248"
    "1ff8f833befa0f93e333b9fdc9f1901c8423e3e0c3dfdf40f8f830f7f721f1f0d8e0fff14d40493ffc00fa330be5d133"
    "2aa93edffe1ffe1fffc3dbcf5d70221111107587d1e79e79e798739c6963c1e63cc79efa6ba28a28a28a28a28a28a28a"
    "28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a"
    "28a28a28a28a28fd747ea8a28a28a28a28a28a28a28a28a28a28211984295f4641d400dfdffedff1ffdffebc71abbd4b"
    "4892239e4e211106b3a54a79e79ee7fa7d3e9f4fa6bfa6ba28a28a28a28a28a28a28a28a28a28a28a28a28a28a28a287"
    "c2f9b42ef9df7f3fff6fe7ffc7febfaa492492488912220ee094a5e792961a38d2f3cf3cf3cf3cf3cf3cf3cf3cf3cf3c"
    "f3cf3da5e79efdfede79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e7bf7fbfdfef4"
    "bcf3cf3cf3cf3cf3dfbfdfeff7fbfdbda5e79e79e79e7bf7fbfdfeff7fbfdfeff7fbfdfeff7fbfdbcf3cf3cf3cf3c463"
    "30792beb0062ab2b32bbf5fff9bff7fff7497a972df16b4924907cbe5f2f9658a28a2891951303184514514514514514"
    "514514514514514514514514f81511634b3fffee6673cf7afffb19ffe5c4b977249244840381818181e207181a30348d"
    "7049ec0319832f843360036670303030303940c0cc8199940c0c0e503030303030303030303030303320606060607281"
    "8181818199030303030303030303030303030303030303030303030303030303332ca060606060606060606060606060"
    "60606060606060606060606060606060666503030303030303030303030303030303030303030303039780fff14d4047"
    "9ffc0102309936448e8c04dff7ffb7fff75d6aee4892248734076767676767676743dc66d71ea303486b5c3a457730b1"
    "2145f460606bc0c0c0c0c0c0d2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2cb2c"
    "b2cb2cb2cb2cb2cb2cb2ca42330864bea40cfdbffaff1ff9ffdf8d6aef52e49249239e4892406edc1859b366c1818181"
    "81818181818181818181818181818181818903035e0606bc0c0d7af03035ebcf49f3b9e7befe7ffed7cffefff97eb6b9"
    "1248922448840583060c19640ca9861c3030303030303030303030303030303030303030303030303030303030303030"
    "303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030"
    "303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030"
    "3030303030303030303030308c660f05f108c04aaacaacccefedfffcbffc7ff5b97772e4b9249244a1f4e779e79e79e7"
    "9e0f04861079e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e798f3cf3cf303cf3c"
    "f3c03cf3cf3cc01e79e79e78079e79e79e79e79e79e79e79e980d7af5ebd7af5eb009f02922c68af3107c241f090742f"
    "fffecdf3e3b9fff17afff2e25ddcb92c1ec90ca01eef8a24448891114bde1b600caa61f0847800ea9e59659659659659"
    "659659659659659659659659659659659659659628a28a28a28a28a28a28a28a28a28a28a28b80fff14d404c1ffc0102"
    "2f911654390f48440a267f5ff8fff5e2af5772e23aa49c4401212121212121270130b991b040c3d341bda1ae259712c9"
    "e3a121212702424242424242424add092b779de79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e"
    "78846610a5f188744017e7ffedff1ffbffdf8d5eb8e2c891244489107183d220c545145145147d3e9f479e79e79e79e7"
    "9e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e67ede79e79e79e79e6001e79e79e79e7bf"
    "7fbfdfef4bcf3cf3cf3cf3cf3dfbfdfeff7fbfdbcf3cf3cf3cf3cf6b9cc0f49af4a07428173cf3dfbfffdafdbff7ffcb"
    "efe52e2492246803cbc031468d1a38e35ad6b746072cb2ca06060606665033328199940c0cc818181818181818181cb2"
    "81818181ca0606060606072cb2cb2818181818181818181818181818181ca463309f00a2ac68a23a1dfffeab2b3332ab"
    "ffecffefecbd5cb924969244203debd7860c18306066bc46918e4813bb08b54e5975cae50060606160c2c18181818181"
    "8181858303030303030b0606065e0c0c2d7af7ebf53cf3febaa79e79e79e79e79e79e79e79e79e79e79e79e79e79e79e"
    "79e79e79e79e79c303030303030303030303030303030606060606060606060606060606060606160c0c2d7af5ebc181"
    "818181818181818181818181818193e014200d0b644368883e52200dffffe2b9e7bc7ffdaf7fff8ed25c694e94579901"
    "c2424ae38e32e28916963ab4e99880df0a4cf2b842cd001b450160c0c0d2cb2cb2cb2cb2cb2cb2cb2cb2c51451451451"
    "4514514514514514514514514514514514514514514517fff14d4040fffc014a330b48bd0aaaaab2b37f6fdf8b972491"
    "1122220788fa3cf3cf3cf3cf3d904ea9e79e79e79e79e79e79e79e79e79ff5fa9e79e79ff5faea9e79e79e7fd7ebaa79"
    "e79e79e79c303032f5ebc181818181818181818185af5ebc18181818181818181818181818185af06060606060606060"
    "6060606060606060606060606060606060606060606060606060606060606060606060606060606108cc2ad3e76fd7bf"
    "e38b5c924488888888420632da68f1e0c983030303030303030303030303030303030303030303030303030303030303"
    "030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303"
    "030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303030303"
    "03030303030303030303030303030303032a0a089f668559959f9cfd75a977248908a000ee1f479e79e7930cf3cf3cf3"
    "cf3cf3cf3cf3860606060606065e0c0c0c0c0c0c0c0c0c0c0c0c2d7830303030303030b5ebc46330a42bebca5552ab39"
    "fdbf5eaeee5c91ef1110840f97ca79e79e79e79e79cf39db783cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf3cf52a5f4e"
    "41d380a159bfecff3c6ae5c79c924bf88420090909090909090909090909090909094a12f6a9cdca6e59659659659659"
    "659659659659658a28a28a28a2e0";

struct Sideband {
    std::int32_t result;
    std::int32_t internalResult;
    std::int32_t consumed;
    std::int32_t produced;
    std::uint64_t total;
    std::array<std::uint32_t, 8> extra;
};

static_assert(offsetof(Sideband, extra) == 24);

std::vector<std::uint8_t> FromHex(std::string_view text) {
    const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    std::vector<std::uint8_t> result;
    for (std::size_t i = 0; i < text.size(); i += 2) result.push_back(static_cast<std::uint8_t>(digit(text[i]) * 16 + digit(text[i + 1])));
    return result;
}

std::vector<std::span<const std::uint8_t>> Frames(const std::vector<std::uint8_t>& stream) {
    std::vector<std::span<const std::uint8_t>> result;
    for (std::size_t offset = 0; offset < stream.size();) {
        CHECK(stream.size() - offset >= 7);
        const std::size_t size = ((std::size_t{stream[offset + 3]} & 3) << 11) | (std::size_t{stream[offset + 4]} << 3) | (stream[offset + 5] >> 5);
        CHECK(size >= 7 && size <= stream.size() - offset);
        result.emplace_back(stream.data() + offset, size);
        offset += size;
    }
    CHECK(result.size() == 7);
    return result;
}

class Batch {
public:
    explicit Batch(std::uint32_t owner) : context(owner) {
        CHECK(sceAjmBatchInitialize(storage.data(), storage.size(), &info) == 0);
    }

    void Submit() {
        AjmBatchError error{};
        std::uint32_t id = 0;
        CHECK(sceAjmBatchStart(context, &info, 0, &error, &id) == 0);
        CHECK(sceAjmBatchWait(context, id, 0, &error) == 0);
    }

    AjmBatchInfo info{};

private:
    std::uint32_t context;
    std::array<std::uint8_t, 1024> storage{};
};

std::uint32_t Create(std::uint32_t context, std::uint32_t channels, bool raw, bool floating, bool delay = false) {
    std::uint32_t instance = 0;
    CHECK(sceAjmInstanceCreate(context, 2, (std::uint64_t{channels} << 3) | (floating ? 2ull << 7 : 0) | (delay ? 0 : Nondelay), &instance) == 0);
    Batch batch(context);
    std::array<std::int32_t, 2> result{-1, -1};
    const std::array<std::uint32_t, 2> config{raw ? 2u : 1u, 3u};
    CHECK(sceAjmBatchJobInitialize(&batch.info, instance, config.data(), sizeof(config), result.data()) == 0);
    batch.Submit();
    CHECK(result[0] == 0 && result[1] == 0);
    return instance;
}

Sideband Run(std::uint32_t context, std::uint32_t instance, std::span<const std::uint8_t> input, void* output, std::size_t capacity, std::uint64_t flags = 0) {
    Batch batch(context);
    Sideband result{};
    CHECK(sceAjmBatchJobRun(&batch.info, instance, flags | Stream, input.data(), input.size(), output, capacity, &result, sizeof(result)) == 0);
    batch.Submit();
    return result;
}

void CheckSignal(const std::vector<float>& pcm, std::size_t channels) {
    CHECK(pcm.size() == 7 * 1024 * channels);
    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (std::size_t frequency = 0; frequency < channels; ++frequency) {
            double cross = 0, signalEnergy = 0, referenceEnergy = 0;
            for (std::size_t i = 2048; i < 6144; ++i) {
                const double signal = pcm[i * channels + channel];
                const double reference = std::sin(2 * 3.14159265358979323846 * Frequencies[frequency] * (i - 1024) / 48000.0);
                cross += signal * reference;
                signalEnergy += signal * signal;
                referenceEnergy += reference * reference;
            }
            const double correlation = cross / std::sqrt(signalEnergy * referenceEnergy);
            if (!(channel == frequency ? correlation > 0.9 : std::fabs(correlation) < 0.05)) std::fprintf(stderr, "channels %zu channel %zu reference %zu correlation %f\n", channels, channel, frequency, correlation);
            CHECK(channel == frequency ? correlation > 0.9 : std::fabs(correlation) < 0.05);
        }
    }
}

std::vector<float> Decode(std::uint32_t context, std::uint32_t channels, bool raw, const std::vector<std::uint8_t>& input, bool floating, bool delay = false) {
    const auto instance = Create(context, channels, raw, floating, delay);
    const auto frames = Frames(input);
    const std::size_t sampleBytes = floating ? 4 : 2;
    std::vector<float> pcm;
    for (std::size_t index = 0; index < frames.size(); ++index) {
        auto packet = frames[index];
        if (raw) packet = packet.subspan(7);
        std::vector<std::uint32_t> storage(1024 * channels + 2, 0x5a5a5a5a);
        auto* output = reinterpret_cast<std::uint8_t*>(storage.data()) + 4;
        const auto result = Run(context, instance, packet, output, 1024 * channels * sampleBytes, channels == 4 ? Format : 0);
        const std::size_t produced = delay && index < 2 ? 0 : 1024 * channels;
        CHECK(result.result == 0 && result.internalResult == 0 && result.consumed == static_cast<int>(packet.size()));
        CHECK(result.produced == static_cast<int>(produced * sampleBytes));
        CHECK(result.total == (delay ? (index < 2 ? 0 : index - 1) : index + 1) * 1024);
        if (channels == 4 && produced != 0) CHECK(result.extra[0] == 4 && result.extra[1] == 0x33 && result.extra[2] == 48000 && result.extra[3] == (floating ? 2u : 0u));
        CHECK(storage.front() == 0x5a5a5a5a);
        CHECK(storage[(1024 * channels * sampleBytes + 4) / 4] == 0x5a5a5a5a);
        for (std::size_t sample = 0; sample < produced; ++sample) {
            if (floating) {
                float value = 0;
                std::memcpy(&value, output + sample * 4, 4);
                pcm.push_back(value);
            } else {
                std::int16_t value = 0;
                std::memcpy(&value, output + sample * 2, 2);
                pcm.push_back(value / 32768.0f);
            }
        }
    }
    Batch query(context);
    std::array<std::uint32_t, 4> codec{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    CHECK(sceAjmBatchJobGetCodecInfo(&query.info, instance, codec.data(), sizeof(codec)) == 0);
    query.Submit();
    CHECK(std::all_of(codec.begin(), codec.end(), [](auto v) { return v == 0; }));
    std::array<std::uint32_t, 8> format{};
    format.fill(0x5a5a5a5a);
    Batch info(context);
    CHECK(sceAjmBatchJobGetInfo(&info.info, instance, format.data()) == 0);
    if (channels == 4) {
        info.Submit();
        CHECK(format[0] == 0 && format[1] == 0 && format[2] == 4 && format[3] == 0x33 && format[4] == 48000 && format[5] == (floating ? 2u : 0u));
    } else {
        try { info.Submit(); }
        catch (const std::runtime_error&) {
            CHECK(std::all_of(format.begin(), format.end(), [](auto v) { return v == 0x5a5a5a5a; }));
        }
    }
    CHECK(sceAjmInstanceDestroy(context, instance) == 0);
    return pcm;
}

void CheckFormatGuard(std::uint32_t context, std::uint32_t channels, bool raw, const std::vector<std::uint8_t>& input) {
    auto instance = Create(context, channels, raw, true);
    const auto frame = Frames(input).front();
    const auto packet = raw ? frame.subspan(7) : frame;
    std::vector<float> output(1024 * channels, -123.0f);
    Sideband status;
    std::memset(&status, 0x5a, sizeof(status));
    const auto original = status;
    Batch batch(context);
    CHECK(sceAjmBatchJobRun(&batch.info, instance, Stream | Format, packet.data(), packet.size(), output.data(), output.size() * 4, &status, sizeof(status)) == 0);
    bool threw = false;
    try { batch.Submit(); } catch (const std::runtime_error&) { threw = true; }
    if (threw) {
        CHECK(std::memcmp(&status, &original, sizeof(status)) == 0);
        CHECK(std::all_of(output.begin(), output.end(), [](auto v) { return v == -123.0f; }));
    } else {
        CHECK(sceAjmInstanceDestroy(context, instance) == 0);
        instance = Create(context, channels, raw, true);
    }
    std::fill(output.begin(), output.end(), -123.0f);
    const auto normal = Run(context, instance, packet, output.data(), output.size() * 4);
    const auto fresh = Create(context, channels, raw, true);
    std::vector<float> expected(output.size());
    const auto freshResult = Run(context, fresh, packet, expected.data(), expected.size() * 4);
    CHECK(normal.result == 0 && freshResult.result == 0 && output == expected);
    CHECK(sceAjmInstanceDestroy(context, fresh) == 0);
    CHECK(sceAjmInstanceDestroy(context, instance) == 0);
}

void CheckMixedFormatGuard(std::uint32_t context, const std::vector<std::uint8_t>& four, const std::vector<std::uint8_t>& three) {
    const auto first = Frames(four).front();
    const auto second = Frames(three).front();
    std::vector<std::uint8_t> mixed(first.begin(), first.end());
    mixed.insert(mixed.end(), second.begin(), second.end());
    auto instance = Create(context, 5, false, true);
    std::vector<float> output(2048 * 5, -123.0f);
    Sideband status;
    std::memset(&status, 0x5a, sizeof(status));
    const auto original = status;
    Batch batch(context);
    CHECK(sceAjmBatchJobRun(&batch.info, instance, Stream | Format | Multiple, mixed.data(), mixed.size(), output.data(), output.size() * 4, &status, sizeof(status)) == 0);
    bool threw = false;
    try { batch.Submit(); } catch (const std::runtime_error&) { threw = true; }
    if (threw) {
        CHECK(std::memcmp(&status, &original, sizeof(status)) == 0);
        CHECK(std::all_of(output.begin(), output.end(), [](auto v) { return v == -123.0f; }));
    } else {
        CHECK(sceAjmInstanceDestroy(context, instance) == 0);
        instance = Create(context, 5, false, true);
    }
    std::fill(output.begin(), output.end(), -123.0f);
    const auto result = Run(context, instance, first, output.data(), output.size() * 4, Format);
    const auto fresh = Create(context, 5, false, true);
    std::vector<float> expected(output.size(), -123.0f);
    const auto freshResult = Run(context, fresh, first, expected.data(), expected.size() * 4, Format);
    CHECK(result.result == 0 && freshResult.result == 0 && output == expected);
    CHECK(sceAjmInstanceDestroy(context, fresh) == 0);
    CHECK(sceAjmInstanceDestroy(context, instance) == 0);
}

void CheckBulkAndReset(std::uint32_t context, std::uint32_t channels, const std::vector<std::uint8_t>& input, const std::vector<float>& reference) {
    const auto instance = Create(context, channels, false, true);
    std::vector<float> output(reference.size());
    const auto result = Run(context, instance, input, output.data(), output.size() * 4, Multiple);
    CHECK(result.result == 0 && result.consumed == static_cast<int>(input.size()) && result.produced == static_cast<int>(output.size() * 4));
    CHECK(result.total == 7 * 1024 && result.extra[0] == 7 && output == reference);
    Batch reset(context);
    std::array<std::int32_t, 2> status{-1, -1};
    CHECK(sceAjmBatchJobClearContext(&reset.info, instance, status.data()) == 0);
    reset.Submit();
    CHECK(status[0] == 0 && status[1] == 0);
    const auto restart = Run(context, instance, input, output.data(), output.size() * 4, Multiple);
    CHECK(restart.result == 0 && restart.total == 7 * 1024 && restart.produced == static_cast<int>(output.size() * 4));
    CheckSignal(output, channels);
    CHECK(sceAjmInstanceDestroy(context, instance) == 0);
}

} // namespace

int main() {
    std::uint32_t context = 0;
    CHECK(sceAjmInitialize(0, &context) == 0);
    const std::array<std::vector<std::uint8_t>, 3> streams{FromHex(Aac3), FromHex(Aac4), FromHex(Aac5)};
    for (std::uint32_t channels = 3; channels <= 5; ++channels) {
        const auto& input = streams[channels - 3];
        const auto floating = Decode(context, channels, false, input, true);
        CheckSignal(floating, channels);
        CheckBulkAndReset(context, channels, input, floating);
        CHECK(Decode(context, channels, true, input, true) == floating);
        const auto integer = Decode(context, channels, false, input, false);
        CHECK(integer.size() == floating.size());
        for (std::size_t i = 0; i < integer.size(); ++i) CHECK(std::fabs(integer[i] - floating[i]) <= 0.5f / 32768);
        const auto delayed = Decode(context, channels, false, input, true, true);
        CHECK(delayed.size() == floating.size() - 2048 * channels);
        CHECK(std::equal(delayed.begin(), delayed.end(), floating.begin() + 2048 * channels));
        if (channels != 4) {
            CheckFormatGuard(context, channels, false, input);
            CheckFormatGuard(context, channels, true, input);
        }
    }
    CheckMixedFormatGuard(context, streams[1], streams[0]);
    CHECK(sceAjmFinalize(context) == 0);
}
