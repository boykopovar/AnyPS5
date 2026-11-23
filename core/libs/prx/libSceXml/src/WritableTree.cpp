#include "prx/libSceXml/src/WritableTree.hpp"

#include "prx/libSceXml/src/AttributeList.hpp"
#include "prx/libSceXml/src/DomReader.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace XmlDom {

namespace {

constexpr int ErrorInvalidParameter = static_cast<int>(0x80850003);

bool IsElementType(std::uint32_t type) {
    return type == NodeTypeElement || type == NodeTypeCreatedElement || type == NodeTypePredElement;
}

bool IsSkippedTextType(std::uint32_t type) {
    return (~type & 7u) == 0;
}

void ConvertSubtree(WritableTree& tree, const Reader& reader, DomNode* parent, std::uint64_t offset, std::set<std::uint64_t>& seenNodes) {
    for (std::uint64_t current = offset; current != 0; current = reader.Sibling(current)) {
        if (!seenNodes.insert(current).second) throw std::logic_error("libSceXml: a node cycle was not measured");
        const std::uint32_t type = reader.NodeType(current);
        DomNode* node = tree.CreateNode(type);
        if (IsElementType(type)) {
            const char* name = reader.NodeName(current);
            if (name != nullptr) node->name = name;
            std::set<std::uint64_t> seenAttributes;
            for (std::uint64_t attr = reader.FirstAttribute(current); attr != 0; attr = reader.NextAttribute(attr)) {
                if (!seenAttributes.insert(attr).second) throw std::logic_error("libSceXml: an attribute cycle was not measured");
                DomNode* attrNode = tree.CreateNode(NodeTypeAttribute);
                const char* attrName = reader.AttributeName(attr);
                const char* attrValue = reader.AttributeValue(attr);
                if (attrName != nullptr) attrNode->name = attrName;
                if (attrValue != nullptr) attrNode->value = attrValue;
                attrNode->parent = node;
                node->attributes.push_back(attrNode);
            }
        } else if (type == NodeTypeText) {
            const char* text = reader.Text(current);
            if (text != nullptr) node->value = text;
        } else if (IsSkippedTextType(type)) {
            const char* skipped = reader.SkippedText(current);
            if (skipped != nullptr) node->value = skipped;
        } else if ((type & 0x0fu) == NodeTypeEntity) {
            node->entityType = reader.EntityType(current);
            const char* entity = reader.Entity(current);
            if (entity != nullptr) {
                node->hasEntityText = true;
                node->entityText = entity;
            }
        }
        node->parent = parent;
        parent->children.push_back(node);
        const std::uint64_t child = reader.FirstChild(current);
        if (child != 0) ConvertSubtree(tree, reader, node, child, seenNodes);
    }
}

}

WritableTree::WritableTree() {
    docRoot = CreateNode(NodeTypeDocument);
}

std::unique_ptr<WritableTree> WritableTree::FromBuffers(const Buffers& buffers) {
    auto tree = std::make_unique<WritableTree>();
    if (buffers.nodes.empty()) return tree;
    const Reader reader(buffers);
    const std::uint64_t root = reader.Root();
    if (root == 0) return tree;
    std::set<std::uint64_t> seenNodes;
    ConvertSubtree(*tree, reader, tree->docRoot, root, seenNodes);
    return tree;
}

DomNode* WritableTree::DocRoot() const {
    return docRoot;
}

DomNode* WritableTree::Root() const {
    if (docRoot == nullptr) return nullptr;
    for (DomNode* child : docRoot->children) {
        if (!IsSkippedText(child)) return child;
    }
    return nullptr;
}

DomNode* WritableTree::Resolve(const void* pointer) const {
    if (pointer == nullptr) return nullptr;
    const auto it = owned.find(static_cast<const DomNode*>(pointer));
    return it != owned.end() ? it->second.get() : nullptr;
}

DomNode* WritableTree::CreateNode(std::uint32_t type) {
    auto node = std::make_unique<DomNode>();
    node->type = type;
    DomNode* raw = node.get();
    owned.emplace(raw, std::move(node));
    return raw;
}

