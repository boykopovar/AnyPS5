#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

struct TestBignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

using Bytes = std::vector<unsigned char>;

constexpr int BignumStaticData = 0x02;
constexpr unsigned char Fill = 0xEE;
constexpr std::size_t Slack = 8;
constexpr std::uint64_t RsaError = 0x4fffull << 12;
constexpr std::uint64_t MallocFailure = RsaError | 65;
constexpr std::uint64_t BadSignature = RsaError | 104;
constexpr std::uint64_t DataTooLarge = RsaError | 109;
constexpr std::uint64_t DataTooLargeForKeySize = RsaError | 110;
constexpr std::uint64_t FirstOctetInvalid = RsaError | 133;
constexpr std::uint64_t LastOctetInvalid = RsaError | 134;
constexpr std::uint64_t SlenRecoveryFailed = RsaError | 135;
constexpr std::uint64_t SlenCheckFailed = RsaError | 136;

enum Digest { DigestMd5, DigestSha256, DigestSha384, DigestSha512, DigestNone };
enum AddKind { AddExact, AddRefused, AddSalted, AddThrows };

struct PssCombo {
    int bits;
    Digest digest;
    const char* mHash;
};

struct PssAdd {
    int combo;
    int saltLength;
    AddKind kind;
    const char* bytes;
};

struct PssVerify {
    int combo;
    int base;
    int position;
    unsigned char flip;
    bool flipHash;
    int saltLength;
    int expected;
};

constexpr PssCombo PssCombos[] {
    {513, DigestMd5, "21ac5b1de9709b054841f3ad8f60410d"},
    {513, DigestSha256, "8320f7e170b4295d6896339c07978bb176e62c209b44e0b4448b4aef9377f1c3"},
    {1031, DigestSha384, "26c7d499979711ac52003600091fa8e51280f49d22b8e8014ac6c773735abf52b81b8e98e34ceb4a29ebc4e9050dcd45"},
    {2048, DigestSha256, "45ba0de80f8312facc1f618e2e59c0d1c25df482bd46ab4f831fdb64ec2c077d"},
    {2049, DigestSha512, "1f125516228564a8d327dd0990427670c4881b8812094ea1e12e5cb857f9fd024116abdbaaed22239f4e29922ae4c0be0288a234a5120a50fdaeb37ed9664ad7"},
    {513, DigestNone, ""},
    {128, DigestMd5, "70361cc2adaf78b8ffdafbfa8a524bd8"},
    {129, DigestMd5, "9e8bc46fb11ac5f30de5cb7c99660e2c"},
    {136, DigestMd5, "5a05baafd1c01cc47eb9686e91ec0171"},
    {137, DigestMd5, "bd5602a03d6346ae03999c6e151a945d"},
    {144, DigestMd5, "2f187ddf20c753036a45e0d94355ea2a"},
    {145, DigestMd5, "84b14089afe8aa32c6356eb961864c15"},
    {140, DigestMd5, "6dc40f213a3cd34a4edba200de020004"},
    {153, DigestMd5, "e4b5b7034b31c3e8f26afcc3bbea19b3"},
};

