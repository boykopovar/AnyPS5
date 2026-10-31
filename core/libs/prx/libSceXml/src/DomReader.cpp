#include "prx/libSceXml/src/DomReader.hpp"

#include <cstring>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace XmlDom {

namespace {

constexpr std::uint64_t MeasuredTail = 2;
constexpr std::uint64_t ZeroedTextCapacity = 5240;
constexpr std::uint64_t ZeroedNameCapacity = 2024;

std::uint64_t FieldLength(std::uint8_t lead) {
    constexpr std::uint8_t Lengths[] = {3, 4, 1, 1, 4};
    return lead >= 0xf0 && lead <= 0xf4 ? Lengths[lead - 0xf0] : 1;
}

}

void Reader::CheckNode(std::uint64_t node) const {
    if (node > buffers.nodes.size() + MeasuredTail) throw std::logic_error("libSceXml: a node offset past the document stream was not measured");
}

std::uint8_t Reader::At(std::uint64_t index) const {
    return index < buffers.nodes.size() ? buffers.nodes[index] : 0;
}

std::uint64_t Reader::Index(std::uint64_t at) const {
    const std::uint8_t lead = At(at);
    if (lead <= 0xef) return lead;
    if (lead == 0xf0) return (static_cast<std::uint64_t>(At(at + 1)) << 8) | At(at + 2);
    if (lead == 0xf1 || lead == 0xf4) return (static_cast<std::uint64_t>(At(at + 1)) << 16) | (static_cast<std::uint64_t>(At(at + 2)) << 8) | At(at + 3);
    if (lead == 0xff) return 0;
    return 1;
}

std::uint64_t Reader::PrologEnd() const {
    std::uint64_t p = 2;
    while ((~At(p) & 7) == 0) p = p + 1 + FieldLength(At(p + 1));
    return p;
}

const char* Reader::String(const std::vector<std::uint8_t>& table, std::uint64_t position, std::uint64_t zeroed) const {
    if (position >= table.size()) {
        if (table.size() < zeroed && position < zeroed) return "";
        throw std::logic_error("libSceXml: a string past the end of its table was not measured");
    }
    const void* end = std::memchr(table.data() + position, 0, table.size() - position);
    if (end == nullptr) throw std::logic_error("libSceXml: a string past the end of its table was not measured");
    return reinterpret_cast<const char*>(table.data() + position);
}

const char* Reader::TextEntry(std::uint64_t index) const {
    const std::uint8_t lead = index < buffers.text.size() ? buffers.text[index] : 0;
    return String(buffers.text, index + FieldLength(lead), ZeroedTextCapacity);
}

std::uint64_t Reader::Root() const {
    if (buffers.nodes.empty()) return 0;
    return FirstChild(1);
}

std::uint64_t Reader::FirstChild(std::uint64_t node) const {
    CheckNode(node);
    if (node == 0) return 0;
    const std::uint8_t b = At(node);
    if ((b & 0x10) == 0) return 0;
    const int type = b & 0x0f;
    if (type >= 4 && type <= 6) return node + 7 + At(node + 7);
    if (b != 0x10) return 0;
    return PrologEnd();
}

std::uint64_t Reader::Sibling(std::uint64_t node) const {
    CheckNode(node);
    if (node == 0) return 0;
    const std::uint8_t b = At(node);
    if ((b & 0x20) == 0) return 0;
    const int kind = b & 0x2f;
    if (kind == 0x22 || kind == 0x27 || kind == 0x28 || kind == 0x2f) return node + 2 + FieldLength(At(node + 1));
    if (kind == 0x24 || kind == 0x25) {
        const std::uint64_t length = (static_cast<std::uint64_t>(At(node + 3)) << 24) | (static_cast<std::uint64_t>(At(node + 4)) << 16) |
                (static_cast<std::uint64_t>(At(node + 5)) << 8) | At(node + 6);
        return node + 3 + length;
    }
    return 0;
}

std::uint64_t Reader::LastChild(std::uint64_t node) const {
    std::uint64_t child = FirstChild(node);
    std::set<std::uint64_t> seen;
    while (child != 0) {
        if (!seen.insert(child).second) throw std::logic_error("libSceXml: a sibling cycle was not measured");
        const std::uint64_t next = Sibling(child);
        if (next == 0) return child;
        child = next;
    }
    return 0;
}

std::uint64_t Reader::Parent(std::uint64_t node, int& status) const {
    CheckNode(node);
    status = 0;
    if (node == 0) {
        status = StatusInvalidNode;
        return 0;
    }
    if (buffers.nodes.empty()) return 0;
    const std::uint64_t root = PrologEnd();
    if (root == node) return 0;
    std::set<std::uint64_t> seen;
    const std::uint64_t found = Search(root, node, seen);
    if (found != 0) return found;
    status = StatusNodeNotFound;
    return 0;
}

