#include <relinker/ICallRegistryWriter.hpp>
#include <memory>
#include <sstream>

namespace Relinker {

static std::string _jsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static std::string _hexOffset(FileByteOffset v) {
    std::ostringstream oss;
    oss << "\"0x" << std::hex << std::uppercase << v << "\"";
    return oss.str();
}

class CallRegistryWriter : public ICallRegistryWriter {
public:
    std::string WriteCallRegistry(const std::vector<CallRegistryEntry>& Entries) override;
};

std::string CallRegistryWriter::WriteCallRegistry(const std::vector<CallRegistryEntry>& Entries) {
    std::ostringstream out;
    out << "[\n";

    for (std::size_t i = 0; i < Entries.size(); ++i) {
        const auto& e = Entries[i];

        out << "  {\n";
        out << "    \"nid\": " << _jsonString(e.Nid) << ",\n";
        out << "    \"library\": " << _jsonString(e.Library) << ",\n";
        out << "    \"relocationType\": " << _jsonString(e.RelocationTypeString) << ",\n";
        out << "    \"relocationOffset\": " << _hexOffset(e.RelocationOffset)<< ",\n";
        out << "    \"targetSection\": " << _jsonString(e.TargetSection) << ",\n";
        out << "    \"targetOffset\": " << _hexOffset(e.TargetOffset) << ",\n";
        out << "    \"callSites\": [";

        for (std::size_t j = 0; j < e.CallSites.size(); ++j) {
            if (j > 0) out << ", ";
            out << _hexOffset(e.CallSites[j]);
        }

        out << "],\n";
        out << "    \"callSitesResolved\": " << (e.CallSitesResolved ? "true" : "false") << "\n";
        out << "  }";

        if (i + 1 < Entries.size()) out << ",";
        out << "\n";
    }

    out << "]\n";
    return out.str();
}

std::shared_ptr<ICallRegistryWriter> MakeCallRegistryWriter() {
    return std::make_shared<CallRegistryWriter>();
}

}
