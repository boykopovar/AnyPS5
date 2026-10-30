#include "prx/libSceAddressSanitizer/src/StackTrace.hpp"
#include "prx/libSceAddressSanitizer/src/ErrorReport.hpp"
#include "prx/libSceAddressSanitizer/src/ModuleLookup.hpp"
#include "prx/libSceAddressSanitizer/src/ReportFile.hpp"
#include <cstdio>
#include <cstring>

namespace AsanRuntime::StackTrace {

namespace {

constexpr char kDefaultFormat[] = "    #%n %p %F %L";

std::string Hex(std::uintptr_t value) {
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

std::string ModuleLocation(const std::string& module, std::uintptr_t offset) {
    return "(" + module + "+" + Hex(offset) + ")";
}

}  // namespace

std::string RenderFrame_nid_no_patch(const char* format, unsigned frameNumber, std::uintptr_t address) {
    if (std::strcmp(format, "DEFAULT") == 0) {
        format = kDefaultFormat;
    }
    std::string module;
    std::uintptr_t offset = 0;
    const bool hasModule = ModuleForAddress_nid_no_patch(address, module, offset);
    std::string out;
    for (const char* p = format; *p != '\0'; ++p) {
        if (*p != '%') {
            out += *p;
            continue;
        }
        ++p;
        switch (*p) {
        case '%':
            out += '%';
            break;
        case 'n':
            out += std::to_string(frameNumber);
            break;
        case 'p':
            out += Hex(address);
            break;
        case 'm':
            out += hasModule ? module : "<null>";
            break;
        case 'o':
            out += Hex(hasModule ? offset : 0);
            break;
        case 'b':
        case 'F':
            break;
        case 'f':
        case 's':
        case 'S':
            out += "<null>";
            break;
        case 'q':
            out += "0x0";
            break;
        case 'l':
        case 'c':
            out += "0";
            break;
        case 'L':
            out += hasModule ? ModuleLocation(module, offset) : "(<unknown module>)";
            break;
        case 'M':
            out += hasModule ? ModuleLocation(StripModuleName_nid_no_patch(module.c_str()), offset) : "(" + Hex(address) + ")";
            break;
        default: {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "Unsupported specifier in stack frame format: %c (%p)!\n", *p, static_cast<const void*>(p));
            ReportFile::Write_nid_no_patch(buffer);
            Die_nid_no_patch();
        }
        }
    }
    return out;
}

std::string Print_nid_no_patch(const std::uintptr_t* returnAddresses, std::size_t count) {
    if (count == 0) {
        return "    <empty stack>\n\n";
    }
    std::string out;
    for (std::size_t i = 0; i < count && returnAddresses[i]; ++i) {
        const std::string frame = RenderFrame_nid_no_patch("DEFAULT", static_cast<unsigned>(i), returnAddresses[i] - 1);
        if (!frame.empty()) {
            out += frame + "\n";
        }
    }
    out += "\n";
    return out;
}

void Symbolize_nid_no_patch(std::uintptr_t pc, const char* format, char* output, std::size_t outputSize) {
    if (outputSize == 0) {
        return;
    }
    const std::string text = RenderFrame_nid_no_patch(format, 0, pc - 1);
    const std::size_t length = text.size() < outputSize - 1 ? text.size() : outputSize - 1;
    std::memcpy(output, text.data(), length);
    output[length] = '\0';
}

}  // namespace AsanRuntime::StackTrace