DomNode* WritableTree::CreateElement(std::string name, const std::vector<Attribute>* attributes, const std::string* text, int& status) {
    if (attributes != nullptr) {
        std::vector<std::pair<std::string, std::string>> uniqueAttrs;
        for (const Attribute& entry : *attributes) {
            if (entry.name.empty()) {
                status = ErrorInvalidParameter;
                return nullptr;
            }
            bool updated = false;
            for (auto& existing : uniqueAttrs) {
                if (existing.first == entry.name) {
                    existing.second = entry.value;
                    updated = true;
                    break;
                }
            }
            if (!updated) {
                if (uniqueAttrs.size() >= MaxElementAttributes) {
                    status = ErrorTooManyAttributes;
                    return nullptr;
                }
                uniqueAttrs.emplace_back(entry.name, entry.value);
            }
        }
        DomNode* element = CreateNode(NodeTypeCreatedElement);
        element->name = std::move(name);
        for (auto& entry : uniqueAttrs) {
            DomNode* attr = CreateNode(NodeTypeAttribute);
            attr->name = std::move(entry.first);
            attr->value = std::move(entry.second);
            attr->parent = element;
            element->attributes.push_back(attr);
        }
        if (text != nullptr && !text->empty()) {
            DomNode* textNode = CreateText(*text);
            textNode->parent = element;
            element->children.push_back(textNode);
        }
        status = 0;
        return element;
    }
    DomNode* element = CreateNode(NodeTypeCreatedElement);
    element->name = std::move(name);
    if (text != nullptr && !text->empty()) {
        DomNode* textNode = CreateText(*text);
        textNode->parent = element;
        element->children.push_back(textNode);
    }
    status = 0;
    return element;
}

DomNode* WritableTree::CreateText(std::string text) {
    DomNode* node = CreateNode(NodeTypeText);
    node->value = std::move(text);
    return node;
}

bool WritableTree::IsElement(const DomNode* node) const {
    return node != nullptr && IsElementType(node->type);
}

bool WritableTree::IsContainer(const DomNode* node) const {
    return node != nullptr && (node->type == NodeTypeDocument || IsElementType(node->type));
}

bool WritableTree::IsSkippedText(const DomNode* node) const {
    return node != nullptr && IsSkippedTextType(node->type);
}

bool WritableTree::IsAncestor(const DomNode* ancestor, const DomNode* descendant) const {
    for (const DomNode* current = descendant; current != nullptr; current = current->parent) {
        if (current == ancestor) return true;
    }
    return false;
}

bool WritableTree::CanAcceptChild(const DomNode* parent, const DomNode* child) const {
    if (parent == nullptr || child == nullptr) return false;
    if (child == docRoot || child->type == NodeTypeAttribute) return false;
    if (IsAncestor(child, parent)) return false;
    if (parent->type == NodeTypeDocument) {
        if (IsElementType(child->type)) {
            for (const DomNode* existing : parent->children) {
                if (existing != child && IsElementType(existing->type)) return false;
            }
            return true;
        }
        return IsSkippedTextType(child->type);
    }
    if (IsElementType(parent->type)) {
        return IsElementType(child->type) || child->type == NodeTypeText || IsSkippedTextType(child->type) || (child->type & 0x0fu) == NodeTypeEntity;
    }
    return false;
}

void WritableTree::Detach(DomNode* node) {
    if (node == nullptr || node->parent == nullptr) return;
    auto& list = node->type == NodeTypeAttribute ? node->parent->attributes : node->parent->children;
    const auto it = std::find(list.begin(), list.end(), node);
    if (it != list.end()) list.erase(it);
    node->parent = nullptr;
}

bool WritableTree::InsertAfter(DomNode* parent, DomNode* refChild, DomNode* child, int& status) {
    if (parent == nullptr || child == nullptr) {
        status = ErrorInvalidParameter;
        return false;
    }
    if (!CanAcceptChild(parent, child)) {
        status = ErrorInvalidParameter;
        return false;
    }
    if (refChild != nullptr) {
        if (refChild == child) {
            status = ErrorInvalidParameter;
            return false;
        }
        if (refChild->parent != parent) {
            status = StatusNodeNotFound;
            return false;
        }
    }
    if (child->parent != nullptr) Detach(child);
    if (refChild == nullptr) {
        parent->children.insert(parent->children.begin(), child);
    } else {
        const auto it = std::find(parent->children.begin(), parent->children.end(), refChild);
        if (it == parent->children.end()) {
            status = StatusNodeNotFound;
            return false;
        }
        parent->children.insert(it + 1, child);
    }
    child->parent = parent;
    status = 0;
    return true;
}

