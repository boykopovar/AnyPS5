#include <elfpatcher/windows/WindowsDependencyStubBuilder.hpp>
#include <io/BufferUtils.hpp>

namespace Elfpatcher::Windows {

namespace {

enum Register : std::uint8_t { Ax, Cx, Dx, Bx, Sp, Bp, Si, Di, R8, R9, R10, R11, R12, R13, R14, R15 };
constexpr std::uint32_t PathCapacity = 32768;
constexpr std::uint32_t NodeSize = PathCapacity + 64;
constexpr std::uint32_t NodeLimit = 128;

class Assembler {
public:
    Assembler(WindowsStubEmitter& code, const WindowsImports& imports, const std::map<std::string, std::uint32_t>& storage) : code(code), imports(imports), storage(storage) {}

    void Mark(const std::string& label) {
        if (!labels.emplace(label, code.GetRva()).second)
            throw Domain::RelinkerException("Duplicate dependency stub label: " + label);
    }

    void Jump(const std::string& label, const std::uint8_t condition = 0) {
        const auto branch = condition == 0 ? code.Branch({0xe9}) : code.Branch({0x0f, condition});
        branches.emplace_back(branch, label);
    }

    void Call(const std::string& label) { branches.emplace_back(code.Branch({0xe8}), label); }
    void Api(const std::string& name) { code.Rip({0xff, 0x15}, imports.Functions.at(name)); }

    void Begin(const std::string& name) {
        Mark(name);
        starts.push_back(code.GetRva());
        code.Emit({0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x48});
    }

    void End() {
        code.Emit({0x48, 0x83, 0xc4, 0x48, 0x41, 0x5f, 0x41, 0x5e, 0x41, 0x5d, 0x41, 0x5c, 0x5f, 0x5e, 0x5d, 0x5b, 0xc3});
        ends.push_back(code.GetRva());
    }

    void Mov(const Register destination, const Register source) { binary(0x89, destination, source); }
    void Add(const Register destination, const Register source) { binary(0x01, destination, source); }
    void Sub(const Register destination, const Register source) { binary(0x29, destination, source); }
    void Compare(const Register left, const Register right) { binary(0x39, left, right); }
    void Test(const Register value) { binary(0x85, value, value); }

    void Value(const Register destination, const std::uint64_t value) {
        code.Emit({static_cast<std::uint8_t>(0x48 | (destination >> 3)), static_cast<std::uint8_t>(0xb8 | (destination & 7))});
        code.U64(value);
    }

    void AddValue(const Register destination, const std::uint32_t value) { immediate(destination, 0, value); }
    void SubValue(const Register destination, const std::uint32_t value) { immediate(destination, 5, value); }
    void CompareValue(const Register destination, const std::uint32_t value) { immediate(destination, 7, value); }
    void Mask(const Register destination, const std::uint32_t value) { immediate(destination, 4, value); }

    void Load(const Register destination, const Register base, const std::uint32_t offset = 0, const std::uint8_t size = 8) { memory(destination, base, offset, size == 1 ? 0xb6 : size == 2 ? 0xb7 : 0x8b, size); }
    void Store(const Register base, const std::uint32_t offset, const Register source) { memory(source, base, offset, 0x89, 8); }
    void StoreByte(const Register base, const Register source) { memory(source, base, 0, 0x88, 1, false); }
    void Address(const Register destination, const Register base, const std::uint32_t offset) { memory(destination, base, offset, 0x8d, 8); }

    void Data(const Register destination, const std::string& name) {
        code.Rip({static_cast<std::uint8_t>(0x48 | ((destination >> 3) << 2)), 0x8d, static_cast<std::uint8_t>(0x05 | ((destination & 7) << 3))}, storage.at(name));
    }

    void Global(const Register destination, const std::string& name) {
        code.Rip({static_cast<std::uint8_t>(0x48 | ((destination >> 3) << 2)), 0x8b, static_cast<std::uint8_t>(0x05 | ((destination & 7) << 3))}, storage.at(name));
    }

    void Save(const std::string& name, const Register source) {
        code.Rip({static_cast<std::uint8_t>(0x48 | ((source >> 3) << 2)), 0x89, static_cast<std::uint8_t>(0x05 | ((source & 7) << 3))}, storage.at(name));
    }

    void Text(const std::string& name) { Data(Cx, name); Call("write"); }