std::uint64_t Reader::Search(std::uint64_t parent, std::uint64_t node, std::set<std::uint64_t>& seen) const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> frames{{parent, FirstChild(parent)}};
    while (!frames.empty()) {
        const auto [owner, child] = frames.back();
        if (child == 0) {
            frames.pop_back();
            if (!frames.empty()) frames.back().second = Sibling(frames.back().second);
            continue;
        }
        if (!seen.insert(child).second) throw std::logic_error("libSceXml: a node cycle was not measured");
        if (child == node) return owner;
        frames.emplace_back(child, FirstChild(child));
    }
    return 0;
}

std::uint64_t Reader::FirstAttribute(std::uint64_t node) const {
    CheckNode(node);
    if (node == 0) return 0;
    const std::uint8_t b = At(node);
    switch (b) {
    case 0x44: case 0x45: case 0x54: case 0x55: case 0x64: case 0x65: case 0x74: case 0x75:
        break;
    default:
        return 0;
    }
    const std::uint64_t block = node + 7;
    if ((b & 0x10) == 0) return block;
    return block + FieldLength(At(block));
}

std::uint64_t Reader::NextAttribute(std::uint64_t attribute) const {
    CheckNode(attribute);
    if (attribute == 0) return 0;
    const std::uint8_t b = At(attribute);
    if ((b & 0x0f) != 1) return 0;
    std::uint64_t d = (b & 0x20) != 0 ? 2 : 1;
    d += FieldLength(At(attribute + d));
    d += FieldLength(At(attribute + d));
    return (At(attribute + d) & 0x0f) == 1 ? attribute + d : 0;
}

std::uint32_t Reader::NodeType(std::uint64_t node) const {
    CheckNode(node);
    if (node == 0) return 0xffffffff;
    const std::uint8_t b = At(node);
    const int type = b & 0x0f;
    if (type == 7 || type == 0x0f) return (b & 0x10) | type;
    return type;
}

std::uint32_t Reader::EntityType(std::uint64_t node) const {
    CheckNode(node);
    if (node == 0) return 0x8085020e;
    const std::uint8_t b = At(node);
    if ((b & 0x10) != 0) return 5;
    if ((b & 0x40) != 0) return 6;
    return At(node + 1);
}

bool Reader::HasChildNodes(std::uint64_t node) const {
    return FirstChild(node) != 0;
}

bool Reader::HasAttributes(std::uint64_t node) const {
    return FirstAttribute(node) != 0;
}

const char* Reader::NodeName(std::uint64_t node) const {
    CheckNode(node);
    if (node == 0) return nullptr;
    const std::uint8_t b = At(node);
    const int type = b & 0x0f;
    if (type == 4 || type == 5) return String(buffers.names, (static_cast<std::uint64_t>(At(node + 1)) << 8) | At(node + 2), ZeroedNameCapacity);
    if (type == 7) return TextEntry(Index(node + 1));
    if (type == 8) return (b & 0xdf) == 8 ? TextEntry(Index(node + 1)) : nullptr;
    if (type == 0x0f) return String(buffers.text, 1 + Index(node + 1), ZeroedTextCapacity);
    return nullptr;
}

const char* Reader::Text(std::uint64_t node) const {
    CheckNode(node);
    if (node == 0 || (At(node) & 0xdf) != 8) return nullptr;
    return TextEntry(Index(node + 1));
}

const char* Reader::SkippedText(std::uint64_t node) const {
    CheckNode(node);
    if (node == 0 || (~At(node) & 7) != 0) return nullptr;
    return TextEntry(Index(node + 1));
}

const char* Reader::AttributeName(std::uint64_t attribute) const {
    CheckNode(attribute);
    if (attribute == 0) return nullptr;
    const std::uint8_t b = At(attribute);
    if ((b & 0x0f) != 1) return nullptr;
    return String(buffers.attributeNames, Index(attribute + ((b & 0x20) != 0 ? 2 : 1)), ZeroedNameCapacity);
}

const char* Reader::AttributeValue(std::uint64_t attribute) const {
    CheckNode(attribute);
    if (attribute == 0) return nullptr;
    const std::uint8_t b = At(attribute);
    if ((b & 0x0f) != 1) return nullptr;
    std::uint64_t at = attribute + ((b & 0x20) != 0 ? 2 : 1);
    at += FieldLength(At(at));
    return TextEntry(Index(at));
}

const char* Reader::Entity(std::uint64_t node) const {
    CheckNode(node);
    if (node == 0) return nullptr;
    const std::uint8_t inverted = static_cast<std::uint8_t>(~At(node));
    if ((inverted & 0x12) != 0 && (inverted & 0x42) != 0) return nullptr;
    return String(buffers.text, 1 + Index(node + 1), ZeroedTextCapacity);
}

const char* Reader::Attribute(std::uint64_t node, const char* name) const {
    std::set<std::uint64_t> seen;
    for (std::uint64_t attribute = FirstAttribute(node); attribute != 0; attribute = NextAttribute(attribute)) {
        if (!seen.insert(attribute).second) throw std::logic_error("libSceXml: an attribute cycle was not measured");
        const char* attributeName = AttributeName(attribute);
        if (attributeName == nullptr) throw std::logic_error("libSceXml: getAttribute on a record that is not an attribute was not measured");
        if (std::strcmp(attributeName, name) == 0) return AttributeValue(attribute);
    }
    return nullptr;
}

}
