#include "State.hpp"
#include <glslang/Public/ShaderLang.h>
#include <limits>
#include <mutex>

using namespace Slim;

namespace {

constexpr std::size_t MaxVertexAttribs = 8;
constexpr std::size_t MaxVertexUniformVectors = 128;
constexpr std::size_t MaxVaryingVectors = 8;
constexpr std::size_t MaxFragmentUniformVectors = 16;
constexpr std::size_t MaxVertexTextureImageUnits = 0;
constexpr std::size_t MaxFragmentTextureImageUnits = 8;
constexpr std::size_t MaxCombinedTextureImageUnits = 8;

bool EnsureGlslang() {
    static std::once_flag once;
    static bool initializedGlslang = false;
    std::call_once(once, [] { initializedGlslang = glslang::InitializeProcess(); });
    return initializedGlslang;
}

template <class T>
void SetDualSourceDrawBuffers(T& r) {
    if constexpr (requires { r.maxDualSourceDrawBuffersEXT = 1; }) {
        r.maxDualSourceDrawBuffersEXT = 1;
    }
}

TBuiltInResource MakeResources() {
    TBuiltInResource r{};
    r.maxLights = 32;
    r.maxClipPlanes = 6;
    r.maxTextureUnits = 8;
    r.maxTextureCoords = 8;
    r.maxVertexAttribs = static_cast<int>(MaxVertexAttribs);
    r.maxVertexUniformComponents = static_cast<int>(MaxVertexUniformVectors * 4);
    r.maxVaryingFloats = static_cast<int>(MaxVaryingVectors * 4);
    r.maxVertexTextureImageUnits = static_cast<int>(MaxVertexTextureImageUnits);
    r.maxCombinedTextureImageUnits = static_cast<int>(MaxCombinedTextureImageUnits);
    r.maxTextureImageUnits = static_cast<int>(MaxFragmentTextureImageUnits);
    r.maxFragmentUniformComponents = static_cast<int>(MaxFragmentUniformVectors * 4);
    r.maxDrawBuffers = 1;
    r.maxVertexUniformVectors = static_cast<int>(MaxVertexUniformVectors);
    r.maxVaryingVectors = static_cast<int>(MaxVaryingVectors);
    r.maxFragmentUniformVectors = static_cast<int>(MaxFragmentUniformVectors);
    r.maxVertexOutputVectors = static_cast<int>(MaxVaryingVectors);
    r.maxFragmentInputVectors = static_cast<int>(MaxVaryingVectors);
    r.minProgramTexelOffset = -8;
    r.maxProgramTexelOffset = 7;
    r.maxClipDistances = 8;
    r.maxComputeWorkGroupCountX = 65535;
    r.maxComputeWorkGroupCountY = 65535;
    r.maxComputeWorkGroupCountZ = 65535;
    r.maxComputeWorkGroupSizeX = 1024;
    r.maxComputeWorkGroupSizeY = 1024;
    r.maxComputeWorkGroupSizeZ = 64;
    r.maxComputeUniformComponents = 1024;
    r.maxComputeTextureImageUnits = 16;
    r.maxComputeImageUniforms = 8;
    r.maxComputeAtomicCounters = 8;
    r.maxComputeAtomicCounterBuffers = 1;
    r.maxVaryingComponents = static_cast<int>(MaxVaryingVectors * 4);
    r.maxVertexOutputComponents = static_cast<int>(MaxVaryingVectors * 4);
    r.maxGeometryInputComponents = 64;
    r.maxGeometryOutputComponents = 128;
    r.maxFragmentInputComponents = static_cast<int>(MaxVaryingVectors * 4);
    r.maxImageUnits = 8;
    r.maxCombinedImageUnitsAndFragmentOutputs = 8;
    r.maxCombinedShaderOutputResources = 8;
    r.maxImageSamples = 0;
    r.maxVertexImageUniforms = 0;
    r.maxTessControlImageUniforms = 0;
    r.maxTessEvaluationImageUniforms = 0;
    r.maxGeometryImageUniforms = 0;
    r.maxFragmentImageUniforms = 8;
    r.maxCombinedImageUniforms = 8;
    r.maxGeometryTextureImageUnits = 16;
    r.maxGeometryOutputVertices = 256;
    r.maxGeometryTotalOutputComponents = 1024;
    r.maxGeometryUniformComponents = 1024;
    r.maxGeometryVaryingComponents = 64;
    r.maxTessControlInputComponents = 128;
    r.maxTessControlOutputComponents = 128;
    r.maxTessControlTextureImageUnits = 16;
    r.maxTessControlUniformComponents = 1024;
    r.maxTessControlTotalOutputComponents = 4096;
    r.maxTessEvaluationInputComponents = 128;
    r.maxTessEvaluationOutputComponents = 128;
    r.maxTessEvaluationTextureImageUnits = 16;
    r.maxTessEvaluationUniformComponents = 1024;
    r.maxTessPatchComponents = 120;
    r.maxPatchVertices = 32;
    r.maxTessGenLevel = 64;
    r.maxViewports = 16;
    r.maxVertexAtomicCounters = 0;
    r.maxTessControlAtomicCounters = 0;
    r.maxTessEvaluationAtomicCounters = 0;
    r.maxGeometryAtomicCounters = 0;
    r.maxFragmentAtomicCounters = 8;
    r.maxCombinedAtomicCounters = 8;
    r.maxAtomicCounterBindings = 1;
    r.maxVertexAtomicCounterBuffers = 0;
    r.maxTessControlAtomicCounterBuffers = 0;
    r.maxTessEvaluationAtomicCounterBuffers = 0;
    r.maxGeometryAtomicCounterBuffers = 0;
    r.maxFragmentAtomicCounterBuffers = 1;
    r.maxCombinedAtomicCounterBuffers = 1;
    r.maxAtomicCounterBufferSize = 16384;
    r.maxTransformFeedbackBuffers = 4;
    r.maxTransformFeedbackInterleavedComponents = 64;
    r.maxCullDistances = 8;
    r.maxCombinedClipAndCullDistances = 8;
    r.maxSamples = 4;
    SetDualSourceDrawBuffers(r);
    r.limits.nonInductiveForLoops = true;
    r.limits.whileLoops = true;
    r.limits.doWhileLoops = true;
    r.limits.generalUniformIndexing = true;
    r.limits.generalAttributeMatrixVectorIndexing = true;
    r.limits.generalVaryingIndexing = true;
    r.limits.generalSamplerIndexing = true;
    r.limits.generalVariableIndexing = true;
    r.limits.generalConstantMatrixVectorIndexing = true;
    return r;
}

bool IsHorizontalSpace(char ch) {
    return ch == ' ' || ch == '\t';
}

bool IsWhitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

bool IsIdentStart(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

bool IsIdentChar(char ch) {
    return IsIdentStart(ch) || (ch >= '0' && ch <= '9');
}

bool CheckEssl100Source(const std::string& source, std::string& error) {
    if (source.find('\0') != std::string::npos) {
        error = "ERROR: 0:1: '' : unexpected null character in shader source\n";
        return false;
    }
    std::size_t i = 0;
    const std::size_t n = source.size();
    while (i < n) {
        while (i < n && IsWhitespace(source[i])) ++i;
        if (i + 1 < n && source[i] == '/' && source[i + 1] == '/') {
            i += 2;
            while (i < n && source[i] != '\n' && source[i] != '\r') {
                if (source[i] == '\\' && i + 1 < n) {
                    ++i;
                    if (source[i] == '\r' && i + 1 < n && source[i + 1] == '\n') ++i;
                }
                ++i;
            }
            continue;
        }
        if (i + 1 < n && source[i] == '/' && source[i + 1] == '*') {
            i += 2;
            bool closed = false;
            while (i + 1 < n) {
                if (source[i] == '*' && source[i + 1] == '/') {
                    i += 2;
                    closed = true;
                    break;
                }
                ++i;
            }
            if (!closed) return true;
            continue;
        }
        break;
    }
    auto skipDirectiveSpaces = [&]() {
        while (i < n) {
            if (IsHorizontalSpace(source[i])) {
                ++i;
                continue;
            }
            if (i + 1 < n && source[i] == '/' && source[i + 1] == '*') {
                std::size_t j = i + 2;
                bool closed = false;
                while (j + 1 < n) {
                    if (source[j] == '\n' || source[j] == '\r') return;
                    if (source[j] == '*' && source[j + 1] == '/') {
                        i = j + 2;
                        closed = true;
                        break;
                    }
                    ++j;
                }
                if (!closed) return;
                continue;
            }
            break;
        }
    };
    if (i >= n || source[i] != '#') return true;
    ++i;
    skipDirectiveSpaces();
    const std::size_t dirStart = i;
    while (i < n && IsIdentChar(source[i])) ++i;
    if (source.compare(dirStart, i - dirStart, "version") != 0) return true;
    skipDirectiveSpaces();
    const std::size_t verStart = i;
    while (i < n && source[i] >= '0' && source[i] <= '9') ++i;
    if (i == verStart) return true;
    if (source.compare(verStart, i - verStart, "100") != 0) {
        error = "ERROR: 0:1: '#version' : unsupported version; OpenGL ES 2.0 supports only version 100\n";
        return false;
    }
    skipDirectiveSpaces();
    if (i < n && IsIdentStart(source[i])) {
        error = "ERROR: 0:1: '#version' : profile qualifiers are not supported in GLSL ES 1.00\n";
        return false;
    }
    return true;
}

bool ParseEssl100(EShLanguage stage, const std::string& source, glslang::TShader& shader, std::string& log) {
    if (!EnsureGlslang()) {
        log = "ERROR: shader compiler initialization failed\n";
        return false;
    }
    if (!CheckEssl100Source(source, log)) return false;
    if (source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        log = "ERROR: shader source exceeds compiler limit\n";
        return false;
    }
    const TBuiltInResource resources = MakeResources();
    const char* data = source.c_str();
    const int length = static_cast<int>(source.size());
    shader.setStringsWithLengths(&data, &length, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientNone, 0);
    shader.setEnvClient(glslang::EShClientNone, static_cast<glslang::EShTargetClientVersion>(0));
    shader.setEnvTarget(glslang::EShTargetNone, static_cast<glslang::EShTargetLanguageVersion>(0));
    const bool ok = shader.parse(&resources, 100, EEsProfile, false, false, EShMsgDefault);
    log = shader.getInfoLog();
    return ok;
}

GLuint AttributeSlotCount(GLenum type) {
    switch (type) {
    case 0x1406: case 0x8b50: case 0x8b51: case 0x8b52: return 1;
    case 0x8b5a: return 2;
    case 0x8b5b: return 3;
    case 0x8b5c: return 4;
    default: return 0;
    }
}

std::size_t UniformComponentCount(GLenum type) {
    switch (type) {
    case 0x1406: case 0x1404: case 0x8b56: case 0x8b5e: case 0x8b60: return 1;
    case 0x8b50: case 0x8b53: case 0x8b57: return 2;
    case 0x8b51: case 0x8b54: case 0x8b58: return 3;
    case 0x8b52: case 0x8b55: case 0x8b59: return 4;
    case 0x8b5a: return 4;
    case 0x8b5b: return 9;
    case 0x8b5c: return 16;
    default: return 0;
    }
}

bool IsFloatBackedUniform(GLenum type) {
    switch (type) {
    case 0x1406: case 0x8b50: case 0x8b51: case 0x8b52:
    case 0x8b5a: case 0x8b5b: case 0x8b5c:
        return true;
    default:
        return false;
    }
}

bool IsBoolUniform(GLenum type) {
    return type >= 0x8b56 && type <= 0x8b59;
}

bool IsSamplerUniform(GLenum type) {
    return type == 0x8b5e || type == 0x8b60;
}

struct PackingItem {
    std::size_t width = 0;
    std::size_t rows = 0;
};

PackingItem TypePacking(GLenum type, GLint count) {
    const auto elems = static_cast<std::size_t>(std::max(count, 1));
    switch (type) {
    case 0x1406: case 0x1404: case 0x8b56: return {1, elems};
    case 0x8b50: case 0x8b53: case 0x8b57: return {2, elems};
    case 0x8b51: case 0x8b54: case 0x8b58: return {3, elems};
    case 0x8b52: case 0x8b55: case 0x8b59: return {4, elems};
    case 0x8b5a: return {4, elems * 2};
    case 0x8b5b: return {3, elems * 3};
    case 0x8b5c: return {4, elems * 4};
    default: return {0, 0};
    }
}

bool FitsVectorBudget(std::vector<PackingItem> items, std::size_t maxRows) {
    std::sort(items.begin(), items.end(), [](const PackingItem& a, const PackingItem& b) {
        if (a.width != b.width) return a.width > b.width;
        return a.rows > b.rows;
    });
    std::vector<std::array<bool, 4>> grid(maxRows, std::array<bool, 4>{false, false, false, false});
    auto tryPlaceAt = [&](std::size_t row, std::size_t col, const PackingItem& item) {
        for (std::size_t r = 0; r < item.rows; ++r) {
            for (std::size_t c = 0; c < item.width; ++c) {
                if (grid[row + r][col + c]) return false;
            }
        }
        for (std::size_t r = 0; r < item.rows; ++r) {
            for (std::size_t c = 0; c < item.width; ++c) grid[row + r][col + c] = true;
        }
        return true;
    };
    auto placeTopDown = [&](std::size_t col, const PackingItem& item) {
        for (std::size_t row = 0; row + item.rows <= maxRows; ++row) {
            if (tryPlaceAt(row, col, item)) return true;
        }
        return false;
    };
    auto placeBottomUp = [&](std::size_t col, const PackingItem& item) {
        for (std::size_t offset = 0; offset <= maxRows - item.rows; ++offset) {
            const std::size_t row = maxRows - item.rows - offset;
            if (tryPlaceAt(row, col, item)) return true;
        }
        return false;
    };
    auto placeBestFitSingleColumn = [&](const PackingItem& item) {
        bool found = false;
        std::size_t bestCol = 0;
        std::size_t bestRow = 0;
        std::size_t bestLeftover = maxRows + 1;
        for (std::size_t col = 0; col < 4; ++col) {
            std::size_t row = 0;
            while (row < maxRows) {
                while (row < maxRows && grid[row][col]) ++row;
                const std::size_t runStart = row;
                while (row < maxRows && !grid[row][col]) ++row;
                const std::size_t runLen = row - runStart;
                if (runLen >= item.rows) {
                    const std::size_t leftover = runLen - item.rows;
                    if (!found || leftover < bestLeftover) {
                        found = true;
                        bestCol = col;
                        bestRow = runStart;
                        bestLeftover = leftover;
                    }
                }
            }
        }
        if (!found) return false;
        return tryPlaceAt(bestRow, bestCol, item);
    };
    for (const auto& item : items) {
        if (item.width == 0 || item.rows == 0) continue;
        if (item.rows > maxRows) return false;
        bool placed = false;
        if (item.width == 4 || item.width == 3) {
            placed = placeTopDown(0, item);
        } else if (item.width == 2) {
            placed = placeTopDown(0, item) || placeBottomUp(2, item);
        } else if (item.width == 1) {
            placed = placeBestFitSingleColumn(item);
        }
        if (!placed) return false;
    }
    return true;
}

bool CheckVaryings(const std::string& vertexSource, const std::string& fragmentSource, std::string& error) {
    glslang::TShader vertexShader(EShLangVertex);
    std::string stageLog;
    if (!ParseEssl100(EShLangVertex, vertexSource, vertexShader, stageLog)) {
        error = stageLog;
        return false;
    }
    glslang::TProgram vertexProgram;
    vertexProgram.addShader(&vertexShader);
    if (!vertexProgram.link(EShMsgDefault) ||
        !vertexProgram.buildReflection(EShReflectionIntermediateIO | EShReflectionAllIOVariables)) {
        error = vertexProgram.getInfoLog();
        return false;
    }
    std::set<std::string> vertexOutputs;
    for (int i = 0; i < vertexProgram.getNumPipeOutputs(); ++i) {
        const auto& out = vertexProgram.getPipeOutput(i);
        if (!out.name.starts_with("gl_")) vertexOutputs.insert(out.name);
    }

    glslang::TShader fragmentShader(EShLangFragment);
    if (!ParseEssl100(EShLangFragment, fragmentSource, fragmentShader, stageLog)) {
        error = stageLog;
        return false;
    }
    glslang::TProgram fragmentProgram;
    fragmentProgram.addShader(&fragmentShader);
    if (!fragmentProgram.link(EShMsgDefault) ||
        !fragmentProgram.buildReflection(EShReflectionIntermediateIO)) {
        error = fragmentProgram.getInfoLog();
        return false;
    }
    std::vector<PackingItem> varyingItems;
    for (int i = 0; i < fragmentProgram.getNumPipeInputs(); ++i) {
        const auto& in = fragmentProgram.getPipeInput(i);
        if (in.name.starts_with("gl_")) continue;
        if (!vertexOutputs.contains(in.name)) {
            error = "ERROR: Fragment varying '" + in.name + "' is not declared in vertex shader.\n";
            return false;
        }
        const auto item = TypePacking(static_cast<GLenum>(in.glDefineType), in.size);
        if (item.width == 0) {
            error = "ERROR: Unsupported varying type for '" + in.name + "'.\n";
            return false;
        }
        varyingItems.push_back(item);
    }
    if (!FitsVectorBudget(std::move(varyingItems), MaxVaryingVectors)) {
        error = "ERROR: Active varyings exceed GL_MAX_VARYING_VECTORS.\n";
        return false;
    }
    return true;
}

int CopyGuestName(const GLchar* guest, std::string& result) {
    if (!guest) return 14;
    const int savedErrno = errno;
    const int status = CopyGuestString_nid_no_patch(guest, 4096, result);
    errno = savedErrno;
    return status;
}

bool ParseNonNegativeIndex(std::string_view text, GLint& index) {
    if (text.empty()) return false;
    if (text.size() > 1 && text[0] == '0') return false;
    std::int64_t value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') return false;
        value = value * 10 + (ch - '0');
        if (value > std::numeric_limits<GLint>::max()) return false;
    }
    index = static_cast<GLint>(value);
    return true;
}

GLint FloatToUniformInt(float value) {
    if (std::isnan(value)) return 0;
    const double rounded = std::round(static_cast<double>(value));
    return static_cast<GLint>(std::clamp(
        rounded,
        static_cast<double>(std::numeric_limits<GLint>::min()),
        static_cast<double>(std::numeric_limits<GLint>::max())));
}

bool MatchesFloatUniform(GLenum type, GLsizei components) {
    switch (components) {
    case 1: return type == 0x1406 || type == 0x8b56;
    case 2: return type == 0x8b50 || type == 0x8b57;
    case 3: return type == 0x8b51 || type == 0x8b58;
    case 4: return type == 0x8b52 || type == 0x8b59;
    default: return false;
    }
}

bool MatchesIntUniform(GLenum type, GLsizei components) {
    switch (components) {
    case 1: return type == 0x1404 || type == 0x8b56 || type == 0x8b5e || type == 0x8b60;
    case 2: return type == 0x8b53 || type == 0x8b57;
    case 3: return type == 0x8b54 || type == 0x8b58;
    case 4: return type == 0x8b55 || type == 0x8b59;
    default: return false;
    }
}

bool MatchesMatrixUniform(GLenum type, GLsizei dim) {
    switch (dim) {
    case 2: return type == 0x8b5a;
    case 3: return type == 0x8b5b;
    case 4: return type == 0x8b5c;
    default: return false;
    }
}

ActiveUniform* PrepareUniformWrite(GLint location, GLsizei count, GLboolean transpose, bool isMatrix, GLint& startElement) {
    auto& c = Current_nid_no_patch();
    if (count < 0) { Error_nid_no_patch(0x0501); return nullptr; }
    if (isMatrix && transpose != 0) { Error_nid_no_patch(0x0501); return nullptr; }
    if (!c.executable) { Error_nid_no_patch(0x0502); return nullptr; }
    if (location == -1) return nullptr;
    if (location < 0 || static_cast<std::size_t>(location) >= c.executable->uniformLocations.size()) {
        Error_nid_no_patch(0x0502);
        return nullptr;
    }
    const auto& slot = c.executable->uniformLocations[static_cast<std::size_t>(location)];
    auto& uniform = c.executable->uniforms[slot.uniformIndex];
    if (count > 1 && !uniform.isArray) { Error_nid_no_patch(0x0502); return nullptr; }
    startElement = slot.elementIndex;
    return &uniform;
}

void WriteFloatUniform(GLint location, GLsizei count, GLsizei components, const GLfloat* guestValues, bool isHostPointer) {
    GLint startElement = 0;
    auto* uniform = PrepareUniformWrite(location, count, 0, false, startElement);
    if (!uniform) return;
    if (!MatchesFloatUniform(uniform->type, components)) { Error_nid_no_patch(0x0502); return; }
    if (count == 0) return;
    const GLint remaining = uniform->size - startElement;
    if (remaining <= 0) return;
    const GLint toWrite = std::min(count, remaining);
    const auto valueCount = static_cast<std::size_t>(toWrite) * static_cast<std::size_t>(components);
    try {
        std::vector<GLfloat> local(valueCount);
        if (isHostPointer) {
            std::copy_n(guestValues, valueCount, local.data());
        } else {
            const int savedErrno = errno;
            const bool copied = GuestCopyIn_nid_no_patch(local.data(), guestValues, valueCount * sizeof(GLfloat));
            errno = savedErrno;
            if (!copied) { Error_nid_no_patch(0x0501); return; }
        }
        const auto offset = static_cast<std::size_t>(startElement) * static_cast<std::size_t>(components);
        if (IsBoolUniform(uniform->type)) {
            for (std::size_t i = 0; i < valueCount; ++i) uniform->intValues[offset + i] = local[i] != 0.0f ? 1 : 0;
        } else {
            for (std::size_t i = 0; i < valueCount; ++i) uniform->floatValues[offset + i] = local[i];
        }
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void WriteIntUniform(GLint location, GLsizei count, GLsizei components, const GLint* guestValues, bool isHostPointer) {
    GLint startElement = 0;
    auto* uniform = PrepareUniformWrite(location, count, 0, false, startElement);
    if (!uniform) return;
    if (!MatchesIntUniform(uniform->type, components)) { Error_nid_no_patch(0x0502); return; }
    if (count == 0) return;
    const GLint remaining = uniform->size - startElement;
    if (remaining <= 0) return;
    const GLint toWrite = std::min(count, remaining);
    const auto valueCount = static_cast<std::size_t>(toWrite) * static_cast<std::size_t>(components);
    try {
        std::vector<GLint> local(valueCount);
        if (isHostPointer) {
            std::copy_n(guestValues, valueCount, local.data());
        } else {
            const int savedErrno = errno;
            const bool copied = GuestCopyIn_nid_no_patch(local.data(), guestValues, valueCount * sizeof(GLint));
            errno = savedErrno;
            if (!copied) { Error_nid_no_patch(0x0501); return; }
        }
        const auto offset = static_cast<std::size_t>(startElement) * static_cast<std::size_t>(components);
        if (IsBoolUniform(uniform->type)) {
            for (std::size_t i = 0; i < valueCount; ++i) uniform->intValues[offset + i] = local[i] != 0 ? 1 : 0;
        } else {
            for (std::size_t i = 0; i < valueCount; ++i) uniform->intValues[offset + i] = local[i];
        }
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void WriteMatrixUniform(GLint location, GLsizei count, GLboolean transpose, GLsizei dim, const GLfloat* guestValues) {
    GLint startElement = 0;
    auto* uniform = PrepareUniformWrite(location, count, transpose, true, startElement);
    if (!uniform) return;
    if (!MatchesMatrixUniform(uniform->type, dim)) { Error_nid_no_patch(0x0502); return; }
    if (count == 0) return;
    const GLint remaining = uniform->size - startElement;
    if (remaining <= 0) return;
    const GLint toWrite = std::min(count, remaining);
    const auto components = static_cast<std::size_t>(dim) * static_cast<std::size_t>(dim);
    const auto valueCount = static_cast<std::size_t>(toWrite) * components;
    try {
        std::vector<GLfloat> local(valueCount);
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(local.data(), guestValues, valueCount * sizeof(GLfloat));
        errno = savedErrno;
        if (!copied) { Error_nid_no_patch(0x0501); return; }
        const auto offset = static_cast<std::size_t>(startElement) * components;
        for (std::size_t i = 0; i < valueCount; ++i) uniform->floatValues[offset + i] = local[i];
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void WriteActiveInfo(const std::string& activeName, GLint activeSize, GLenum activeType,
                     GLsizei bufSize, GLsizei* length, GLint* count, GLenum* type, GLchar* name) {
    const int savedErrno1 = errno;
    const bool countCopied = GuestCopyOut_nid_no_patch(count, &activeSize, sizeof(activeSize));
    errno = savedErrno1;
    if (!countCopied) { Error_nid_no_patch(0x0501); return; }

    const int savedErrno2 = errno;
    const bool typeCopied = GuestCopyOut_nid_no_patch(type, &activeType, sizeof(activeType));
    errno = savedErrno2;
    if (!typeCopied) { Error_nid_no_patch(0x0501); return; }

    CopyString_nid_no_patch(activeName, bufSize, length, name);
}

}

extern "C" {

void APS5_VABI glCompileShader_nid_postfix(GLuint shader) {
    std::lock_guard lock(mutex);
    auto* object = FindShader_nid_no_patch(shader);
    if (!object) return;
    try {
        const EShLanguage stage = object->type == 0x8b31 ? EShLangVertex : EShLangFragment;
        glslang::TShader glslShader(stage);
        std::string log;
        const bool ok = ParseEssl100(stage, object->source, glslShader, log);
        std::string compiledCopy;
        if (ok) compiledCopy = object->source;
        object->compiled = ok;
        object->infoLog = std::move(log);
        object->compiledSource = std::move(compiledCopy);
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void APS5_VABI glLinkProgram_nid_postfix(GLuint program) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram_nid_no_patch(program);
    if (!object) return;

    auto fail = [object](std::string message) {
        object->linked = false;
        object->validated = false;
        object->executable.reset();
        object->infoLog = std::move(message);
    };

    try {
        std::shared_ptr<Slim::Shader> vertex;
        std::shared_ptr<Slim::Shader> fragment;
        for (const auto& entry : object->shaders) {
            if (entry.second->type == 0x8b31) vertex = entry.second;
            else if (entry.second->type == 0x8b30) fragment = entry.second;
        }
        if (!vertex) { fail("ERROR: Program is missing an attached vertex shader.\n"); return; }
        if (!fragment) { fail("ERROR: Program is missing an attached fragment shader.\n"); return; }
        if (!vertex->compiled) { fail("ERROR: Attached vertex shader has not been successfully compiled.\n"); return; }
        if (!fragment->compiled) { fail("ERROR: Attached fragment shader has not been successfully compiled.\n"); return; }

        glslang::TShader vertexShader(EShLangVertex);
        std::string log;
        if (!ParseEssl100(EShLangVertex, vertex->compiledSource, vertexShader, log)) {
            fail(std::move(log));
            return;
        }
        glslang::TShader fragmentShader(EShLangFragment);
        if (!ParseEssl100(EShLangFragment, fragment->compiledSource, fragmentShader, log)) {
            fail(std::move(log));
            return;
        }

        glslang::TProgram glslProgram;
        glslProgram.addShader(&vertexShader);
        glslProgram.addShader(&fragmentShader);
        if (!glslProgram.link(EShMsgDefault)) {
            fail(glslProgram.getInfoLog());
            return;
        }
        if (!glslProgram.buildReflection(EShReflectionBasicArraySuffix | EShReflectionAllBlockVariables)) {
            fail(glslProgram.getInfoLog());
            return;
        }

        std::string varyingError;
        if (!CheckVaryings(vertex->compiledSource, fragment->compiledSource, varyingError)) {
            fail(std::move(varyingError));
            return;
        }

        auto exec = std::make_shared<Executable>();

        for (int i = 0; i < glslProgram.getNumPipeInputs(); ++i) {
            const auto& input = glslProgram.getPipeInput(i);
            if (input.name.starts_with("gl_")) continue;
            const auto glType = static_cast<GLenum>(input.glDefineType);
            if (AttributeSlotCount(glType) == 0) {
                fail("ERROR: Unsupported vertex attribute type for '" + input.name + "'.\n");
                return;
            }
            ActiveAttribute attr;
            attr.name = input.name;
            attr.type = glType;
            attr.size = std::max(input.size, 1);
            exec->attributes.push_back(std::move(attr));
        }

        std::array<bool, MaxVertexAttribs> usedSlots{};
        for (auto& attr : exec->attributes) {
            auto binding = object->attributes.find(attr.name);
            if (binding == object->attributes.end()) continue;
            const GLuint base = binding->second;
            const GLuint slots = AttributeSlotCount(attr.type);
            if (base >= MaxVertexAttribs || slots > MaxVertexAttribs - base) {
                fail("ERROR: Bound vertex attribute '" + attr.name + "' exceeds GL_MAX_VERTEX_ATTRIBS.\n");
                return;
            }
            for (GLuint s = 0; s < slots; ++s) {
                if (usedSlots[base + s]) {
                    fail("ERROR: Aliased vertex attribute location for '" + attr.name + "'.\n");
                    return;
                }
            }
            for (GLuint s = 0; s < slots; ++s) usedSlots[base + s] = true;
            attr.location = static_cast<GLint>(base);
        }

        std::vector<std::size_t> unbound;
        for (std::size_t i = 0; i < exec->attributes.size(); ++i) {
            if (exec->attributes[i].location < 0) unbound.push_back(i);
        }
        std::stable_sort(unbound.begin(), unbound.end(), [&](std::size_t a, std::size_t b) {
            return AttributeSlotCount(exec->attributes[a].type) > AttributeSlotCount(exec->attributes[b].type);
        });
        for (std::size_t idx : unbound) {
            auto& attr = exec->attributes[idx];
            const GLuint slots = AttributeSlotCount(attr.type);
            GLint assigned = -1;
            for (GLuint base = 0; base + slots <= MaxVertexAttribs; ++base) {
                bool freeRun = true;
                for (GLuint s = 0; s < slots; ++s) {
                    if (usedSlots[base + s]) { freeRun = false; break; }
                }
                if (freeRun) {
                    for (GLuint s = 0; s < slots; ++s) usedSlots[base + s] = true;
                    assigned = static_cast<GLint>(base);
                    break;
                }
            }
            if (assigned < 0) {
                fail("ERROR: Active vertex attributes exceed GL_MAX_VERTEX_ATTRIBS.\n");
                return;
            }
            attr.location = assigned;
        }
        for (const auto& attr : exec->attributes) exec->attributeLocations[attr.name] = attr.location;

        struct PendingUniform {
            std::string name;
            std::string baseName;
            GLenum type = 0;
            GLint size = 1;
            bool isArray = false;
            unsigned stages = 0;
            std::size_t components = 0;
        };

        std::vector<PendingUniform> pendingUniforms;
        std::vector<PackingItem> vertexUniforms;
        std::vector<PackingItem> fragmentUniforms;
        std::size_t vertexSamplers = 0;
        std::size_t fragmentSamplers = 0;

        for (int i = 0; i < glslProgram.getNumUniformVariables(); ++i) {
            const auto& refl = glslProgram.getUniform(i);
            if (refl.name.starts_with("gl_")) continue;
            const auto glType = static_cast<GLenum>(refl.glDefineType);
            const std::size_t components = UniformComponentCount(glType);
            if (components == 0) {
                fail("ERROR: Unsupported uniform type for '" + refl.name + "'.\n");
                return;
            }
            const bool isArray = refl.name.ends_with("[0]");
            const GLint count = std::max(refl.size, 1);
            const auto elemCount = static_cast<std::size_t>(count);
            const bool inVertex = (refl.stages & EShLangVertexMask) != 0;
            const bool inFragment = (refl.stages & EShLangFragmentMask) != 0;

            if (IsSamplerUniform(glType)) {
                if (inVertex) {
                    if (elemCount > MaxVertexTextureImageUnits - vertexSamplers) {
                        fail("ERROR: Active vertex shader samplers exceed GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS.\n");
                        return;
                    }
                    vertexSamplers += elemCount;
                }
                if (inFragment) {
                    if (elemCount > MaxFragmentTextureImageUnits - fragmentSamplers) {
                        fail("ERROR: Active fragment shader samplers exceed GL_MAX_TEXTURE_IMAGE_UNITS.\n");
                        return;
                    }
                    fragmentSamplers += elemCount;
                }
            } else {
                const auto pack = TypePacking(glType, count);
                if (inVertex) {
                    if (pack.rows > MaxVertexUniformVectors) {
                        fail("ERROR: Active vertex uniforms exceed GL_MAX_VERTEX_UNIFORM_VECTORS.\n");
                        return;
                    }
                    vertexUniforms.push_back(pack);
                }
                if (inFragment) {
                    if (pack.rows > MaxFragmentUniformVectors) {
                        fail("ERROR: Active fragment uniforms exceed GL_MAX_FRAGMENT_UNIFORM_VECTORS.\n");
                        return;
                    }
                    fragmentUniforms.push_back(pack);
                }
            }

            PendingUniform pending;
            pending.name = refl.name;
            pending.baseName = isArray ? refl.name.substr(0, refl.name.size() - 3) : refl.name;
            pending.type = glType;
            pending.size = count;
            pending.isArray = isArray;
            pending.stages = static_cast<unsigned>(refl.stages);
            pending.components = components;
            pendingUniforms.push_back(std::move(pending));
        }

        if (vertexSamplers + fragmentSamplers > MaxCombinedTextureImageUnits) {
            fail("ERROR: Active combined samplers exceed GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS.\n");
            return;
        }
        if (!FitsVectorBudget(std::move(vertexUniforms), MaxVertexUniformVectors)) {
            fail("ERROR: Active vertex uniforms exceed GL_MAX_VERTEX_UNIFORM_VECTORS.\n");
            return;
        }
        if (!FitsVectorBudget(std::move(fragmentUniforms), MaxFragmentUniformVectors)) {
            fail("ERROR: Active fragment uniforms exceed GL_MAX_FRAGMENT_UNIFORM_VECTORS.\n");
            return;
        }

        for (auto& pending : pendingUniforms) {
            ActiveUniform u;
            u.name = std::move(pending.name);
            u.baseName = std::move(pending.baseName);
            u.type = pending.type;
            u.size = pending.size;
            u.isArray = pending.isArray;
            u.baseLocation = static_cast<GLint>(exec->uniformLocations.size());
            u.stages = pending.stages;
            const auto totalComponents = static_cast<std::size_t>(pending.size) * pending.components;
            if (IsFloatBackedUniform(pending.type)) u.floatValues.assign(totalComponents, 0.0f);
            else u.intValues.assign(totalComponents, 0);

            const std::size_t uniformIndex = exec->uniforms.size();
            for (GLint elem = 0; elem < pending.size; ++elem) exec->uniformLocations.push_back({uniformIndex, elem});
            exec->uniforms.push_back(std::move(u));
        }

        std::string linkLog = glslProgram.getInfoLog();
        object->linked = true;
        object->validated = false;
        object->infoLog = std::move(linkLog);
        object->executable = std::move(exec);

        for (auto& entry : contexts) {
            if (entry.second && entry.second->program.get() == object) entry.second->executable = object->executable;
        }
    } catch (const std::bad_alloc&) {
        object->linked = false;
        object->validated = false;
        object->executable.reset();
        object->infoLog.clear();
        Error_nid_no_patch(0x0505);
    }
}

void APS5_VABI glValidateProgram_nid_postfix(GLuint program) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram_nid_no_patch(program);
    if (!object) return;
    try {
        if (!object->linked || !object->executable) {
            std::string log = "Program is not linked.\n";
            object->validated = false;
            object->infoLog = std::move(log);
            return;
        }
        std::array<GLenum, MaxFragmentTextureImageUnits> unitTypes{};
        for (const auto& uniform : object->executable->uniforms) {
            if (!IsSamplerUniform(uniform.type)) continue;
            for (GLint i = 0; i < uniform.size; ++i) {
                const GLint unit = uniform.intValues[static_cast<std::size_t>(i)];
                if (unit < 0 || static_cast<std::size_t>(unit) >= MaxFragmentTextureImageUnits) {
                    std::string log = "Sampler uniform texture image unit is out of range.\n";
                    object->validated = false;
                    object->infoLog = std::move(log);
                    return;
                }
                const auto slot = static_cast<std::size_t>(unit);
                if (unitTypes[slot] != 0 && unitTypes[slot] != uniform.type) {
                    std::string log = "Two active samplers of different types refer to the same texture image unit.\n";
                    object->validated = false;
                    object->infoLog = std::move(log);
                    return;
                }
                unitTypes[slot] = uniform.type;
            }
        }
        object->validated = true;
        object->infoLog.clear();
    } catch (const std::bad_alloc&) {
        object->validated = false;
        object->infoLog.clear();
        Error_nid_no_patch(0x0505);
    }
}

void APS5_VABI glShaderBinary_nid_postfix(GLsizei count, const GLuint* shaders, GLenum format, const void* binary, GLsizei length) {
    (void)format;
    std::lock_guard lock(mutex);
    Current_nid_no_patch();
    if (count < 0 || length < 0) { Error_nid_no_patch(0x0501); return; }
    if (count > MaxBatchNames) { Error_nid_no_patch(0x0505); return; }
    if ((count > 0 && !shaders) || (length > 0 && !binary)) { Error_nid_no_patch(0x0501); return; }
    bool seenVertex = false;
    bool seenFragment = false;
    std::array<GLuint, 256> local{};
    for (GLsizei done = 0; done < count; done += static_cast<GLsizei>(local.size())) {
        const auto n = std::min(count - done, static_cast<GLsizei>(local.size()));
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(local.data(), shaders + done, static_cast<std::size_t>(n) * sizeof(GLuint));
        errno = savedErrno;
        if (!copied) { Error_nid_no_patch(0x0501); return; }
        for (GLsizei i = 0; i < n; ++i) {
            auto* shader = FindShader_nid_no_patch(local[i]);
            if (!shader) return;
            bool& seen = shader->type == 0x8b31 ? seenVertex : seenFragment;
            if (seen) { Error_nid_no_patch(0x0502); return; }
            seen = true;
        }
    }
    Error_nid_no_patch(0x0500);
}

void APS5_VABI glGetShaderPrecisionFormat_nid_postfix(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
    std::lock_guard lock(mutex);
    Current_nid_no_patch();
    if (shadertype != 0x8b30 && shadertype != 0x8b31) { Error_nid_no_patch(0x0500); return; }
    GLint rangeValues[2];
    GLint precisionValue;
    switch (precisiontype) {
    case 0x8df0: case 0x8df1: case 0x8df2:
        rangeValues[0] = 127;
        rangeValues[1] = 127;
        precisionValue = 23;
        break;
    case 0x8df3: case 0x8df4: case 0x8df5:
        rangeValues[0] = 31;
        rangeValues[1] = 30;
        precisionValue = 0;
        break;
    default:
        Error_nid_no_patch(0x0500);
        return;
    }
    const int savedErrno1 = errno;
    const bool rangeCopied = GuestCopyOut_nid_no_patch(range, rangeValues, sizeof(rangeValues));
    errno = savedErrno1;
    if (!rangeCopied) { Error_nid_no_patch(0x0501); return; }
    const int savedErrno2 = errno;
    const bool precCopied = GuestCopyOut_nid_no_patch(precision, &precisionValue, sizeof(precisionValue));
    errno = savedErrno2;
    if (!precCopied) Error_nid_no_patch(0x0501);
}

GLint APS5_VABI glGetAttribLocation_nid_postfix(GLuint program, const GLchar* name) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram_nid_no_patch(program);
    if (!object) return -1;
    if (!object->linked || !object->executable) { Error_nid_no_patch(0x0502); return -1; }
    std::string localName;
    const int status = CopyGuestName(name, localName);
    if (status == 63) return -1;
    if (status == 12) { Error_nid_no_patch(0x0505); return -1; }
    if (status != 0) { Error_nid_no_patch(0x0501); return -1; }
    if (localName.starts_with("gl_")) return -1;
    auto it = object->executable->attributeLocations.find(localName);
    return it != object->executable->attributeLocations.end() ? it->second : -1;
}

GLint APS5_VABI glGetUniformLocation_nid_postfix(GLuint program, const GLchar* name) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram_nid_no_patch(program);
    if (!object) return -1;
    if (!object->linked || !object->executable) { Error_nid_no_patch(0x0502); return -1; }
    std::string localName;
    const int status = CopyGuestName(name, localName);
    if (status == 63) return -1;
    if (status == 12) { Error_nid_no_patch(0x0505); return -1; }
    if (status != 0) { Error_nid_no_patch(0x0501); return -1; }
    if (localName.empty() || localName.starts_with("gl_")) return -1;
    for (char ch : localName) if (IsWhitespace(ch)) return -1;

    if (localName.back() == ']') {
        const auto open = localName.rfind('[');
        if (open == std::string::npos || open == 0) return -1;
        GLint elemIndex = 0;
        const std::string_view indexText(localName.data() + open + 1, localName.size() - open - 2);
        if (!ParseNonNegativeIndex(indexText, elemIndex)) return -1;
        const std::string_view prefix(localName.data(), open);
        for (const auto& uniform : object->executable->uniforms) {
            if (uniform.isArray && uniform.baseName == prefix) {
                return elemIndex < uniform.size ? uniform.baseLocation + elemIndex : -1;
            }
        }
        return -1;
    }

    for (const auto& uniform : object->executable->uniforms) {
        if (uniform.baseName == localName) return uniform.baseLocation;
    }
    return -1;
}

void APS5_VABI glGetActiveAttrib_nid_postfix(GLuint program, GLuint index, GLsizei size, GLsizei* length, GLint* count, GLenum* type, GLchar* name) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram_nid_no_patch(program);
    if (!object) return;
    if (size < 0 || !object->executable || index >= object->executable->attributes.size()) {
        Error_nid_no_patch(0x0501);
        return;
    }
    const auto& attr = object->executable->attributes[index];
    WriteActiveInfo(attr.name, attr.size, attr.type, size, length, count, type, name);
}

void APS5_VABI glGetActiveUniform_nid_postfix(GLuint program, GLuint index, GLsizei size, GLsizei* length, GLint* count, GLenum* type, GLchar* name) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram_nid_no_patch(program);
    if (!object) return;
    if (size < 0 || !object->executable || index >= object->executable->uniforms.size()) {
        Error_nid_no_patch(0x0501);
        return;
    }
    const auto& uniform = object->executable->uniforms[index];
    WriteActiveInfo(uniform.name, uniform.size, uniform.type, size, length, count, type, name);
}

void APS5_VABI glGetUniformfv_nid_postfix(GLuint program, GLint location, GLfloat* params) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram_nid_no_patch(program);
    if (!object) return;
    if (!object->linked || !object->executable) { Error_nid_no_patch(0x0502); return; }
    if (location < 0 || static_cast<std::size_t>(location) >= object->executable->uniformLocations.size()) {
        Error_nid_no_patch(0x0502);
        return;
    }
    const auto& slot = object->executable->uniformLocations[static_cast<std::size_t>(location)];
    const auto& uniform = object->executable->uniforms[slot.uniformIndex];
    const std::size_t components = UniformComponentCount(uniform.type);
    const std::size_t offset = static_cast<std::size_t>(slot.elementIndex) * components;
    std::array<GLfloat, 16> local{};
    if (IsFloatBackedUniform(uniform.type)) {
        for (std::size_t i = 0; i < components; ++i) local[i] = uniform.floatValues[offset + i];
    } else {
        for (std::size_t i = 0; i < components; ++i) local[i] = static_cast<GLfloat>(uniform.intValues[offset + i]);
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(params, local.data(), components * sizeof(GLfloat));
    errno = savedErrno;
    if (!copied) Error_nid_no_patch(0x0501);
}

void APS5_VABI glGetUniformiv_nid_postfix(GLuint program, GLint location, GLint* params) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram_nid_no_patch(program);
    if (!object) return;
    if (!object->linked || !object->executable) { Error_nid_no_patch(0x0502); return; }
    if (location < 0 || static_cast<std::size_t>(location) >= object->executable->uniformLocations.size()) {
        Error_nid_no_patch(0x0502);
        return;
    }
    const auto& slot = object->executable->uniformLocations[static_cast<std::size_t>(location)];
    const auto& uniform = object->executable->uniforms[slot.uniformIndex];
    const std::size_t components = UniformComponentCount(uniform.type);
    const std::size_t offset = static_cast<std::size_t>(slot.elementIndex) * components;
    std::array<GLint, 16> local{};
    if (IsFloatBackedUniform(uniform.type)) {
        for (std::size_t i = 0; i < components; ++i) local[i] = FloatToUniformInt(uniform.floatValues[offset + i]);
    } else {
        for (std::size_t i = 0; i < components; ++i) local[i] = uniform.intValues[offset + i];
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(params, local.data(), components * sizeof(GLint));
    errno = savedErrno;
    if (!copied) Error_nid_no_patch(0x0501);
}

void APS5_VABI glUniform1f_nid_postfix(GLint location, GLfloat value) {
    std::lock_guard lock(mutex);
    WriteFloatUniform(location, 1, 1, &value, true);
}

void APS5_VABI glUniform2f_nid_postfix(GLint location, GLfloat v0, GLfloat v1) {
    std::lock_guard lock(mutex);
    const GLfloat values[2]{v0, v1};
    WriteFloatUniform(location, 1, 2, values, true);
}

void APS5_VABI glUniform3f_nid_postfix(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    std::lock_guard lock(mutex);
    const GLfloat values[3]{v0, v1, v2};
    WriteFloatUniform(location, 1, 3, values, true);
}

void APS5_VABI glUniform4f_nid_postfix(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    std::lock_guard lock(mutex);
    const GLfloat values[4]{v0, v1, v2, v3};
    WriteFloatUniform(location, 1, 4, values, true);
}

void APS5_VABI glUniform1fv_nid_postfix(GLint location, GLsizei count, const GLfloat* value) {
    std::lock_guard lock(mutex);
    WriteFloatUniform(location, count, 1, value, false);
}

void APS5_VABI glUniform2fv_nid_postfix(GLint location, GLsizei count, const GLfloat* value) {
    std::lock_guard lock(mutex);
    WriteFloatUniform(location, count, 2, value, false);
}

void APS5_VABI glUniform3fv_nid_postfix(GLint location, GLsizei count, const GLfloat* value) {
    std::lock_guard lock(mutex);
    WriteFloatUniform(location, count, 3, value, false);
}

void APS5_VABI glUniform4fv_nid_postfix(GLint location, GLsizei count, const GLfloat* value) {
    std::lock_guard lock(mutex);
    WriteFloatUniform(location, count, 4, value, false);
}

void APS5_VABI glUniform1i_nid_postfix(GLint location, GLint value) {
    std::lock_guard lock(mutex);
    WriteIntUniform(location, 1, 1, &value, true);
}

void APS5_VABI glUniform2i_nid_postfix(GLint location, GLint v0, GLint v1) {
    std::lock_guard lock(mutex);
    const GLint values[2]{v0, v1};
    WriteIntUniform(location, 1, 2, values, true);
}

void APS5_VABI glUniform3i_nid_postfix(GLint location, GLint v0, GLint v1, GLint v2) {
    std::lock_guard lock(mutex);
    const GLint values[3]{v0, v1, v2};
    WriteIntUniform(location, 1, 3, values, true);
}

void APS5_VABI glUniform4i_nid_postfix(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    std::lock_guard lock(mutex);
    const GLint values[4]{v0, v1, v2, v3};
    WriteIntUniform(location, 1, 4, values, true);
}

void APS5_VABI glUniform1iv_nid_postfix(GLint location, GLsizei count, const GLint* value) {
    std::lock_guard lock(mutex);
    WriteIntUniform(location, count, 1, value, false);
}

void APS5_VABI glUniform2iv_nid_postfix(GLint location, GLsizei count, const GLint* value) {
    std::lock_guard lock(mutex);
    WriteIntUniform(location, count, 2, value, false);
}

void APS5_VABI glUniform3iv_nid_postfix(GLint location, GLsizei count, const GLint* value) {
    std::lock_guard lock(mutex);
    WriteIntUniform(location, count, 3, value, false);
}

void APS5_VABI glUniform4iv_nid_postfix(GLint location, GLsizei count, const GLint* value) {
    std::lock_guard lock(mutex);
    WriteIntUniform(location, count, 4, value, false);
}

void APS5_VABI glUniformMatrix2fv_nid_postfix(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    std::lock_guard lock(mutex);
    WriteMatrixUniform(location, count, transpose, 2, value);
}

void APS5_VABI glUniformMatrix3fv_nid_postfix(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    std::lock_guard lock(mutex);
    WriteMatrixUniform(location, count, transpose, 3, value);
}

void APS5_VABI glUniformMatrix4fv_nid_postfix(GLint location, GLsizei count, GLboolean transpose, const GLfloat* values) {
    std::lock_guard lock(mutex);
    WriteMatrixUniform(location, count, transpose, 4, values);
}

}
