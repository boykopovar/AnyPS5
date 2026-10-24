#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool bytesAt(const std::vector<std::uint8_t>& stub, std::size_t& offset, const std::vector<std::uint8_t>& expected) {
    if (offset + expected.size() > stub.size() || std::memcmp(stub.data() + offset, expected.data(), expected.size()) != 0) return false;
    offset += expected.size();
    return true;
}

void stubTerminatesTheFrameChain() {
    constexpr std::uint64_t stubVaddr = 0xb32db98;
    constexpr std::uint64_t entryVaddr = 0x80;
    const auto stub = Elfpatcher::EntryStubBuilder().BuildEntryStub(stubVaddr, entryVaddr);
    std::size_t offset = 0;
    require(bytesAt(stub, offset, {0x48, 0x89, 0xe7}), "the stub does not pass the initial stack pointer in rdi");
    require(bytesAt(stub, offset, {0x48, 0x83, 0xe4, 0xf0}), "the stub does not align the stack to 16 bytes");
    require(bytesAt(stub, offset, {0x6a, 0x00, 0x6a, 0x00}), "the stub does not push a zero frame record");
    require(bytesAt(stub, offset, {0x48, 0x89, 0xe5}), "the stub does not point rbp at the zero frame record");
    require(bytesAt(stub, offset, {0x48, 0x31, 0xf6}), "the stub does not clear rsi");
    require(offset < stub.size() && stub[offset] == 0xe8, "the stub does not call the entry point");
    std::int32_t rel32 = 0;
    std::memcpy(&rel32, stub.data() + offset + 1, sizeof(rel32));
    offset += 5;
    require(stubVaddr + offset + static_cast<std::int64_t>(rel32) == entryVaddr, "the stub calls the wrong address");
    require(bytesAt(stub, offset, {0x0f, 0x0b}) && offset == stub.size(), "the stub does not end with ud2 after the call");
}

std::uint32_t wideStringRva(const Elfpatcher::Windows::PeSection& section, const std::u16string& text) {
    std::vector<std::uint8_t> needle;
    for (const auto unit : text) {
        needle.push_back(static_cast<std::uint8_t>(unit));
        needle.push_back(static_cast<std::uint8_t>(unit >> 8));
    }
    needle.insert(needle.end(), {0, 0});
    for (std::size_t offset = 0; offset + needle.size() <= section.Data.size(); offset += 2) {
        if (std::memcmp(section.Data.data() + offset, needle.data(), needle.size()) == 0) return section.Rva + static_cast<std::uint32_t>(offset);
    }
    throw std::runtime_error("the stub data does not hold the expected path");
}

std::size_t ripReference(const Elfpatcher::Windows::PeSection& code, const std::vector<std::uint8_t>& opcode, const std::uint32_t target, const std::size_t from = 0) {
    for (std::size_t offset = from; offset + opcode.size() + 4 <= code.Data.size(); ++offset) {
        if (std::memcmp(code.Data.data() + offset, opcode.data(), opcode.size()) != 0) continue;
        std::int32_t displacement = 0;
        std::memcpy(&displacement, code.Data.data() + offset + opcode.size(), sizeof(displacement));
        if (code.Rva + offset + opcode.size() + 4 + static_cast<std::int64_t>(displacement) == target) return offset;
    }
    return std::string::npos;
}

Elfpatcher::Windows::WindowsEntryStub windowsStub(const Elfpatcher::Windows::WindowsImports& nativeImports, const std::string& runPath, const std::vector<std::string>& libraries) {
    std::vector<Elfpatcher::Windows::PeImport> imports;
    for (std::size_t index = 0; index < libraries.size(); ++index) {
        imports.push_back({.Name = "import" + std::to_string(index), .TargetRva = static_cast<std::uint32_t>(0x2000 + index * 8), .Addend = 0, .Library = libraries[index]});
    }
    return Elfpatcher::Windows::WindowsEntryStubBuilder().Build(0x40000, 0x1000, nativeImports, libraries, imports, runPath, false, false);
}

