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

void RequireUtf8Failure(const std::string& input, const std::string& expectedMessage) {
    Relinker::CallRegistryWriter writer;
    try {
        writer.WriteNeededLibraries({input});
    } catch (const Relinker::RelinkerException& error) {
        Require(std::string(error.what()).find(expectedMessage) != std::string::npos,
                "Expected message '" + expectedMessage + "', got: " + error.what());
        return;
    }
    throw std::runtime_error("Expected failure for input: " + input);
}

void Utf8ValidationAndJsonEscaping() {
    Relinker::CallRegistryWriter writer;

    for (const std::string& invalid : {
        std::string("\x80", 1),
        std::string("\xBF", 1),
        std::string("\xC0", 1),
        std::string("\xC1", 1),
        std::string("\xF5", 1),
        std::string("\xFF", 1),
        std::string("prefix\x80postfix", 14),
        std::string("lib\xC0.prx", 8)
    }) {
        RequireUtf8Failure(invalid, "Invalid UTF-8 lead byte in call registry string");
    }

    for (const std::string& truncated : {
        std::string("\xC2", 1),
        std::string("\xDF", 1),
        std::string("\xE0", 1),
        std::string("\xE2", 1),
        std::string("\xE2\x82", 2),
        std::string("\xF0", 1),
        std::string("\xF0\x9F", 2),
        std::string("\xF0\x9F\x98", 3),
        std::string("hello\xC2", 6),
        std::string("test\xE2\x82", 6)
    }) {
        RequireUtf8Failure(truncated, "Truncated UTF-8 sequence in call registry string");
    }

    for (const std::string& invalidContinuation : {
        std::string("\xC2\x00", 2),
        std::string("\xC2\x7F", 2),
        std::string("\xC2\xC0", 2),
        std::string("\xC2\x20", 2),
        std::string("\xE2\x00\x80", 3),
        std::string("\xE2\x82\x00", 3),
        std::string("\xE2\x82\x7F", 3),
        std::string("\xE2\x82\xC0", 3),
        std::string("\xF0\x00\x80\x80", 4),
        std::string("\xF0\x9F\x00\x80", 4),
        std::string("\xF0\x9F\x98\x00", 4),
        std::string("\xF0\x9F\x98\x7F", 4)
    }) {
        RequireUtf8Failure(invalidContinuation, "Invalid UTF-8 continuation byte in call registry string");
    }

    for (const std::string& invalidCodepoint : {
        std::string("\xE0\x80\xAF", 3),
        std::string("\xE0\x9F\xBF", 3),
        std::string("\xF0\x80\x80\x80", 4),
        std::string("\xF0\x8F\xBF\xBF", 4),
        std::string("\xED\xA0\x80", 3),
        std::string("\xED\xAD\xBF", 3),
        std::string("\xED\xAE\x80", 3),
        std::string("\xED\xBF\xBF", 3),
        std::string("\xF4\x90\x80\x80", 4)
    }) {
        RequireUtf8Failure(invalidCodepoint, "Invalid UTF-8 codepoint in call registry string");
    }

    Relinker::CallRegistryEntry invalidEntry{};
    invalidEntry.Nid = std::string("\xFF", 1);
    invalidEntry.CallSitesResolved = false;
    try {
        writer.WriteCallRegistry({invalidEntry});
        throw std::runtime_error("Expected failure in WriteCallRegistry for invalid UTF-8");
    } catch (const Relinker::RelinkerException& error) {
        Require(std::string(error.what()).find("Invalid UTF-8 lead byte in call registry string") != std::string::npos,
                "Expected lead byte error in WriteCallRegistry");
    }

    try {
        writer.WriteModuleImports({invalidEntry});
        throw std::runtime_error("Expected failure in WriteModuleImports for invalid UTF-8");
    } catch (const Relinker::RelinkerException& error) {
        Require(std::string(error.what()).find("Invalid UTF-8 lead byte in call registry string") != std::string::npos,
                "Expected lead byte error in WriteModuleImports");
    }

    const std::string specialChars = "\"\\\n\r\t";
    const std::string specialJson = writer.WriteNeededLibraries({specialChars});
    Require(specialJson.find("\"\\\"\\\\\\n\\r\\t\"") != std::string::npos, "special characters are not escaped properly in JSON");

    const std::string controlChars = std::string("\x00\x01\x07\x08\x0B\x0C\x1B\x1F", 8);
    const std::string controlJson = writer.WriteNeededLibraries({controlChars});
    Require(controlJson.find("\"\\u0000\\u0001\\u0007\\u0008\\u000b\\u000c\\u001b\\u001f\"") != std::string::npos,
            "control characters < 0x20 are not escaped to \\u00XX in JSON");

    const std::string mixedEscapes = std::string("lib \"test\\v1\"\n\r\t\x07.prx", 21);
    const std::string mixedJson = writer.WriteNeededLibraries({mixedEscapes});
    Require(mixedJson.find("\"lib \\\"test\\\\v1\\\"\\n\\r\\t\\u0007.prx\"") != std::string::npos,
            "mixed escaping failed in JSON");

    const std::string valid2Byte = "\xC2\xA2";
    const std::string valid3Byte = "\xE2\x82\xAC";
    const std::string valid4Byte = "\xF0\x9F\x98\x80";
    const std::string validCombined = "lib_\xC2\xA2_\xE2\x82\xAC_\xF0\x9F\x98\x80.prx";

    const std::string validJson = writer.WriteNeededLibraries({valid2Byte, valid3Byte, valid4Byte, validCombined});
    Require(validJson.find("  \"" + valid2Byte + "\"") != std::string::npos, "valid 2-byte UTF-8 sequence was not preserved");
    Require(validJson.find("  \"" + valid3Byte + "\"") != std::string::npos, "valid 3-byte UTF-8 sequence was not preserved");
    Require(validJson.find("  \"" + valid4Byte + "\"") != std::string::npos, "valid 4-byte UTF-8 sequence was not preserved");
    Require(validJson.find("  \"" + validCombined + "\"") != std::string::npos, "combined valid UTF-8 sequences were not preserved");

    Relinker::CallRegistryEntry validEntry{};
    validEntry.Nid = validCombined;
    validEntry.CallSitesResolved = true;
    validEntry.Library = valid2Byte;
    validEntry.RelocationTypeString = valid3Byte;
    validEntry.TargetSection = valid4Byte;
    const std::string validRegistry = writer.WriteCallRegistry({validEntry});
    Require(validRegistry.find("\"nid\": \"" + validCombined + "\"") != std::string::npos, "NID UTF-8 not preserved in call registry");
    Require(validRegistry.find("\"library\": \"" + valid2Byte + "\"") != std::string::npos, "Library UTF-8 not preserved in call registry");
    Require(validRegistry.find("\"relocationType\": \"" + valid3Byte + "\"") != std::string::npos, "RelocationType UTF-8 not preserved in call registry");
    Require(validRegistry.find("\"targetSection\": \"" + valid4Byte + "\"") != std::string::npos, "TargetSection UTF-8 not preserved in call registry");
}

}

int main() {
    try {
        RegistryOffsets();
        Utf8ValidationAndJsonEscaping();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
