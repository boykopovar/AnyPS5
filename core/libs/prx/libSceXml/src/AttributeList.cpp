#include "prx/libSceXml/src/AttributeList.hpp"

#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace XmlDom {

namespace {

constexpr int ErrorNoMemory = static_cast<int>(0x80850001);
constexpr int ErrorInvalidParameter = static_cast<int>(0x80850003);
constexpr int ErrorDuplicateAttribute = static_cast<int>(0x8085001a);
constexpr std::size_t WrapperSize = 0x20;
constexpr std::size_t TableSize = 0x28;
constexpr std::size_t NameLimit = 0x80;
constexpr std::size_t ChunkSize = 0x1000;

struct ListState {
    const XmlHandle* initializer = nullptr;
    void* table = nullptr;
    bool hasTable = false;
    bool shared = false;
    std::vector<AttributeEntry> entries;
};

struct AttrState {
    const XmlHandle* initializer = nullptr;
    std::string name;
    std::string value;
};

template <typename State>
State* StateOf(const XmlHandle* self) {
    State* state = nullptr;
    std::memcpy(&state, self->impl, sizeof(state));
    return state;
}

void RequireHandle(const void* self, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
}

[[noreturn]] void Fault(const char* function, const char* what) {
    throw std::logic_error(std::string(function) + ": " + what + " faults in 13.60");
}

const XmlInitParameter* ParameterOf(const XmlHandle* initializer) {
    return static_cast<const XmlInitParameter*>(initializer->impl);
}

void* Allocate(const XmlInitParameter* parameter, std::size_t size) {
    if (parameter->allocator == nullptr) throw std::invalid_argument("sce::Xml: an initializer with a null allocator");
    return parameter->allocator->vtable->allocate(parameter->allocator, size, parameter->userData);
}

void Release(const XmlInitParameter* parameter, void* block) {
    parameter->allocator->vtable->deallocate(parameter->allocator, block, parameter->userData);
}

template <typename State>
void Attach(XmlHandle* self, void* block, State* state) {
    std::memcpy(block, &state, sizeof(state));
    self->impl = block;
}

std::string ReadBytes(const char* source, std::size_t size, const char* function) {
    std::string text(size, '\0');
    if (size != 0 && !GuestCopyIn_nid_no_patch(text.data(), source, size)) Fault(function, "reading a string where it is not readable");
    return text;
}

std::string ReadTerminated(const char* source, std::size_t limit, const char* function) {
    std::string text;
    auto address = reinterpret_cast<std::uintptr_t>(source);
    char chunk[ChunkSize];
    while (text.size() < limit) {
        const std::size_t count = std::min(ChunkSize - static_cast<std::size_t>(address % ChunkSize), limit - text.size());
        if (!GuestCopyIn_nid_no_patch(chunk, reinterpret_cast<const void*>(address), count)) Fault(function, "reading a string where it is not readable");
        if (const void* end = std::memchr(chunk, 0, count)) {
            text.append(chunk, static_cast<std::size_t>(static_cast<const char*>(end) - chunk));
            return text;
        }
        text.append(chunk, count);
        address += count;
    }
    return text;
}

bool Usable(const XmlString* string) {
    return string != nullptr && string->buffer != nullptr && string->size != 0;
}

void ReleaseTable(ListState* state, const char* function) {
    if (!state->hasTable) return;
    if (!state->shared) {
        const XmlInitParameter* parameter = ParameterOf(state->initializer);
        if (parameter == nullptr) Fault(function, "freeing the attribute table after the initializer was terminated");
        Release(parameter, state->table);
    }
    state->table = nullptr;
    state->hasTable = false;
    state->shared = false;
    state->entries.clear();
}

XmlHandle* Fill(XmlHandle* result, const XmlHandle* initializer, const AttributeEntry& entry) {
    if (AttrInitialize(result, initializer) != 0) return result;
    AttrState* state = StateOf<AttrState>(result);
    state->name = entry.name;
    state->value = entry.value;
    return result;
}

XmlString* SetString(XmlString* result, const std::string& text) {
    result->buffer = text.c_str();
    result->size = text.size();
    return result;
}

XmlString* SetEmpty(XmlString* result) {
    result->buffer = nullptr;
    result->size = 0;
    return result;
}

int SetText(XmlHandle* self, const XmlString* text, std::string AttrState::*field, const char* function) {
    RequireHandle(self, function);
    if (!Usable(text)) return ErrorInvalidParameter;
    if (self->impl == nullptr) return NotInitialized;
    const auto size = static_cast<std::uint32_t>(text->size);
    if (size > static_cast<std::uint32_t>(INT_MAX)) Fault(function, "a string whose size does not fit in an int");
    StateOf<AttrState>(self)->*field = ReadBytes(text->buffer, size, function);
    return 0;
}

XmlString* GetText(XmlString* result, const XmlHandle* self, std::string AttrState::*field, const char* function) {
    RequireHandle(self, function);
    if (result == nullptr) throw std::invalid_argument(std::string(function) + ": null result");
    if (self->impl == nullptr) return SetEmpty(result);
    return SetString(result, StateOf<AttrState>(self)->*field);
}

}