void windowsStubLoadsTheAllocatorFirst(const std::string& runPath, const std::u16string& directory, const std::vector<std::uint8_t>& pathOpcode) {
    const auto nativeImports = Elfpatcher::Windows::WindowsImportBuilder().Build(0x1000);
    const auto stub = windowsStub(nativeImports, runPath, {"libc.prx", "libkernel.prx"});
    const auto allocator = ripReference(stub.Code, pathOpcode, wideStringRva(stub.Data, directory + u"mimalloc.dll"));
    const auto library = ripReference(stub.Code, pathOpcode, wideStringRva(stub.Data, directory + u"libc.prx"));
    require(allocator != std::string::npos, runPath + ": the stub does not use mimalloc.dll's path");
    require(library != std::string::npos, runPath + ": the stub does not use the first library's path");
    const auto load = ripReference(stub.Code, {0xff, 0x15}, nativeImports.Functions.at("LoadLibraryExW"), allocator);
    require(allocator < library && load < library, runPath + ": the stub loads a library before mimalloc.dll");
    std::size_t flags = load - 8;
    require(bytesAt(stub.Code.Data, flags, {0x31, 0xd2, 0x41, 0xb8, 0x00, 0x11, 0x00, 0x00}), runPath + ": mimalloc.dll is not loaded from the stub's search directories");
}

void windowsStubLoadsTheAllocatorOnRequest(const std::string& runPath, const std::u16string& directory, const std::vector<std::uint8_t>& pathOpcode) {
    const auto nativeImports = Elfpatcher::Windows::WindowsImportBuilder().Build(0x1000);
    const auto stub = windowsStub(nativeImports, runPath, {"libc.prx"});
    const auto variable = ripReference(stub.Code, {0x48, 0x8d, 0x0d}, wideStringRva(stub.Data, u"APS5_MIMALLOC"));
    const auto allocator = ripReference(stub.Code, pathOpcode, wideStringRva(stub.Data, directory + u"mimalloc.dll"));
    require(variable != std::string::npos && allocator != std::string::npos && variable < allocator, runPath + ": the stub does not read APS5_MIMALLOC before loading mimalloc.dll");
    std::size_t cursor = variable + 7;
    require(bytesAt(stub.Code.Data, cursor, {0x31, 0xd2, 0x45, 0x31, 0xc0}) && ripReference(stub.Code, {0xff, 0x15}, nativeImports.Functions.at("GetEnvironmentVariableW"), cursor) == cursor, runPath + ": APS5_MIMALLOC is not read with GetEnvironmentVariableW");
    cursor += 6;
    require(bytesAt(stub.Code.Data, cursor, {0x85, 0xc0, 0x0f, 0x84}), runPath + ": an unset APS5_MIMALLOC does not branch");
    std::int32_t displacement = 0;
    std::memcpy(&displacement, stub.Code.Data.data() + cursor, sizeof(displacement));
    const auto target = static_cast<std::int64_t>(cursor) + 4 + displacement;
    const auto load = ripReference(stub.Code, {0xff, 0x15}, nativeImports.Functions.at("LoadLibraryExW"), allocator);
    require(load != std::string::npos && target == static_cast<std::int64_t>(load) + 6, runPath + ": an unset APS5_MIMALLOC does not skip exactly the load of mimalloc.dll");
}

void windowsStubRejectsAnOverlongAllocatorPath() {
    const auto nativeImports = Elfpatcher::Windows::WindowsImportBuilder().Build(0x1000);
    const auto runPath = [](const std::size_t length) { return "C:\\" + std::string(length - 4, 'a') + "\\"; };
    constexpr std::size_t PathUnits = 32768;
    const auto longest = PathUnits - 1 - std::string_view("mimalloc.dll").size() - 1;
    static_cast<void>(windowsStub(nativeImports, runPath(longest), {"libc.prx"}));
    try {
        static_cast<void>(windowsStub(nativeImports, runPath(longest + 1), {"libc.prx"}));
    } catch (const Domain::RelinkerException& error) {
        const std::string message = error.what();
        require(message.starts_with("Windows library path exceeds the startup buffer: ") && message.ends_with("\\mimalloc.dll"), "an overlong allocator path fails with another error: " + message);
        return;
    }
    throw std::runtime_error("an allocator path past the startup buffer was accepted");
}

