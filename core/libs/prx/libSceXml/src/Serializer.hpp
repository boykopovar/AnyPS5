#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace XmlDom {

constexpr char SerializeDeclaration[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";

template <typename TTree>
class Serializer {
public:
    explicit Serializer(const TTree& tree) : tree(tree) {}

    std::string Run(bool declaration) {
        out.clear();
        startTagOpen = false;
        if (declaration) out += SerializeDeclaration;
        Siblings(tree.Root());
        if (out.empty()) throw std::logic_error("sce::Xml::Dom::Document::serialize: with nothing written the console returns its new output buffer unterminated, so the result is whatever that memory held");
        return out;
    }

private:
    static constexpr std::uint32_t EntityReference = 2;
    static constexpr std::uint32_t Element = 4;
    static constexpr std::uint32_t EmptyElement = 5;
    static constexpr std::uint32_t Text = 8;
    static constexpr std::uint32_t Comment = 31;
    static constexpr std::uint32_t TextAlternate = 40;
    static constexpr std::array<const char*, 5> Entities{"&", "\"", "<", ">", "'"};

    void CloseStartTag() {
        if (!startTagOpen) return;
        out += '>';
        startTagOpen = false;
    }

    static const char* Printed(const char* text) {
        return text != nullptr ? text : "(null)";
    }

    using Node = std::decay_t<decltype(std::declval<const TTree&>().Root())>;

    void Siblings(Node node) {
        const char* closeName = nullptr;
        while (node != Node{}) {
            switch (tree.NodeType(node)) {
            case Element:
            case EmptyElement: {
                closeName = tree.NodeName(node);
                if (closeName == nullptr) throw std::logic_error("sce::Xml::Dom::Document::serialize: the console measures a null element name with strlen and faults");
                CloseStartTag();
                out += '<';
                out += closeName;
                for (Node attribute = tree.FirstAttribute(node); attribute != Node{}; attribute = tree.NextAttribute(attribute)) {
                    out += ' ';
                    out += Printed(tree.AttributeName(attribute));
                    out += "=\"";
                    out += Printed(tree.AttributeValue(attribute));
                    out += '"';
                }
                startTagOpen = true;
                break;
            }
            case EntityReference: {
                const std::uint32_t entity = tree.EntityType(node);
                out += entity < Entities.size() ? Entities[entity] : Printed(tree.Entity(node));
                break;
            }
            case Text:
            case TextAlternate:
                if (const char* text = tree.Text(node)) {
                    CloseStartTag();
                    out += text;
                }
                break;
            case Comment:
                if (const char* text = tree.SkippedText(node)) {
                    CloseStartTag();
                    out += "<!--";
                    out += text;
                }
                break;
            default:
                if (const char* text = tree.SkippedText(node)) {
                    CloseStartTag();
                    out += text;
                }
                break;
            }
            Siblings(tree.FirstChild(node));
            const Node next = tree.Sibling(node);
            if (closeName != nullptr) {
                if (startTagOpen) {
                    out += "/>";
                    startTagOpen = false;
                } else {
                    out += "</";
                    out += closeName;
                    out += '>';
                    closeName = nullptr;
                }
            }
            node = next;
        }
    }

    const TTree& tree;
    std::string out;
    bool startTagOpen = false;
};

}
