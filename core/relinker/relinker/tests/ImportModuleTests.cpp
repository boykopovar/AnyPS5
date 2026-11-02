#include <domain/ImportModule.hpp>
#include <relinker/guest/GuestImage.hpp>
#include <io/BufferUtils.hpp>
#include <cstring>
#include <array>
#include <iostream>
using Bytes = std::vector<std::uint8_t>;
static void Ph(Bytes& b, int i, unsigned type, unsigned flags, unsigned off, unsigned va, unsigned size) {
    const auto p=64+i*56; Io::WriteU32(b,p,type); Io::WriteU32(b,p+4,flags);
    Io::WriteU64(b,p+8,off);Io::WriteU64(b,p+16,va);Io::WriteU64(b,p+24,va);
    Io::WriteU64(b,p+32,size);Io::WriteU64(b,p+40,size);Io::WriteU64(b,p+48,8);
}
static Bytes Fixture(bool reverse=false, const char* hostName="libmonosgen-2.0.prx", const char* logicalName="libmonosgen") {
    Bytes b(0x4000);std::memcpy(b.data(),"\x7f" "ELF\x02\x01\x01",7);
    Io::WriteU16(b,16,3);Io::WriteU16(b,18,62);Io::WriteU32(b,20,1);
    Io::WriteU64(b,32,64);Io::WriteU16(b,52,64);Io::WriteU16(b,54,56);Io::WriteU16(b,56,3);
    Ph(b,0,1,5,0x1000,0,0x1000);Ph(b,1,1,6,0x2000,0x2000,0x1000);
    b[0x1020]=0xc3;b[0x1028]=0xc3;
    std::string strings(1,'\0');auto str=[&](const char* s){auto o=strings.size();strings+=s;strings+='\0';return o;};
    const auto a=str(reverse?"alias#B#A":"alias#A#A"),c=str(reverse?"alias#A#A":"alias#B#A");
    const auto imp=str("imported#A#B"),host=str(hostName),module=str("fixtureModule"),logical=str(logicalName);
    std::copy(strings.begin(),strings.end(),b.begin()+0x2200);
    const std::pair<std::uint64_t,std::uint64_t> tags[]={{5,0x2200},{10,strings.size()},{6,0x2400},{11,24},{4,0x2600},{7,0x2700},{8,48},{9,24},{1,host},{0x61000043,module},{0x61000045,(1ull<<48)|logical},{0,0}};
    Ph(b,2,2,6,0x2000,0x2000,sizeof(tags));
    for(unsigned i=0;i<std::size(tags);++i){Io::WriteU64(b,0x2000+i*16,tags[i].first);Io::WriteU64(b,0x2008+i*16,tags[i].second);}
    Io::WriteU32(b,0x2600,1);Io::WriteU32(b,0x2604,4);
    for(unsigned i=1;i<=3;++i){const auto o=0x2400+i*24;Io::WriteU32(b,o,i==1?a:i==2?c:imp);b[o+4]=0x12;b[o+5]=i==3?0:3;Io::WriteU16(b,o+6,i==3?0:1);Io::WriteU64(b,o+8,i==3?0:0x20);Io::WriteU64(b,o+16,i==3?0:1);}
    for(unsigned i=0;i<2;++i){Io::WriteU64(b,0x2700+i*24,0x2800+i*8);Io::WriteU64(b,0x2708+i*24,((i+1ull)<<32)|1);}
    return b;
}