    void Finish() {
        for (const auto& [offset, label] : branches)
            code.PatchBranch(offset, labels.at(label));
    }

    WindowsDependencyStub Result(const std::uint32_t unwind) const {
        WindowsDependencyStub result{labels.at("diagnose"), {}};
        for (std::size_t index = 0; index < starts.size(); ++index)
            result.Functions.push_back({starts[index], ends.at(index), unwind});
        return result;
    }

private:
    void binary(const std::uint8_t opcode, const Register destination, const Register source) {
        code.Emit({static_cast<std::uint8_t>(0x48 | ((source >> 3) << 2) | (destination >> 3)), opcode, static_cast<std::uint8_t>(0xc0 | ((source & 7) << 3) | (destination & 7))});
    }

    void immediate(const Register destination, const std::uint8_t operation, const std::uint32_t value) {
        code.Emit({static_cast<std::uint8_t>(0x48 | (destination >> 3)), 0x81, static_cast<std::uint8_t>(0xc0 | (operation << 3) | (destination & 7))});
        code.U32(value);
    }

    void memory(const Register value, const Register base, const std::uint32_t offset, const std::uint8_t opcode, const std::uint8_t size, const bool extend = true) {
        code.Emit({static_cast<std::uint8_t>((size == 8 ? 0x48 : 0x40) | ((value >> 3) << 2) | (base >> 3))});
        if (size < 4 && extend)
            code.Emit({0x0f});
        code.Emit({opcode, static_cast<std::uint8_t>(0x80 | ((value & 7) << 3) | (base & 7))});
        if ((base & 7) == Sp)
            code.Emit({0x24});
        code.U32(offset);
    }

