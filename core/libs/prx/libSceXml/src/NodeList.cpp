#include "prx/libSceXml/src/NodeList.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace XmlDom {

namespace {

constexpr std::size_t BlockSize = 72;

NodeListState* StateOf(const XmlHandle* self) {
    NodeListState* state = nullptr;
    std::memcpy(&state, self->impl, sizeof(state));
    return state;
}

void RequireHandle(const XmlHandle* self, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
}

XmlNodeId NullNode() {
    return XmlNodeId{nullptr, nullptr, 0};
}

bool SameNode(const XmlNodeId& left, const XmlNodeId& right) {
    return left.first == right.first && left.second == right.second && left.flag == right.flag;
}

void Attach(XmlHandle* self, XmlMemAllocator* allocator, void* userData) {
    auto state = std::make_unique<NodeListState>();
    state->allocator = allocator;
    state->userData = userData;
    void* block = allocator->vtable->allocate(allocator, BlockSize, userData);
    if (block == nullptr) throw std::logic_error("sce::Xml::Dom::NodeList: an allocation failure while initializing a node list was not measured");
    NodeListState* owned = state.release();
    std::memcpy(block, &owned, sizeof(owned));
    self->impl = block;
}

}

int NodeListInitialize(XmlHandle* self, const XmlHandle* initializer) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::initialize");
    if (self->impl != nullptr) return ListAlreadyInitialized;
    if (initializer == nullptr || initializer->impl == nullptr) return ListNotInitialized;
    const auto* parameter = static_cast<const XmlInitParameter*>(initializer->impl);
    if (parameter->allocator == nullptr) throw std::invalid_argument("sce::Xml::Dom::NodeList::initialize: null allocator");
    Attach(self, parameter->allocator, parameter->userData);
    return 0;
}

void NodeListDestroy(XmlHandle* self) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::~NodeList");
    if (self->impl == nullptr) return;
    NodeListState* state = StateOf(self);
    state->allocator->vtable->deallocate(state->allocator, self->impl, state->userData);
    delete state;
    self->impl = nullptr;
}

int NodeListTerminate(XmlHandle* self) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::terminate");
    if (self->impl == nullptr) return ListNotInitialized;
    NodeListDestroy(self);
    return 0;
}

void NodeListCopy(XmlHandle* self, const XmlHandle* other) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::NodeList");
    RequireHandle(other, "sce::Xml::Dom::NodeList::NodeList");
    self->impl = nullptr;
    if (other->impl == nullptr) return;
    const NodeListState* source = StateOf(other);
    Attach(self, source->allocator, source->userData);
    StateOf(self)->items = source->items;
}

bool NodeListIsAvailable(const XmlHandle* self) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::isAvailable");
    return self->impl != nullptr;
}

int NodeListClear(XmlHandle* self) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::clear");
    if (self->impl == nullptr) return ListNotInitialized;
    StateOf(self)->items.clear();
    return 0;
}

int NodeListInsertFirst(XmlHandle* self, XmlNodeId item) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::insertFirst");
    if (self->impl == nullptr) return ListNotInitialized;
    StateOf(self)->items.insert(StateOf(self)->items.begin(), item);
    return 0;
}

int NodeListInsertLast(XmlHandle* self, XmlNodeId item) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::insertLast");
    if (self->impl == nullptr) return ListNotInitialized;
    StateOf(self)->items.push_back(item);
    return 0;
}

int NodeListRemoveItem(XmlHandle* self, XmlNodeId item) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::removeItem");
    if (self->impl == nullptr) return ListNotInitialized;
    std::vector<XmlNodeId>& items = StateOf(self)->items;
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!SameNode(items[index], item)) continue;
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
        return 0;
    }
    return ListItemNotFound;
}

std::uint32_t NodeListLength(const XmlHandle* self) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::getLength");
    if (self->impl == nullptr) return 0;
    return static_cast<std::uint32_t>(StateOf(self)->items.size());
}

XmlNodeId NodeListItem(const XmlHandle* self, std::uint32_t index) {
    RequireHandle(self, "sce::Xml::Dom::NodeList::item");
    if (self->impl == nullptr) return NullNode();
    const std::vector<XmlNodeId>& items = StateOf(self)->items;
    if (index >= items.size()) return NullNode();
    return items[index];
}

int NodeListReplace(XmlHandle* self, const std::vector<XmlNodeId>& items) {
    RequireHandle(self, "sce::Xml::Dom::NodeList: filling a list");
    if (self->impl == nullptr) return ListNotInitialized;
    StateOf(self)->items = items;
    return 0;
}

}