constexpr const char* PssBases[] {
    "00f4bd0bf2c44a8ccbd882d559cad6a386245b73dc6539c41a2d9f8235f1b0a08b2ffef65036982d4bed9e54ac1fb198b6b0980a1e9f3677ea918385799edb74bc",
    "00086972df57b13ba30aad31321db52de6826d8c16a0ea3e67feba1af3ed013ff7d076d8d614cd01f6bba7653de0a7637e8992139b1b29b596f536a217bad2babc",
    "00e39045ebf81031fb47bcd4e54c2c1e0005e27cf89cb2aa4ed5061679a1af5bdf986cc39c195c439a9e62b061100359d4e0c882746b7b8a606073037ab98152bc",
    "00aa4a51b1a2f4ee6640a8416f710a410a522b4cf0f42f7d4a602b407cf646166fd8576aee18b3238e7ae7038e673858201008da043cd5b99aa2fa3c9007b69dbc",
    "00731dd86a968018a75d3bea08114cf776925ab01105e4912234f975cb06c3359a4e08c5af9870493a34af95b87f2c94f612ac5101b2abc9ae4e7889bfe9acb9bc",
    "0030a4bd4926664219b884ed1755500cf049d97aa01a38e248ea709d74ee185eb6fcf154155eaf9ea7d77633ff79520dfe98128914a51c702c623a0df54b678fbc",
    "00e8625f59f364d4a379adaf09b109d6d1da124cc4bf6bc964cedb0c761fa462a906c4f91198deeb422189d003fd6edf402419093c915c2c3da3aa1e6a036525bc",
    "009757ed0300d5f56df28cb588088c8ffe542a1bbf582b47ece20fd386030bca2f182f6cf2c031e01d5b4bd0dfe82e7a0de413de686d62066f014d648171de41bc",
    "00d0a8333da28ada1b6005000b4dc1f0d7096648740c6dabb1d97e8b9cad13877f37c813b775669f2b0c99c5d10cfe4456bde21391007cbad7fe4b7c6bf6d8efbc",
    "34c59fa0fe3b39bf0fdbd312562b0a729ce56c11c942b18522dcaa22709a7031c827edba370f07709e160bb32b1372604bcfa1e70e1bf6d06cb260f154fc3f2c813f655d3608a65093149b1b696710520950a7df545592ed98275e43fda29760244f3ad45701facedbd9f79b9116aae630c3081e7aca4e1fdc8621021abbfb65bc",
    "09be5a1e7d85f97beaf78f793aa20c4d44fd64fa1910dc1cefd3bd2959015618a2f33453757b2bbfd74706eb7a4fc0148a9bed7bbf301240af527941f1e0002326efd5587296df441178708c692775256a67f0c837a59ecbda19c1157e0cd63cc0de6ace5cb83017633355e7227effaee66536165cec14fe67cbc0ac194eaeeebc",
    "0f71934ab6a7bc117815df33a576e63bd58b7673bb6d8921d9dab66e15c352eb9a2e018769960fa8f2d6881a0d3cb88bf5a71e2492698108447a440022fac23cde56572028738d38bc1e622c531a84f600f3cbd6eda4c5754f26d515bd9c2f2226a35f53b67a18cbeb634f4139bae37edad2c251a291c241f7ea9adb426a3799bc",
    "36394135aa0765ec014b1812463e17fbac2b48fe00a9d639d56e49ac2c0e1724be924e7c465d52f78451b2582a7834847f1cebe0d72b80ffa9adc0b1956d1d9d2d340a04ef486fe0d809c1dcb9bfa9d70996e197184d3f7134c7e74defe5ff82824ba7c8212dead0b984714ca9d0fd402931e0e7dc5d4522838f33928fc790fcbc",
    "1d396bf5df9fe94753301195eacdaf27935d2514e6789ddb99339de3d20029dd0f25b0504ff1cb63d09d6a77299f875b9cb019d8fa10ceb801c63c7092581cc5f2695c0333861229db5d071694c98490b1321b0a42de4b86bfc1d876c204db00fd63d5746dad1459926c4ff7e97b1b26f02227ec8b9171ec57a177074ba89938bc",
    "0d963ae6697dde16a11eabe840221c2924b7efe093f3c4d78681c13197e88b9030fd1a003568f21c87daf6c9c253d8415ad6dffcd21702cd9ebad14707f9bf5078932c9345ade75007eef085db3f0d9a58ac8213373972794d674923eec1ba8e3a7a015059bfe8c2ae7e75e6bc9575fdce0ce688af448f79ec23aec852d6fad8ba5b7323032c5de0688c5d1ae850650814190456e9a8eb19f57df0736199df11c22794b77fe1245f4cbe29f6c044b4f1841a13779a1ddb8b25c88fae79b0a16a56269070f7c97e2bb4040aa98b980263c3f2bbf83a9b3399f3a3d5f9aba9b2902b3f3f0bf68843307ed8beefc3e45253527095dabbda0b33cd74856a928085bc",
    "5028013dbc6ed20c1f00bcb4be6a227b363fe6bfa198a2bc3f725c4b4c0887becf645c19b3b0ab7cfedb34f8c714b8961b5f6a91cd30d7db7dd09c3300089fa28b696e228fbe2d710630b6179f03ffeead86b0a1cf8f62012478de9f378f4366d988ec81d2f4bd580bf6e181f36ec8562fafefb88f5d3695b76adcc31ea1dbaf7f0e11118e49e5481859d535b234ece25342771980ea9ea54f496dfda411e4fb9ec439470b6f42ceb2b909f85d3cf0e02d05b540bd73934f88d446c9199bd1c811f2a24df4443cafee68ba8478dfefa4b2df3bf0f6f9d2fcc00667ab38917941c83cc46fb1d75b18da4b94d36671f02d834c15748f78dcbd4c9bb460a42e64bc",
    "0072f2697740cb607abaa45d4a132da9254d3ae078c3444135965f3b2454edbee4801fe055e150f30ea844228829a2c8633fc0f4c54ec2f84aeec795091f7b678bfd83c7e9f102c107167941c39678c96023fa67d47b653ba6f7f54e9857b9795463b42b0061a68709c438dac64a6b7a4a135d7a6ec5096caa342b55cad1059c250c0d10f43619a5a64ca050ca441659008dc80637d3ff70cb65a8ff9cd5877e38e93b4e56a58faeba2cd0ee8cef5152058d5f1f5b4422155811bf945aa31379ec8e4ae717646b36a8e70c9ab65cd5f444e3f793c5d4d54f1cafbfd4ebe753aa04114193ff5339fedd96b082682b1c21d160ac4392334e9fb478122e062c1f9abc",
    "0034dc973858d799784f9c18c8064980ac1a840d4f15c80fc63ba713b42d2be20c43431d962543361d5e014cf2885f047f84536d2ff3783e4c4318440eac4c54d1d951727d02d919ec76a89e26289d7549bca79e98efe5ebc4ad676c7dd05614595730290fcd6b386e88d228c81a59293beffee8c3f952a325c02213b54854965d2f747074bd06622f13807e7ed1572b2c3ab160be6acb1501cd65e6349866a2b8ea1d00e04e2cbf8c83e403b3c9c8634a22a06d9926c4fb6794dbca5252861de47aeca4bd8159025f74115c142a1c11a6944eb4a8170c6620c00c2ac3f644af3fcd4bd9b80ad072b7c050c2a7f170333cf78594252b288b4e19ceba073da38ebc",
    "003862d81dd2a0562f805b117cc47d74b1c2bc",
    "2170cb30d6ba8f341a766a6d62bd43c9ffbc",
    "0002dee1c99e65b942c4f51d8610e53019bc",
    "00e1b67b47551543233c26cd18756cd964db70bc",
    "0089f6ae19aba4238eb9934b04b611a402d170bc",
};

constexpr PssAdd PssAdds[] {
    {0, -1, AddSalted, nullptr},
    {0, -2, AddSalted, nullptr},
    {0, 0, AddExact, PssBases[2]},
    {0, 1, AddSalted, nullptr},
    {0, 20, AddSalted, nullptr},
    {0, 46, AddSalted, nullptr},
    {0, 47, AddRefused, "00"},
    {1, -1, AddRefused, "00"},
    {1, -2, AddSalted, nullptr},
    {1, 0, AddExact, PssBases[6]},
    {1, 1, AddSalted, nullptr},
    {1, 20, AddSalted, nullptr},
    {1, 30, AddSalted, nullptr},
    {1, 31, AddRefused, "00"},
    {5, -1, AddRefused, ""},
    {2, -1, AddSalted, nullptr},
    {2, -2, AddSalted, nullptr},
    {2, 0, AddExact, PssBases[11]},
    {2, 1, AddSalted, nullptr},
    {2, 20, AddSalted, nullptr},
    {2, 79, AddSalted, nullptr},
    {2, 80, AddRefused, ""},
    {3, -1, AddSalted, nullptr},
    {3, -2, AddSalted, nullptr},
    {3, 0, AddExact, PssBases[15]},
    {3, 1, AddSalted, nullptr},
    {3, 20, AddSalted, nullptr},
    {3, 222, AddSalted, nullptr},
    {3, 223, AddRefused, ""},
    {4, -1, AddSalted, nullptr},
    {4, -2, AddSalted, nullptr},
    {4, 0, AddExact, PssBases[17]},
    {4, 1, AddSalted, nullptr},
    {4, 20, AddSalted, nullptr},
    {4, 190, AddSalted, nullptr},
    {4, 191, AddRefused, "00"},
    {7, -1, AddRefused, "00"},
    {7, -2, AddThrows, nullptr},
    {7, 0, AddRefused, "00"},
    {7, 1, AddRefused, "00"},
    {7, 20, AddRefused, "00"},
    {6, -1, AddRefused, ""},
    {6, -2, AddThrows, nullptr},
    {6, 0, AddRefused, ""},
    {6, 1, AddRefused, ""},
    {6, 20, AddRefused, ""},
    {9, -1, AddRefused, "00"},
    {9, -2, AddThrows, nullptr},
    {9, 0, AddRefused, "00"},
    {9, 1, AddRefused, "00"},
    {9, 20, AddRefused, "00"},
    {8, -1, AddRefused, ""},
    {8, -2, AddThrows, nullptr},
    {8, 0, AddRefused, ""},
    {8, 1, AddRefused, ""},
    {8, 20, AddRefused, ""},
    {11, -1, AddRefused, "00"},
    {11, -2, AddExact, PssBases[18]},
    {11, 0, AddExact, PssBases[18]},
    {11, 1, AddRefused, "00"},
    {11, 20, AddRefused, "00"},
    {10, -1, AddRefused, ""},
    {10, -2, AddExact, PssBases[19]},
    {10, 0, AddExact, PssBases[19]},
    {10, 1, AddRefused, ""},
    {10, 20, AddRefused, ""},
    {12, -1, AddRefused, ""},
    {12, -2, AddExact, PssBases[20]},
    {12, 0, AddExact, PssBases[20]},
    {12, 1, AddRefused, ""},
    {12, 20, AddRefused, ""},
    {13, -1, AddRefused, "00"},
    {13, -2, AddSalted, nullptr},
    {13, 0, AddExact, PssBases[22]},
    {13, 1, AddSalted, nullptr},
    {13, 20, AddRefused, "00"},
    {13, 2, AddRefused, "00"},
};