int main() {
    const auto image = Relinker::GuestImageReader().Read("consumer.prx", Fixture(), true);
    if (image.Symbols.size() != 4 || image.Symbols[3].Library != "libmonosgen-2.0.prx") return 4;
    const std::map<std::uint64_t, std::string> modules{{1, "libmonosgen"}};
    const std::array<std::string, 1> matching{"libmonosgen-2.0.prx"};
    const std::array<std::string, 1> other{"libmonosgen-3.0.prx"};
    const std::array<std::string, 2> ambiguous{"libmonosgen-2.0.prx", "libmonosgen-2.0.sprx"};
    if (Domain::ImportModule("nid#A#B", modules, matching) != matching[0]) return 1;
    if (!Domain::ImportModule("nid#A#B", modules, other).empty()) return 2;
    try { Domain::ImportModule("nid#A#B", modules, ambiguous); return 3; }
    catch (const Domain::RelinkerException&) {}
    const auto sslImage = Relinker::GuestImageReader().Read("ssl-consumer.prx", Fixture(false, "libSceLibreSsl3.prx", "libSceLibreSSl3"), true);
    if (sslImage.Symbols.size() != 4 || sslImage.Symbols[3].Library != "libSceLibreSsl3.prx") return 9;
    const std::map<std::uint64_t, std::string> ssl3{{1, "libSceLibreSSl3"}};
    const std::map<std::uint64_t, std::string> ssl{{1, "libSceLibreSSL"}};
    const std::array<std::string, 1> ssl3Dependency{"libSceLibreSsl3.prx"};
    const std::array<std::string, 1> sslDependency{"libSceLibreSsl.prx"};
    const std::array<std::string, 2> sslAmbiguous{"libSceLibreSsl3.prx", "libSceLibreSsl3.sprx"};
    if (Domain::ImportModule("nid#A#B", ssl3, ssl3Dependency) != ssl3Dependency[0]) return 5;
    if (Domain::ImportModule("nid#A#B", ssl, sslDependency) != sslDependency[0]) return 6;
    if (!Domain::ImportModule("nid#A#B", ssl3, sslDependency).empty()) return 7;
    try { Domain::ImportModule("nid#A#B", ssl3, sslAmbiguous); return 8; }
    catch (const Domain::RelinkerException&) {}
    const std::map<std::uint64_t, std::string> json{{1, "libSceJson"}};
    const std::array<std::string, 1> json2Dependency{"libSceJson2.prx"};
    const std::array<std::string, 2> bothJson{"libSceJson.prx", "libSceJson2.prx"};
    const std::array<std::string, 1> jsonOther{"libSceJson3.prx"};
    const std::array<std::string, 2> jsonAmbiguous{"libSceJson2.prx", "libSceJson2.sprx"};
    if (Domain::ImportModule("nid#A#B", json, json2Dependency) != json2Dependency[0]) return 10;
    if (Domain::ImportModule("nid#A#B", json, bothJson) != bothJson[0]) return 11;
    if (!Domain::ImportModule("nid#A#B", json, jsonOther).empty()) return 12;
    try { Domain::ImportModule("nid#A#B", json, jsonAmbiguous); return 13; }
    catch (const Domain::RelinkerException&) {}
    const std::map<std::uint64_t, std::string> json2{{1, "libSceJson2"}};
    const std::array<std::string, 1> jsonLegacy{"libSceJson.prx"};
    if (!Domain::ImportModule("nid#A#B", json2, jsonLegacy).empty()) return 35;
    const auto jsonImage = Relinker::GuestImageReader().Read("json-consumer.prx", Fixture(false, "libSceJson2.prx", "libSceJson"), true);
    if (jsonImage.Symbols.size() != 4 || jsonImage.Symbols[3].Library != "libSceJson2.prx") return 14;
    const std::map<std::uint64_t, std::string> versioned{{1, "libmonosgen-2.0"}};
    const std::array<std::string, 1> legacy{"libmonosgen.prx"};
    const std::array<std::string, 2> both{"libmonosgen.prx", "libmonosgen-2.0.prx"};
    if (!Domain::ImportModule("nid#A#B", versioned, legacy).empty()) return 5;
    if (Domain::ImportModule("nid#A#B", modules, both) != both[0]) return 6;
    const std::map<std::uint64_t, std::string> ssl3Physical{{1, "libSceLibreSsl3"}};
    const std::array<std::string, 1> ssl3Logical{"libSceLibreSSl3.prx"};
    const std::map<std::uint64_t, std::string> sslPhysical{{1, "libSceLibreSsl"}};
    const std::array<std::string, 1> sslLogical{"libSceLibreSSL.prx"};
    if (!Domain::ImportModule("nid#A#B", ssl3Physical, ssl3Logical).empty()) return 40;
    if (!Domain::ImportModule("nid#A#B", sslPhysical, sslLogical).empty()) return 41;
    const std::array<std::string, 1> normalizedMono{"libmonosgen-module.prx"};
    if (Domain::ImportModule("nid#A#B", modules, normalizedMono) != normalizedMono[0]) return 43;
    std::cout << "module mapping checks passed\n";
}
