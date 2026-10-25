#include <relinker/output/CallRegistryWriter.hpp>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class GroupedPunctuation : public std::numpunct<char> {
public:
    explicit GroupedPunctuation(char separator) : separator(separator) {}

protected:
    char do_thousands_sep() const override { return separator; }
    std::string do_grouping() const override { return "\3"; }

private:
    char separator;
};

class ScopedLocale {
public:
    explicit ScopedLocale(const std::locale& locale) : previous(std::locale::global(locale)) {}
    ~ScopedLocale() { std::locale::global(previous); }

private:
    std::locale previous;
};

void RegistryOffsets() {
    Relinker::CallRegistryWriter writer;
    Relinker::CallRegistryEntry entry;
    entry.Nid = "testNid";
    entry.Library = "test.prx";
    entry.RelocationTypeString = "R_X86_64_JUMP_SLOT";
    entry.RelocationOffset = 0x12345678;
    entry.TargetSection = ".text";
    entry.TargetOffset = 0xabcdef0123456789ULL;
    entry.CallSites = {0, 0xfff, 0x1000, 0xffffffffffffffffULL};
    entry.CallSitesResolved = true;
    entry.Weak = true;
    std::string registry;
    std::string imports;
    std::string libraries;
    {
        const ScopedLocale classic(std::locale::classic());
        registry = writer.WriteCallRegistry({entry});
        imports = writer.WriteModuleImports({entry});
        libraries = writer.WriteNeededLibraries({entry.Library});
    }
    Require(registry.find("\"relocationOffset\": \"0x12345678\"") != std::string::npos, "relocation offset is not canonical hexadecimal");
    Require(registry.find("\"targetOffset\": \"0xABCDEF0123456789\"") != std::string::npos, "target offset is not canonical hexadecimal");
    Require(registry.find("\"callSites\": [\"0x0\", \"0xFFF\", \"0x1000\", \"0xFFFFFFFFFFFFFFFF\"]") != std::string::npos, "call sites are not canonical hexadecimal");
    Require(imports.find("\"targetOffset\": \"0xABCDEF0123456789\"") != std::string::npos, "module target offset is not canonical hexadecimal");
    for (const char separator : {',', '.'}) {
        const ScopedLocale grouped(std::locale(std::locale::classic(), new GroupedPunctuation(separator)));
        std::ostringstream control;
        control << std::hex << 0x12345678;
        Require(control.str().find(separator) != std::string::npos, "grouping locale is not active");
        Require(writer.WriteCallRegistry({entry}) == registry, "call registry depends on the host locale");
        Require(writer.WriteModuleImports({entry}) == imports, "module imports depend on the host locale");
        Require(writer.WriteNeededLibraries({entry.Library}) == libraries, "needed libraries depend on the host locale");
        Require(std::use_facet<std::numpunct<char>>(std::locale()).thousands_sep() == separator, "registry writer changed the global locale");
    }
}

}

int main() {
    try {
        RegistryOffsets();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