int ListInitialize(XmlHandle* self, const XmlHandle* initializer) {
    RequireHandle(self, "sce::Xml::AttributeList::initialize");
    if (self->impl != nullptr) return AlreadyInitialized;
    if (initializer == nullptr || initializer->impl == nullptr) return ErrorNoMemory;
    const XmlInitParameter* parameter = ParameterOf(initializer);
    void* block = Allocate(parameter, WrapperSize);
    if (block == nullptr) return ErrorNoMemory;
    auto* state = new ListState;
    state->initializer = initializer;
    Attach(self, block, state);
    void* table = Allocate(parameter, TableSize);
    if (table == nullptr) Fault("sce::Xml::AttributeList::initialize", "an allocation failure for the attribute table");
    state->table = table;
    state->hasTable = true;
    return 0;
}

void ListDestroy(XmlHandle* self) {
    RequireHandle(self, "sce::Xml::AttributeList::~AttributeList");
    if (self->impl == nullptr) return;
    ListState* state = StateOf<ListState>(self);
    ReleaseTable(state, "sce::Xml::AttributeList::~AttributeList");
    const XmlInitParameter* parameter = ParameterOf(state->initializer);
    if (parameter == nullptr) return;
    Release(parameter, self->impl);
    delete state;
    self->impl = nullptr;
}

int ListTerminate(XmlHandle* self) {
    RequireHandle(self, "sce::Xml::AttributeList::terminate");
    if (self->impl == nullptr) return 0;
    if (ParameterOf(StateOf<ListState>(self)->initializer) == nullptr) return 0;
    ListDestroy(self);
    return 0;
}

void ListCopy(XmlHandle* self, const XmlHandle* other) {
    RequireHandle(self, "sce::Xml::AttributeList::AttributeList");
    RequireHandle(other, "sce::Xml::AttributeList::AttributeList");
    self->impl = nullptr;
    if (other->impl == nullptr) Fault("sce::Xml::AttributeList::AttributeList", "copying a list that was not initialized");
    const ListState* source = StateOf<ListState>(other);
    const XmlInitParameter* parameter = ParameterOf(source->initializer);
    if (parameter == nullptr) return;
    if (!source->hasTable) Fault("sce::Xml::AttributeList::AttributeList", "copying a list without an attribute table");
    if (source->shared) throw std::logic_error("sce::Xml::AttributeList::AttributeList: a copy of the list a Sax::Parser hands to startElement shares the parser's table in 13.60, which is not modelled");
    void* block = Allocate(parameter, WrapperSize);
    if (block == nullptr) return;
    auto* state = new ListState;
    state->initializer = source->initializer;
    Attach(self, block, state);
    void* table = Allocate(parameter, TableSize);
    if (table == nullptr) return;
    state->table = table;
    state->hasTable = true;
    state->entries = source->entries;
}

bool ListIsAvailable(const XmlHandle* self) {
    RequireHandle(self, "sce::Xml::AttributeList::isAvailable");
    return self->impl != nullptr && StateOf<ListState>(self)->hasTable;
}

int ListAddAttribute(XmlHandle* self, const XmlString* name, const XmlString* value) {
    constexpr const char* function = "sce::Xml::AttributeList::addAttribute";
    RequireHandle(self, function);
    if (self->impl == nullptr) return NotInitialized;
    ListState* state = StateOf<ListState>(self);
    if (!state->hasTable) return NotInitialized;
    if (!Usable(name) || !Usable(value)) return ErrorInvalidParameter;
    std::string key = ReadTerminated(name->buffer, name->size > NameLimit ? NameLimit : SIZE_MAX, function);
    if (key.empty()) return ErrorInvalidParameter;
    std::string text = ReadTerminated(value->buffer, SIZE_MAX, function);
    for (const AttributeEntry& entry : state->entries) {
        if (entry.name == key) return ErrorDuplicateAttribute;
    }
    state->entries.push_back(AttributeEntry{std::move(key), std::move(text)});
    return 0;
}

void ListClear(XmlHandle* self) {
    RequireHandle(self, "sce::Xml::AttributeList::clear");
    if (self->impl == nullptr) return;
    ListState* state = StateOf<ListState>(self);
    if (state->hasTable) state->entries.clear();
}

int ListGetLength(const XmlHandle* self) {
    RequireHandle(self, "sce::Xml::AttributeList::getLength");
    if (self->impl == nullptr) return 0;
    const ListState* state = StateOf<ListState>(self);
    return state->hasTable ? static_cast<int>(state->entries.size()) : 0;
}

