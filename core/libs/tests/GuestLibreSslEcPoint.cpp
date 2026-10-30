#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string_view>
#include <vector>

struct Vector {
    int nid;
    const char* encoded;
    unsigned char nonsquareX;
};

static constexpr Vector Vectors[] = {
    {409, "04188da80eb03090f67cbf20eb43a18800f4ff0afd82ff101207192b95ffc8da78631011ed6b24cdd573f977a11e794811", 1},
    {409, "04dafebf5828783f2ad35534631588a3f629a70fb16982a888dd6bda0d993da0fa46b27bbc141b868f59331afa5c7e93ab", 1},
    {409, "0444275cd2e1f46dc3f9f57636c2b4213b8bb445930510ff8aefad8348fde30c87de438612a818e98d9b76a67ad25ddfd0", 1},
    {415, "046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c2964fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5", 1},
    {415, "047cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc4766997807775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1", 1},
    {415, "0447776904c0f1cc3a9c0984b66f75301a5fa68678f0d64af8ba1abce34738a73eaa005ee6b5b957286231856577648e8381b2804428d5733f32f787ff71f1fcdc", 1},
    {711, "04db4ff10ec057e9ae26b07d0280b7f4341da5d1b1eae06c7d9b2f2f6d9c5628a7844163d015be86344082aa88d95e2f9d", 0},
    {711, "04f091cf6331b1747684f5d2549cd1d4b3a8bed93b94f93cb6fd7af42e1e7565a02e6268661c5e42e603da2d98a18f2ed5", 0},
    {711, "04cd04834b06e08576a530d2cef7635c6a8d9490ca8578dbbd7372d1a48719565fc104f2671e7b996afeb668f72229ceb7", 0},
    {712, "04a1455b334df099df30fc28a169a467e9e47075a90f7e650eb6b7a45c7e089fed7fba344282cafbd6f7e319f7c0b0bd59e2ca4bdb556d61a5", 0},
    {712, "0486c0deb56aeb9712390999a0232b9bf596b9639fa1ce8cf426749e608f598c954e1085555b474a79906b855c539ed633dbf4a9fa9f06b69a", 0},
    {712, "04fdabead21c6447aef87bd0c829da0bf9624f9797e9a3ba1e4d559d91ec0ac8282033745276a46c4d898b22ab4f38642a17659c074e03d956", 0},
    {713, "04b70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21bd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34", 0},
    {713, "04706a46dc76dcb76798e60e6d89474788d16dc18032d268fd1a704fa61c2b76a7bc25e7702a704fa986892849fca629487acf3709d2e4e8bb", 0},
    {713, "04b8357c3a6ceef288310e17b8bfeff9200846ca8c1942497c484403bcff149efa6606a6bd20ef7d1b06bd92f6904639dce5174db6cc554a26", 0},
    {714, "0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8", 0},
    {714, "04c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee51ae168fea63dc339a3c58419466ceaeef7f632653266d0e1236431a950cfe52a", 0},
    {714, "04defdea4cdb677750a420fee807eacf21eb9898ae79b9768766e4faa04a2d4a344211ab0694635168e997b0ead2a93daeced1f4a04a95c0f6cfb199f69e56eb77", 0},
    {715, "04aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab73617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f", 1},
    {715, "0408d999057ba3d2d969260045c55b97f089025959a6f434d651d207d19fb96e9e4fe0e86ebe0e64f85b96a9c75295df618e80f1fa5b1b3cedb7bfe8dffd6dba74b275d875bc6cc43e904e505f256ab4255ffd43e94d39e22d61501e700a940e80", 1},
    {715, "044099952208b4889600a5ebbcb13e1a32692befb0733b41e6dcc614e42e5805f817012a991af1f486caf3a9add9ffcc035ecf94777833059839474594af603598163ad3f8008ad0cd9b797d277f2388b304da4d2faa9680ecfa650ef5e23b09a0", 1},
    {716, "0400c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66011839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650", 3},
    {716, "0400433c219024277e7e682fcb288148c282747403279b1ccc06352c6e5505d769be97b3b204da6ef55507aa104a3a35c5af41cf2fa364d60fd967f43e3933ba6d783d00f4bb8cc7f86db26700a7f3eceeeed3f0b5c6b5107c4da97740ab21a29906c42dbbb3e377de9f251f6b93937fa99a3248f4eafcbe95edc0f4f71be356d661f41b02", 3},
    {716, "0401b00ddb707f130eda13a0b874645923906a99ee9e269fa2b3b4d66524f269250858760a69e674fe0287df4e799b5681380ff8c3042af0d1a41076f817a853110ae00085683f1d7db16576dbc111d4e4aeddd106b799534cf69910a98d68ac2b22a1323df9da564ef6dd0bf0d2f6757f16adf420e6905594c2b755f535b9cb7c70e64647", 3},
    {928, "048bd2aeb9cb7e57cb2c4b482ffc81b7afb9de27e1e3bd23c23a4453bd9ace3262547ef835c3dac4fd97f8461a14611dc9c27745132ded8e545c1d54c72f046997", 0},
    {928, "04743cf1b8b5cd4f2eb55f8aa369593ac436ef044166699e37d51a14c2ce13ea0e36ed163337deba9c946fe0bb776529da38df059f69249406892ada097eeb7cd4", 0},
    {928, "04884d1f975768ce45c3663e5f98b9dafada7b65585bfbaada0b73fcb1a0c86824569ea2dd9a21654f601705fe7f902e370f42120813fc7e966205d05254c57ad7", 0},
    {932, "041d1c64f068cf45ffa2a63a81b7c13f6b8847a3e77ef14fe3db7fcafe0cbd10e8e826e03436d646aaef87b2e247d4af1e8abe1d7520f9c2a45cb1eb8e95cfd55262b70b29feec5864e19c054ff99129280e4646217791811142820341263c5315", 0},
    {932, "042282bc382a2f4dfcb95c3495d7b4fd590ad520b3eb6be4d6ec2f80c4e0f70df87c4ba74a09b553ebb427b58df9d59fca0edda83773ac68735768d14a24f37a57ce9bedbc170921ce4d89dd051728fc3eb4b4ea69ab64fc288f1b29502b6e1d30", 0},
    {932, "044d0bfeac9bfd1564268c0a910c4f9601388f576ac92467f7ca9d72edc36ce5b387e9b66563d3af2f4b857eab44faa70243c90643ba2e1abce349008bcaca8e78702a753f8dc81bca9539d69bdada447552e1e0726b03b4fec2568ca0ae0cb469", 0},
    {934, "0481aee4bdd82ed9645a21322e9c4c6a9385ed9f70b5d916c1b43b62eef4d0098eff3b1f78e2d0d48d50d1687b93b97d5f7c6d5047406a5e688b352209bcb9f8227dde385d566332ecc0eabfa9cf7822fdf209f70024a57b1aa000c55b881f8111b2dcde494a5f485e5bca4bd88a2763aed1ca2b2fa8f0540678cd1e0f3ad80892", 0},
    {934, "049f4945f680edf9800a63285758f399b3d18d8141b8a18064a30d3035f4cb6581957877f3a8f0f72597116e702915a4f4f698f404089a4cc5080447def02f48506d6b4b188b699c5649826b716292f29d149ce1238d3f1e0f5a2c366b03e5d1b2fdf99bb1709c700fa5c3b602b0960cbf63a42e4181fd929ce269ad21be592e71", 0},
    {934, "047e8ec7d1a5bc63e371e963451a74e85556f7c503863666546ae78e6f6e3c6e2d86c02d4b2e0822172e893e5ef66f8f3982b69de20e152e714633f02af2e5904e06b2a67269285258a087ada1fe365cb895fecb9a53add1f70a1ea8b7bda4434a4b8aa9c481570d9ecd19de87b4ea3dc2c7dc76af0beec3fefb4118e0663c185d", 0},
};

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

