#include "prx/libSceXml/src/DomParser.hpp"

#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace XmlDom {

namespace {

using Bytes = std::vector<std::uint8_t>;
using View = std::string_view;

constexpr std::size_t NotFound = View::npos;
constexpr std::size_t MaximumNameLength = 1024;
constexpr std::size_t MaximumAttributes = 128;

bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void AppendLengthPrefix(Bytes& out, std::size_t n) {
    if (n < 0xf0) {
        out.push_back(static_cast<std::uint8_t>(n));
    } else if (n < 0x10000) {
        out.insert(out.end(), {0xf0, static_cast<std::uint8_t>(n >> 8), static_cast<std::uint8_t>(n)});
    } else if (n < 0x1000000) {
        out.insert(out.end(), {0xf1, static_cast<std::uint8_t>(n >> 16), static_cast<std::uint8_t>(n >> 8), static_cast<std::uint8_t>(n)});
    } else {
        throw std::logic_error("libSceXml: a text entry of 16 MiB or more has no measured encoding");
    }
}

void AppendIndex(Bytes& out, std::size_t index) {
    if (index <= 0xef) {
        out.push_back(static_cast<std::uint8_t>(index));
    } else if (index < 0x10000) {
        out.insert(out.end(), {0xf0, static_cast<std::uint8_t>(index >> 8), static_cast<std::uint8_t>(index)});
    } else if (index < 0x1000000) {
        out.insert(out.end(), {0xf1, static_cast<std::uint8_t>(index >> 16), static_cast<std::uint8_t>(index >> 8), static_cast<std::uint8_t>(index)});
    } else {
        throw std::logic_error("libSceXml: a table offset of 16 MiB or more has no measured encoding");
    }
}

void StoreBigEndian(Bytes& out, std::size_t at, std::uint64_t value, int width) {
    for (int k = 0; k < width; ++k) out[at + k] = static_cast<std::uint8_t>(value >> (8 * (width - 1 - k)));
}

int NameHash(View name) {
    int h = 0;
    for (char c : name) h = ((h << 8) + static_cast<signed char>(c)) % 211;
    return h;
}

class NameTable {
public:
    std::uint32_t Add(View name) {
        const int hash = NameHash(name);
        auto& bucket = buckets[hash];
        for (std::uint32_t index : bucket) {
            if (PrefixMatches(name, index)) return index;
        }
        data.push_back(static_cast<std::uint8_t>(name.size()));
        const auto index = static_cast<std::uint32_t>(data.size());
        data.insert(data.end(), name.begin(), name.end());
        data.push_back(0);
        bucket.insert(bucket.begin(), index);
        undo.push_back(hash);
        return index;
    }

    std::pair<std::size_t, std::size_t> Mark() const {
        return {data.size(), undo.size()};
    }

    void Rewind(std::pair<std::size_t, std::size_t> mark) {
        while (undo.size() > mark.second) {
            auto& bucket = buckets[undo.back()];
            bucket.erase(bucket.begin());
            undo.pop_back();
        }
        data.resize(mark.first);
    }

    Bytes data;

private:
    bool PrefixMatches(View name, std::uint32_t index) const {
        for (std::size_t k = 0; k < name.size(); ++k) {
            const std::uint8_t a = static_cast<std::uint8_t>(name[k]);
            const std::uint8_t b = index + k < data.size() ? data[index + k] : 0;
            if (a != b) return false;
            if (a == 0) return true;
        }
        return true;
    }

    std::map<int, std::vector<std::uint32_t>> buckets;
    std::vector<int> undo;
};

class TextTable {
public:
    TextTable() : data{0} {}

    std::uint32_t Add(View text) {
        const auto index = static_cast<std::uint32_t>(data.size());
        AppendLengthPrefix(data, text.size());
        data.insert(data.end(), text.begin(), text.end());
        data.push_back(0);
        return index;
    }

    Bytes data;
};

struct Level {
    std::size_t owner;
    View raw;
    Bytes block;
    bool written;
    std::optional<std::size_t> previous;
    std::optional<std::size_t> stale;
};

class NodeWriter {
public:
    NodeWriter() : buffer{0x00, 0x10} {
        levels.push_back(Level{1, {}, {}, true, std::nullopt, std::nullopt});
    }

    void Leaf(std::uint8_t code, std::uint32_t index) {
        Level& level = levels.back();
        if (Link(false)) Placed(level, buffer.size());
        buffer.push_back(code);
        AppendIndex(buffer, index);
    }