void windowsLibSceAmprObjectImportsFallBackToTheLibcProvider() {
    using namespace Elfpatcher::Windows;
    const auto nativeImports = WindowsImportBuilder().Build(0x1000);
    const std::vector<std::string> libraries{"libSceAmpr.prx", "libc.prx"};
    const std::vector<PeImport> amprImport{{.Name = "-mLzBSk-VGs", .TargetRva = 0x2000, .Addend = 0, .Library = "libSceAmpr.prx"}};
    const auto amprStub = WindowsEntryStubBuilder().Build(0x40000, 0x1000, nativeImports, libraries, amprImport, "$ORIGIN/libs", false, false);
    const std::vector<std::uint8_t> fallbackOrder{0, 0, 0, 0, 1, 0, 0, 0};
    require(std::search(amprStub.Data.Data.begin(), amprStub.Data.Data.end(), fallbackOrder.begin(), fallbackOrder.end()) != amprStub.Data.Data.end(),
            "a libSceAmpr-qualified locale object import does not fall back to libc");

    const std::vector<PeImport> libcImport{{.Name = "-mLzBSk-VGs", .TargetRva = 0x2000, .Addend = 0, .Library = "libc.prx"}};
    const auto libcStub = WindowsEntryStubBuilder().Build(0x40000, 0x1000, nativeImports, libraries, libcImport, "$ORIGIN/libs", false, false);
    const std::vector<std::uint8_t> libcOrder{1, 0, 0, 0};
    require(std::search(libcStub.Data.Data.begin(), libcStub.Data.Data.end(), libcOrder.begin(), libcOrder.end()) != libcStub.Data.Data.end(),
            "a libc-qualified locale object import does not resolve through libc");
}

void windowsKernelVariantImportsSearchCanonicalKernelAfterTheRequestedVariant() {
    using namespace Elfpatcher::Windows;
    const auto nativeImports = WindowsImportBuilder().Build(0x1000);
    const std::vector<std::string> libraries{"libkernel_sys.prx", "libc.prx", "libkernel.prx"};
    const std::vector<PeImport> imports{{.Name = "getpagesize", .TargetRva = 0x2000, .Addend = 0, .Library = "libkernel_sys.prx"}};
    const auto stub = WindowsEntryStubBuilder().Build(0x40000, 0x1000, nativeImports, libraries, imports, "$ORIGIN/libs", false, false);
    const std::vector<std::uint8_t> expected{0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0};
    require(std::search(stub.Data.Data.begin(), stub.Data.Data.end(), expected.begin(), expected.end()) != stub.Data.Data.end(),
            "a scoped libkernel_sys import does not search the requested variant before canonical libkernel and libc");
}

void windowsKernelVariantDependenciesLoadCanonicalKernel() {
    using namespace Elfpatcher::Windows;
    for (const auto& variant : {std::string("libkernel_sys.prx"), std::string("libkernel_web.prx")}) {
        Domain::SysVDynamicSection dynamic;
        dynamic.DynStrData.insert(dynamic.DynStrData.end(), variant.begin(), variant.end());
        dynamic.DynStrData.push_back(0);
        dynamic.DynamicSegmentData.resize(16);
        Io::WriteU64(dynamic.DynamicSegmentData, 0, 1);
        Io::WriteU64(dynamic.DynamicSegmentData, 8, 0);
        require(WindowsImportBuilder().ReadLibraries(dynamic) == std::vector<std::string>{variant, "libkernel.prx", "libc.prx"},
                variant + ": canonical kernel fallback and its libc provider are not loaded after the requested module");
    }
}

}

int main() {
    try {
        stubTerminatesTheFrameChain();
        windowsStubLoadsTheAllocatorFirst("C:/Games/Title/libs", u"C:\\Games\\Title\\libs\\", {0x48, 0x8d, 0x0d});
        windowsStubLoadsTheAllocatorFirst("$ORIGIN/libs", u"libs\\", {0x48, 0x8d, 0x35});
        windowsStubLoadsTheAllocatorOnRequest("C:/Games/Title/libs", u"C:\\Games\\Title\\libs\\", {0x48, 0x8d, 0x0d});
        windowsStubLoadsTheAllocatorOnRequest("$ORIGIN/libs", u"libs\\", {0x48, 0x8d, 0x35});
        windowsStubRejectsAnOverlongAllocatorPath();
        windowsLibSceAmprObjectImportsFallBackToTheLibcProvider();
        windowsKernelVariantImportsSearchCanonicalKernelAfterTheRequestedVariant();
        windowsKernelVariantDependenciesLoadCanonicalKernel();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
