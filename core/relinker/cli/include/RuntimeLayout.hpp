#pragma once

#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace Cli {

class RuntimeLayout {
public:
    static void Print(std::ostream& stream, const std::string& executable, const std::vector<std::string>& modules, const std::vector<std::string>& extraExecutables = {}) {
        Node root;
        for (const auto& module : modules) {
            auto* node = &root;
            std::size_t start = 0;
            for (;;) {
                const auto end = module.find('/', start);
                const auto name = module.substr(start, end == std::string::npos ? end : end - start);
                node = &node->children[name];
                if (end == std::string::npos) break;
                start = end + 1;
            }
        }
        stream << "Arrange the files like this:\n"
               << "folder/\n"
               << "  |-- " << executable << "\n";
        for (const auto& extra : extraExecutables) stream << "  |-- " << extra << '\n';
        stream << "  |-- libs/\n"
               << "        |-- *.prx (system libraries)\n"
               << "  |-- app0/\n"
               << "        |-- <another app files>\n";
        printChildren(stream, root, "        ");
        stream << '\n';
    }

private:
    struct Node {
        std::map<std::string, Node> children;
    };

    static void printChildren(std::ostream& stream, const Node& node, const std::string& prefix) {
        for (const auto& [name, child] : node.children) {
            stream << prefix << "|-- " << name << (child.children.empty() ? "\n" : "/\n");
            printChildren(stream, child, prefix + "      ");
        }
    }
};

}
