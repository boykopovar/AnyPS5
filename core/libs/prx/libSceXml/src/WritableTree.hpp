#pragma once

#include "prx/libSceXml/src/DomParser.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace XmlDom {

struct Attribute;

constexpr int ErrorReadOnly = static_cast<int>(0x80850202);
constexpr int ErrorInvalidNodeType = static_cast<int>(0x8085020a);
constexpr std::uint32_t NodeTypeDocument = 0;
constexpr std::uint32_t NodeTypeAttribute = 1;
constexpr std::uint32_t NodeTypeEntity = 2;
constexpr std::uint32_t NodeTypeElement = 4;
constexpr std::uint32_t NodeTypeCreatedElement = 5;
constexpr std::uint32_t NodeTypePredElement = 6;
constexpr std::uint32_t NodeTypeText = 8;
constexpr std::uint32_t InvalidNodeTypeValue = 0xffffffffu;
constexpr std::uint32_t InvalidEntityTypeValue = 0x8085020eu;
constexpr std::size_t MaxElementAttributes = 128;

struct DomNode {
    std::uint32_t type = 0;
    std::string name;
    std::string value;
    std::uint32_t entityType = 0;
    bool hasEntityText = false;
    std::string entityText;
    DomNode* parent = nullptr;
    std::vector<DomNode*> children;
    std::vector<DomNode*> attributes;
};

class WritableTree {
public:
    WritableTree();
    static std::unique_ptr<WritableTree> FromBuffers(const Buffers& buffers);

    DomNode* DocRoot() const;
    DomNode* Root() const;
    DomNode* Resolve(const void* pointer) const;

    DomNode* CreateNode(std::uint32_t type);
    DomNode* CreateElement(std::string name, const std::vector<XmlDom::Attribute>* attributes, const std::string* text, int& status);
    DomNode* CreateText(std::string text);

    bool IsElement(const DomNode* node) const;
    bool IsContainer(const DomNode* node) const;
    bool IsAncestor(const DomNode* ancestor, const DomNode* descendant) const;
    bool CanAcceptChild(const DomNode* parent, const DomNode* child) const;

    void Detach(DomNode* node);
    bool InsertAfter(DomNode* parent, DomNode* refChild, DomNode* child, int& status);
    bool AppendChild(DomNode* parent, DomNode* child, int& status);
    bool RemoveChild(DomNode* child, DomNode* parent, int& status);
    void DeleteSubtree(DomNode* node);

    DomNode* FirstChild(const DomNode* node) const;
    DomNode* LastChild(const DomNode* node) const;
    DomNode* NextSibling(const DomNode* node) const;
    DomNode* Parent(const DomNode* node, int& status) const;
    DomNode* FirstAttribute(const DomNode* node) const;
    DomNode* NextAttribute(const DomNode* attribute) const;

    const char* NodeName(const DomNode* node) const;
    const char* Text(const DomNode* node) const;
    const char* SkippedText(const DomNode* node) const;
    const char* AttributeName(const DomNode* attribute) const;
    const char* AttributeValue(const DomNode* attribute) const;
    const char* Entity(const DomNode* node) const;
    const char* FindAttribute(const DomNode* node, const std::function<bool(const char*)>& matches) const;

    int SetText(DomNode* node, std::string text);
    int SetAttribute(DomNode* node, std::string name, std::string value, bool create);
    int SetAttributeList(DomNode* node, const std::vector<XmlDom::Attribute>& attributes);
    int RemoveAttribute(DomNode* node, const std::string& name);
    int RemoveAttributes(DomNode* node);

private:
    bool IsSkippedText(const DomNode* node) const;
    void EraseAttributes(DomNode* node);

    DomNode* docRoot = nullptr;
    std::unordered_map<const DomNode*, std::unique_ptr<DomNode>> owned;
};

}
