#include "prx/libSceXml/src/NodeList.hpp"

#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace XmlDom {

namespace {

constexpr std::size_t BlockSize = 72;

template <typename T>
bool CopyInGuest(T& destination, const T* source, std::size_t bytes = sizeof(T)) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&destination, source, bytes);
    errno = savedErrno;
    return copied;
}

template <typename T>
bool CopyOutGuest(T* destination, const T& source, std::size_t bytes = sizeof(T)) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &source, bytes);
    errno = savedErrno;
    return copied;
}

bool IsGuestWritable(void* destination, std::size_t bytes) {
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    return writable;
}

XmlHandle ReadGuestHandle(const XmlHandle* guest, const char* function) {
    XmlHandle local{};
    if (!CopyInGuest(local, guest)) {
        throw std::logic_error(std::string(function) + ": reading an unreadable object");
    }
    return local;
}

void WriteGuestHandle(XmlHandle* guest, const XmlHandle& value, const char* function) {
    if (!CopyOutGuest(guest, value)) {
        throw std::logic_error(std::string(function) + ": writing to an unwritable object");
    }
}

NodeListState* StateOf(const XmlHandle& self, const char* function) {
    NodeListState* state = nullptr;
    if (!CopyInGuest(state, reinterpret_cast<NodeListState* const*>(self.impl))) {
        throw std::logic_error(std::string(function) + ": reading an unreadable node list block");
    }
    return state;
}

void RequireHandle(const XmlHandle* self, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
}

NodeListState* RequireLiveState(const XmlHandle* self, const char* function) {
    RequireHandle(self, function);
    const XmlHandle localSelf = ReadGuestHandle(self, function);
    if (localSelf.impl == nullptr) return nullptr;
    return StateOf(localSelf, function);
}

XmlNodeId NullNode() {
    return XmlNodeId{nullptr, nullptr, 0};
}

bool SameNode(const XmlNodeId& left, const XmlNodeId& right) {
    return left.first == right.first && left.second == right.second && left.flag == right.flag;
}

void Attach(XmlHandle* self, XmlMemAllocator* allocator, void* userData, const std::vector<XmlNodeId>* initialItems, const char* function) {
    if (!IsGuestWritable(self, sizeof(XmlHandle))) {
        throw std::logic_error(std::string(function) + ": writing to an unwritable object");
    }
    auto state = std::make_unique<NodeListState>();
    state->allocator = allocator;
    state->userData = userData;
    if (initialItems != nullptr) state->items = *initialItems;
    void* block = allocator->vtable->allocate(allocator, BlockSize, userData);
    if (block == nullptr) throw std::logic_error("sce::Xml::Dom::NodeList: an allocation failure while initializing a node list was not measured");
    NodeListState* rawState = state.get();
    if (!CopyOutGuest(static_cast<NodeListState**>(block), rawState)) {
        allocator->vtable->deallocate(allocator, block, userData);
        throw std::logic_error(std::string(function) + ": writing to an unwritable node list block");
    }
    const XmlHandle updated{block};
    if (!CopyOutGuest(self, updated)) {
        allocator->vtable->deallocate(allocator, block, userData);
        throw std::logic_error(std::string(function) + ": writing to an unwritable object");
    }
    state.release();
}

bool DestroyNodeList(XmlHandle* self, const char* function) {
    RequireHandle(self, function);
    const XmlHandle localSelf = ReadGuestHandle(self, function);
    if (localSelf.impl == nullptr) return false;
    NodeListState* state = StateOf(localSelf, function);
    WriteGuestHandle(self, XmlHandle{nullptr}, function);
    state->allocator->vtable->deallocate(state->allocator, localSelf.impl, state->userData);
    delete state;
    return true;
}

}