constexpr PssVerify PssVerifies[] {
    {0, 0, -1, 0x00, false, -1, 1},
    {0, 0, -1, 0x00, false, -2, 1},
    {0, 0, -1, 0x00, false, 16, 1},
    {0, 0, -1, 0x00, false, 17, 0},
    {0, 0, -1, 0x00, false, 15, 0},
    {0, 0, -1, 0x00, false, -3, 0},
    {0, 0, 0, 0x80, false, 16, 0},
    {0, 0, 0, 0x80, false, -2, 0},
    {0, 0, 0, 0x01, false, 16, 0},
    {0, 0, 0, 0x01, false, -2, 0},
    {0, 0, 1, 0x80, false, 16, 0},
    {0, 0, 1, 0x80, false, -2, 0},
    {0, 0, 64, 0x01, false, 16, 0},
    {0, 0, 64, 0x01, false, -2, 0},
    {0, 0, 30, 0x01, false, 16, 0},
    {0, 0, 30, 0x01, false, -2, 0},
    {0, 0, 31, 0x01, false, 16, 0},
    {0, 0, 31, 0x01, false, -2, 0},
    {0, 0, 31, 0x02, false, 16, 0},
    {0, 0, 31, 0x02, false, -2, 0},
    {0, 0, 40, 0x10, false, 16, 0},
    {0, 0, 40, 0x10, false, -2, 0},
    {0, 0, 56, 0x01, false, 16, 0},
    {0, 0, 56, 0x01, false, -2, 0},
    {0, 0, -1, 0x00, true, 16, 0},
    {0, 0, -1, 0x00, true, -2, 0},
    {0, 1, -1, 0x00, false, -2, 1},
    {0, 2, -1, 0x00, false, -2, 1},
    {0, 2, -1, 0x00, false, 0, 1},
    {0, 2, 0, 0x80, false, 0, 0},
    {0, 2, 0, 0x80, false, -2, 0},
    {0, 2, 0, 0x01, false, 0, 0},
    {0, 2, 0, 0x01, false, -2, 0},
    {0, 2, 1, 0x80, false, 0, 0},
    {0, 2, 1, 0x80, false, -2, 0},
    {0, 2, 64, 0x01, false, 0, 0},
    {0, 2, 64, 0x01, false, -2, 0},
    {0, 2, 46, 0x01, false, 0, 0},
    {0, 2, 46, 0x01, false, -2, 0},
    {0, 2, 47, 0x01, false, 0, 0},
    {0, 2, 47, 0x01, false, -2, 0},
    {0, 2, 47, 0x02, false, 0, 0},
    {0, 2, 47, 0x02, false, -2, 0},
    {0, 2, 56, 0x01, false, 0, 0},
    {0, 2, 56, 0x01, false, -2, 0},
    {0, 2, -1, 0x00, true, 0, 0},
    {0, 2, -1, 0x00, true, -2, 0},
    {0, 3, -1, 0x00, false, -2, 1},
    {0, 3, -1, 0x00, false, 1, 1},
    {0, 4, -1, 0x00, false, -2, 1},
    {0, 4, -1, 0x00, false, 20, 1},
    {0, -1, -1, 0x00, false, -2, 0},
    {1, 5, -1, 0x00, false, -2, 1},
    {1, 6, -1, 0x00, false, -2, 1},
    {1, 6, -1, 0x00, false, 0, 1},
    {1, 7, -1, 0x00, false, -2, 1},
    {1, 7, -1, 0x00, false, 1, 1},
    {1, 8, -1, 0x00, false, -2, 1},
    {1, 8, -1, 0x00, false, 20, 1},
    {1, -1, -1, 0x00, false, -2, 0},
    {2, 9, -1, 0x00, false, -1, 1},
    {2, 9, -1, 0x00, false, -2, 1},
    {2, 9, -1, 0x00, false, 48, 1},
    {2, 9, -1, 0x00, false, 49, 0},
    {2, 9, -1, 0x00, false, 47, 0},
    {2, 9, -1, 0x00, false, -3, 0},
    {2, 9, 0, 0x80, false, 48, 0},
    {2, 9, 0, 0x80, false, -2, 0},
    {2, 9, 0, 0x40, false, 48, 0},
    {2, 9, 0, 0x40, false, -2, 0},
    {2, 9, 0, 0x20, false, 48, 0},
    {2, 9, 0, 0x20, false, -2, 0},
    {2, 9, 128, 0x01, false, 48, 0},
    {2, 9, 128, 0x01, false, -2, 0},
    {2, 9, 30, 0x01, false, 48, 0},
    {2, 9, 30, 0x01, false, -2, 0},
    {2, 9, 31, 0x01, false, 48, 0},
    {2, 9, 31, 0x01, false, -2, 0},
    {2, 9, 31, 0x02, false, 48, 0},
    {2, 9, 31, 0x02, false, -2, 0},
    {2, 9, 56, 0x10, false, 48, 0},
    {2, 9, 56, 0x10, false, -2, 0},
    {2, 9, 104, 0x01, false, 48, 0},
    {2, 9, 104, 0x01, false, -2, 0},
    {2, 9, -1, 0x00, true, 48, 0},
    {2, 9, -1, 0x00, true, -2, 0},
    {2, 10, -1, 0x00, false, -2, 1},
    {2, 11, -1, 0x00, false, -2, 1},
    {2, 11, -1, 0x00, false, 0, 1},
    {2, 12, -1, 0x00, false, -2, 1},
    {2, 12, -1, 0x00, false, 1, 1},
    {2, 13, -1, 0x00, false, -2, 1},
    {2, 13, -1, 0x00, false, 20, 1},
    {2, -1, -1, 0x00, false, -2, 0},
    {3, 14, -1, 0x00, false, -1, 1},
    {3, 14, -1, 0x00, false, -2, 1},
    {3, 14, -1, 0x00, false, 32, 1},
    {3, 14, -1, 0x00, false, 33, 0},
    {3, 14, -1, 0x00, false, 31, 0},
    {3, 14, -1, 0x00, false, -3, 0},
    {3, 14, 0, 0x80, false, 32, 0},
    {3, 14, 0, 0x80, false, -2, 0},
    {3, 14, 0, 0x40, false, 32, 0},
    {3, 14, 0, 0x40, false, -2, 0},
    {3, 14, 255, 0x01, false, 32, 0},
    {3, 14, 255, 0x01, false, -2, 0},
    {3, 14, 189, 0x01, false, 32, 0},
    {3, 14, 189, 0x01, false, -2, 0},
    {3, 14, 190, 0x01, false, 32, 0},
    {3, 14, 190, 0x01, false, -2, 0},
    {3, 14, 190, 0x02, false, 32, 0},
    {3, 14, 190, 0x02, false, -2, 0},
    {3, 14, 207, 0x10, false, 32, 0},
    {3, 14, 207, 0x10, false, -2, 0},
    {3, 14, 239, 0x01, false, 32, 0},
    {3, 14, 239, 0x01, false, -2, 0},
    {3, 14, -1, 0x00, true, 32, 0},
    {3, 14, -1, 0x00, true, -2, 0},
    {3, 15, -1, 0x00, false, -2, 1},
    {3, 15, -1, 0x00, false, 0, 1},
    {3, -1, -1, 0x00, false, -2, 0},
    {4, 16, -1, 0x00, false, -1, 1},
    {4, 16, -1, 0x00, false, -2, 1},
    {4, 16, -1, 0x00, false, 64, 1},
    {4, 16, -1, 0x00, false, 65, 0},
    {4, 16, -1, 0x00, false, 63, 0},
    {4, 16, -1, 0x00, false, -3, 0},
    {4, 16, 0, 0x80, false, 64, 0},
    {4, 16, 0, 0x80, false, -2, 0},
    {4, 16, 0, 0x01, false, 64, 0},
    {4, 16, 0, 0x01, false, -2, 0},
    {4, 16, 1, 0x80, false, 64, 0},
    {4, 16, 1, 0x80, false, -2, 0},
    {4, 16, 256, 0x01, false, 64, 0},
    {4, 16, 256, 0x01, false, -2, 0},
    {4, 16, 126, 0x01, false, 64, 0},
    {4, 16, 126, 0x01, false, -2, 0},
    {4, 16, 127, 0x01, false, 64, 0},
    {4, 16, 127, 0x01, false, -2, 0},
    {4, 16, 127, 0x02, false, 64, 0},
    {4, 16, 127, 0x02, false, -2, 0},
    {4, 16, 160, 0x10, false, 64, 0},
    {4, 16, 160, 0x10, false, -2, 0},
    {4, 16, 224, 0x01, false, 64, 0},
    {4, 16, 224, 0x01, false, -2, 0},
    {4, 16, -1, 0x00, true, 64, 0},
    {4, 16, -1, 0x00, true, -2, 0},
    {4, 17, -1, 0x00, false, -2, 1},
    {4, 17, -1, 0x00, false, 0, 1},
    {4, -1, -1, 0x00, false, -2, 0},
    {7, -1, -1, 0x00, false, -1, 0},
    {7, -1, -1, 0x00, false, -2, 0},
    {7, -1, -1, 0x00, false, 0, 0},
    {7, -1, -1, 0x00, false, 1, 0},
    {6, -1, -1, 0x00, false, -1, 0},
    {6, -1, -1, 0x00, false, -2, 0},
    {6, -1, -1, 0x00, false, 0, 0},
    {6, -1, -1, 0x00, false, 1, 0},
    {9, -1, -1, 0x00, false, -1, 0},
    {9, -1, -1, 0x00, false, -2, -1},
    {9, -1, -1, 0x00, false, 0, 0},
    {9, -1, -1, 0x00, false, 1, 0},
    {8, -1, -1, 0x00, false, -1, 0},
    {8, -1, -1, 0x00, false, -2, -1},
    {8, -1, -1, 0x00, false, 0, 0},
    {8, -1, -1, 0x00, false, 1, 0},
    {11, 18, -1, 0x00, false, -1, 0},
    {11, 18, -1, 0x00, false, -2, 1},
    {11, 18, -1, 0x00, false, 0, 1},
    {11, 18, -1, 0x00, false, 1, 0},
    {11, 18, -1, 0x00, false, -3, 0},
    {11, 18, 0, 0x80, false, 0, 0},
    {11, 18, 0, 0x80, false, -2, 0},
    {11, 18, 0, 0x01, false, 0, 0},
    {11, 18, 0, 0x01, false, -2, 0},
    {11, 18, 1, 0x80, false, 0, 0},
    {11, 18, 1, 0x80, false, -2, 0},
    {11, 18, 18, 0x01, false, 0, 0},
    {11, 18, 18, 0x01, false, -2, 0},
    {11, 18, 1, 0x01, false, 0, 0},
    {11, 18, 1, 0x01, false, -2, 0},
    {11, 18, 1, 0x02, false, 0, 0},
    {11, 18, 1, 0x02, false, -2, 0},
    {11, 18, 10, 0x01, false, 0, 0},
    {11, 18, 10, 0x01, false, -2, 0},
    {11, 18, -1, 0x00, true, 0, 0},
    {11, 18, -1, 0x00, true, -2, 0},
    {11, -1, -1, 0x00, false, -1, 0},
    {11, -1, -1, 0x00, false, -2, 0},
    {11, -1, -1, 0x00, false, 0, 0},
    {11, -1, -1, 0x00, false, 1, 0},
    {10, 19, -1, 0x00, false, -1, 0},
    {10, 19, -1, 0x00, false, -2, 1},
    {10, 19, -1, 0x00, false, 0, 1},
    {10, 19, -1, 0x00, false, 1, 0},
    {10, 19, -1, 0x00, false, -3, 0},
    {10, 19, 0, 0x80, false, 0, 0},
    {10, 19, 0, 0x80, false, -2, 0},
    {10, 19, 0, 0x40, false, 0, 0},
    {10, 19, 0, 0x40, false, -2, 0},
    {10, 19, 17, 0x01, false, 0, 0},
    {10, 19, 17, 0x01, false, -2, 0},
    {10, 19, 0, 0x01, false, 0, 0},
    {10, 19, 0, 0x01, false, -2, 0},
    {10, 19, 0, 0x02, false, 0, 0},
    {10, 19, 0, 0x02, false, -2, 0},
    {10, 19, 9, 0x01, false, 0, 0},
    {10, 19, 9, 0x01, false, -2, 0},
    {10, 19, -1, 0x00, true, 0, 0},
    {10, 19, -1, 0x00, true, -2, 0},
    {10, -1, -1, 0x00, false, -1, 0},
    {10, -1, -1, 0x00, false, -2, 0},
    {10, -1, -1, 0x00, false, 0, 0},
    {10, -1, -1, 0x00, false, 1, 0},
    {12, 20, -1, 0x00, false, -1, 0},
    {12, 20, -1, 0x00, false, -2, 1},
    {12, 20, -1, 0x00, false, 0, 1},
    {12, 20, -1, 0x00, false, 1, 0},
    {12, 20, -1, 0x00, false, -3, 0},
    {12, 20, 0, 0x80, false, 0, 0},
    {12, 20, 0, 0x80, false, -2, 0},
    {12, 20, 0, 0x08, false, 0, 0},
    {12, 20, 0, 0x08, false, -2, 0},
    {12, 20, 0, 0x04, false, 0, 0},
    {12, 20, 0, 0x04, false, -2, 0},
    {12, 20, 17, 0x01, false, 0, 0},
    {12, 20, 17, 0x01, false, -2, 0},
    {12, 20, 0, 0x01, false, 0, 0},
    {12, 20, 0, 0x01, false, -2, 0},
    {12, 20, 0, 0x02, false, 0, 0},
    {12, 20, 0, 0x02, false, -2, 0},
    {12, 20, 9, 0x01, false, 0, 0},
    {12, 20, 9, 0x01, false, -2, 0},
    {12, 20, -1, 0x00, true, 0, 0},
    {12, 20, -1, 0x00, true, -2, 0},
    {12, -1, -1, 0x00, false, -1, 0},
    {12, -1, -1, 0x00, false, -2, 0},
    {12, -1, -1, 0x00, false, 0, 0},
    {12, -1, -1, 0x00, false, 1, 0},
    {13, 21, -1, 0x00, false, -1, 0},
    {13, 21, -1, 0x00, false, -2, 1},
    {13, 21, -1, 0x00, false, 1, 1},
    {13, 21, -1, 0x00, false, 2, 0},
    {13, 21, -1, 0x00, false, 0, 0},
    {13, 21, -1, 0x00, false, -3, 0},
    {13, 21, 0, 0x80, false, 1, 0},
    {13, 21, 0, 0x80, false, -2, 0},
    {13, 21, 0, 0x01, false, 1, 0},
    {13, 21, 0, 0x01, false, -2, 0},
    {13, 21, 1, 0x80, false, 1, 0},
    {13, 21, 1, 0x80, false, -2, 0},
    {13, 21, 19, 0x01, false, 1, 0},
    {13, 21, 19, 0x01, false, -2, 0},
    {13, 21, 1, 0x01, false, 1, 0},
    {13, 21, 1, 0x01, false, -2, 0},
    {13, 21, 1, 0x02, false, 1, 0},
    {13, 21, 1, 0x02, false, -2, 0},
    {13, 21, 2, 0x10, false, 1, 0},
    {13, 21, 2, 0x10, false, -2, 0},
    {13, 21, 11, 0x01, false, 1, 0},
    {13, 21, 11, 0x01, false, -2, 0},
    {13, 21, -1, 0x00, true, 1, 0},
    {13, 21, -1, 0x00, true, -2, 0},
    {13, 22, -1, 0x00, false, -1, 0},
    {13, 22, -1, 0x00, false, -2, 1},
    {13, 22, -1, 0x00, false, 0, 1},
    {13, 22, -1, 0x00, false, 1, 0},
    {13, 22, -1, 0x00, false, -3, 0},
    {13, 22, 0, 0x80, false, 0, 0},
    {13, 22, 0, 0x80, false, -2, 0},
    {13, 22, 0, 0x01, false, 0, 0},
    {13, 22, 0, 0x01, false, -2, 0},
    {13, 22, 1, 0x80, false, 0, 0},
    {13, 22, 1, 0x80, false, -2, 0},
    {13, 22, 19, 0x01, false, 0, 0},
    {13, 22, 19, 0x01, false, -2, 0},
    {13, 22, 1, 0x01, false, 0, 0},
    {13, 22, 1, 0x01, false, -2, 0},
    {13, 22, 2, 0x01, false, 0, 0},
    {13, 22, 2, 0x01, false, -2, 0},
    {13, 22, 2, 0x02, false, 0, 0},
    {13, 22, 2, 0x02, false, -2, 0},
    {13, 22, 11, 0x01, false, 0, 0},
    {13, 22, 11, 0x01, false, -2, 0},
    {13, 22, -1, 0x00, true, 0, 0},
    {13, 22, -1, 0x00, true, -2, 0},
    {13, -1, -1, 0x00, false, -1, 0},
    {13, -1, -1, 0x00, false, -2, 0},
    {13, -1, -1, 0x00, false, 0, 0},
    {13, -1, -1, 0x00, false, 1, 0},
};

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL RSA PSS check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static bool QueueHolds(std::uint64_t error) {
    const std::uint64_t first = ERR_get_error_nid_postfix();
    const std::uint64_t second = ERR_get_error_nid_postfix();
    return first == error && !second;
}

