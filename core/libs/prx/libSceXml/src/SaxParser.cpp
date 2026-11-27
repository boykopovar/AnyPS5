#include "prx/libSceXml/src/SaxParser.hpp"

#include "prx/libSceXml/src/AttributeList.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace XmlSax {

namespace {

constexpr int ErrorNoMemory = static_cast<int>(0x80850001);
constexpr int ErrorNotInitialized = static_cast<int>(0x80850002);
constexpr int ErrorInvalidParameter = static_cast<int>(0x80850003);
constexpr int ErrorTruncated = static_cast<int>(0x80850008);
constexpr int ErrorEndTag = static_cast<int>(0x8085000a);
constexpr int ErrorOutsideRoot = static_cast<int>(0x8085000b);
constexpr int ErrorEndTagMarkup = static_cast<int>(0x8085000e);
constexpr int ErrorNoEquals = static_cast<int>(0x8085000f);
constexpr int ErrorOpenReference = static_cast<int>(0x80850010);
constexpr int ErrorUnquoted = static_cast<int>(0x80850011);
constexpr int ErrorOpenComment = static_cast<int>(0x80850012);
constexpr int ErrorOpenCdata = static_cast<int>(0x80850013);
constexpr int ErrorOpenDoctype = static_cast<int>(0x80850014);
constexpr int ErrorHandlerNotSet = static_cast<int>(0x80850016);
constexpr int ErrorOpenPi = static_cast<int>(0x80850017);
constexpr int ErrorNoRoot = static_cast<int>(0x80850019);
constexpr int ErrorDuplicateAttribute = static_cast<int>(0x8085001a);
constexpr int StatusInProgress = 0x00850001;
constexpr int StatusAborted = 0x00850002;
constexpr int AlreadyInitialized = 0x00850003;

constexpr std::size_t ImplSize = 0x10;
constexpr std::size_t EngineSize = 0x20;
constexpr std::size_t CoreSize = 0x168;
constexpr std::size_t AdapterSize = 0x20;

constexpr int KindDoctype = 5;
constexpr int KindProcessingInstruction = 7;
constexpr int KindCdata = 9;
constexpr int KindComment = 0xb;
constexpr int RefKindNamed = 5;

struct HandlerVtable {
    void* destructor;
    void* deletingDestructor;
    int(APS5_VABI* startDocument)(void* self, void* userData);
    int(APS5_VABI* startElement)(void* self, void* userData, const XmlString* name, const XmlHandle* attributes);
    int(APS5_VABI* characters)(void* self, void* userData, const XmlString* text);
    int(APS5_VABI* endElement)(void* self, void* userData, const XmlString* name);
    int(APS5_VABI* endDocument)(void* self, void* userData);
    int(APS5_VABI* node)(void* self, void* userData, int kind, const XmlString* text);
    int(APS5_VABI* reference)(void* self, void* userData, int kind, const XmlString* text);
    void(APS5_VABI* error)(void* self, void* userData, int status);
};

static_assert(offsetof(HandlerVtable, startDocument) == 0x10);
static_assert(offsetof(HandlerVtable, startElement) == 0x18);
static_assert(offsetof(HandlerVtable, characters) == 0x20);
static_assert(offsetof(HandlerVtable, endElement) == 0x28);
static_assert(offsetof(HandlerVtable, endDocument) == 0x30);
static_assert(offsetof(HandlerVtable, node) == 0x38);
static_assert(offsetof(HandlerVtable, reference) == 0x40);
static_assert(offsetof(HandlerVtable, error) == 0x48);

struct DocumentState {
    bool started = false;
    bool rootSeen = false;
    bool hadDoctype = false;
    bool previousWasText = false;
    std::vector<std::string> open;
    std::string pending;
};

struct ParserState {
    XmlMemAllocator* allocator = nullptr;
    void* allocatorData = nullptr;
    const XmlHandle* initializer = nullptr;
    void* implBlock = nullptr;
    void* engineBlock = nullptr;
    void* coreBlock = nullptr;
    void* adapterBlock = nullptr;
    void* handler = nullptr;
    bool handlerSet = false;
    void* userData = nullptr;
    bool skipWhiteSpace = true;
    bool resolveEntity = false;
    bool inProgress = false;
    DocumentState document;
};

ParserState* StateOf(const XmlHandle* self) {
    ParserState* state = nullptr;
    std::memcpy(&state, self->impl, sizeof(state));
    return state;
}

void* Allocate(ParserState* state, std::size_t size) {
    return state->allocator->vtable->allocate(state->allocator, size, state->allocatorData);
}

void Deallocate(ParserState* state, void* block) {
    state->allocator->vtable->deallocate(state->allocator, block, state->allocatorData);
}

const HandlerVtable& Vtable(const ParserState* state) {
    return **static_cast<const HandlerVtable* const*>(state->handler);
}

bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

XmlString Make(std::string_view text) {
    return XmlString{text.data(), text.size()};
}

[[noreturn]] void Unmeasured(const char* what) {
    throw std::logic_error(std::string("sce::Xml::Sax::Parser::parse: ") + what);
}

void ReportError(ParserState* state, int status) {
    Vtable(state).error(state->handler, state->userData, status);
}

int Abort(ParserState* state) {
    ReportError(state, StatusAborted);
    state->inProgress = false;
    state->document.pending.clear();
    return StatusAborted;
}

int Fail(ParserState* state, int status) {
    ReportError(state, status);
    state->inProgress = false;
    state->document.pending.clear();
    return status;
}

struct Tag {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    bool selfClosing = false;
};

std::size_t SkipSpace(std::string_view in, std::size_t at) {
    while (at < in.size() && IsSpace(in[at])) ++at;
    return at;
}

std::optional<char> PredefinedEntity(std::string_view name) {
    if (name == "amp") return '&';
    if (name == "lt") return '<';
    if (name == "gt") return '>';
    if (name == "quot") return '"';
    if (name == "apos") return '\'';
    return std::nullopt;
}

enum class NumericKind { Syntax, Range, Valid };

std::pair<NumericKind, std::uint32_t> DecodeNumeric(std::string_view name) {
    std::string_view digits = name.substr(1);
    std::uint32_t base = 10;
    if (!digits.empty() && digits[0] == 'x') {
        digits = digits.substr(1);
        base = 16;
    }
    if (digits.empty()) return {NumericKind::Syntax, 0};
    std::uint32_t value = 0;
    for (char c : digits) {
        std::uint32_t digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<std::uint32_t>(c - '0');
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            digit = static_cast<std::uint32_t>(c - 'a' + 10);
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            digit = static_cast<std::uint32_t>(c - 'A' + 10);
        } else {
            return {NumericKind::Syntax, 0};
        }
        if (value <= 0x10ffffu) {
            value = value * base + digit;
        }
    }
    const bool valid = value == 0x9 || value == 0xa || value == 0xd || (value >= 0x20 && value <= 0xd7ff) ||
            (value >= 0xe000 && value <= 0xfffd) || (value >= 0x10000 && value <= 0x10ffff);
    if (!valid) return {NumericKind::Range, 0};
    return {NumericKind::Valid, value};
}