    void Open(std::uint32_t name, View raw, const std::vector<std::pair<std::uint32_t, std::uint32_t>>& attributes) {
        if (name > 0xffff) throw std::logic_error("libSceXml: an element name offset above 0xffff has no measured encoding");
        Bytes block;
        if (attributes.size() == 1) {
            block.push_back(0x01);
            block.push_back(static_cast<std::uint8_t>(attributes[0].first));
            AppendIndex(block, attributes[0].second);
        } else if (!attributes.empty()) {
            block.push_back(0x21);
            block.push_back(static_cast<std::uint8_t>(attributes.size()));
            for (std::size_t k = 0; k < attributes.size(); ++k) {
                if (k != 0) block.push_back(0x01);
                AppendIndex(block, attributes[k].first);
                AppendIndex(block, attributes[k].second);
            }
        }
        Level& level = levels.back();
        if (Link(true)) Placed(level, buffer.size());
        const std::size_t position = buffer.size();
        buffer.push_back(static_cast<std::uint8_t>(0x04 | (attributes.empty() ? 0 : 0x40)));
        buffer.push_back(static_cast<std::uint8_t>(name >> 8));
        buffer.push_back(static_cast<std::uint8_t>(name));
        buffer.insert(buffer.end(), 4, 0);
        levels.push_back(Level{position, raw, std::move(block), false, std::nullopt, std::nullopt});
    }

    void Close() {
        Level level = std::move(levels.back());
        levels.pop_back();
        if (!level.written) buffer.insert(buffer.end(), level.block.begin(), level.block.end());
    }

    void Unlinked(std::uint8_t code, std::uint32_t index) {
        buffer.push_back(code);
        AppendIndex(buffer, index);
    }

    void MarkReference(bool numeric) {
        buffer[levels.back().owner] |= static_cast<std::uint8_t>(0x02 | (numeric ? 0x40 : 0));
    }

    std::size_t Depth() const {
        return levels.size() - 1;
    }

    View CurrentName() const {
        return levels.back().raw;
    }

    Bytes buffer;

private:
    bool Link(bool element) {
        Level& level = levels.back();
        if (!level.written) {
            const std::size_t size = 1 + level.block.size();
            if (size >= 0xf0 && !level.block.empty() && level.block[0] == 0x21) {
                const std::size_t field = size - 1 + (element ? 1 : 0);
                if (field > 0xffff) throw std::logic_error("libSceXml: an attribute block above 0xffff bytes has no measured encoding");
                buffer.insert(buffer.end(), {0xf0, static_cast<std::uint8_t>(field >> 8), static_cast<std::uint8_t>(field)});
                buffer.insert(buffer.end(), level.block.begin() + 2, level.block.end());
                level.stale = level.owner + 7 + 0xf0;
            } else {
                AppendIndex(buffer, size);
                buffer.insert(buffer.end(), level.block.begin(), level.block.end());
            }
            level.written = true;
            buffer[level.owner] |= 0x10;
        }
        if (level.previous) {
            const std::size_t p = *level.previous;
            if (p >= buffer.size()) throw std::logic_error("libSceXml: a sibling link past the node stream was not measured");
            buffer[p] |= 0x20;
            const int kind = buffer[p] & 0x0f;
            if (kind == 4 && p + 7 > buffer.size()) throw std::logic_error("libSceXml: a sibling length past the node stream was not measured");
            if (kind == 4) {
                StoreBigEndian(buffer, p + 3, buffer.size() - (p + 3), 4);
            } else {
                buffer.push_back(0x01);
            }
            if (kind == 6) return false;
        }
        return true;
    }

    static void Placed(Level& level, std::size_t position) {
        level.previous = position;
        if (level.stale) {
            level.previous = level.stale;
            level.stale.reset();
        }
    }