XmlHandle* ListGetAttributeByName(XmlHandle* result, const XmlHandle* self, const XmlString* name) {
    constexpr const char* function = "sce::Xml::AttributeList::getAttribute";
    RequireHandle(self, function);
    RequireHandle(result, function);
    result->impl = nullptr;
    if (self->impl == nullptr) return result;
    const ListState* state = StateOf<ListState>(self);
    if (!state->hasTable || !Usable(name)) return result;
    const auto wanted = static_cast<std::int32_t>(static_cast<std::uint32_t>(name->size));
    if (wanted <= 0) return result;
    std::string key;
    bool read = false;
    for (const AttributeEntry& entry : state->entries) {
        if (static_cast<std::int32_t>(entry.name.size()) != wanted) continue;
        if (!read) {
            key = ReadBytes(name->buffer, static_cast<std::size_t>(wanted), function);
            read = true;
        }
        if (entry.name == key) return Fill(result, state->initializer, entry);
    }
    return result;
}

XmlHandle* ListGetAttributeByIndex(XmlHandle* result, const XmlHandle* self, std::uint32_t index) {
    constexpr const char* function = "sce::Xml::AttributeList::getAttribute";
    RequireHandle(self, function);
    RequireHandle(result, function);
    result->impl = nullptr;
    if (self->impl == nullptr) return result;
    const ListState* state = StateOf<ListState>(self);
    if (!state->hasTable) return result;
    if (index > state->entries.size()) Fault(function, "an index past the end of the list");
    if (index == state->entries.size()) return result;
    return Fill(result, state->initializer, state->entries[index]);
}

void ListShare(XmlHandle* self, std::vector<AttributeEntry> entries) {
    ListState* state = StateOf<ListState>(self);
    ReleaseTable(state, "sce::Xml::Sax::Parser::parse");
    state->hasTable = true;
    state->shared = true;
    state->entries = std::move(entries);
}

int AttrInitialize(XmlHandle* self, const XmlHandle* initializer) {
    RequireHandle(self, "sce::Xml::Attr::initialize");
    if (self->impl != nullptr) return AlreadyInitialized;
    if (initializer == nullptr || initializer->impl == nullptr) return NotInitialized;
    void* block = Allocate(ParameterOf(initializer), WrapperSize);
    if (block == nullptr) return ErrorNoMemory;
    auto* state = new AttrState;
    state->initializer = initializer;
    Attach(self, block, state);
    return 0;
}

void AttrDestroy(XmlHandle* self) {
    RequireHandle(self, "sce::Xml::Attr::~Attr");
    if (self->impl == nullptr) return;
    AttrState* state = StateOf<AttrState>(self);
    const XmlInitParameter* parameter = ParameterOf(state->initializer);
    if (parameter == nullptr) Fault("sce::Xml::Attr::~Attr", "freeing an Attr after its initializer was terminated");
    Release(parameter, self->impl);
    delete state;
    self->impl = nullptr;
}

int AttrTerminate(XmlHandle* self) {
    RequireHandle(self, "sce::Xml::Attr::terminate");
    if (self->impl == nullptr) return NotInitialized;
    AttrDestroy(self);
    return 0;
}

void AttrCopy(XmlHandle* self, const XmlHandle* other) {
    RequireHandle(self, "sce::Xml::Attr::Attr");
    RequireHandle(other, "sce::Xml::Attr::Attr");
    self->impl = nullptr;
    if (other->impl == nullptr) return;
    const AttrState* source = StateOf<AttrState>(other);
    const XmlInitParameter* parameter = ParameterOf(source->initializer);
    if (parameter == nullptr) return;
    void* block = Allocate(parameter, WrapperSize);
    if (block == nullptr) return;
    Attach(self, block, new AttrState(*source));
}

XmlHandle* AttrAssign(XmlHandle* self, const XmlHandle* other) {
    RequireHandle(self, "sce::Xml::Attr::operator=");
    RequireHandle(other, "sce::Xml::Attr::operator=");
    if (self == other) return self;
    if (other->impl == nullptr) {
        AttrDestroy(self);
        return self;
    }
    const AttrState* source = StateOf<AttrState>(other);
    if (self->impl != nullptr) {
        AttrState* target = StateOf<AttrState>(self);
        target->name = source->name;
        target->value = source->value;
        return self;
    }
    AttrCopy(self, other);
    return self;
}

bool AttrIsAvailable(const XmlHandle* self) {
    RequireHandle(self, "sce::Xml::Attr::isAvailable");
    return self->impl != nullptr;
}

int AttrSetName(XmlHandle* self, const XmlString* name) {
    return SetText(self, name, &AttrState::name, "sce::Xml::Attr::setName");
}

int AttrSetValue(XmlHandle* self, const XmlString* value) {
    return SetText(self, value, &AttrState::value, "sce::Xml::Attr::setValue");
}

XmlString* AttrGetName(XmlString* result, const XmlHandle* self) {
    return GetText(result, self, &AttrState::name, "sce::Xml::Attr::getName");
}

XmlString* AttrGetValue(XmlString* result, const XmlHandle* self) {
    return GetText(result, self, &AttrState::value, "sce::Xml::Attr::getValue");
}

}