static bool QueueHoldsOneRsaError() {
    const std::uint64_t first = ERR_get_error_nid_postfix();
    const std::uint64_t second = ERR_get_error_nid_postfix();
    return (first & ~std::uint64_t(0xfff)) == RsaError && (first & 0xfff) && !second;
}

static bool QueueHoldsNoRsaError() {
    bool clean = true;
    for (std::uint64_t error = ERR_get_error_nid_postfix(); error; error = ERR_get_error_nid_postfix()) clean = clean && (error >> 24) != (RsaError >> 24);
    return clean;
}

template<typename TCall> static void RequireQueue(TCall call, int expected, std::uint64_t error, std::source_location location = std::source_location::current()) {
    ERR_clear_error_nid_postfix();
    const int result = call();
    Require(result == expected && QueueHolds(error), location);
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

static Bytes Hex(const char* text) {
    const auto nibble = [](char digit) { return static_cast<unsigned char>(digit <= '9' ? digit - '0' : digit - 'a' + 10); };
    Bytes bytes;
    for (; text[0] && text[1]; text += 2) bytes.push_back(static_cast<unsigned char>(nibble(text[0]) << 4 | nibble(text[1])));
    return bytes;
}

static Bytes Pattern(std::size_t length, unsigned seed) {
    Bytes bytes(length);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = static_cast<unsigned char>(seed + 29 * index);
    return bytes;
}

static const EvpMd* DigestFor(Digest digest) {
    switch (digest) {
    case DigestMd5: return EVP_md5_nid_postfix();
    case DigestSha256: return EVP_sha256_nid_postfix();
    case DigestSha384: return EVP_sha384_nid_postfix();
    case DigestSha512: return EVP_sha512_nid_postfix();
    case DigestNone: return nullptr;
    }
    return nullptr;
}

struct TestKey {
    std::vector<std::uint64_t> words;
    TestBignum number;
    LibreSslRsa* rsa;

    explicit TestKey(int bits, int zeroWords = 0) : words(static_cast<std::size_t>((bits + 63) / 64 + zeroWords) + 1), rsa(RSA_new_nid_postfix()) {
        if (bits) {
            words[static_cast<std::size_t>(bits - 1) / 64] |= std::uint64_t(1) << ((bits - 1) % 64);
            words[0] |= 1;
        }
        number = {words.data(), (bits + 63) / 64 + zeroWords, static_cast<int>(words.size()), 0, BignumStaticData};
        rsa->n = &number;
    }
    TestKey(const TestKey&) = delete;
    ~TestKey() { RSA_free_nid_postfix(rsa); }
    std::size_t Size() const { return static_cast<std::size_t>(RSA_size_nid_postfix(rsa)); }
};

static int Add(const TestKey& key, Bytes& out, const Bytes& mHash, const EvpMd* digest, int saltLength) {
    out.assign(key.Size() + Slack, Fill);
    return RSA_padding_add_PKCS1_PSS_nid_postfix(key.rsa, out.data(), mHash.data(), digest, saltLength);
}

static int Verify(const TestKey& key, const Bytes& mHash, const EvpMd* digest, const Bytes& em, int saltLength) {
    const Bytes copy = em;
    const int result = RSA_verify_PKCS1_PSS_nid_postfix(key.rsa, mHash.data(), digest, em.data(), saltLength);
    Require(em == copy);
    return result;
}

static bool Untouched(const Bytes& out, std::size_t from) {
    return std::all_of(out.begin() + static_cast<long>(from), out.end(), [](unsigned char value) { return value == Fill; });
}

static Bytes Encoded(const Bytes& out, std::size_t size) {
    return Bytes(out.begin(), out.begin() + static_cast<long>(size));
}

static bool Framed(const Bytes& out, std::size_t size, int bits) {
    const int topBits = (bits - 1) & 7;
    const unsigned char forbidden = topBits ? static_cast<unsigned char>(0xFF << topBits) : 0xFF;
    return !(out[0] & forbidden) && out[size - 1] == 0xbc && Untouched(out, size);
}

static Bytes HashFor(const PssCombo& combo) {
    Bytes mHash = Hex(combo.mHash);
    if (mHash.empty()) mHash = Pattern(64, 5);
    return mHash;
}

static void TestAddVectors() {
    for (const auto& vector : PssAdds) {
        const auto& combo = PssCombos[vector.combo];
        TestKey key(combo.bits);
        const auto* digest = DigestFor(combo.digest);
        const auto mHash = HashFor(combo);
        Bytes out;
        switch (vector.kind) {
        case AddExact:
        case AddRefused: {
            const auto expected = Hex(vector.bytes);
            ERR_clear_error_nid_postfix();
            Require(Add(key, out, mHash, digest, vector.saltLength) == (vector.kind == AddExact) && std::equal(expected.begin(), expected.end(), out.begin()) && Untouched(out, expected.size()));
            Require(!digest ? QueueHoldsNoRsaError() : vector.kind == AddExact ? QueueHolds(0) : QueueHoldsOneRsaError());
            break;
        }
        case AddSalted: {
            ERR_clear_error_nid_postfix();
            Require(Add(key, out, mHash, digest, vector.saltLength) == 1 && Framed(out, key.Size(), combo.bits) && QueueHolds(0));
            const auto em = Encoded(out, key.Size());
            Require(Verify(key, mHash, digest, em, vector.saltLength) == 1 && Verify(key, mHash, digest, em, -2) == 1);
            break;
        }
        case AddThrows:
            RequireThrows([&] { Add(key, out, mHash, digest, vector.saltLength); });
            break;
        }
    }
}

static void TestVerifyVectors() {
    for (const auto& vector : PssVerifies) {
        const auto& combo = PssCombos[vector.combo];
        TestKey key(combo.bits);
        auto mHash = HashFor(combo);
        if (vector.flipHash) mHash[0] ^= 0x01;
        Bytes em;
        if (vector.base < 0) {
            em.assign(key.Size(), 0);
            em.back() = 0xbc;
        } else {
            em = Hex(PssBases[vector.base]);
        }
        Require(em.size() == key.Size());
        if (vector.position >= 0) em[static_cast<std::size_t>(vector.position)] ^= vector.flip;
        if (vector.expected < 0) RequireThrows([&] { Verify(key, mHash, DigestFor(combo.digest), em, vector.saltLength); });
        else {
            ERR_clear_error_nid_postfix();
            Require(Verify(key, mHash, DigestFor(combo.digest), em, vector.saltLength) == vector.expected);
            Require(vector.expected ? QueueHolds(0) : QueueHoldsOneRsaError());
        }
    }
}

static void TestSaltBounds() {
    for (const auto& combo : PssCombos) {
        if (combo.digest == DigestNone) continue;
        TestKey key(combo.bits);
        const auto* digest = DigestFor(combo.digest);
        const int hashBytes = EVP_MD_size_nid_postfix(digest);
        const bool topZero = !((combo.bits - 1) & 7);
        const auto mHash = HashFor(combo);
        Bytes em(key.Size(), 0);
        em.back() = 0xbc;
        Bytes out;
        for (int saltLength : {-3, INT_MIN}) Require(Add(key, out, mHash, digest, saltLength) == 0 && Untouched(out, 0) && !Verify(key, mHash, digest, em, saltLength));
        Require(Add(key, out, mHash, digest, INT_MAX - hashBytes - 2) == 0 && Untouched(out, topZero) && (!topZero || !out[0]));
        Require(!Verify(key, mHash, digest, em, INT_MAX - hashBytes - 2));
        for (int saltLength : {INT_MAX - hashBytes - 1, INT_MAX}) RequireThrows([&] { Add(key, out, mHash, digest, saltLength); });
        RequireThrows([&] { Verify(key, mHash, digest, em, INT_MAX - hashBytes - 1); });
    }
}

static void TestRoundTrips() {
    const Digest digests[] {DigestMd5, DigestSha256, DigestSha384, DigestSha512};
    for (int bits : {513, 1031, 2048, 4096}) {
        TestKey key(bits);
        for (Digest type : digests) {
            const auto* digest = DigestFor(type);
            const int hashBytes = EVP_MD_size_nid_postfix(digest);
            const auto mHash = Pattern(static_cast<std::size_t>(hashBytes), static_cast<unsigned>(bits) + static_cast<unsigned>(type));
            auto otherHash = mHash;
            otherHash.back() ^= 0x80;
            const int longest = static_cast<int>(key.Size()) - ((bits - 1) & 7 ? 0 : 1) - hashBytes - 2;
            for (int saltLength : {-1, -2, 0, 1, 20, longest}) {
                Bytes out;
                const int resolved = saltLength == -1 ? hashBytes : saltLength == -2 ? longest : saltLength;
                if (resolved < 0) {
                    RequireThrows([&] { Add(key, out, mHash, digest, saltLength); });
                    continue;
                }
                if (resolved > longest) {
                    Require(Add(key, out, mHash, digest, saltLength) == 0 && Untouched(out, (bits - 1) & 7 ? 0 : 1));
                    continue;
                }
                Require(Add(key, out, mHash, digest, saltLength) == 1 && Framed(out, key.Size(), bits));
                const auto em = Encoded(out, key.Size());
                Require(Verify(key, mHash, digest, em, saltLength) == 1 && Verify(key, mHash, digest, em, -2) == 1 && Verify(key, mHash, digest, em, resolved) == 1);
                Require(!Verify(key, mHash, digest, em, resolved + 1) && !Verify(key, otherHash, digest, em, -2));
                if (resolved) Require(!Verify(key, mHash, digest, em, resolved - 1));
                if (resolved < 8) continue;
                Bytes again;
                Require(Add(key, again, mHash, digest, saltLength) == 1 && again != out && Verify(key, mHash, digest, Encoded(again, key.Size()), resolved) == 1);
            }
            Bytes out;
            Require(Add(key, out, mHash, digest, longest + 1) == 0);
        }
    }
}

static void TestNonNormalisedModulus() {
    const auto mHash = Pattern(32, 9);
    const auto* digest = EVP_sha256_nid_postfix();
    TestKey padded(1025, 1);
    TestKey wide(1088);
    TestKey exact(1025);
    Require(padded.Size() == 136 && exact.Size() == 129);
    Bytes fromPadded;
    Bytes fromWide;
    Bytes fromExact;
    Require(Add(padded, fromPadded, mHash, digest, 0) == 1 && Add(wide, fromWide, mHash, digest, 0) == 1 && fromPadded == fromWide);
    Require(Add(exact, fromExact, mHash, digest, 0) == 1 && fromExact[0] == 0 && Untouched(fromExact, 129));
    Require(Verify(padded, mHash, digest, Encoded(fromPadded, 136), 0) == 1 && Verify(padded, mHash, digest, Encoded(fromPadded, 136), -2) == 1);
}

static void TestOrdering() {
    const auto mHash = Pattern(64, 3);
    const auto* digest = EVP_sha256_nid_postfix();
    ERR_clear_error_nid_postfix();
    Require(RSA_padding_add_PKCS1_PSS_nid_postfix(nullptr, nullptr, nullptr, nullptr, -1) == 0 && QueueHoldsNoRsaError());
    Require(RSA_verify_PKCS1_PSS_nid_postfix(nullptr, nullptr, nullptr, nullptr, -1) == 0 && QueueHoldsNoRsaError());
    for (int saltLength : {-3, INT_MIN}) {
        Require(RSA_padding_add_PKCS1_PSS_nid_postfix(nullptr, nullptr, mHash.data(), digest, saltLength) == 0);
        Require(RSA_verify_PKCS1_PSS_nid_postfix(nullptr, mHash.data(), digest, nullptr, saltLength) == 0);
    }
    TestKey narrow(1031);
    Require(RSA_padding_add_PKCS1_PSS_nid_postfix(narrow.rsa, nullptr, mHash.data(), digest, 96) == 0);
    for (int bits : {136, 137}) {
        TestKey small(bits);
        Require(!Verify(small, mHash, EVP_md5_nid_postfix(), Bytes(small.Size(), 0), -2));
    }
    TestKey low(513);
    Bytes out(low.Size() + Slack, 0xAA);
    Require(RSA_padding_add_PKCS1_PSS_nid_postfix(low.rsa, out.data(), mHash.data(), digest, 31) == 0 && out[0] == 0 &&
        std::all_of(out.begin() + 1, out.end(), [](unsigned char value) { return value == 0xAA; }));
}

static Bytes Flipped(Bytes bytes, std::size_t position, unsigned char flip) {
    bytes[position] ^= flip;
    return bytes;
}

static void TestErrorQueue() {
    const auto& combo = PssCombos[0];
    TestKey key(combo.bits);
    TestKey tiny(PssCombos[7].bits);
    const auto* md5 = DigestFor(combo.digest);
    const auto mHash = HashFor(combo);
    auto otherHash = mHash;
    otherHash[0] ^= 0x01;
    const Bytes em = Hex(PssBases[0]);
    Bytes zero(tiny.Size(), 0);
    zero.back() = 0xbc;
    Bytes out;
    Require(key.Size() == 65 && tiny.Size() == 17);

    RequireQueue([&] { return Verify(key, mHash, md5, em, 16); }, 1, 0);
    RequireQueue([&] { return Verify(key, mHash, md5, em, -2); }, 1, 0);
    RequireQueue([&] { return Verify(key, mHash, md5, em, -1); }, 1, 0);
    RequireQueue([&] { return Verify(key, mHash, md5, em, -3); }, 0, SlenCheckFailed);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 0, 0x01), -3); }, 0, SlenCheckFailed);
    RequireQueue([&] { return RSA_verify_PKCS1_PSS_nid_postfix(nullptr, mHash.data(), md5, nullptr, INT_MIN); }, 0, SlenCheckFailed);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 0, 0x01), 16); }, 0, FirstOctetInvalid);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 0, 0x80), 47); }, 0, FirstOctetInvalid);
    RequireQueue([&] { return Verify(key, mHash, md5, em, 47); }, 0, DataTooLarge);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 64, 0x01), 47); }, 0, DataTooLarge);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 64, 0x01), 16); }, 0, LastOctetInvalid);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 64, 0x01), -2); }, 0, LastOctetInvalid);
    RequireQueue([&] { return Verify(tiny, mHash, md5, zero, -2); }, 0, MallocFailure);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 31, 0x02), 16); }, 0, SlenRecoveryFailed);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 31, 0x02), -2); }, 0, SlenRecoveryFailed);
    RequireQueue([&] { return Verify(key, mHash, md5, em, 17); }, 0, SlenCheckFailed);
    RequireQueue([&] { return Verify(key, mHash, md5, em, 15); }, 0, SlenCheckFailed);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 30, 0x01), 16); }, 0, SlenCheckFailed);
    RequireQueue([&] { return Verify(key, otherHash, md5, em, 16); }, 0, BadSignature);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 30, 0x01), -2); }, 0, BadSignature);
    RequireQueue([&] { return Verify(key, mHash, md5, Flipped(em, 40, 0x10), -2); }, 0, BadSignature);

    RequireQueue([&] { return Add(key, out, mHash, md5, 16); }, 1, 0);
    RequireQueue([&] { return Add(key, out, mHash, md5, -2); }, 1, 0);
    RequireQueue([&] { return Add(key, out, mHash, md5, 46); }, 1, 0);
    RequireQueue([&] { return Add(key, out, mHash, md5, -3); }, 0, SlenCheckFailed);
    RequireQueue([&] { return RSA_padding_add_PKCS1_PSS_nid_postfix(nullptr, nullptr, mHash.data(), md5, INT_MIN); }, 0, SlenCheckFailed);
    RequireQueue([&] { return Add(key, out, mHash, md5, 47); }, 0, DataTooLargeForKeySize);
    RequireQueue([&] { return Add(key, out, mHash, EVP_sha512_nid_postfix(), -1); }, 0, DataTooLargeForKeySize);

    ERR_clear_error_nid_postfix();
    Require(Verify(key, mHash, md5, em, -3) == 0 && Verify(key, otherHash, md5, em, 16) == 0 && Add(key, out, mHash, md5, 47) == 0);
    Require(ERR_get_error_nid_postfix() == SlenCheckFailed && ERR_get_error_nid_postfix() == BadSignature && QueueHolds(DataTooLargeForKeySize));
}