std::string EncodeUtf8(std::uint32_t v) {
    std::string out;
    if (v < 0x80) {
        out.push_back(static_cast<char>(v));
    } else if (v < 0x800) {
        out.push_back(static_cast<char>(0xc0 | (v >> 6)));
        out.push_back(static_cast<char>(0x80 | (v & 0x3f)));
    } else if (v < 0x10000) {
        out.push_back(static_cast<char>(0xe0 | (v >> 12)));
        out.push_back(static_cast<char>(0x80 | ((v >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (v & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (v >> 18)));
        out.push_back(static_cast<char>(0x80 | ((v >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((v >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (v & 0x3f)));
    }
    return out;
}

bool DecodeAttributeValue(std::string_view value, std::string& out, int& status) {
    out.clear();
    std::size_t i = 0;
    while (i < value.size()) {
        if (value[i] != '&') {
            out.push_back(value[i++]);
            continue;
        }
        const std::size_t k = value.find(';', i + 1);
        if (k == std::string_view::npos) {
            status = ErrorOpenReference;
            return false;
        }
        const std::string_view name = value.substr(i + 1, k - i - 1);
        if (const auto c = PredefinedEntity(name)) {
            out.push_back(*c);
        } else if (!name.empty() && name[0] == '#') {
            const auto [kind, code] = DecodeNumeric(name);
            if (kind != NumericKind::Valid) {
                status = ErrorOutsideRoot;
                return false;
            }
            out += EncodeUtf8(code);
        } else {
            out.append(value.substr(i, k - i + 1));
        }
        i = k + 1;
    }
    return true;
}

bool StartsWithPrefix(std::string_view text, std::string_view target) {
    return target.size() >= text.size() && target.compare(0, text.size(), text) == 0;
}

std::size_t FindDoctypeEnd(std::string_view rest) {
    std::size_t close = rest.find('>', 9);
    const std::size_t bracket = rest.find('[', 9);
    if (bracket != std::string_view::npos && (close == std::string_view::npos || bracket < close)) {
        const std::size_t endBracket = rest.find(']', bracket + 1);
        if (endBracket == std::string_view::npos) return std::string_view::npos;
        close = rest.find('>', endBracket + 1);
    }
    return close;
}

std::size_t ReadStartTag(std::string_view in, std::size_t at, bool resolveEntity, Tag& tag, int& status, bool& incomplete) {
    incomplete = false;
    std::size_t end = at;
    while (end < in.size() && in[end] != '/' && in[end] != '>' && !IsSpace(in[end])) ++end;
    if (end >= in.size()) {
        incomplete = true;
        return at;
    }
    if (end == at) {
        status = ErrorEndTagMarkup;
        return at;
    }
    tag.name.assign(in.substr(at, end - at));
    at = end;
    status = 0;
    for (;;) {
        const std::size_t afterSpace = SkipSpace(in, at);
        if (afterSpace >= in.size()) {
            incomplete = true;
            return at;
        }
        const char c = in[afterSpace];
        if (c == '>') return afterSpace + 1;
        if (c == '/') {
            if (afterSpace + 1 >= in.size()) {
                incomplete = true;
                return at;
            }
            if (in[afterSpace + 1] != '>') {
                status = ErrorEndTagMarkup;
                return afterSpace;
            }
            tag.selfClosing = true;
            return afterSpace + 2;
        }
        if (afterSpace == at && !tag.attributes.empty()) {
            status = ErrorEndTagMarkup;
            return at;
        }
        std::size_t nameEnd = afterSpace;
        while (nameEnd < in.size() && in[nameEnd] != '=' && !IsSpace(in[nameEnd])) ++nameEnd;
        if (nameEnd >= in.size()) {
            incomplete = true;
            return at;
        }
        const std::string_view attributeName = in.substr(afterSpace, nameEnd - afterSpace);
        at = SkipSpace(in, nameEnd);
        if (at >= in.size()) {
            incomplete = true;
            return at;
        }
        if (in[at] != '=') {
            status = ErrorNoEquals;
            return at;
        }
        at = SkipSpace(in, at + 1);
        if (at >= in.size()) {
            incomplete = true;
            return at;
        }
        if (in[at] != '"' && in[at] != '\'') {
            status = ErrorUnquoted;
            return at;
        }
        const char quote = in[at];
        const std::size_t close = in.find(quote, at + 1);
        if (close == std::string_view::npos) {
            incomplete = true;
            return at;
        }
        const std::string_view rawValue = in.substr(at + 1, close - at - 1);
        std::string decodedValue;
        if (resolveEntity && rawValue.find('&') != std::string_view::npos) {
            if (!DecodeAttributeValue(rawValue, decodedValue, status)) {
                return at;
            }
        } else {
            decodedValue.assign(rawValue);
        }
        for (const auto& existing : tag.attributes) {
            if (existing.first == attributeName) {
                status = ErrorDuplicateAttribute;
                return close + 1;
            }
        }
        tag.attributes.emplace_back(std::string(attributeName), std::move(decodedValue));
        at = close + 1;
    }
}

bool CallNode(ParserState* state, int kind, std::string_view text) {
    const XmlString string = Make(text);
    return Vtable(state).node(state->handler, state->userData, kind, &string) != 0;
}

bool CallReference(ParserState* state, int kind, std::string_view text) {
    const XmlString string = Make(text);
    return Vtable(state).reference(state->handler, state->userData, kind, &string) != 0;
}

bool CallStartElement(ParserState* state, const Tag& tag) {
    XmlHandle list{nullptr};
    const int status = XmlDom::ContainerInitialize(&list, state->initializer);
    if (status == 0) {
        for (const auto& attribute : tag.attributes) {
            const XmlString name = Make(attribute.first);
            const XmlString value = Make(attribute.second);
            XmlDom::ContainerAddAttribute(&list, &name, &value);
        }
    }
    const XmlString name = Make(tag.name);
    const int result = Vtable(state).startElement(state->handler, state->userData, &name, &list);
    XmlDom::ContainerTerminate(&list);
    return result != 0;
}

bool CallEndElement(ParserState* state, std::string_view name) {
    const XmlString string = Make(name);
    return Vtable(state).endElement(state->handler, state->userData, &string) != 0;
}

bool CallCharacters(ParserState* state, std::string_view text) {
    const XmlString string = Make(text);
    return Vtable(state).characters(state->handler, state->userData, &string) != 0;
}

}

int ParserInitialize(XmlHandle* self, const XmlHandle* initializer) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Sax::Parser::initialize: null object");
    if (self->impl != nullptr) return AlreadyInitialized;
    if (initializer == nullptr || initializer->impl == nullptr) return ErrorNotInitialized;
    const auto* parameter = static_cast<const XmlInitParameter*>(initializer->impl);
    if (parameter->allocator == nullptr) throw std::invalid_argument("sce::Xml::Sax::Parser::initialize: null allocator");
    auto state = std::make_unique<ParserState>();
    state->allocator = parameter->allocator;
    state->allocatorData = parameter->userData;
    state->initializer = initializer;
    state->implBlock = Allocate(state.get(), ImplSize);
    if (state->implBlock == nullptr) return ErrorNoMemory;
    state->engineBlock = Allocate(state.get(), EngineSize);
    if (state->engineBlock == nullptr) {
        Deallocate(state.get(), state->implBlock);
        throw std::logic_error("sce::Xml::Sax::Parser::initialize: the 13.42 module leaves the object pointing at the released block when the second allocation fails");
    }
    state->coreBlock = Allocate(state.get(), CoreSize);
    if (state->coreBlock == nullptr) throw std::logic_error("sce::Xml::Sax::Parser::initialize: a failure of the third allocation leaves a partial engine that is not measured");
    ParserState* owned = state.release();
    std::memcpy(owned->implBlock, &owned, sizeof(owned));
    self->impl = owned->implBlock;
    return 0;
}

void ParserDestroy(XmlHandle* self) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Sax::Parser::~Parser: null object");
    if (self->impl == nullptr) return;
    ParserState* state = StateOf(self);
    if (state->adapterBlock != nullptr) Deallocate(state, state->adapterBlock);
    Deallocate(state, state->coreBlock);
    Deallocate(state, state->engineBlock);
    Deallocate(state, state->implBlock);
    delete state;
    self->impl = nullptr;
}

int ParserTerminate(XmlHandle* self) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Sax::Parser::terminate: null object");
    if (self->impl == nullptr) return ErrorNotInitialized;
    ParserDestroy(self);
    return 0;
}

int ParserReset(XmlHandle* self) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Sax::Parser::reset: null object");
    if (self->impl == nullptr) return ErrorNotInitialized;
    ParserState* state = StateOf(self);
    state->document = DocumentState{};
    state->inProgress = false;
    return 0;
}

int ParserSetDocumentHandler(XmlHandle* self, void* handler) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Sax::Parser::setDocumentHandler: null object");
    if (self->impl == nullptr) throw std::logic_error("sce::Xml::Sax::Parser::setDocumentHandler: the 13.42 module returns without setting a result before initialize");
    ParserState* state = StateOf(self);
    if (state->adapterBlock != nullptr) {
        Deallocate(state, state->adapterBlock);
        state->adapterBlock = nullptr;
    }
    state->adapterBlock = Allocate(state, AdapterSize);
    if (state->adapterBlock == nullptr) throw std::logic_error("sce::Xml::Sax::Parser::setDocumentHandler: the 13.42 module keeps the released adapter when its allocation fails");
    state->handler = handler;
    state->handlerSet = true;
    return 0;
}

void ParserSetUserData(XmlHandle* self, void* userData) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Sax::Parser::setUserData: null object");
    if (self->impl == nullptr) return;
    StateOf(self)->userData = userData;
}

