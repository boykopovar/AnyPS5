#include "prx/libSceXml/src/DocumentBuilder.hpp"

#include "prx/libSceXml/src/DomParser.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace XmlDom {

namespace {

constexpr int ErrorNoMemory = static_cast<int>(0x80850001);
constexpr int ErrorNotInitialized = static_cast<int>(0x80850002);
constexpr int ErrorInvalidParameter = static_cast<int>(0x80850003);
constexpr int AlreadyInitialized = 0x00850003;

constexpr std::array<std::size_t, 15> BlockSizes{80, 440, 84, 84, 84, 104, 200, 72, 1240, 96, 2088, 5304, 2088, 1752, 1752};
constexpr std::array<std::size_t, 15> ReleaseOrder{6, 5, 7, 3, 4, 2, 1, 14, 13, 12, 11, 10, 9, 8, 0};
constexpr std::array<std::size_t, 4> LateFailureReleaseOrder{13, 11, 10, 12};
constexpr std::size_t FirstLateBlock = 9;

struct BuilderState {
    XmlMemAllocator* allocator = nullptr;
    void* userData = nullptr;
    std::array<void*, 15> blocks{};
    ParseOptions options;
    ParseOptions documentOptions;
    bool initializeFailed = false;
    bool failed = false;
    bool inProgress = false;
    bool stalledInsideTag = false;
    std::string input;
    Buffers buffers;
};

BuilderState* StateOf(const XmlHandle* self) {
    BuilderState* state = nullptr;
    std::memcpy(&state, self->impl, sizeof(state));
    return state;
}

void Release(BuilderState* state, std::size_t block) {
    if (state->blocks[block] == nullptr) return;
    state->allocator->vtable->deallocate(state->allocator, state->blocks[block], state->userData);
    state->blocks[block] = nullptr;
}

bool SameOptions(const ParseOptions& a, const ParseOptions& b) {
    return a.resolveEntity == b.resolveEntity && a.skipWhiteSpace == b.skipWhiteSpace && a.skipText == b.skipText;
}

}

int BuilderInitialize(XmlHandle* self, const XmlHandle* initializer) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Dom::DocumentBuilder::initialize: null object");
    if (self->impl != nullptr) return AlreadyInitialized;
    if (initializer == nullptr || initializer->impl == nullptr) return ErrorNotInitialized;
    const auto* parameter = static_cast<const XmlInitParameter*>(initializer->impl);
    auto* state = new BuilderState;
    state->allocator = parameter->allocator;
    state->userData = parameter->userData;
    for (std::size_t block = 0; block < BlockSizes.size(); ++block) {
        void* pointer = state->allocator->vtable->allocate(state->allocator, BlockSizes[block], state->userData);
        if (pointer != nullptr) {
            state->blocks[block] = pointer;
            continue;
        }
        if (block == 0) {
            delete state;
            return ErrorNoMemory;
        }
        if (block < FirstLateBlock) {
            for (std::size_t release = block; release-- > 0;) Release(state, release);
            delete state;
            throw std::logic_error("sce::Xml::Dom::DocumentBuilder::initialize: 13.42 faults or keeps a partial engine when allocation " +
                    std::to_string(block + 1) + " fails");
        }
        for (std::size_t release : LateFailureReleaseOrder) Release(state, release);
        state->initializeFailed = true;
        break;
    }
    std::memcpy(state->blocks[0], &state, sizeof(state));
    self->impl = state->blocks[0];
    return state->initializeFailed ? ErrorNoMemory : 0;
}

void BuilderDestroy(XmlHandle* self) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Dom::DocumentBuilder::~DocumentBuilder: null object");
    if (self->impl == nullptr) return;
    BuilderState* state = StateOf(self);
    for (std::size_t block : ReleaseOrder) Release(state, block);
    delete state;
    self->impl = nullptr;
}

int BuilderTerminate(XmlHandle* self) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Dom::DocumentBuilder::terminate: null object");
    if (self->impl == nullptr) return ErrorNotInitialized;
    BuilderDestroy(self);
    return 0;
}

int BuilderParse(XmlHandle* self, const XmlString* text, bool last) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Dom::DocumentBuilder::parse: null object");
    if (self->impl == nullptr) return ErrorNotInitialized;
    if (text == nullptr || text->buffer == nullptr || text->size == 0) return ErrorInvalidParameter;
    BuilderState* state = StateOf(self);
    if (state->initializeFailed) throw std::logic_error("sce::Xml::Dom::DocumentBuilder::parse: parsing after a failed 13.42 initialize is not measured");
    if (state->failed) throw std::logic_error("sce::Xml::Dom::DocumentBuilder::parse: the builder stopped at an unmeasured case; terminate it first");
    if (!state->inProgress) {
        state->input.clear();
        state->documentOptions = state->options;
        state->stalledInsideTag = false;
    } else {
        if (state->stalledInsideTag) {
            state->failed = true;
            throw std::logic_error("sce::Xml::Dom::DocumentBuilder::parse: 13.42 resumes a stall inside a tag differently by chunk");
        }
        if (!SameOptions(state->options, state->documentOptions)) {
            state->failed = true;
            throw std::logic_error("sce::Xml::Dom::DocumentBuilder::parse: options changed during a document");
        }
    }
    state->input.append(text->buffer, text->size);
    ParseResult result;
    try {
        result = Parse(reinterpret_cast<const std::uint8_t*>(state->input.data()), state->input.size(), state->documentOptions, last);
    } catch (...) {
        state->failed = true;
        throw;
    }
    state->buffers = std::move(result.buffers);
    state->stalledInsideTag = result.stalledInsideTag;
    state->inProgress = result.status == StatusInProgress;
    return result.status;
}

void BuilderSetOption(XmlHandle* self, BuilderOption option, bool value) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Dom::DocumentBuilder: null object");
    if (self->impl == nullptr) return;
    BuilderState* state = StateOf(self);
    switch (option) {
    case BuilderOption::ResolveEntity:
        state->options.resolveEntity = value;
        break;
    case BuilderOption::SkipIgnorableWhiteSpace:
        state->options.skipWhiteSpace = value;
        break;
    case BuilderOption::SkipIgnorableText:
        state->options.skipText = value;
        break;
    }
}

}