int NodeListInitialize(XmlHandle* self, const XmlHandle* initializer) {
    constexpr const char* fn = "sce::Xml::Dom::NodeList::initialize";
    RequireHandle(self, fn);
    const XmlHandle localSelf = ReadGuestHandle(self, fn);
    if (localSelf.impl != nullptr) return ListAlreadyInitialized;
    if (initializer == nullptr) return ListNotInitialized;
    const XmlHandle localInit = ReadGuestHandle(initializer, fn);
    if (localInit.impl == nullptr) return ListNotInitialized;
    XmlInitParameter parameter{};
    if (!CopyInGuest(parameter, static_cast<const XmlInitParameter*>(localInit.impl))) {
        throw std::logic_error(std::string(fn) + ": reading an unreadable initializer parameter");
    }
    if (parameter.allocator == nullptr) throw std::invalid_argument("sce::Xml::Dom::NodeList::initialize: null allocator");
    Attach(self, parameter.allocator, parameter.userData, nullptr, fn);
    return 0;
}

void NodeListDestroy(XmlHandle* self) {
    DestroyNodeList(self, "sce::Xml::Dom::NodeList::~NodeList");
}

int NodeListTerminate(XmlHandle* self) {
    if (!DestroyNodeList(self, "sce::Xml::Dom::NodeList::terminate")) return ListNotInitialized;
    return 0;
}

void NodeListCopy(XmlHandle* self, const XmlHandle* other) {
    constexpr const char* fn = "sce::Xml::Dom::NodeList::NodeList";
    RequireHandle(self, fn);
    RequireHandle(other, fn);
    const XmlHandle localOther = ReadGuestHandle(other, fn);
    if (localOther.impl == nullptr) {
        WriteGuestHandle(self, XmlHandle{nullptr}, fn);
        return;
    }
    const NodeListState* source = StateOf(localOther, fn);
    if (self == other) {
        if (!IsGuestWritable(self, sizeof(XmlHandle))) {
            throw std::logic_error(std::string(fn) + ": writing to an unwritable object");
        }
        return;
    }
    Attach(self, source->allocator, source->userData, &source->items, fn);
}

bool NodeListIsAvailable(const XmlHandle* self) {
    constexpr const char* fn = "sce::Xml::Dom::NodeList::isAvailable";
    RequireHandle(self, fn);
    return ReadGuestHandle(self, fn).impl != nullptr;
}

int NodeListClear(XmlHandle* self) {
    NodeListState* state = RequireLiveState(self, "sce::Xml::Dom::NodeList::clear");
    if (state == nullptr) return ListNotInitialized;
    state->items.clear();
    return 0;
}

int NodeListInsertFirst(XmlHandle* self, XmlNodeId item) {
    NodeListState* state = RequireLiveState(self, "sce::Xml::Dom::NodeList::insertFirst");
    if (state == nullptr) return ListNotInitialized;
    state->items.insert(state->items.begin(), item);
    return 0;
}

int NodeListInsertLast(XmlHandle* self, XmlNodeId item) {
    NodeListState* state = RequireLiveState(self, "sce::Xml::Dom::NodeList::insertLast");
    if (state == nullptr) return ListNotInitialized;
    state->items.push_back(item);
    return 0;
}

int NodeListRemoveItem(XmlHandle* self, XmlNodeId item) {
    NodeListState* state = RequireLiveState(self, "sce::Xml::Dom::NodeList::removeItem");
    if (state == nullptr) return ListNotInitialized;
    std::vector<XmlNodeId>& items = state->items;
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!SameNode(items[index], item)) continue;
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
        return 0;
    }
    return ListItemNotFound;
}

std::uint32_t NodeListLength(const XmlHandle* self) {
    const NodeListState* state = RequireLiveState(self, "sce::Xml::Dom::NodeList::getLength");
    if (state == nullptr) return 0;
    return static_cast<std::uint32_t>(state->items.size());
}

XmlNodeId NodeListItem(const XmlHandle* self, std::uint32_t index) {
    const NodeListState* state = RequireLiveState(self, "sce::Xml::Dom::NodeList::item");
    if (state == nullptr) return NullNode();
    const std::vector<XmlNodeId>& items = state->items;
    if (index >= items.size()) return NullNode();
    return items[index];
}

int NodeListReplace(XmlHandle* self, const std::vector<XmlNodeId>& items) {
    NodeListState* state = RequireLiveState(self, "sce::Xml::Dom::NodeList: filling a list");
    if (state == nullptr) return ListNotInitialized;
    state->items = items;
    return 0;
}

}