static std::vector<unsigned char> Bytes(std::string_view hex) {
    std::vector<unsigned char> result;
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        result.push_back(static_cast<unsigned char>((digit(hex[i]) << 4) | digit(hex[i + 1])));
    }
    return result;
}

static std::vector<unsigned char> Coordinates(const EcPoint* point, size_t width) {
    std::vector<unsigned char> result(1 + 2 * width, 0);
    if (mbedtls_mpi_cmp_int(&point->point.MBEDTLS_PRIVATE(Z), 0) == 0) return {0};
    Require(mbedtls_mpi_cmp_int(&point->point.MBEDTLS_PRIVATE(Z), 1) == 0, "affine coordinates");
    result[0] = 4;
    Require(mbedtls_mpi_write_binary(&point->point.MBEDTLS_PRIVATE(X), result.data() + 1, width) == 0, "read X");
    Require(mbedtls_mpi_write_binary(&point->point.MBEDTLS_PRIVATE(Y), result.data() + 1 + width, width) == 0, "read Y");
    return result;
}

static uint64_t EcError(unsigned reason) {
    return 0x10fff000u | reason;
}

static void CheckVector(const Vector& vector) {
    std::unique_ptr<EcGroup, decltype(&EC_GROUP_free_nid_postfix)> group(EC_GROUP_new_by_curve_name_nid_postfix(vector.nid), EC_GROUP_free_nid_postfix);
    std::unique_ptr<EcPoint, decltype(&EC_POINT_free_nid_postfix)> point(EC_POINT_new_nid_postfix(group.get()), EC_POINT_free_nid_postfix);
    const auto expected = Bytes(vector.encoded);
    const size_t width = (expected.size() - 1) / 2;
    Require(Coordinates(point.get(), width) == std::vector<unsigned char>{0}, "new point is infinity");
    auto decode = [&](const std::vector<unsigned char>& input) {
        return EC_POINT_oct2point_nid_postfix(group.get(), point.get(), input.data(), input.size(), nullptr);
    };
    for (const int form : {4, 2, 6}) {
        auto input = expected;
        input[0] = static_cast<unsigned char>(form | (form == 4 ? 0 : (expected.back() & 1)));
        if (form == 2) input.resize(1 + width);
        ERR_clear_error_nid_postfix();
        LibreSslPutError_nid_no_patch(1, 2, 3);
        Require(decode(input) == 1, "valid encoding");
        Require(Coordinates(point.get(), width) == expected, "reference coordinates");
        Require(ERR_get_error_nid_postfix() == (form == 2 ? 0 : 0x01002003u), "compressed decoding clears errors");
        Require(ERR_get_error_nid_postfix() == 0, "no extra errors");
    }
    auto compressed = expected;
    compressed.resize(1 + width);
    compressed[0] = static_cast<unsigned char>(2 | ((expected.back() & 1) ^ 1));
    Require(decode(compressed) == 1, "opposite compressed parity");
    mbedtls_mpi sum;
    mbedtls_mpi_init(&sum);
    Require(mbedtls_mpi_read_binary(&sum, expected.data() + 1 + width, width) == 0, "reference Y");
    Require(mbedtls_mpi_add_mpi(&sum, &sum, &point->point.MBEDTLS_PRIVATE(Y)) == 0, "sum Y");
    Require(mbedtls_mpi_cmp_mpi(&sum, &group->group.P) == 0, "opposite point Y is p minus reference Y");
    mbedtls_mpi_free(&sum);
    Require(decode(expected) == 1, "reset point");
    auto reject = [&](std::vector<unsigned char> input, unsigned reason) {
        ERR_clear_error_nid_postfix();
        Require(decode(input) == 0, "reject malformed input");
        Require(ERR_get_error_nid_postfix() == EcError(reason), "reference rejection error");
        Require(ERR_get_error_nid_postfix() == 0, "one error");
        Require(Coordinates(point.get(), width) == expected, "early rejection preserves point");
    };
    reject({}, 100);
    for (unsigned prefix = 0; prefix < 256; ++prefix) {
        if (prefix == 2 || prefix == 3 || prefix == 4 || prefix == 6 || prefix == 7) continue;
        auto input = expected;
        input[0] = static_cast<unsigned char>(prefix);
        reject(input, 102);
    }
    for (const unsigned form : {2u, 4u, 6u}) {
        auto input = expected;
        input[0] = static_cast<unsigned char>(form | (form == 4 ? 0 : (expected.back() & 1)));
        const size_t correct = form == 2 ? 1 + width : expected.size();
        for (size_t length = 1; length <= correct + 1; ++length) {
            if (length == correct) continue;
            input.resize(length);
            reject(input, 102);
        }
    }
    auto hybrid = expected;
    hybrid[0] = static_cast<unsigned char>(6 | ((expected.back() & 1) ^ 1));
    reject(hybrid, 102);
    for (const bool x : {true, false}) {
        auto input = expected;
        Require(mbedtls_mpi_write_binary(&group->group.P, input.data() + 1 + (x ? 0 : width), width) == 0, "encode field modulus");
        reject(input, 102);
        if (x) {
            input[0] = 2;
            input.resize(1 + width);
            reject(input, 102);
        }
    }
    std::vector<unsigned char> nonsquare(1 + width, 0);
    nonsquare[0] = 2;
    nonsquare.back() = vector.nonsquareX;
    for (const unsigned char prefix : {2, 3}) {
        nonsquare[0] = prefix;
        ERR_clear_error_nid_postfix();
        LibreSslPutError_nid_no_patch(1, 2, 3);
        Require(decode(nonsquare) == 0, "reject compressed nonresidue");
        Require(ERR_get_error_nid_postfix() == EcError(110), "compressed failure replaces error queue");
        Require(ERR_get_error_nid_postfix() == 0, "compressed failure has one error");
        Require(Coordinates(point.get(), width) == expected, "compressed failure preserves point");
    }
    auto offCurve = expected;
    offCurve.at(offCurve.size() - 1) ^= 1;
    ERR_clear_error_nid_postfix();
    Require(decode(offCurve) == 0, "reject off-curve coordinates");
    Require(ERR_get_error_nid_postfix() == EcError(107), "off-curve error");
    Require(Coordinates(point.get(), width) == offCurve, "late rejection leaves decoded coordinates");
    Require(decode({0}) == 1, "decode infinity");
    Require(Coordinates(point.get(), width) == std::vector<unsigned char>{0}, "infinity state");
    Require(decode(expected) == 1, "reuse infinity point");
    auto copy = std::unique_ptr<EcGroup, decltype(&EC_GROUP_free_nid_postfix)>(EC_GROUP_new_by_curve_name_nid_postfix(vector.nid), EC_GROUP_free_nid_postfix);
    group.reset();
    Require(EC_POINT_oct2point_nid_postfix(copy.get(), point.get(), expected.data(), expected.size(), nullptr) == 1, "point survives original group");
}