    WindowsStubEmitter& code;
    const WindowsImports& imports;
    const std::map<std::string, std::uint32_t>& storage;
    std::map<std::string, std::uint32_t> labels;
    std::vector<std::pair<std::size_t, std::string>> branches;
    std::vector<std::uint32_t> starts;
    std::vector<std::uint32_t> ends;
};

}

WindowsDependencyStubBuilder::WindowsDependencyStubBuilder(PeSection& data) {
    for (const auto& name : {"rootDirectory", "executableDirectory", "systemDirectory", "scratch", "forwarder"}) {
        storage.emplace(name, CheckedRva(data.Rva + data.Data.size()));
        data.Data.resize(data.Data.size() + PathCapacity);
    }
    for (const auto& name : {"nodes", "count", "root", "executable", "digits"}) {
        storage.emplace(name, CheckedRva(data.Rva + data.Data.size()));
        data.Data.resize(data.Data.size() + 32);
    }
    const std::map<std::string, std::string> messages = {
        {"invalid", "FAIL: invalid or unsupported PE dependency metadata\n"},
        {"limit", "FAIL: dependency diagnostics capacity exceeded\n"},
        {"io", "FAIL: dependency diagnostics Windows API failed\n"},
        {"missingModule", "FAIL: dependency module not found: "},
        {"moduleImporter", "\nImporter: "},
        {"importer", "FAIL: unresolved dependency import\nImporter: "},
        {"provider", "\nProvider: "},
        {"symbol", "\nSymbol: "},
        {"chain", "\nChain: "},
        {"arrow", " -> "},
        {"newline", "\n"},
        {"ordinal", "#"},
        {"extension", ".dll"},
        {"unresolved", "Dependency tables contain no missing imports; original load failure remains fatal.\n"}
    };
    for (const auto& [name, value] : messages) {
        storage.emplace(name, CheckedRva(data.Rva + data.Data.size()));
        Io::AppendString(data.Data, value);
    }
    Io::AlignBuffer(data.Data, 4);
    unwindRva = CheckedRva(data.Rva + data.Data.size());
    data.Data.insert(data.Data.end(), {1, 16, 9, 0, 16, 0x82, 12, 0xf0, 10, 0xe0, 8, 0xd0, 6, 0xc0, 4, 0x70, 3, 0x60, 2, 0x50, 1, 0x30, 0, 0});
}

WindowsDependencyStub WindowsDependencyStubBuilder::Build(WindowsStubEmitter& code, const WindowsImports& imports) const {
    Assembler a(code, imports, storage);

    a.Begin("write");
    a.Mov(Si, Cx);
    a.Api("lstrlenA");
    a.Mov(Bx, Ax);
    a.Value(Cx, 0xfffffff4u);
    a.Api("GetStdHandle");
    a.Mov(Cx, Ax);
    a.Mov(Dx, Si);
    a.Mov(R8, Bx);
    a.Address(R9, Sp, 40);
    a.Value(Ax, 0);
    a.Store(Sp, 32, Ax);
    a.Api("WriteFile");
    a.Test(Ax);
    a.Jump("terminate", 0x84);
    a.Load(Ax, Sp, 40, 4);
    a.Compare(Ax, Bx);
    a.Jump("terminate", 0x85);
    a.End();

    a.Begin("fatal");
    a.Call("write");
    a.Mark("terminate");
    a.Value(Cx, 0xc0000135u);
    a.Value(Dx, 1);
    a.Value(R8, 0);
    a.Value(R9, 0);
    a.Api("RaiseException");
    code.Emit({0x0f, 0x0b});
    a.Mark("invalid");
    a.Text("invalid");
    a.Jump("terminate");
    a.Mark("io");
    a.Text("io");
    a.Jump("terminate");
    a.Mark("limit");
    a.Text("limit");
    a.Jump("terminate");
    a.End();

    a.Begin("copy");
    a.Mov(Bx, Cx);
    a.Mov(Si, Dx);
    a.Mov(Cx, Dx);
    a.Api("lstrlenA");
    a.CompareValue(Ax, PathCapacity);
    a.Jump("limit", 0x83);
    a.Mov(Cx, Bx);
    a.Mov(Dx, Si);
    a.Api("lstrcpyA");
    a.Test(Ax);
    a.Jump("io", 0x84);
    a.End();

    a.Begin("directory");
    a.Mov(Bx, Cx);
    a.Call("copy");
    a.Mov(Cx, Bx);
    a.Api("lstrlenA");
    a.Mov(Si, Bx);
    a.Add(Si, Ax);
    a.Mark("directoryLoop");
    a.Compare(Si, Bx);
    a.Jump("invalid", 0x84);
    a.SubValue(Si, 1);
    a.Load(Ax, Si, 0, 1);
    a.CompareValue(Ax, '\\');
    a.Jump("directoryLoop", 0x85);
    a.AddValue(Si, 1);
    a.Value(Ax, 0);
    a.StoreByte(Si, Ax);
    a.End();

    a.Begin("join");
    a.Mov(Bx, Cx);
    a.Mov(Si, Dx);
    a.Mov(Di, R8);
    a.Mov(Cx, Dx);
    a.Api("lstrlenA");
    a.Mov(Bp, Ax);
    a.Mov(Cx, Di);
    a.Api("lstrlenA");
    a.Add(Ax, Bp);
    a.CompareValue(Ax, PathCapacity);
    a.Jump("limit", 0x83);
    a.Mov(Cx, Bx);
    a.Mov(Dx, Si);
    a.Call("copy");
    a.Mov(Cx, Bx);
    a.Mov(Dx, Di);
    a.Api("lstrcatA");
    a.Test(Ax);
    a.Jump("io", 0x84);
    a.End();

    a.Begin("bounds");
    a.Load(Ax, Cx, 16);
    a.Compare(Dx, Ax);
    a.Jump("invalid", 0x87);
    a.Sub(Ax, Dx);
    a.Compare(R8, Ax);
    a.Jump("invalid", 0x87);
    a.Load(Ax, Cx);
    a.Add(Ax, Dx);
    a.End();

    a.Begin("string");
    a.Mov(Bx, Cx);
    a.Value(R8, 1);
    a.Call("bounds");
    a.Load(Dx, Bx);
    a.Load(Cx, Bx, 16);
    a.Add(Dx, Cx);
    a.Mov(Cx, Ax);
    a.Mark("stringLoop");
    a.Compare(Cx, Dx);
    a.Jump("invalid", 0x83);
    a.Load(R8, Cx, 0, 1);
    a.Test(R8);
    a.Jump("stringDone", 0x84);
    a.AddValue(Cx, 1);
    a.Jump("stringLoop");
    a.Mark("stringDone");
    a.End();

    a.Begin("register");
    a.Mov(Si, Cx);
    a.Global(Bx, "nodes");
    a.Global(Di, "count");
    a.Mark("registerSearch");
    a.Test(Di);
    a.Jump("registerNew", 0x84);
    a.Address(Cx, Bx, 64);
    a.Mov(Dx, Si);
    a.Api("lstrcmpiA");
    a.Test(Ax);
    a.Jump("registerDone", 0x84);
    a.AddValue(Bx, NodeSize);
    a.SubValue(Di, 1);
    a.Jump("registerSearch");
    a.Mark("registerNew");
    a.Global(Ax, "count");
    a.CompareValue(Ax, NodeLimit);
    a.Jump("limit", 0x83);
    a.AddValue(Ax, 1);
    a.Save("count", Ax);
    a.Address(Cx, Bx, 64);
    a.Mov(Dx, Si);
    a.Call("copy");
    a.Address(Cx, Bx, 64);
    a.Api("GetModuleHandleA");
    a.Store(Bx, 8, Ax);
    a.Test(Ax);
    a.Jump("registerMapped", 0x85);
    a.Address(Cx, Bx, 64);
    a.Value(Dx, 0);
    a.Value(R8, 0x20);
    a.Api("LoadLibraryExA");
    a.Test(Ax);
    a.Jump("io", 0x84);
    a.Mask(Ax, 0xfffffffcu);
    a.Mark("registerMapped");
    a.Store(Bx, 0, Ax);
    a.Mov(Si, Ax);
    a.Load(Ax, Si, 0, 2);
    a.CompareValue(Ax, 0x5a4d);
    a.Jump("invalid", 0x85);
    a.Load(Di, Si, 0x3c, 4);
    a.CompareValue(Di, 0x1000);
    a.Jump("invalid", 0x83);
    a.Add(Di, Si);
    a.Load(Ax, Di, 0, 4);
    a.CompareValue(Ax, 0x4550);
    a.Jump("invalid", 0x85);
    a.Load(Ax, Di, 24, 2);
    a.CompareValue(Ax, 0x20b);
    a.Jump("invalid", 0x85);
    a.Load(Ax, Di, 80, 4);
    a.Store(Bx, 16, Ax);
    a.Load(Ax, Di, 144, 4);
    a.Store(Bx, 24, Ax);
    a.Load(Dx, Di, 148, 4);
    a.Add(Dx, Ax);
    a.Store(Bx, 32, Dx);
    a.Mark("registerDone");
    a.Mov(Ax, Bx);
    a.End();

    a.Begin("resolve");
    a.Mov(Bx, Cx);
    a.Mov(Si, Dx);
    a.Api("GetModuleHandleA");
    a.Test(Ax);
    a.Jump("resolveLoaded", 0x85);
    a.Mov(Cx, Bx);
    a.Api("lstrlenA");
    a.Test(Ax);
    a.Jump("invalid", 0x84);
    a.CompareValue(Ax, 4);
    a.Jump("resolveDirectories", 0x82);
    a.Load(Ax, Bx, 0, 4);
    a.CompareValue(Ax, 0x2d697061);
    a.Jump("resolveContract", 0x84);
    a.CompareValue(Ax, 0x2d747865);
    a.Jump("resolveContract", 0x84);
    a.Mark("resolveDirectories");
    for (const auto& directory : {"rootDirectory", "executableDirectory", "systemDirectory"}) {
        a.Data(Cx, "scratch");
        a.Data(Dx, directory);
        a.Mov(R8, Bx);
        a.Call("join");
        a.Data(Cx, "scratch");
        a.Api("GetFileAttributesA");
        a.Value(Dx, 0xffffffffu);
        a.Compare(Ax, Dx);
        a.Jump(std::string("resolveNext") + directory, 0x84);
        a.Mask(Ax, 0x10);
        a.Test(Ax);
        a.Jump("resolvePath", 0x84);
        a.Mark(std::string("resolveNext") + directory);
    }
    a.Text("missingModule");
    a.Mov(Cx, Bx);
    a.Call("write");
    a.Text("moduleImporter");
    a.Address(Cx, Si, 64);
    a.Call("write");
    a.Text("chain");
    a.Mov(Cx, Si);
    a.Call("chain");
    a.Text("arrow");
    a.Mov(Cx, Bx);
    a.Call("write");
    a.Text("newline");
    a.Jump("terminate");
    a.Mark("resolveContract");
    a.Mov(Cx, Bx);
    a.Value(Dx, 0);
    a.Value(R8, 0x800);
    a.Api("LoadLibraryExA");
    a.Test(Ax);
    a.Jump("io", 0x84);
    a.Mark("resolveLoaded");
    a.Mov(Cx, Ax);
    a.Data(Dx, "scratch");
    a.Value(R8, PathCapacity);
    a.Api("GetModuleFileNameA");
    a.Test(Ax);
    a.Jump("io", 0x84);
    a.CompareValue(Ax, PathCapacity);
    a.Jump("limit", 0x83);
    a.Mark("resolvePath");
    a.Data(Cx, "scratch");
    a.Call("register");
    a.End();

    a.Begin("parent");
    a.Global(Ax, "root");
    a.Compare(Cx, Ax);
    a.Jump("parentDone", 0x84);
    a.Load(Ax, Cx, 40);
    a.Test(Ax);
    a.Jump("parentDone", 0x85);
    a.Store(Cx, 40, Dx);
    a.Mark("parentDone");
    a.End();

    a.Begin("chain");
    a.Mov(Bx, Cx);
    a.Load(Cx, Bx, 40);
    a.Test(Cx);
    a.Jump("chainRoot", 0x84);
    a.Call("chain");
    a.Jump("chainNode");
    a.Mark("chainRoot");
    a.Global(Cx, "executable");
    a.Call("write");
    a.Mark("chainNode");
    a.Text("arrow");
    a.Address(Cx, Bx, 64);
    a.Call("write");
    a.End();

    a.Begin("report");
    a.Mov(Bx, Cx);
    a.Mov(Si, Dx);
    a.Mov(Di, R8);
    a.Text("importer");
    a.Address(Cx, Bx, 64);
    a.Call("write");
    a.Text("provider");
    a.Address(Cx, Si, 64);
    a.Call("write");
    a.Text("symbol");
    a.CompareValue(Di, 0xffff);
    a.Jump("reportName", 0x87);
    a.Text("ordinal");
    a.Mov(Ax, Di);
    a.Data(Di, "digits");
    a.AddValue(Di, 31);
    a.Value(Dx, 0);
    a.StoreByte(Di, Dx);
    a.Value(R8, 10);
    a.Mark("reportDigit");
    code.Emit({0x31, 0xd2, 0x49, 0xf7, 0xf0});
    a.AddValue(Dx, '0');
    a.SubValue(Di, 1);
    a.StoreByte(Di, Dx);
    a.Test(Ax);
    a.Jump("reportDigit", 0x85);
    a.Mark("reportName");
    a.Mov(Cx, Di);
    a.Call("write");
    a.Text("chain");
    a.Mov(Cx, Bx);
    a.Call("chain");
    a.Text("arrow");
    a.Address(Cx, Si, 64);
    a.Call("write");
    a.Text("newline");
    a.Jump("terminate");
    a.End();

    a.Begin("export");
    a.Mov(Bx, Cx);
    a.Mov(Si, Dx);
    a.Mov(Di, R9);
    a.Store(Sp, 56, R8);
    a.Test(R8);
    a.Jump("limit", 0x84);
    a.Load(Cx, Bx, 8);
    a.Test(Cx);
    a.Jump("exportStatic", 0x84);
    a.Mov(Dx, Si);
    a.Api("GetProcAddress");
    a.Test(Ax);
    a.Jump("exportMissing", 0x84);
    a.Jump("exportDone");
    a.Mark("exportStatic");
    a.Load(Ax, Bx);
    a.Load(Dx, Ax, 0x3c, 4);
    a.Add(Ax, Dx);
    a.Load(R12, Ax, 136, 4);
    a.Load(R13, Ax, 140, 4);
    a.Test(R12);
    a.Jump("exportMissing", 0x84);
    a.Mov(Cx, Bx);
    a.Mov(Dx, R12);
    a.Value(R8, 40);
    a.Call("bounds");
    a.Mov(Bp, Ax);
    a.CompareValue(Si, 0xffff);
    a.Jump("exportNames", 0x87);
    a.Load(Ax, Bp, 16, 4);
    a.Compare(Si, Ax);
    a.Jump("exportMissing", 0x82);
    a.Mov(R14, Si);
    a.Sub(R14, Ax);
    a.Jump("exportAddress");
    a.Mark("exportNames");
    a.Value(R14, 0);
    a.Load(R15, Bp, 24, 4);
    a.Mark("exportNameLoop");
    a.Compare(R14, R15);
    a.Jump("exportMissing", 0x83);
    a.Load(Dx, Bp, 32, 4);
    a.Mov(Ax, R14);
    a.Add(Ax, Ax);
    a.Add(Ax, Ax);
    a.Add(Dx, Ax);
    a.Mov(Cx, Bx);
    a.Value(R8, 4);
    a.Call("bounds");
    a.Load(Dx, Ax, 0, 4);
    a.Mov(Cx, Bx);
    a.Call("string");
    a.Mov(Cx, Ax);
    a.Mov(Dx, Si);
    a.Api("lstrcmpA");
    a.Test(Ax);
    a.Jump("exportNameFound", 0x84);
    a.AddValue(R14, 1);
    a.Jump("exportNameLoop");
    a.Mark("exportNameFound");
    a.Load(Dx, Bp, 36, 4);
    a.Add(Dx, R14);
    a.Add(Dx, R14);
    a.Mov(Cx, Bx);
    a.Value(R8, 2);
    a.Call("bounds");
    a.Load(R14, Ax, 0, 2);
    a.Mark("exportAddress");
    a.Load(Ax, Bp, 20, 4);
    a.Compare(R14, Ax);
    a.Jump("exportMissing", 0x83);
    a.Load(Dx, Bp, 28, 4);
    a.Add(R14, R14);
    a.Add(R14, R14);
    a.Add(Dx, R14);
    a.Mov(Cx, Bx);
    a.Value(R8, 4);
    a.Call("bounds");
    a.Load(R14, Ax, 0, 4);
    a.Test(R14);
    a.Jump("exportMissing", 0x84);
    a.Compare(R14, R12);
    a.Jump("exportDone", 0x82);
    a.Mov(Ax, R14);
    a.Sub(Ax, R12);
    a.Compare(Ax, R13);
    a.Jump("exportDone", 0x83);
    a.Mov(Cx, Bx);
    a.Mov(Dx, R14);
    a.Call("string");
    a.Mov(R14, Ax);
    a.Data(Cx, "forwarder");
    a.Mov(Dx, Ax);
    a.Call("copy");
    a.Mov(Cx, R14);
    a.Api("lstrlenA");
    a.Mov(R15, Ax);
    a.Mark("forwardSeparator");
    a.Test(R15);
    a.Jump("invalid", 0x84);
    a.SubValue(R15, 1);
    a.Mov(Ax, R14);
    a.Add(Ax, R15);
    a.Load(Dx, Ax, 0, 1);
    a.CompareValue(Dx, '.');
    a.Jump("forwardSeparator", 0x85);
    a.AddValue(Ax, 1);
    a.Mov(R14, Ax);
    a.Data(Ax, "forwarder");
    a.Add(Ax, R15);
    a.Value(Dx, 0);
    a.StoreByte(Ax, Dx);
    a.CompareValue(R15, PathCapacity - 5);
    a.Jump("limit", 0x83);
    a.Data(Cx, "forwarder");
    a.Data(Dx, "extension");
    a.Api("lstrcatA");
    a.Data(Cx, "forwarder");
    a.Mov(Dx, Bx);
    a.Call("resolve");
    a.Mov(R15, Ax);
    a.Mov(Cx, Ax);
    a.Mov(Dx, Bx);
    a.Call("parent");
    a.Load(Ax, R14, 0, 1);
    a.CompareValue(Ax, '#');
    a.Jump("forwardCall", 0x85);
    a.AddValue(R14, 1);
    a.Value(R12, 0);
    a.Mark("forwardOrdinal");
    a.Load(Ax, R14, 0, 1);
    a.Test(Ax);
    a.Jump("forwardOrdinalDone", 0x84);
    a.SubValue(Ax, '0');
    a.CompareValue(Ax, 9);
    a.Jump("invalid", 0x87);
    a.Mov(Dx, R12);
    a.Add(R12, R12);
    a.Add(Dx, Dx);
    a.Add(Dx, Dx);
    a.Add(Dx, Dx);
    a.Add(R12, Dx);
    a.Add(R12, Ax);
    a.CompareValue(R12, 0xffff);
    a.Jump("invalid", 0x87);
    a.AddValue(R14, 1);
    a.Jump("forwardOrdinal");
    a.Mark("forwardOrdinalDone");
    a.Test(R12);
    a.Jump("invalid", 0x84);
    a.Mov(R14, R12);
    a.Mark("forwardCall");
    a.Mov(Cx, R15);
    a.Mov(Dx, R14);
    a.Load(R8, Sp, 56);
    a.SubValue(R8, 1);
    a.Mov(R9, Bx);
    a.Call("export");
    a.Jump("exportDone");
    a.Mark("exportMissing");
    a.Mov(Cx, Di);
    a.Mov(Dx, Bx);
    a.Mov(R8, Si);
    a.Call("report");
    a.Mark("exportDone");
    a.End();

    a.Begin("walk");
    a.Mov(Bx, Cx);
    a.Load(Ax, Bx, 48);
    a.Test(Ax);
    a.Jump("walkDone", 0x85);
    a.Value(Ax, 1);
    a.Store(Bx, 48, Ax);
    a.Load(Ax, Bx, 8);
    a.Test(Ax);
    a.Jump("walkDone", 0x85);
    a.Load(Bp, Bx, 24);
    a.Test(Bp);
    a.Jump("walkDone", 0x84);
    a.Mark("descriptor");
    a.Mov(Ax, Bp);
    a.AddValue(Ax, 20);
    a.Load(Dx, Bx, 32);
    a.Compare(Ax, Dx);
    a.Jump("invalid", 0x87);
    a.Mov(Cx, Bx);
    a.Mov(Dx, Bp);
    a.Value(R8, 20);
    a.Call("bounds");
    a.Load(Si, Ax, 0, 4);
    a.Load(Dx, Ax, 12, 4);
    a.Test(Dx);
    a.Jump("walkDone", 0x84);
    a.Test(Si);
    a.Jump("invalid", 0x84);
    a.Mov(Cx, Bx);
    a.Call("string");
    a.Mov(Cx, Ax);
    a.Mov(Dx, Bx);
    a.Call("resolve");
    a.Mov(Di, Ax);
    a.Mov(Cx, Di);
    a.Mov(Dx, Bx);
    a.Call("parent");
    a.Mark("thunk");
    a.Mov(Cx, Bx);
    a.Mov(Dx, Si);
    a.Value(R8, 8);
    a.Call("bounds");
    a.Load(R12, Ax);
    a.Test(R12);
    a.Jump("walkProvider", 0x84);
    a.Jump("thunkOrdinal", 0x88);
    a.Mov(Dx, R12);
    a.AddValue(Dx, 2);
    a.Mov(Cx, Bx);
    a.Call("string");
    a.Mov(R12, Ax);
    a.Jump("thunkCheck");
    a.Mark("thunkOrdinal");
    a.Mask(R12, 0xffff);
    a.Mark("thunkCheck");
    a.Mov(Cx, Di);
    a.Mov(Dx, R12);
    a.Value(R8, 64);
    a.Mov(R9, Bx);
    a.Call("export");
    a.AddValue(Si, 8);
    a.Jump("thunk");
    a.Mark("walkProvider");
    a.Mov(Cx, Di);
    a.Call("walk");
    a.AddValue(Bp, 20);
    a.Jump("descriptor");
    a.Mark("walkDone");
    a.End();

    a.Begin("diagnose");
    a.Mov(Bx, Cx);
    a.Mov(Si, Dx);
    a.Save("executable", Dx);
    a.Data(Cx, "rootDirectory");
    a.Mov(Dx, Bx);
    a.Call("directory");
    a.Data(Cx, "executableDirectory");
    a.Mov(Dx, Si);
    a.Call("directory");
    a.Data(Cx, "systemDirectory");
    a.Value(Dx, PathCapacity - 1);
    a.Api("GetSystemDirectoryA");
    a.Test(Ax);
    a.Jump("io", 0x84);
    a.CompareValue(Ax, PathCapacity - 1);
    a.Jump("limit", 0x83);
    a.Data(Dx, "systemDirectory");
    a.Add(Dx, Ax);
    a.Value(Ax, '\\');
    a.StoreByte(Dx, Ax);
    a.AddValue(Dx, 1);
    a.Value(Ax, 0);
    a.StoreByte(Dx, Ax);
    a.Value(Cx, 0);
    a.Value(Dx, NodeSize * NodeLimit);
    a.Value(R8, 0x3000);
    a.Value(R9, 4);
    a.Api("VirtualAlloc");
    a.Test(Ax);
    a.Jump("io", 0x84);
    a.Save("nodes", Ax);
    a.Mov(Cx, Bx);
    a.Call("register");
    a.Save("root", Ax);
    a.Mov(Cx, Ax);
    a.Call("walk");
    a.Text("unresolved");
    a.Jump("terminate");
    a.End();

    a.Finish();
    return a.Result(unwindRva);
}

}
