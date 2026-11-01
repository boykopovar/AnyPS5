#pragma once

#include "prx/libSceXml/src/DomParser.hpp"

#include <cstdint>
#include <set>

namespace XmlDom {

constexpr int StatusInvalidNode = static_cast<int>(0x80850003);
constexpr int StatusNodeNotFound = static_cast<int>(0x80850201);

class Reader {
public:
    explicit Reader(const Buffers& buffers) : buffers(buffers) {}

    std::uint64_t Root() const;
    std::uint64_t FirstChild(std::uint64_t node) const;
    std::uint64_t Sibling(std::uint64_t node) const;
    std::uint64_t LastChild(std::uint64_t node) const;
    std::uint64_t Parent(std::uint64_t node, int& status) const;
    std::uint64_t FirstAttribute(std::uint64_t node) const;
    std::uint64_t NextAttribute(std::uint64_t attribute) const;
    std::uint32_t NodeType(std::uint64_t node) const;
    std::uint32_t EntityType(std::uint64_t node) const;
    bool HasChildNodes(std::uint64_t node) const;
    bool HasAttributes(std::uint64_t node) const;
    const char* NodeName(std::uint64_t node) const;
    const char* Text(std::uint64_t node) const;
    const char* SkippedText(std::uint64_t node) const;
    const char* AttributeName(std::uint64_t attribute) const;
    const char* AttributeValue(std::uint64_t attribute) const;
    const char* Entity(std::uint64_t node) const;
    const char* Attribute(std::uint64_t node, const char* name) const;

private:
    void CheckNode(std::uint64_t node) const;
    std::uint8_t At(std::uint64_t index) const;
    std::uint64_t Index(std::uint64_t at) const;
    std::uint64_t PrologEnd() const;
    std::uint64_t Search(std::uint64_t parent, std::uint64_t node, std::set<std::uint64_t>& seen) const;
    const char* String(const std::vector<std::uint8_t>& table, std::uint64_t position, std::uint64_t zeroed) const;
    const char* TextEntry(std::uint64_t index) const;

    const Buffers& buffers;
};

}