static int APS5_VABI CustomInit(EvpMdContext*) { return 1; }
static int APS5_VABI CustomUpdate(EvpMdContext*, const void*, std::size_t) { return 1; }
static int APS5_VABI CustomFinal(EvpMdContext*, unsigned char*) { return 1; }
static int APS5_VABI FailingInit(EvpMdContext*) { return 0; }

static void TestThrows() {
    const auto mHash = Pattern(64, 7);
    const auto* sha256 = EVP_sha256_nid_postfix();
    TestKey key(1031);
    TestKey low(513);
    Bytes out;
    Bytes em(key.Size(), 0);
    em.back() = 0xbc;
    RequireThrows([&] { RSA_padding_add_PKCS1_PSS_nid_postfix(nullptr, out.data(), mHash.data(), sha256, -1); });
    RequireThrows([&] { RSA_verify_PKCS1_PSS_nid_postfix(nullptr, mHash.data(), sha256, em.data(), -1); });
    out.assign(key.Size() + Slack, Fill);
    RequireThrows([&] { RSA_padding_add_PKCS1_PSS_nid_postfix(key.rsa, nullptr, mHash.data(), sha256, -1); });
    RequireThrows([&] { RSA_padding_add_PKCS1_PSS_nid_postfix(low.rsa, nullptr, mHash.data(), sha256, 95); });
    RequireThrows([&] { RSA_verify_PKCS1_PSS_nid_postfix(key.rsa, mHash.data(), sha256, nullptr, -1); });
    key.rsa->n = nullptr;
    RequireThrows([&] { RSA_padding_add_PKCS1_PSS_nid_postfix(key.rsa, out.data(), mHash.data(), sha256, -1); });
    RequireThrows([&] { RSA_verify_PKCS1_PSS_nid_postfix(key.rsa, mHash.data(), sha256, em.data(), -1); });
    key.rsa->n = &key.number;
    for (int saltLength : {INT_MAX - 33, INT_MAX}) {
        RequireThrows([&] { Add(key, out, mHash, sha256, saltLength); });
        RequireThrows([&] { Verify(key, mHash, sha256, em, saltLength); });
    }
    Require(Add(key, out, mHash, sha256, INT_MAX - 34) == 0 && Untouched(out, 0) && Verify(key, mHash, sha256, em, INT_MAX - 34) == 0);

    EvpMd custom {};
    custom.init = CustomInit;
    custom.update = CustomUpdate;
    custom.finalize = CustomFinal;
    custom.contextSize = 8;
    EvpMd failing = custom;
    failing.init = FailingInit;
    failing.mdSize = 64;
    for (int bits : {136, 137}) {
        TestKey small(bits);
        ERR_clear_error_nid_postfix();
        Require(Add(small, out, mHash, &failing, -2) == 0 && Untouched(out, bits & 1) && (!(bits & 1) || !out[0]) && QueueHolds(0));
    }
    RequireQueue([&] { return RSA_padding_add_PKCS1_PSS_nid_postfix(key.rsa, nullptr, mHash.data(), &failing, 0); }, 0, 0);
    custom.mdSize = 65;
    ERR_clear_error_nid_postfix();
    Require(Add(key, out, mHash, &custom, 0) == 0 && QueueHoldsNoRsaError() && Verify(key, mHash, &custom, em, -2) == 0 && QueueHoldsNoRsaError());
    custom.mdSize = INT_MAX;
    RequireThrows([&] { Add(key, out, mHash, &custom, -1); });
    TestKey one(1);
    RequireThrows([&] { Add(one, out, mHash, &custom, -2); });
    RequireThrows([&] { Verify(key, mHash, &custom, em, -1); });
    custom.mdSize = 0;
    RequireThrows([&] { Add(key, out, mHash, &custom, 0); });
    RequireThrows([&] { Verify(key, mHash, &custom, em, -2); });
    TestKey empty(0);
    TestKey single(1);
    Require(empty.Size() == 0 && single.Size() == 1);
    const Bytes zero(1, 0);
    RequireThrows([&] { Verify(empty, mHash, &custom, zero, -2); });
    Require(Verify(single, mHash, &custom, zero, -2) == 0);
    Require(Verify(empty, mHash, &custom, zero, -1) == 0);
    custom.mdSize = 32;
    custom.contextSize = 0;
    RequireThrows([&] { Add(key, out, mHash, &custom, 0); });
}

int main() {
    InitializeHeap();
    TestAddVectors();
    TestVerifyVectors();
    TestSaltBounds();
    TestRoundTrips();
    TestNonNormalisedModulus();
    TestOrdering();
    TestThrows();
    TestErrorQueue();
    std::puts("libSceLibreSsl RSA PSS tests passed");
}