bool WritableTree::AppendChild(DomNode* parent, DomNode* child, int& status) {
    if (parent == nullptr || child == nullptr) {
        status = ErrorInvalidParameter;
        return false;
    }
    if (!CanAcceptChild(parent, child)) {
        status = ErrorInvalidParameter;
        return false;
    }
    if (child->parent != nullptr) Detach(child);
    parent->children.push_back(child);
    child->parent = parent;
    status = 0;
    return true;
}

bool WritableTree::RemoveChild(DomNode* child, DomNode* parent, int& status) {
    if (child == nullptr || parent == nullptr) {
        status = ErrorInvalidParameter;
        return false;
    }
    if (child->parent != parent) {
        status = StatusNodeNotFound;
        return false;
    }
    Detach(child);
    status = 0;
    return true;
}

void WritableTree::EraseAttributes(DomNode* node) {
    std::vector<DomNode*> attributes = std::move(node->attributes);
    node->attributes.clear();
    for (DomNode* attr : attributes) {
        owned.erase(attr);
    }
}

void WritableTree::DeleteSubtree(DomNode* node) {
    if (node == nullptr || node == docRoot) return;
    Detach(node);
    std::vector<DomNode*> stack{node};
    while (!stack.empty()) {
        DomNode* current = stack.back();
        stack.pop_back();
        EraseAttributes(current);
        for (DomNode* child : current->children) {
            child->parent = nullptr;
            stack.push_back(child);
        }
        current->children.clear();
        owned.erase(current);
    }
}

DomNode* WritableTree::FirstChild(const DomNode* node) const {
    if (node == nullptr || node->children.empty()) return nullptr;
    if (node == docRoot) return Root();
    return node->children.front();
}

DomNode* WritableTree::LastChild(const DomNode* node) const {
    if (node == nullptr || node->children.empty()) return nullptr;
    if (node == docRoot) {
        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
            if (!IsSkippedText(*it)) return *it;
        }
        return nullptr;
    }
    return node->children.back();
}

DomNode* WritableTree::NextSibling(const DomNode* node) const {
    if (node == nullptr || node->parent == nullptr || node->type == NodeTypeAttribute) return nullptr;
    const auto& siblings = node->parent->children;
    const auto it = std::find(siblings.begin(), siblings.end(), node);
    if (it == siblings.end() || it + 1 == siblings.end()) return nullptr;
    return *(it + 1);
}

DomNode* WritableTree::Parent(const DomNode* node, int& status) const {
    status = 0;
    if (node == nullptr || node->type == NodeTypeAttribute) {
        status = StatusNodeNotFound;
        return nullptr;
    }
    if (node == docRoot || node->parent == docRoot) return nullptr;
    if (node->parent == nullptr) {
        status = StatusNodeNotFound;
        return nullptr;
    }
    return node->parent;
}

DomNode* WritableTree::FirstAttribute(const DomNode* node) const {
    if (!IsElement(node) || node->attributes.empty()) return nullptr;
    return node->attributes.front();
}

DomNode* WritableTree::NextAttribute(const DomNode* attribute) const {
    if (attribute == nullptr || attribute->type != NodeTypeAttribute || attribute->parent == nullptr) return nullptr;
    const auto& attributes = attribute->parent->attributes;
    const auto it = std::find(attributes.begin(), attributes.end(), attribute);
    if (it == attributes.end() || it + 1 == attributes.end()) return nullptr;
    return *(it + 1);
}

const char* WritableTree::NodeName(const DomNode* node) const {
    if (node == nullptr) return nullptr;
    if (IsElementType(node->type)) return node->name.c_str();
    if (node->type == NodeTypeText || IsSkippedTextType(node->type)) return node->value.c_str();
    return nullptr;
}

const char* WritableTree::Text(const DomNode* node) const {
    if (node == nullptr || node->type != NodeTypeText) return nullptr;
    return node->value.c_str();
}