    std::vector<Level> levels;
};

std::optional<char> Predefined(View name) {
    if (name == "amp") return '&';
    if (name == "lt") return '<';
    if (name == "gt") return '>';
    if (name == "quot") return '"';
    if (name == "apos") return '\'';
    return std::nullopt;
}

enum class NumericKind { Syntax, Range, Valid };

std::pair<NumericKind, std::uint32_t> Numeric(View name) {
    View digits = name.substr(1);
    std::uint32_t base = 10;
    if (!digits.empty() && digits[0] == 'x') {
        digits = digits.substr(1);
        base = 16;
    }
    if (digits.empty()) return {NumericKind::Syntax, 0};
    std::uint32_t value = 0;
    for (char c : digits) {
        std::uint32_t digit;
        if (c >= '0' && c <= '9') {
            digit = static_cast<std::uint32_t>(c - '0');
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            digit = static_cast<std::uint32_t>(c - 'a' + 10);
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            digit = static_cast<std::uint32_t>(c - 'A' + 10);
        } else {
            return {NumericKind::Syntax, 0};
        }
        value = value * base + digit;
    }
    const bool character = value == 0x9 || value == 0xa || value == 0xd || (value >= 0x20 && value <= 0xd7ff) ||
            (value >= 0xe000 && value <= 0xfffd) || (value >= 0x10000 && value <= 0x10ffff);
    if (!character) return {NumericKind::Range, 0};
    return {NumericKind::Valid, value};
}

std::string Utf8(std::uint32_t v) {
    std::string out;
    if ((v & 0xffff) < 0x80) {
        out.push_back(static_cast<char>(v & 0xff));
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

std::optional<std::string> DecodeAttribute(View value) {
    std::string out;
    std::size_t i = 0;
    while (i < value.size()) {
        if (value[i] != '&') {
            out.push_back(value[i++]);
            continue;
        }
        const std::size_t k = value.find(';', i + 1);
        if (k == NotFound) return std::nullopt;
        const View name = value.substr(i + 1, k - i - 1);
        if (const auto c = Predefined(name)) {
            out.push_back(*c);
        } else if (!name.empty() && name[0] == '#') {
            const auto [kind, code] = Numeric(name);
            if (kind != NumericKind::Valid) return std::nullopt;
            out += Utf8(code);
        } else {
            out.append(value.substr(i, k - i + 1));
        }
        i = k + 1;
    }
    return out;
}

class Parser {
public:
    Parser(View source, const ParseOptions& options) : src(source), n(source.size()), options(options) {}

    void Run() {
        if (src.substr(0, 3) == "\xef\xbb\xbf") i = 3;
        bool skip = false;
        while (i < n && !error && !stall) {
            afterSkip = skip;
            skip = false;
            const bool top = writer.Depth() == 0;
            if (src[i] == '<' && i + 1 >= n) break;
            if (StartsWith("<!")) {
                const View rest = src.substr(i + 2, 7);
                if (!rest.empty() && rest[0] == '-' && rest.size() < 2) break;
                if (rest.substr(0, 2) != "--") {
                    if (rest.size() < 7) break;
                    if (rest != (rest[0] == '[' ? View("[CDATA[") : View("DOCTYPE"))) {
                        error = ErrorOutsideRoot;
                        break;
                    }
                }
            }
            if (StartsWith("<!--")) {
                const std::size_t j = src.find("-->", i + 4);
                if (j == NotFound) {
                    stall = ErrorOpenComment;
                    break;
                }
                i = j + 3;
                skip = true;
                if (top && rootDone) content = true;
            } else if (StartsWith("<?")) {
                const std::size_t j = src.find("?>", i + 2);
                const View body = j == NotFound ? src.substr(i + 2) : src.substr(i + 2, j + 2 - (i + 2));
                if (!options.skipText) {
                    if (top) {
                        writer.Unlinked(0x17, text.Add(body));
                    } else {
                        writer.Leaf(0x17, text.Add(body));
                    }
                }
                if (j == NotFound) {
                    stall = ErrorOpenPi;
                    break;
                }
                i = j + 2;
                skip = true;
                if (top && rootDone) content = true;
            } else if (StartsWith("<![CDATA[")) {
                if (top && !content) {
                    error = ErrorOutsideRoot;
                    break;
                }
                const std::size_t j = src.find("]]>", i + 9);
                View body = j == NotFound ? src.substr(i + 9) : src.substr(i + 9, j - (i + 9));
                if (j != NotFound && body.empty()) body = "]]>";
                if (!options.skipText) writer.Leaf(0x07, text.Add(body));
                if (j == NotFound) {
                    stall = ErrorOpenCdata;
                    break;
                }
                i = j + 3;
                skip = true;
            } else if (StartsWith("<!")) {
                if (!top || rootDone || hadDoctype) {
                    error = ErrorOutsideRoot;
                    break;
                }
                hadDoctype = true;
                std::size_t j = src.find('>', i);
                const std::size_t bracket = src.find('[', i);
                if (bracket != NotFound && (j == NotFound || bracket < j)) {
                    const std::size_t close = src.find(']', bracket);
                    j = close == NotFound ? NotFound : src.find('>', close);
                }
                const View body = j == NotFound ? src.substr(i + 9) : src.substr(i + 9, j + 1 - (i + 9));
                if (!options.skipText) writer.Unlinked(0x0f, text.Add(body));
                if (j == NotFound) {
                    stall = ErrorOpenDoctype;
                    break;
                }
                i = j + 1;
            } else if (StartsWith("</")) {
                if (top) {
                    error = content ? ErrorEndTag : ErrorOutsideRoot;
                    break;
                }
                if (!EndTag()) break;
            } else if (src[i] == '<') {
                if (top && rootDone) {
                    error = content ? ErrorElementAfterRoot : ErrorOutsideRoot;
                    break;
                }
                if (!StartTag()) break;
            } else {
                if (top && !content) {
                    if (IsSpace(src[i])) {
                        ++i;
                        continue;
                    }
                    error = ErrorOutsideRoot;
                    break;
                }
                if (!TextRun()) break;
            }
        }
    }

    ParseResult Finish(bool last) {
        ParseResult result;
        if (error) {
            result.status = *error;
        } else if (!last) {
            result.status = StatusInProgress;
        } else {
            std::optional<int> end = stall;
            if (!end && writer.Depth() > 0) end = ErrorTruncated;
            if (!end && !rootSeen) end = ErrorNoRoot;
            result.status = end ? *end : StatusComplete;
        }
        result.stalledInsideTag = stalledInsideTag;
        result.buffers.nodes = std::move(writer.buffer);
        result.buffers.text = std::move(text.data);
        result.buffers.names = std::move(names.data);
        result.buffers.attributeNames = std::move(attributeNames.data);
        return result;
    }

private:
    bool StartsWith(View prefix) const {
        return src.compare(i, prefix.size(), prefix) == 0;
    }

    bool EndTag() {
        const View want = writer.CurrentName();
        std::size_t j = i + 2;
        for (char c : want) {
            if (j >= n) {
                stall = ErrorTruncated;
                return false;
            }
            if (src[j] != c) {
                error = ErrorEndTag;
                return false;
            }
            ++j;
        }
        while (j < n && IsSpace(src[j])) ++j;
        if (j >= n) {
            stall = ErrorTruncated;
            return false;
        }
        if (src[j] != '>') {
            error = ErrorMarkup;
            return false;
        }
        writer.Close();
        i = j + 1;
        if (want.size() > MaximumNameLength) {
            error = ErrorLongName;
            return false;
        }
        if (writer.Depth() == 0) rootDone = true;
        return true;
    }

    bool StartTag() {
        const auto nameMark = names.Mark();
        const auto attributeMark = attributeNames.Mark();
        const std::size_t textMark = text.data.size();
        auto rewind = [&] {
            names.Rewind(nameMark);
            attributeNames.Rewind(attributeMark);
            text.data.resize(textMark);
        };
        std::size_t j = i + 1;
        while (j < n && !IsSpace(src[j]) && src[j] != '/' && src[j] != '>') ++j;
        if (j >= n) {
            rewind();
            if (j > i + 1) stall = ErrorTruncated;
            return false;
        }
        const View raw = src.substr(i + 1, j - i - 1);
        const std::uint32_t name = names.Add(raw.substr(0, MaximumNameLength));
        std::vector<std::pair<View, std::string>> attributes;
        std::optional<int> bad;
        bool insideTag = false;
        bool selfClose = false;
        while (true) {
            while (j < n && IsSpace(src[j])) ++j;
            if (j >= n) {
                bad = ErrorTruncated;
                break;
            }
            if (src[j] == '>') break;
            if (src[j] == '/') {
                if (j + 1 >= n) {
                    bad = ErrorTruncated;
                    break;
                }
                if (src[j + 1] != '>') {
                    bad = ErrorMarkup;
                    break;
                }
                selfClose = true;
                ++j;
                break;
            }
            std::size_t k = j;
            while (k < n && !IsSpace(src[k]) && src[k] != '=') ++k;
            const View attributeName = src.substr(j, k - j);
            while (k < n && IsSpace(src[k])) ++k;
            if (k >= n) {
                bad = ErrorTruncated;
                break;
            }
            if (src[k] != '=') {
                bad = ErrorNoEquals;
                break;
            }
            ++k;
            while (k < n && IsSpace(src[k])) ++k;
            if (k >= n) {
                bad = ErrorTruncated;
                break;
            }
            if (src[k] != '"' && src[k] != '\'') {
                bad = ErrorUnquoted;
                break;
            }
            const std::size_t e = src.find(src[k], k + 1);
            if (e == NotFound) {
                bad = ErrorTruncated;
                break;
            }
            std::string value(src.substr(k + 1, e - k - 1));
            if (options.resolveEntity) {
                auto decoded = DecodeAttribute(value);
                if (!decoded) {
                    bad = ErrorTruncated;
                    insideTag = true;
                    break;
                }
                value = std::move(*decoded);
            }
            bool duplicate = false;
            for (const auto& attribute : attributes) duplicate = duplicate || attribute.first == attributeName;
            if (duplicate) {
                bad = ErrorTruncated;
                insideTag = true;
                break;
            }
            attributes.emplace_back(attributeName, std::move(value));
            j = e + 1;
            if (j >= n) {
                bad = ErrorTruncated;
                break;
            }
            if (!IsSpace(src[j]) && src[j] != '/' && src[j] != '>') {
                bad = ErrorMarkup;
                break;
            }
        }
        if (bad) {
            rewind();
            if (*bad == ErrorTruncated) {
                stall = ErrorTruncated;
                stalledInsideTag = stalledInsideTag || insideTag;
            } else {
                error = *bad;
            }
            return false;
        }
        const bool overflow = attributes.size() > MaximumAttributes;
        if (overflow) attributes.resize(MaximumAttributes);
        std::vector<std::pair<std::uint32_t, std::uint32_t>> indices;
        for (const auto& attribute : attributes) {
            const std::uint32_t attributeIndex = attributeNames.Add(attribute.first);
            indices.emplace_back(attributeIndex, text.Add(attribute.second));
        }
        writer.Open(name, raw, indices);
        rootSeen = true;
        if (overflow) {
            error = ErrorTooManyAttributes;
            return false;
        }
        i = j + 1;
        if (selfClose) {
            writer.Close();
            if (raw.size() > MaximumNameLength) {
                error = ErrorLongName;
                return false;
            }
            if (writer.Depth() == 0) rootDone = true;
        }
        return true;
    }

    bool TextRun() {
        if (options.skipWhiteSpace || afterSkip) {
            while (i < n && IsSpace(src[i])) ++i;
        }
        std::string buffer;
        auto flush = [&] {
            if (!buffer.empty()) writer.Leaf(0x08, text.Add(buffer));
            buffer.clear();
        };
        while (i < n && src[i] != '<') {
            if (!options.resolveEntity || src[i] != '&') {
                buffer.push_back(src[i++]);
                continue;
            }
            const std::size_t k = src.find(';', i + 1);
            if (k == NotFound) {
                flush();
                stall = ErrorNoSemicolon;
                return false;
            }
            const View name = src.substr(i + 1, k - i - 1);
            if (name.empty()) {
                flush();
            } else if (const auto c = Predefined(name)) {
                buffer.push_back(*c);
                flush();
            } else {
                flush();
                if (name[0] == '#') {
                    const auto [kind, code] = Numeric(name);
                    if (kind == NumericKind::Range) {
                        error = ErrorReference;
                        return false;
                    }
                    writer.MarkReference(true);
                    if (kind == NumericKind::Syntax) {
                        writer.Leaf(0x02, text.Add(name));
                        error = ErrorReference;
                        return false;
                    }
                    writer.Leaf(0x02, text.Add(Utf8(code)));
                } else {
                    writer.MarkReference(false);
                    writer.Leaf(0x02, text.Add(name));
                }
            }
            i = k + 1;
        }
        flush();
        return true;
    }

    View src;
    std::size_t n;
    ParseOptions options;
    std::size_t i = 0;
    std::optional<int> error;
    std::optional<int> stall;
    bool stalledInsideTag = false;
    bool afterSkip = false;
    bool hadDoctype = false;
    bool rootSeen = false;
    bool rootDone = false;
    bool content = false;
    NodeWriter writer;
    TextTable text;
    NameTable names;
    NameTable attributeNames;
};

}

ParseResult Parse(const std::uint8_t* data, std::size_t size, const ParseOptions& options, bool last) {
    Parser parser(View(reinterpret_cast<const char*>(data), size), options);
    parser.Run();
    return parser.Finish(last);
}

}
