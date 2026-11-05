#include "prx/libSceLibreSsl/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "boundary check failed at line %u\n", location.line());
        std::abort();
    }
}

class Guarded {
    unsigned char* memory;
    std::size_t page;
    std::size_t writable;
public:
    explicit Guarded(std::size_t capacity = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
#endif
        writable = ((capacity + 128 + page - 1) / page) * page;
#ifdef _WIN32
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, writable + page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        memory = static_cast<unsigned char*>(mmap(nullptr, writable + page * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
        Reset();
    }
    ~Guarded() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, writable + page * 2) == 0);
#endif
    }
    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;
    void Protect(bool readOnly) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory + page, writable, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(memory + page, writable, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
    }
    void Reset() {
        Protect(false);
        std::memset(memory + page, 0xa5, writable);
    }
    unsigned char* End(std::size_t count) {
        Require(count + 64 <= writable);
        return memory + page + writable - (std::max)(count, std::size_t{1});
    }
    void Prefix(const void* address) const {
        const auto* bytes = static_cast<const unsigned char*>(address);
        for (int index = 1; index <= 32; ++index) Require(bytes[-index] == 0xa5);
    }
};

struct Vector { std::size_t length; const char* sha256; const char* sha512; };
static const Vector vectors[]{{0,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"},
{1,"4a64a107f0cb32536e5bce6c98c393db21cca7f4ea187ba8c4dca8b51d4ea80a","4dab249e3ef1f27d323fea6d443dd6f2ab3655e2b4faf1078c7ad6af83a18f2efbbed88b2a63b1afff3d196f8b9707f28cfa8cc08579db5428d2a5dc1c5c9d63"},
{2,"64d4cb14307f8cf59558dbd24a8c021105cf64817d149dbef6dcd8f1a06d38c0","e9828a68d48567101bcdaf4573f5307901b73e079c8e085e92608fd6dec4d8ffad8fdb1dc1c8e38d6cf8771486d474cdafd706314c5ad3d0f1bb253ed7a18556"},
{31,"0f36f8082cbd478871b32e88e6b54d50ab7571644f227d92370f22393b16e734","d964d5eaff37280ccedba5e287596095aee205a20b5f029cdb94d66c539567d285103a92d2086641ed76dde0ca9b2a2db5cb60603658e63c7f059bd17b7a22ea"},
{54,"6596f144e3753023792ddb1353075af5eb485d0603061ed66d0da14db041505c","05c2c57b9dda92565de778e3ca2040adfe6f6fb115cd45a68677d06a3e9717b11e2dc04c111cf9233fb98c256ee097a90ecd5150295562c82ecd3c25c52a6b55"},
{55,"2ac4d4b71cb2b089e0ae4e7dd40233dce1620f5ddb664af7843b1f260fb30256","0c03e86d484fa11d4d6b58c995392335dd85e1b03da3a2556d8356fab03cde3c9b69671f0da811e288e7bcbe38480f7387f6af98f464306118f29b97def95ca5"},
{56,"6b38af1c7ff0ab5784fd3a69b1ec7bc7c53c62f4f9285ffe0530a48129475c01","a6a1c663e3588867e88278c8df49c4cd46928d28d89789e19958e6bd5949a8853c318cbc6a9e6504c386d02f82fe90f2c00bd2c025bb902b23ede594fdfef45e"},
{57,"69e9add917eb79723e83f20508c279e432dca2be195bc382db97edca91352a66","b2c8c2458791fe08c75656596cc515470cb899170cabf9982e6942f73149aab5c5d5de9394aa23d91514320a83d6d479e1f0a759f5d8df6e65bde10f6af924c4"},
{63,"2c25eb6a366ea3cad3e0c55bec4c445320cd725222a949a8f4153864585e489a","c6d3d7059b2e3404deff0fe5ca779cb299d79af107529a6263ed1a76eb63a1430d30b77d110824f6e734be77f0dc1dbe8a2428047fa2fb0149da51370d3d4d6f"},
{64,"f908cb5a766b7ff7e66524100ce39860932c70daae76646eeb64c88af4846248","e9496ea128459bf37ca8fb0b01f8c6bde9ec29ee9d0dc7bc6309a17ffc34441fde3a27ebe06a8948c3ab7bea5b6b92adf435c26121747eef285c0aa48a82ec80"},
{65,"aa388866d286649aae6c2893cb0843d12312c14a93591fe8b65ad8ad7c11e898","6391d93ce279a462279e1f8f0700a165b02fe35f0ce245751313b8cedd207953c39ee31684777285e4ae9cfa8f4a14456f5be4ef51de0b4ab1b70871753f25e2"},
{110,"913206e35a78522e8df14087865b8c30b3095f990770c5e0ba43b1d5ebbc125f","d57df9ccb177b942619f3b33788fbeda6befbd4ed025f4cecd2d5521b6ea984cfefe6b079edfe1906aad645553e891b58afd0c3145b0c6ee46ea9339adefbd76"},
{111,"7db05f545e7b90987079612860f55ca9640676aaaeb9c6cc80553ae236cd8e89","3e5794e8d9b7e367695406d93b03b0e8e503fcdafee584a77696b15521cc57df1a7b6072f425500515ae7ae99393b72d3d6223b4f921bf65ddd84cf233fb2ff3"},
{112,"989d451a15e794aef158f971b35431617553e68704bad8fc97d0ca17d03a50ce","12edd38171b2774ca451029be5e3af01da63c2e9d0f8efb5e8405ed848f1e7d14b8dcf61ffdb12838020598f33409341f5991c5dbc45a40329f7a2a5f0343a69"},
{113,"af22a40dd7773cb4db256a1aa47626a32e486b97b894ed5c0bd3b9335dd549a1","9f0b6bcc0e1430994e247157c5cd8ad4e1912fdf41c47862d62d60e4140ccb613bb9b180129dd9e3254f50b956d7d9ecb4516b10b9d6ebd1817bef9873c05e24"},
{127,"6d658993c024638c9422df783a79734ccf97a7d72c9569a9ba9a0a6dfeb93dd9","9081635240fec99f953b5d9fd1e92bc58ca332ec01c41dc656086d542947ebec1075cb12823731f3255e1e71ebb2eb0ece57fae92be92c79ab1fe081528042b3"},
{128,"d317ae146660678bac9b6df8b8e46cac41820e9795390e656d36300d7c739ad4","effe331b1762938ea18e5a432d6aa0711e7234b410bafa9e13018ee65e4eb919acf56640fe2a2a847e4c8c3c7f851c1c14d1728c2bcd85e9a63d94e563dc8761"},
{129,"2ae63cabddb4537f5223edc5eb45b84b9c444a6a6fe6b07c68e0eb9c243767d8","ad4db0311b4a65d6e4bba179f8fe11805785da6b33c37c8c6fde7260de6e2f96dd45a1f5e664f9692f4c3f5c1a2b74246d6b1234686e2dc3cb818ccc3bf7c078"},
{255,"2566af4ed1c32d1f63b5dc772ea30fb34ab9efc644fce727ddcb2030913e6d14","fda6d9e664fa4b3eb08bb514c8cfbb2a6f29eff31da72d7c99a6f1874b782d05a7e292b80daf7799c5ef10550bffc6e70bae4ddeecd98c6ae5371ff8f05a6ea1"},
{256,"e224556bdda7fc66a618f4995f3b37d2a19d8bbc25176114a9d13891db2f8753","e0ed4f3c7583f10a9246af32c684ab9a1e3a80aaa38edc9cc63bf80075e24ba989c5468670e258cdcf0eb5ffea709e1b96745600ee67cfc7a0341bbdd105ac78"}};
static std::string Hex(const unsigned char* bytes, std::size_t count) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (std::size_t i = 0; i < count; ++i) { result += digits[bytes[i] >> 4]; result += digits[bytes[i] & 15]; }
    return result;
}
template<class TContext, auto TInit, auto TUpdate, auto TFinal>
static std::size_t Check(bool wide, std::size_t digestLength) {
    std::size_t cases = 0;
    for (const auto& vector : vectors) {
        Guarded input(vector.length);
        auto* source = input.End(vector.length);
        for (std::size_t i = 0; i < vector.length; ++i) source[i] = static_cast<unsigned char>(i * 73 + 17);
        const std::vector<unsigned char> saved(source, source + vector.length);
        input.Protect(true);
        for (std::size_t chunk : {std::size_t{1},std::size_t{7},std::size_t{63},std::size_t{128},std::size_t{257}}) {
            Guarded contextMemory(sizeof(TContext)), output(digestLength);
            auto* context = std::construct_at(reinterpret_cast<TContext*>(contextMemory.End(sizeof(TContext))));
            auto* digest = output.End(digestLength);
            Require(TInit(context) == 1);
            for (std::size_t offset = 0; offset < vector.length; offset += chunk) {
                const auto count = (std::min)(chunk, vector.length - offset);
                Require(TUpdate(context, source + offset, count) == 1);
            }
            Require(TFinal(digest, context) == 1);
            Require(Hex(digest, digestLength) == (wide ? vector.sha512 : vector.sha256));
            Require(std::equal(saved.begin(), saved.end(), source));
            input.Prefix(source); contextMemory.Prefix(context); output.Prefix(digest);
            ++cases;
        }
    }
    return cases;
}
int main() {
    const auto count = Check<Sha256Context,SHA256_Init_nid_postfix,SHA256_Update_nid_postfix,SHA256_Final_nid_postfix>(false,32)
                     + Check<Sha512Context,SHA512_Init_nid_postfix,SHA512_Update_nid_postfix,SHA512_Final_nid_postfix>(true,64);
    std::printf("%zu guarded SHA input/context/output and padding cases passed\n", count);
}
