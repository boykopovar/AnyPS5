#pragma once

#include "prx/libSceXml/include/Api.hpp"

#include <cstdint>
#include <vector>

namespace XmlDom {

constexpr int ListNotInitialized = static_cast<int>(0x80850002);
constexpr int ListAlreadyInitialized = 0x00850003;
constexpr int ListItemNotFound = static_cast<int>(0x80850201);

struct NodeListState {
    XmlMemAllocator* allocator = nullptr;
    void* userData = nullptr;
    std::vector<XmlNodeId> items;
};

int NodeListInitialize(XmlHandle* self, const XmlHandle* initializer);
int NodeListTerminate(XmlHandle* self);
void NodeListDestroy(XmlHandle* self);
void NodeListCopy(XmlHandle* self, const XmlHandle* other);
bool NodeListIsAvailable(const XmlHandle* self);
int NodeListClear(XmlHandle* self);
int NodeListInsertFirst(XmlHandle* self, XmlNodeId item);
int NodeListInsertLast(XmlHandle* self, XmlNodeId item);
int NodeListRemoveItem(XmlHandle* self, XmlNodeId item);
std::uint32_t NodeListLength(const XmlHandle* self);
XmlNodeId NodeListItem(const XmlHandle* self, std::uint32_t index);
int NodeListReplace(XmlHandle* self, const std::vector<XmlNodeId>& items);

}
