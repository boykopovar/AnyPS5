#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstddef>
#include <string_view>
constexpr std::array<std::size_t,13> Lengths{0,3,55,56,63,64,65,119,120,127,128,129,1000};
constexpr std::array<std::array<std::string_view,13>,6> Expected{{
{"d41d8cd98f00b204e9800998ecf8427e","900150983cd24fb0d6963f7d28e17f72","f34a17c47cedbb29f0a56d9dac7ed796","43db5a12fb403e69293c5e639d740155","e0783f61352e8c16449123b04a72fb10","0d34e944593011a67566319633eb4406","b2a9d13fbfd2aefa4bd162473e3af90f","59ea2057c9e1b1f137d053baece0bb97","3ea453eec6c21fd652d049e9cc25c5ae","e39fe4d60a641efea2ef3724536daabc","7ed83cb81d420530e66ce83690f0dbbc","4044cbe80e7ef1337721aa57abc3ad00","0fa02bd13703a789a34cdb97afb5c8cf"},
{"da39a3ee5e6b4b0d3255bfef95601890afd80709","a9993e364706816aba3e25717850c26c9cd0d89d","0ea5c907ba2f0d2106326f5e0270984860e23ed6","c8ec72d24a68e71cfd5cd981e588b9bd595a11a4","c21a2ff77cce7a47e2d87057ce98b2c4f1cb48a5","750d7e56670067b3f796fac1d23163aa1986cf4b","fbd95dd26c4f1c9250e0c5d2b71186705e70431a","77dc50fe23267e5b5c099e6b7a99991a44bbaf8c","c9c4f4feb30989a9810bcba6cb041e8488c281c3","c27d8c228f23a2a5d072fe244646497231a3699e","f40c02c67c95d990d0bef388684cebfa2db4db8b","dc327e8446da653bad8569157fc4dd541ee92b23","45512318920a1c04f4949209b9da64d021f17bb4"},
{"d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f","23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7","e548730d2e01b259dacb11b71d97469e9e3bed0c553b93f76ab363f1","bf3f3fb80543b7303888ecf3ca3211f6ab4385fd4e1b00cc3de226e1","e11dc7554f519feb86af1b8f1f2a724ea9acd7d3350022fa9654181f","e7984c0f2ed1344a823558c5e9487476368c7ad7ee194a725fa6dfc4","1e4b44d191b441b3a2a2c265e00ca5f84d13f594c150ab07cacf8fd2","285e1bd7d96fa75f1d98bee8b819027ed81dc8a16033e04b2d059a1d","98f127327c85e5445bc9490b9699e290d41573c40e70941aa024de3e","3bb8e70be9b791712b3fbeb5074a7b65ff13cdbe05b264a7b3f22e2b","49c0798cb572d72d5f9fa1827bdace1980929794b39174823d473194","307ae7fb47c20f2e4ca7e2023f653637388e01ce3fbbc922081eada9","d71f21e76d29eb0e0318c79f33423386e2d2971aa9df9383e13ef335"},
{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","6a2031e2faddf08585d3d5bf3772f81b3a2b84b9ebd0dadf19f441cf0594f8f2","bf917a3a0c3d5a50bc4bebab3467fec95ffe807d25a0475f35ffbd2d6318ce61","df9dafac0d0c133edf7e32ab4125eef45a9d988ae1ceb1490d462b59a0fe33e0","1c7132d40ea890ee701965a8bd89a927404b1c051b21def7bf24aa994f81e68d","73e54a745fb0017ee987b877c75e3bc57c04b8517d594fcdc57bc10bdeb4c0a5","d2d05520a825d4d41a3c00e178793892332d996ce273d518733454f9402d17a3","2d660746a50c0261c3637cdf2908e82f36d6525ae2baa9f59114b6fef425839c","9da241922c056880941a5f0ef5125773d8b4bbb50dc085cec3c61649c2c88c84","3e93cae50d3c4094cf01381f3a0eda1541221ef058a7f6c86979d838feb7f277","6e1a974560764131ca51d67fc0c5149d52ac6b28eda561f6fa0e34b56c55be2c","f399f531ab8d90d7e21ed9b38a588526081af93fd39d4fcab7f74a94e2e99a8f"},
{"38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b","cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7","70c548c94c7707314853d14aeda0b22bd6f8620a9555eaa880c69f947d815285a4a36a034b741573f0703187004f444c","a8fd3a9d8495c39ee8ed02de969e21b075593f07f4fd93e8f1b89f81240e0a5b41ba4520e71594e74d78c44b9772bd24","8413992ad869a49d3c52ccb02344a36207f30ff9d3b45dffc7e9dd78ec5bb7ae252deb6ea7b50f4081aecec229789f38","49c39457f0bf44c6128bef3250f9ea65f4a5bf4aa100710e2406890faa87146bdc81353ef4b3a6fa0ce401a36593f11c","fdd4141506512c161cf60d4b1ec5624cc1b8edb1e74cb82887f6312e2d76ab5c32ce7d55504ba6af9de44cf8e1afb281","4d9c7694c165afce56368aace0dbebf063ffc9ac88916f66c2df0b96048e15e09a15eeb0ee261dbb934e27494dd3d144","fa92b9dd596d9780107a84e5656eae85f7d173fabb24bc63770b55a83f7103c7f7e620b5812aeabf30d417d2f6940c8e","c0e201f335b0b8c16ad6917ac94a7c8f6b7b90f4ac4d3800603f342bac5d3fb1c6d922a020a90e4e9c55d092d9b0a50e","c475856ed18471c1439ab22c7983f1a707186c05d01f3011daeed2057b1856858e6be403d182494a20ae118c938e236d","980574fa218083dbf3e48a5b03e820fd22e46bfdca467045b02d1adb50fbf2396aa91a9988ac4f644db978ecd1e4a803","82f84958b89855de123364d4ae68fccc3365bd8e4765d5d4d2e05960dabceeae4db729e8ecccb29d0c25e9a95bc43ca0"},
{"cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e","ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f","34a3e74d1b83588f892aebb2fe72303f0a346ea398c49b4a81aeb6a37fb491d15861cc290134f359ba17a60222aa236bad7fee671b382ac639cfac06f92716f2","61a2ccd0d6a5f967e014031effd5af7bba63ee14693334b99cb0898a77eaae81d513999ae4b40309b569adefa4656c090cd6ed084ad35e6e3383cd7f4373d457","66fb952d2d60cb4c0beea4d6120ccac579ae775c684eb8a0f78a5b0777ba527e8ac9475302a118507f6b2c944b14ffa73fd5efe28e137719043339d336cd8310","80de44fce6541bfaba417f75a634b4e9eb75210c31b79e79a181efd4fdfd4900189b3f7124ad2c46b43dccb7b8ee78c317cd1ba21688f30bff296baace6b2bf6","e2c83200bdade659cf9e4300441ca9b93a8e264d3cfc0041e72157c1d445e8520f7dc0555c147d60201060d2bfeac573cabff2f8f21291f6dbe6245a07b36aa7","3650523105a34a64c61e23205eb7f4226a033e316c1e8e895571fa7361c5443cdb3ff8fad198356c35b61c25cad131878ce1c271ec838e6a0976586c6c8274d7","cf86f5cf5f8cc01015e1339c034a666688a13f45b71733ca2f8e204ebb2a14ce8da5026e42aebf6077faaefbb00d63a6b57b39c1da621413a673f0f32f8134dd","446c96b68dc329e4601eb7c32df513b1f44b4dbb8fe3584b46648f601bae227e30b34fed2978bbd726c0b7da68135b5755633c0bb003d1adf8cf33700471a059","c2b8eda0ff92b7b56e101ebd9e6ea09b28729a2d28ae647cfbbfef50e75b242f0f57e87ebf7308dd281a03ca611df3e42c73d246298a72af0bd12f35b0117c20","6cfd593f346a98c349778f91ff684c3d2dcd239914e9652194c5b5c6cbb7fe62911d3b9f3d4bc0d4ffdcc5f1292a8a21402e56a76d0c6fc03b34ccc4de5a1a01","d64a386806beea82e2999b7bbab774a812240cd9639f116f6ce344e58c54f91851e21ef8883994815edcf6984e886180f0a6cb8b496860bedac1dea9d9eaa2af"},
}};

using DigestContext = EvpMdContext;
using DigestMethod = EvpMd;

static const char* moduleDirectory;

static DigestContext* NewContext() {
    return reinterpret_cast<DigestContext*>(EVP_MD_CTX_new_nid_postfix());
}

static void FreeContext(DigestContext* context) {
    EVP_MD_CTX_free_nid_postfix(reinterpret_cast<LibreSsl3::DigestContext*>(context));
}

static int InitContext(DigestContext* context, const DigestMethod* method) {
    return EVP_DigestInit_nid_postfix(reinterpret_cast<LibreSsl3::DigestContext*>(context),
        reinterpret_cast<const LibreSsl3::DigestMethod*>(method));
}

static const DigestMethod* Sha1Method() {
    return reinterpret_cast<const DigestMethod*>(EVP_sha1_nid_postfix());
}

static const DigestMethod* Sha224Method() {
    return reinterpret_cast<const DigestMethod*>(EVP_sha224_nid_postfix());
}

static void Require(bool ok, std::source_location where = std::source_location::current()) {
    if (!ok) { std::fprintf(stderr, "mixed EVP check line%u\n", where.line()); std::abort(); }
}
template<typename T> static void Throws(T call) { try { call(); } catch (const std::exception&) { return; } Require(false); }
static bool failAllocation;
static std::size_t allocations;
static void* APS5_VABI Allocate(std::size_t size) { if (failAllocation) return nullptr; auto* p = std::malloc(size); if (p) ++allocations; return p; }
static void APS5_VABI Free(void* p) { if (p) { Require(allocations > 0); --allocations; } std::free(p); }
static void* APS5_VABI Calloc(std::size_t count, std::size_t size) { if (failAllocation) return nullptr; auto* p = std::calloc(count, size); if (p) ++allocations; return p; }
static void* APS5_VABI Reallocate(void* p, std::size_t size) { if (!p) return Allocate(size); return std::realloc(p, size); }
static void* APS5_VABI Align(std::size_t, std::size_t size) { return Allocate(size); }
static void* APS5_VABI Realign(void* p, std::size_t size, std::size_t) { return Reallocate(p, size); }
static int APS5_VABI PosixAlign(void** p, std::size_t, std::size_t size) { *p=Allocate(size); return *p ? 0 : 12; }
static void Register() {
    void* api[10]{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}
static std::string Hex(const unsigned char* p,std::size_t size) { std::string s; for(std::size_t i=0;i<size;++i){s+="0123456789abcdef"[p[i]>>4];s+="0123456789abcdef"[p[i]&15];}return s; }
static std::string Message(std::size_t length) { std::string s(length,'a'); for(std::size_t i=0;i<length;++i)s[i]=static_cast<char>('a'+i%23);return s; }
static void Owner(const void* pointer,const char* expected) { Dl_info info{}; Require(dladdr(pointer,&info)); const auto* directory=moduleDirectory; Require(directory); Require(std::filesystem::canonical(info.dli_fname)==std::filesystem::canonical(std::filesystem::path(directory)/expected)); std::printf("binding %s\n",info.dli_fname); }
static unsigned cleanupCalls;
static std::uint64_t cleanupFlags;
static bool clearKey;
static int APS5_VABI CustomInit(DigestContext* c) { Require(c->mdData); *static_cast<std::uint32_t*>(c->mdData)=7; return 1; }
static int APS5_VABI CustomUpdate(DigestContext* c,const void* p,std::size_t n) { for(std::size_t i=0;i<n;++i)*static_cast<std::uint32_t*>(c->mdData)+=static_cast<const unsigned char*>(p)[i];return 1; }
static int APS5_VABI CustomFinal(DigestContext* c,unsigned char* p) { const auto n=*static_cast<std::uint32_t*>(c->mdData); for(unsigned i=0;i<4;++i)p[i]=static_cast<unsigned char>(n>>(8*i));return 1; }
static int APS5_VABI CustomCleanup(DigestContext* c) { ++cleanupCalls;c->flags|=cleanupFlags;if(clearKey)c->pkeyContext=nullptr;return 0; }
static void Customs() {
    DigestMethod m{987,0,4,0,CustomInit,CustomUpdate,CustomFinal,nullptr,CustomCleanup,nullptr,nullptr,{},4,4,nullptr};
    for(bool create:{false,true})for(bool init:{false,true}) {
        auto* c=create?EVP_MD_CTX_create_nid_postfix():NewContext();Require(c);
        Require((init?EVP_DigestInit_ex_nid_postfix(c,&m,nullptr):InitContext(c,&m))==1);
        Require(EVP_DigestUpdate_nid_postfix(c,"abc",3)==1);std::array<unsigned char,5> out{};out[4]=0xa5;std::uint32_t n=0;
        Require(EVP_DigestFinal_ex_nid_postfix(c,out.data(),&n)==1&&n==4&&out[4]==0xa5&&out[0]==45&&out[1]==1);
        const auto before=cleanupCalls;FreeContext(c);Require(cleanupCalls==before);
    }
    auto* c=EVP_MD_CTX_create_nid_postfix();Require(InitContext(c,&m)==1);auto* data=c->mdData;cleanupFlags=4|0x400;c->pkeyContext=c;const auto before=cleanupCalls;FreeContext(c);Require(cleanupCalls==before+1);Require(*static_cast<std::uint32_t*>(data)==7);Free(data);cleanupFlags=0;
    c=EVP_MD_CTX_create_nid_postfix();Require(EVP_DigestInit_ex_nid_postfix(c,&m,nullptr)==1);c->pkeyContext=c;clearKey=true;FreeContext(c);clearKey=false;
    c=NewContext();Require(EVP_DigestInit_ex_nid_postfix(c,&m,nullptr)==1);c->flags=4;data=c->mdData;Require(EVP_MD_CTX_cleanup_nid_postfix(c)==1);Require(*static_cast<std::uint32_t*>(data)==7);Free(data);FreeContext(c);
    c=NewContext();c->flags=0x100;const auto clean=cleanupCalls;Require(EVP_DigestInit_ex_nid_postfix(c,&m,nullptr)==1&&!c->mdData&&!c->update);EVP_MD_CTX_destroy_nid_postfix(c);Require(cleanupCalls==clean+1);
    c=EVP_MD_CTX_create_nid_postfix();failAllocation=true;Require(EVP_DigestInit_ex_nid_postfix(c,&m,nullptr)==0);failAllocation=false;Require(!c->digest&&!c->mdData&&!c->update);Require(EVP_DigestInit_ex_nid_postfix(c,&m,nullptr)==1);FreeContext(c);
}
extern "C" int LibreSsl3MixedEvp_nid_no_patch(const char* directory, const char* mode) {
    moduleDirectory=directory;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Register();
    if(mode!=nullptr) {
        auto* c=NewContext();const auto* m=Sha1Method();
        if(std::strcmp(mode,"red-update")==0){Require(InitContext(c,m)==1);EVP_DigestUpdate_nid_postfix(c,"abc",3);}
        else EVP_DigestInit_ex_nid_postfix(c,m,nullptr);
        FreeContext(c);return 0;
    }
    Owner(reinterpret_cast<const void*>(&EVP_MD_CTX_new_nid_postfix),"libSceLibreSsl3.prx");
    Owner(reinterpret_cast<const void*>(&EVP_MD_CTX_free_nid_postfix),"libSceLibreSsl3.prx");
    Owner(reinterpret_cast<const void*>(&EVP_DigestInit_nid_postfix),"libSceLibreSsl3.prx");
    Owner(reinterpret_cast<const void*>(&EVP_sha1_nid_postfix),"libSceLibreSsl3.prx");
    Owner(reinterpret_cast<const void*>(&EVP_sha224_nid_postfix),"libSceLibreSsl3.prx");
    Owner(reinterpret_cast<const void*>(&EVP_MD_CTX_create_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_MD_CTX_destroy_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_MD_CTX_cleanup_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_MD_CTX_init_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_DigestInit_ex_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_DigestUpdate_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_DigestFinal_ex_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_Digest_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_MD_size_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_md5_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_sha256_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_sha384_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&EVP_sha512_nid_postfix),"libSceLibreSsl.prx");
    Owner(reinterpret_cast<const void*>(&HMAC_nid_postfix),"libSceLibreSsl.prx");
    const std::array<const DigestMethod*,6> methods{EVP_md5_nid_postfix(),Sha1Method(),Sha224Method(),EVP_sha256_nid_postfix(),EVP_sha384_nid_postfix(),EVP_sha512_nid_postfix()};
    std::size_t cases=0;
    for(std::size_t method=0;method<methods.size();++method)for(std::size_t input=0;input<Lengths.size();++input)for(auto stride:{std::size_t{1},std::size_t{63},std::size_t{129}})for(bool create:{false,true})for(bool init:{false,true})for(unsigned release=0;release<4;++release) {
        const auto text=Message(Lengths[input]);const auto* m=methods[method];auto* c=create?EVP_MD_CTX_create_nid_postfix():NewContext();Require(c);
        Require((init?EVP_DigestInit_ex_nid_postfix(c,m,nullptr):InitContext(c,m))==1);
        Require(EVP_DigestUpdate_nid_postfix(c,nullptr,0)==1);
        for(std::size_t offset=0;offset<text.size();offset+=stride)Require(EVP_DigestUpdate_nid_postfix(c,text.data()+offset,std::min(stride,text.size()-offset))==1);
        std::array<unsigned char,65> out;out.fill(0xa5);std::uint32_t size=0;Require(EVP_DigestFinal_ex_nid_postfix(c,out.data(),&size)==1);Require(size==static_cast<std::uint32_t>(EVP_MD_size_nid_postfix(m))&&out[size]==0xa5&&Hex(out.data(),size)==Expected[method][input]);
        if(release>=2) {Require(EVP_MD_CTX_cleanup_nid_postfix(c)==1);Require(EVP_DigestInit_ex_nid_postfix(c,m,nullptr)==1);Require(EVP_DigestUpdate_nid_postfix(c,text.data(),text.size())==1);Require(EVP_DigestFinal_ex_nid_postfix(c,out.data(),&size)==1&&Hex(out.data(),size)==Expected[method][input]);Require(EVP_MD_CTX_cleanup_nid_postfix(c)==1);EVP_MD_CTX_init_nid_postfix(c);}
        if(release%2)EVP_MD_CTX_destroy_nid_postfix(c);else FreeContext(c);++cases;
    }
    for(std::size_t method=0;method<methods.size();++method)for(std::size_t input=0;input<Lengths.size();++input){const auto s=Message(Lengths[input]);std::array<unsigned char,65> out;out.fill(0xa5);std::uint32_t n=0;Require(EVP_Digest_nid_postfix(s.data(),s.size(),out.data(),&n,methods[method],nullptr)==1&&Hex(out.data(),n)==Expected[method][input]&&out[n]==0xa5);++cases;}
    for (std::size_t method=1;method<=2;++method) {
        const std::array<unsigned char,20> key{11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11};
        const std::array<const char*,2> expected{"b617318655057264e28bc0b6fb378c8ef146be00","896fb1128abbdf196832107cd49df33f47b4b1169912ba4f53684b22"};
        std::array<unsigned char,65> output;output.fill(0xa5);std::uint32_t length=0;
        Require(HMAC_nid_postfix(methods[method],key.data(),static_cast<int>(key.size()),reinterpret_cast<const unsigned char*>("Hi There"),8,output.data(),&length)==output.data());
        Require(Hex(output.data(),length)==expected[method-1]&&output[length]==0xa5);++cases;
    }
    Customs();Require(allocations==0);std::printf("mixed EVP %zu lifecycle/vector cases and custom/error/ownership checks passed\n",cases);
    return 0;
}
