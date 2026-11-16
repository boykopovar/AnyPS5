#pragma once

#include "prx/libSceXml/include/Api.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace XmlDom {

constexpr int NotInitialized = static_cast<int>(0x80850002);
constexpr int AlreadyInitialized = 0x00850003;

struct AttributeEntry {
    std::string name;
    std::string value;
};

int ListInitialize(XmlHandle* self, const XmlHandle* initializer);
int ListTerminate(XmlHandle* self);
void ListDestroy(XmlHandle* self);
void ListCopy(XmlHandle* self, const XmlHandle* other);
bool ListIsAvailable(const XmlHandle* self);
int ListAddAttribute(XmlHandle* self, const XmlString* name, const XmlString* value);
void ListClear(XmlHandle* self);
int ListGetLength(const XmlHandle* self);
XmlHandle* ListGetAttributeByName(XmlHandle* result, const XmlHandle* self, const XmlString* name);
XmlHandle* ListGetAttributeByIndex(XmlHandle* result, const XmlHandle* self, std::uint32_t index);
void ListShare(XmlHandle* self, std::vector<AttributeEntry> entries);

int AttrInitialize(XmlHandle* self, const XmlHandle* initializer);
int AttrTerminate(XmlHandle* self);
void AttrDestroy(XmlHandle* self);
void AttrCopy(XmlHandle* self, const XmlHandle* other);
XmlHandle* AttrAssign(XmlHandle* self, const XmlHandle* other);
bool AttrIsAvailable(const XmlHandle* self);
int AttrSetName(XmlHandle* self, const XmlString* name);
int AttrSetValue(XmlHandle* self, const XmlString* value);
XmlString* AttrGetName(XmlString* result, const XmlHandle* self);
XmlString* AttrGetValue(XmlString* result, const XmlHandle* self);

}