template<typename TAction> static void RequireThrows(TAction action, const char* what) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, what);
}

static void CheckUnsupportedInputs() {
    auto group = std::unique_ptr<EcGroup, decltype(&EC_GROUP_free_nid_postfix)>(EC_GROUP_new_by_curve_name_nid_postfix(415), EC_GROUP_free_nid_postfix);
    auto point = std::unique_ptr<EcPoint, decltype(&EC_POINT_free_nid_postfix)>(EC_POINT_new_nid_postfix(group.get()), EC_POINT_free_nid_postfix);
    auto bytes = Bytes(Vectors[3].encoded);
    RequireThrows([&] { EC_POINT_oct2point_nid_postfix(nullptr, point.get(), bytes.data(), bytes.size(), nullptr); }, "null decoding group throws");
    RequireThrows([&] { EC_POINT_oct2point_nid_postfix(group.get(), nullptr, bytes.data(), bytes.size(), nullptr); }, "null decoding point throws");
    RequireThrows([&] { EC_POINT_oct2point_nid_postfix(group.get(), point.get(), nullptr, 1, nullptr); }, "null nonempty buffer throws");

}

int main() {
    for (const auto& vector : Vectors) CheckVector(vector);
    ERR_clear_error_nid_postfix();
    Require(EC_POINT_new_nid_postfix(nullptr) == nullptr, "null group allocation");
    Require(ERR_get_error_nid_postfix() == EcError(67), "null group error");
    EC_POINT_free_nid_postfix(nullptr);
    CheckUnsupportedInputs();
    return 0;
}