void ParserSetOption(XmlHandle* self, ParserOption option, bool value) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Sax::Parser: null object");
    if (self->impl == nullptr) return;
    ParserState* state = StateOf(self);
    if (option == ParserOption::SkipIgnorableWhiteSpace) {
        state->skipWhiteSpace = value;
    } else {
        state->resolveEntity = value;
    }
}

int ParserParse(XmlHandle* self, const XmlString* text, bool last) {
    if (self == nullptr) throw std::invalid_argument("sce::Xml::Sax::Parser::parse: null object");
    if (self->impl == nullptr) return ErrorNotInitialized;
    if (text == nullptr || text->buffer == nullptr || text->size == 0) return ErrorInvalidParameter;
    ParserState* state = StateOf(self);
    if (!state->inProgress) state->document = DocumentState{};
    if (!state->handlerSet) {
        state->inProgress = false;
        return ErrorHandlerNotSet;
    }
    if (state->handler == nullptr) Unmeasured("a null document handler is dereferenced by the 13.42 module and is not measured");

    DocumentState& doc = state->document;
    std::string combined;
    std::string_view in(text->buffer, text->size);
    if (!doc.pending.empty()) {
        combined = std::move(doc.pending);
        doc.pending.clear();
        combined.append(in);
        in = combined;
    }
    if (!doc.started) {
        const bool partialBom1 = in.size() == 1 && static_cast<unsigned char>(in[0]) == 0xef;
        const bool partialBom2 = in.size() == 2 && static_cast<unsigned char>(in[0]) == 0xef &&
                                 static_cast<unsigned char>(in[1]) == 0xbb;
        if (!last && (partialBom1 || partialBom2)) {
            doc.pending.assign(in);
            state->inProgress = true;
            return StatusInProgress;
        }
        if (in.size() >= 3 && static_cast<unsigned char>(in[0]) == 0xef &&
                static_cast<unsigned char>(in[1]) == 0xbb && static_cast<unsigned char>(in[2]) == 0xbf) {
            in.remove_prefix(3);
        }
    }

    auto needMore = [&](std::size_t from, int eofStatus) -> int {
        if (!last) {
            doc.pending.assign(in.substr(from));
            state->inProgress = true;
            return StatusInProgress;
        }
        return Fail(state, eofStatus);
    };

    std::size_t at = 0;
    if (!doc.started) {
        at = SkipSpace(in, at);
        doc.started = true;
        if (Vtable(state).startDocument(state->handler, state->userData) != 0) return Abort(state);
    }

    for (;;) {
        if (doc.open.empty() || (state->skipWhiteSpace && !doc.previousWasText)) at = SkipSpace(in, at);
        if (at >= in.size()) break;
        const char c = in[at];
        if (c != '<') {
            if (doc.open.empty()) return Fail(state, ErrorOutsideRoot);
            if (c == '&' && state->resolveEntity) {
                const std::size_t semi = in.find(';', at + 1);
                if (semi == std::string_view::npos) return needMore(at, ErrorOpenReference);
                const std::string_view refName = in.substr(at + 1, semi - at - 1);
                if (!refName.empty()) {
                    if (const auto ch = PredefinedEntity(refName)) {
                        const char decoded = *ch;
                        if (CallCharacters(state, std::string_view(&decoded, 1))) return Abort(state);
                    } else if (refName[0] == '#') {
                        const auto [kind, code] = DecodeNumeric(refName);
                        if (kind != NumericKind::Valid) return Fail(state, ErrorOutsideRoot);
                        const std::string utf8 = EncodeUtf8(code);
                        if (CallCharacters(state, utf8)) return Abort(state);
                    } else {
                        if (CallReference(state, RefKindNamed, refName)) return Abort(state);
                    }
                }
                doc.previousWasText = true;
                at = semi + 1;
                continue;
            }
            std::size_t end = at;
            while (end < in.size() && in[end] != '<' && !(in[end] == '&' && state->resolveEntity)) ++end;
            if (CallCharacters(state, in.substr(at, end - at))) return Abort(state);
            doc.previousWasText = true;
            at = end;
            continue;
        }
        doc.previousWasText = false;
        const std::string_view rest = in.substr(at);
        if (rest.size() == 1) return needMore(at, ErrorTruncated);
        if (rest[1] == '!') {
            if (rest.size() == 2 || (rest.size() == 3 && rest[2] == '-')) return needMore(at, ErrorTruncated);
            if (rest.substr(0, 4) == "<!--") {
                const std::size_t close = rest.find("-->", 4);
                if (close == std::string_view::npos) return needMore(at, ErrorOpenComment);
                if (CallNode(state, KindComment, rest.substr(4, close + 3 - 4))) return Abort(state);
                at += close + 3;
            } else if (rest.size() < 9) {
                if (StartsWithPrefix(rest, "<![CDATA[") || StartsWithPrefix(rest, "<!DOCTYPE")) {
                    return needMore(at, ErrorTruncated);
                }
                return Fail(state, ErrorOutsideRoot);
            } else if (rest.substr(0, 9) == "<![CDATA[") {
                if (doc.open.empty()) return Fail(state, ErrorOutsideRoot);
                const std::size_t close = rest.find("]]>", 9);
                if (close == std::string_view::npos) return needMore(at, ErrorOpenCdata);
                if (CallNode(state, KindCdata, rest.substr(9, close + 3 - 9))) return Abort(state);
                at += close + 3;
            } else if (rest.substr(0, 9) == "<!DOCTYPE") {
                if (!doc.open.empty() || doc.rootSeen || doc.hadDoctype) return Fail(state, ErrorOutsideRoot);
                const std::size_t close = FindDoctypeEnd(rest);
                if (close == std::string_view::npos) return needMore(at, ErrorOpenDoctype);
                doc.hadDoctype = true;
                if (CallNode(state, KindDoctype, rest.substr(9, close + 1 - 9))) return Abort(state);
                at += close + 1;
            } else {
                return Fail(state, ErrorOutsideRoot);
            }
        } else if (rest.substr(0, 2) == "<?") {
            const std::size_t close = rest.find("?>", 2);
            if (close == std::string_view::npos) return needMore(at, ErrorOpenPi);
            if (CallNode(state, KindProcessingInstruction, rest.substr(2, close))) return Abort(state);
            at += close + 2;
        } else if (rest.substr(0, 2) == "</") {
            if (doc.open.empty()) return Fail(state, ErrorOutsideRoot);
            const std::string& expected = doc.open.back();
            std::size_t position = at + 2;
            const std::size_t avail = in.size() - position;
            const std::size_t cmpLen = avail < expected.size() ? avail : expected.size();
            if (in.compare(position, cmpLen, expected.data(), cmpLen) != 0) return Fail(state, ErrorEndTag);
            if (avail < expected.size()) return needMore(at, ErrorTruncated);
            position = SkipSpace(in, position + expected.size());
            if (position >= in.size()) return needMore(at, ErrorTruncated);
            if (in[position] != '>') return Fail(state, ErrorEndTagMarkup);
            const std::string closed = expected;
            doc.open.pop_back();
            if (CallEndElement(state, closed)) return Abort(state);
            at = position + 1;
        } else {
            Tag tag;
            int tagStatus = 0;
            bool incomplete = false;
            const std::size_t end = ReadStartTag(in, at + 1, state->resolveEntity, tag, tagStatus, incomplete);
            if (incomplete) return needMore(at, ErrorTruncated);
            if (tagStatus != 0) return Fail(state, tagStatus);
            if (doc.open.empty()) {
                if (doc.rootSeen) return Fail(state, ErrorOutsideRoot);
                doc.rootSeen = true;
            }
            if (CallStartElement(state, tag)) return Abort(state);
            if (tag.selfClosing) {
                if (CallEndElement(state, tag.name)) return Abort(state);
            } else {
                doc.open.emplace_back(std::move(tag.name));
            }
            at = end;
        }
    }

    if (!last) {
        state->inProgress = true;
        return StatusInProgress;
    }
    if (!doc.open.empty()) return Fail(state, ErrorTruncated);
    if (!doc.rootSeen) return Fail(state, ErrorNoRoot);
    if (Vtable(state).endDocument(state->handler, state->userData) != 0) return Abort(state);
    state->inProgress = false;
    return 0;
}

}
