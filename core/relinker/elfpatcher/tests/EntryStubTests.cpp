#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/windows/WindowsEntryStubBuilder.hpp>
#include <domain/Types.hpp>
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

}

int main() {
    try {
        stubTerminatesTheFrameChain();
        windowsStubLoadsTheAllocatorFirst("C:/Games/Title/libs", u"C:\\Games\\Title\\libs\\", {0x48, 0x8d, 0x0d});
        windowsStubLoadsTheAllocatorFirst("$ORIGIN/libs", u"libs\\", {0x48, 0x8d, 0x35});
        windowsStubRejectsAnOverlongAllocatorPath();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