const char* WritableTree::SkippedText(const DomNode* node) const {
    if (node == nullptr || !IsSkippedTextType(node->type)) return nullptr;
    return node->value.c_str();
}

const char* WritableTree::AttributeName(const DomNode* attribute) const {
    if (attribute == nullptr || attribute->type != NodeTypeAttribute) return nullptr;
    return attribute->name.c_str();
}

const char* WritableTree::AttributeValue(const DomNode* attribute) const {
    if (attribute == nullptr || attribute->type != NodeTypeAttribute) return nullptr;
    return attribute->value.c_str();
}

const char* WritableTree::Entity(const DomNode* node) const {
    if (node == nullptr || (node->type & 0x0fu) != NodeTypeEntity || !node->hasEntityText) return nullptr;
    return node->entityText.c_str();
}

const char* WritableTree::FindAttribute(const DomNode* node, const std::function<bool(const char*)>& matches) const {
    if (!IsElement(node)) return nullptr;
    for (const DomNode* attr : node->attributes) {
        if (matches(attr->name.c_str())) return attr->value.c_str();
    }
    return nullptr;
}

int WritableTree::SetText(DomNode* node, std::string text) {
    if (node == nullptr) return ErrorInvalidParameter;
    if (node->type == NodeTypeText) {
        node->value = std::move(text);
        return 0;
    }
    if (IsElementType(node->type)) {
        for (DomNode* child : node->children) {
            if (child->type == NodeTypeText) {
                child->value = std::move(text);
                return 0;
            }
        }
        DomNode* textNode = CreateText(std::move(text));
        textNode->parent = node;
        node->children.push_back(textNode);
        return 0;
    }
    return ErrorInvalidNodeType;
}

int WritableTree::SetAttribute(DomNode* node, std::string name, std::string value, bool create) {
    if (node == nullptr) return ErrorInvalidParameter;
    if (!IsElementType(node->type)) return ErrorInvalidNodeType;
    for (DomNode* attr : node->attributes) {
        if (attr->name == name) {
            attr->value = std::move(value);
            return 0;
        }
    }
    if (!create) return 0;
    if (node->attributes.size() >= MaxElementAttributes) return ErrorTooManyAttributes;
    DomNode* attr = CreateNode(NodeTypeAttribute);
    attr->name = std::move(name);
    attr->value = std::move(value);
    attr->parent = node;
    node->attributes.push_back(attr);
    return 0;
}

int WritableTree::SetAttributeList(DomNode* node, const std::vector<Attribute>& attributes) {
    if (node == nullptr) return ErrorInvalidParameter;
    if (!IsElementType(node->type)) return ErrorInvalidNodeType;
    std::vector<std::pair<std::string, std::string>> uniqueAttrs;
    for (const Attribute& entry : attributes) {
        if (entry.name.empty()) return ErrorInvalidParameter;
        bool updated = false;
        for (auto& existing : uniqueAttrs) {
            if (existing.first == entry.name) {
                existing.second = entry.value;
                updated = true;
                break;
            }
        }
        if (!updated) {
            if (uniqueAttrs.size() >= MaxElementAttributes) return ErrorTooManyAttributes;
            uniqueAttrs.emplace_back(entry.name, entry.value);
        }
    }
    EraseAttributes(node);
    for (auto& entry : uniqueAttrs) {
        DomNode* attr = CreateNode(NodeTypeAttribute);
        attr->name = std::move(entry.first);
        attr->value = std::move(entry.second);
        attr->parent = node;
        node->attributes.push_back(attr);
    }
    return 0;
}

int WritableTree::RemoveAttribute(DomNode* node, const std::string& name) {
    if (node == nullptr) return ErrorInvalidParameter;
    if (!IsElementType(node->type)) return ErrorInvalidNodeType;
    for (auto it = node->attributes.begin(); it != node->attributes.end(); ++it) {
        if ((*it)->name == name) {
            DomNode* attr = *it;
            node->attributes.erase(it);
            owned.erase(attr);
            return 0;
        }
    }
    return 0;
}

int WritableTree::RemoveAttributes(DomNode* node) {
    if (node == nullptr) return ErrorInvalidParameter;
    if (!IsElementType(node->type)) return ErrorInvalidNodeType;
    EraseAttributes(node);
    return 0;
}

}
