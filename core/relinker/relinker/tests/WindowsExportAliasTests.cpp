#include <relinker/guest/GuestImage.hpp>
#include <elfpatcher/general/GuestModuleWriter.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <io/BufferUtils.hpp>
#include <domain/ImportModule.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

using Bytes = std::vector<std::uint8_t>;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void Ph(Bytes& b, int i, unsigned type, unsigned flags, unsigned off, unsigned va, unsigned size) {
    const auto p=64+i*56; Io::WriteU32(b,p,type); Io::WriteU32(b,p+4,flags);
    Io::WriteU64(b,p+8,off);Io::WriteU64(b,p+16,va);Io::WriteU64(b,p+24,va);
    Io::WriteU64(b,p+32,size);Io::WriteU64(b,p+40,size);Io::WriteU64(b,p+48,8);
}
static Bytes Fixture(bool reverse=false) {
    Bytes b(0x4000);std::memcpy(b.data(),"\x7f" "ELF\x02\x01\x01",7);
    Io::WriteU16(b,16,3);Io::WriteU16(b,18,62);Io::WriteU32(b,20,1);
    Io::WriteU64(b,32,64);Io::WriteU16(b,52,64);Io::WriteU16(b,54,56);Io::WriteU16(b,56,3);
    Ph(b,0,1,5,0x1000,0,0x1000);Ph(b,1,1,6,0x2000,0x2000,0x1000);
    b[0x1020]=0xc3;b[0x1028]=0xc3;
    std::string strings(1,'\0');auto str=[&](const char* s){auto o=strings.size();strings+=s;strings+='\0';return o;};
    const auto a=str(reverse?"alias#B#A":"alias#A#A"),c=str(reverse?"alias#A#A":"alias#B#A");
    const auto imp=str("imported#A#B"),host=str("fixtureHost.prx"),module=str("fixtureModule");
    std::copy(strings.begin(),strings.end(),b.begin()+0x2200);
    const std::pair<std::uint64_t,std::uint64_t> tags[]={{5,0x2200},{10,strings.size()},{6,0x2400},{11,24},{4,0x2600},{7,0x2700},{8,48},{9,24},{1,host},{0x61000043,module},{0x61000045,(1ull<<48)|host},{0,0}};
    Ph(b,2,2,6,0x2000,0x2000,sizeof(tags));
    for(unsigned i=0;i<std::size(tags);++i){Io::WriteU64(b,0x2000+i*16,tags[i].first);Io::WriteU64(b,0x2008+i*16,tags[i].second);}
    Io::WriteU32(b,0x2600,1);Io::WriteU32(b,0x2604,4);
    for(unsigned i=1;i<=3;++i){const auto o=0x2400+i*24;Io::WriteU32(b,o,i==1?a:i==2?c:imp);b[o+4]=0x12;b[o+5]=i==3?0:3;Io::WriteU16(b,o+6,i==3?0:1);Io::WriteU64(b,o+8,i==3?0:0x20);Io::WriteU64(b,o+16,i==3?0:1);}
    for(unsigned i=0;i<2;++i){Io::WriteU64(b,0x2700+i*24,0x2800+i*8);Io::WriteU64(b,0x2708+i*24,((i+1ull)<<32)|1);}
    return b;
}
static std::size_t Offset(const Bytes& pe,std::uint32_t rva) {
    const auto nt=Io::ReadU32(pe,0x3c);const auto count=Io::ReadU16(pe,nt+6);const auto sections=nt+24+Io::ReadU16(pe,nt+20);
    for(unsigned i=0;i<count;++i){auto p=sections+i*40;auto start=Io::ReadU32(pe,p+12),size=Io::ReadU32(pe,p+16);if(rva>=start&&rva-start<size)return Io::ReadU32(pe,p+20)+rva-start;}
    throw std::runtime_error("unmapped PE fixture RVA");
}
template<class F> static void Reject(F&& f,const char* expected) {
    bool rejected=false;try{f();}catch(const Domain::RelinkerException& e){rejected=std::string(e.what()).find(expected)!=std::string::npos;}
    Check(rejected,expected);
}
static void Positive(bool reverse) {
    const auto bytes=Fixture(reverse);const auto g=Relinker::GuestImageReader().Read("generic.prx",bytes,true);
    Check(g.Symbols.size()==4&&g.Bytes==bytes,"input symbols/bytes changed");
    Check(g.Symbols[1].Name=="alias"&&g.Symbols[2].Name=="alias","stripped lookup names");
    Check(g.Symbols[1].QualifiedName==(reverse?"alias#B#A":"alias#A#A")&&g.Symbols[2].QualifiedName==(reverse?"alias#A#A":"alias#B#A"),"qualified names lost");
    Check(g.Symbols[1].Info==18&&g.Symbols[2].Info==18&&g.Symbols[1].Visibility==3&&g.Symbols[2].Visibility==3&&g.Symbols[1].Section==1&&g.Symbols[2].Section==1&&g.Symbols[1].Value==0x20&&g.Symbols[2].Value==0x20&&g.Symbols[1].Size==1&&g.Symbols[2].Size==1,"raw tuple changed");
    Check((Io::ReadU64(g.Dynamic.RelaData,8)>>32)==1&&(Io::ReadU64(g.Dynamic.RelaData,32)>>32)==2,"alias relocation indices changed");
    Check(g.Symbols[3].QualifiedName=="imported#A#B"&&g.Symbols[3].Library=="fixtureHost.prx","qualified import changed");
    Domain::GuestRuntime runtime;const auto pe=Elfpatcher::GuestModuleWriter().WriteWindows(g,runtime);
    const auto directory=Offset(pe,Io::ReadU32(pe,Io::ReadU32(pe,0x3c)+24+112));
    Check(Io::ReadU32(pe,directory+20)==1&&Io::ReadU32(pe,directory+24)==1,"multiple PE exports");
    const auto function=Io::ReadU32(pe,Offset(pe,Io::ReadU32(pe,directory+28)));
    const auto nameRva=Io::ReadU32(pe,Offset(pe,Io::ReadU32(pe,directory+32)));
    Check(std::strcmp(reinterpret_cast<const char*>(pe.data()+Offset(pe,nameRva)),"alias")==0,"wrong export name");
    Elfpatcher::Windows::WindowsLoadImage image(g.Bytes,g.Headers,false);
    Check(function==image.GetRva(0x20,1),"wrong export RVA");
    for(auto target:{0x2800,0x2808})Check(Io::ReadU64(pe,Offset(pe,image.GetRva(target,8)))==Elfpatcher::Windows::ImageBase+function,"wrong alias relocation value");
}
int main() {
    try {
        Positive(false);Positive(true);
        Reject([]{Relinker::GuestImageReader().Read("generic.prx",Fixture());},"Duplicate or empty export");
        const std::array<std::pair<unsigned,std::uint64_t>,7> changes={{{4,0x22},{4,0x11},{5,0},{6,2},{8,0x28},{16,2},{6,Relinker::AbsoluteSection}}};
        for(const auto& [field,value]:changes){auto b=Fixture();auto off=0x2400+48+field;if(field<6)b[off]=value;else if(field==6)Io::WriteU16(b,off,value);else Io::WriteU64(b,off,value);Reject([&]{Relinker::GuestImageReader().Read("generic.prx",b,true);},"Duplicate or empty export");}
        auto good=Relinker::GuestImageReader().Read("generic.prx",Fixture(),true);
        auto symbolic=good;symbolic.Symbolic=true;Domain::GuestRuntime symbolicRuntime;Reject([&]{Elfpatcher::GuestModuleWriter().WriteWindows(symbolic,symbolicRuntime);},"DF_SYMBOLIC");
        for(unsigned kind=0;kind<10;++kind){auto g=good;auto& s=g.Symbols[2];switch(kind){case 0:s.Value+=8;break;case 1:s.Size+=1;break;case 2:s.Info=0x22;break;case 3:s.Info=0x11;break;case 4:s.Visibility=0;break;case 5:s.Section=2;break;case 6:g.Symbols[1].Info=s.Info=0x11;break;case 7:g.Symbols[1].Section=s.Section=Relinker::AbsoluteSection;break;case 8:g.Symbols[1].Info=s.Info=0x16;g.Dynamic.RelaData.clear();break;case 9:g.Symbols[1].Info=s.Info=0x10;break;}Check(!Relinker::IdenticalOrdinaryFunctionExport(g.Symbols[1],s),"nonordinary/conflicting tuple accepted");Domain::GuestRuntime r;Reject([&]{Elfpatcher::GuestModuleWriter().WriteWindows(g,r);},kind==8?"TLS":"Duplicate guest export");}
        Check(Domain::DeclaredImportModule("same#A#B",{{1,"one"},{2,"two"}})=="one"&&Domain::DeclaredImportModule("same#A#C",{{1,"one"},{2,"two"}})=="two","module-qualified IDs merged");
        std::cout<<"WINDOWS_EXPORT_ALIAS_CASES_PASSED\npreserved-indices reversed-order conflicting-fields strict-nonwindows qualified-import writer-guard\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
