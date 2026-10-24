#include <relinker/output/CallRegistryWriter.hpp>
#include <cstdint>
#include <sstream>

namespace Relinker {

static std::string _jsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else if (c < 0x20) {
                constexpr char digits[] = "0123456789abcdef";
                out += "\\u00";
                out.push_back(digits[c >> 4]);
                out.push_back(digits[c & 0x0f]);
            } else out.push_back(static_cast<char>(c));
            ++i;
            continue;
        }
        unsigned count;
        std::uint32_t codepoint;
        std::uint32_t minimum;
        if (c >= 0xC2 && c <= 0xDF) {
            count = 1;
            codepoint = c & 0x1F;
            minimum = 0x80;
        } else if (c >= 0xE0 && c <= 0xEF) {
            count = 2;
            codepoint = c & 0x0F;
            minimum = 0x800;
        } else if (c >= 0xF0 && c <= 0xF4) {
            count = 3;
            codepoint = c & 0x07;
            minimum = 0x10000;
        } else {
            throw RelinkerException("Invalid UTF-8 lead byte in call registry string");
        }
        if (count > s.size() - (i + 1)) {
            throw RelinkerException("Truncated UTF-8 sequence in call registry string");
        }
        for (unsigned index = 0; index < count; ++index) {
            const auto byte = static_cast<unsigned char>(s[i + 1 + index]);
            if ((byte & 0xC0) != 0x80) {
                throw RelinkerException("Invalid UTF-8 continuation byte in call registry string");
            }
            codepoint = (codepoint << 6) | (byte & 0x3F);
        }
        if (codepoint < minimum || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            throw RelinkerException("Invalid UTF-8 codepoint in call registry string");
        }
        out.push_back(static_cast<char>(c));
        out.append(s, i + 1, count);
        i += count + 1;
    }
    out.push_back('"');
    return out;
}

static std::string _hexOffset(FileByteOffset v) {
    std::ostringstream oss;
    oss << "\"0x" << std::hex << std::uppercase << v << "\"";
    return oss.str();
}

std::string CallRegistryWriter::WriteCallRegistry(const std::vector<CallRegistryEntry>& entries) {
    std::ostringstream out;
    out << "[\n";

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        out << "  {\n";
        out << "    \"nid\": " << _jsonString(e.Nid) << ",\n";
        out << "    \"library\": " << _jsonString(e.Library) << ",\n";
        out << "    \"relocationType\": " << _jsonString(e.RelocationTypeString) << ",\n";
        out << "    \"relocationOffset\": " << _hexOffset(e.RelocationOffset) << ",\n";
        out << "    \"targetSection\": " << _jsonString(e.TargetSection) << ",\n";
        out << "    \"targetOffset\": " << _hexOffset(e.TargetOffset) << ",\n";
        out << "    \"callSites\": [";

        for (std::size_t j = 0; j < e.CallSites.size(); ++j) {
            if (j > 0) out << ", ";
            out << _hexOffset(e.CallSites[j]);
        }

        out << "],\n";
        out << "    \"callSitesResolved\": " << (e.CallSitesResolved ? "true" : "false") << ",\n";
        out << "    \"weak\": " << (e.Weak ? "true" : "false") << "\n";
        out << "  }";
        if (i + 1 < entries.size()) out << ",";
        out << "\n";
    }

    out << "]\n";
    return out.str();
}

std::string CallRegistryWriter::WriteModuleImports(const std::vector<CallRegistryEntry>& entries) {
    std::ostringstream out;
    out << "[\n";

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        out << "  {\n";
        out << "    \"nid\": " << _jsonString(e.Nid) << ",\n";
        out << "    \"library\": " << _jsonString(e.Library) << ",\n";
        out << "    \"targetOffset\": " << _hexOffset(e.TargetOffset);
        if (e.Weak) out << ",\n    \"weak\": true";
        out << "\n";
        out << "  }";
        if (i + 1 < entries.size()) out << ",";
        out << "\n";
    }

    out << "]\n";
    return out.str();
}

std::string CallRegistryWriter::WriteNeededLibraries(const std::vector<std::string>& libraries) {
    std::ostringstream out;
    out << "[\n";
    for (std::size_t index = 0; index < libraries.size(); ++index) {
        out << "  " << _jsonString(libraries[index]);
        if (index + 1 < libraries.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

}
