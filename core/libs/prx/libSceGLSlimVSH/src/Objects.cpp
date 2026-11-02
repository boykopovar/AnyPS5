#include "State.hpp"
#include <limits>

using namespace Slim;

namespace {

std::shared_ptr<Buffer>* BufferBinding(GLenum target) {
    auto& c = Current_nid_no_patch();
    if (target == 0x8892) return &c.arrayBuffer;
    if (target == 0x8893) return &c.elementBuffer;
    Error_nid_no_patch(0x0500);
    return nullptr;
}

Buffer* BoundBuffer(GLenum target) {
    auto binding = BufferBinding(target);
    if (!binding) return nullptr;
    if (!*binding) { Error_nid_no_patch(0x0502); return nullptr; }
    return binding->get();
}

Slim::Shader* FindShader(GLuint name) {
    auto& share = *Current_nid_no_patch().share;
    auto it = share.shaders.find(name);
    if (it != share.shaders.end()) return it->second.get();
    Error_nid_no_patch(share.programs.contains(name) ? 0x0502 : 0x0501);
    return nullptr;
}

Program* FindProgram(GLuint name) {
    auto& share = *Current_nid_no_patch().share;
    auto it = share.programs.find(name);
    if (it != share.programs.end()) return it->second.get();
    Error_nid_no_patch(share.shaders.contains(name) ? 0x0502 : 0x0501);
    return nullptr;
}

GLuint ShaderName(ShareGroup& share) {
    for (GLuint name = 1; name; ++name) if (!share.shaders.contains(name) && !share.programs.contains(name)) return name;
    throw std::bad_alloc();
}

void CollectShaders(ShareGroup& share) {
    std::erase_if(share.shaders, [](const auto& item) { return item.second->deleted && item.second.use_count() == 1; });
}

void CopyString(const std::string& source, GLsizei size, GLsizei* length, GLchar* output) {
    GLsizei count = size ? static_cast<GLsizei>(std::min<std::size_t>(source.size(), size - 1)) : 0;
    if (size) { std::memcpy(output, source.data(), count); output[count] = 0; }
    if (length) *length = count;
}

}

extern "C" {

void APS5_VABI glGenBuffers_nid_postfix(GLsizei count, GLuint* names) {
    std::lock_guard lock(mutex);
    Generate_nid_no_patch(count, names, Current_nid_no_patch().share->buffers);
}

void APS5_VABI glBindBuffer_nid_postfix(GLenum target, GLuint name) {
    std::lock_guard lock(mutex);
    auto* binding = BufferBinding(target);
    if (!binding) return;
    if (!name) { binding->reset(); return; }
    try {
        auto& object = Current_nid_no_patch().share->buffers[name];
        if (!object) { object = std::make_shared<Buffer>(); object->name = name; }
        *binding = object;
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

GLboolean APS5_VABI glIsBuffer_nid_postfix(GLuint name) {
    std::lock_guard lock(mutex);
    auto& objects = Current_nid_no_patch().share->buffers;
    auto it = objects.find(name);
    return it != objects.end() && bool(it->second);
}

void APS5_VABI glDeleteBuffers_nid_postfix(GLsizei count, const GLuint* names) {
    std::lock_guard lock(mutex);
    if (count < 0) { Error_nid_no_patch(0x0501); return; }
    auto& c = Current_nid_no_patch();
    for (GLsizei i = 0; i < count; ++i) {
        auto it = c.share->buffers.find(names[i]);
        if (it == c.share->buffers.end()) continue;
        auto object = it->second;
        if (object) {
            if (c.arrayBuffer == object) c.arrayBuffer.reset();
            if (c.elementBuffer == object) c.elementBuffer.reset();
            for (auto& a : c.attributes) if (a.buffer == object) a.buffer.reset();
        }
        c.share->buffers.erase(it);
    }
}

void APS5_VABI glBufferData_nid_postfix(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    std::lock_guard lock(mutex);
    auto* buffer = BoundBuffer(target);
    if (!buffer) return;
    if (size < 0) { Error_nid_no_patch(0x0501); return; }
    if (usage != 0x88e0 && usage != 0x88e4 && usage != 0x88e8) { Error_nid_no_patch(0x0500); return; }
    try {
        std::vector<unsigned char> next;
        if (static_cast<std::size_t>(size) > next.max_size()) { Error_nid_no_patch(0x0505); return; }
        next.resize(size);
        if (size && data) std::memcpy(next.data(), data, size);
        buffer->data.swap(next);
        buffer->usage = usage;
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void APS5_VABI glBufferSubData_nid_postfix(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    std::lock_guard lock(mutex);
    auto* buffer = BoundBuffer(target);
    if (!buffer) return;
    if (offset < 0 || size < 0 || static_cast<std::size_t>(offset) > buffer->data.size() || static_cast<std::size_t>(size) > buffer->data.size() - offset) { Error_nid_no_patch(0x0501); return; }
    if (size) std::memcpy(buffer->data.data() + offset, data, size);
}

void APS5_VABI glGetBufferParameteriv_nid_postfix(GLenum target, GLenum name, GLint* value) {
    std::lock_guard lock(mutex);
    auto* buffer = BoundBuffer(target);
    if (!buffer) return;
    if (name == 0x8764) *value = static_cast<GLint>(std::min<std::size_t>(buffer->data.size(), std::numeric_limits<GLint>::max()));
    else if (name == 0x8765) *value = buffer->usage;
    else Error_nid_no_patch(0x0500);
}

GLuint APS5_VABI glCreateShader_nid_postfix(GLenum type) {
    std::lock_guard lock(mutex);
    if (type != 0x8b30 && type != 0x8b31) { Error_nid_no_patch(0x0500); return 0; }
    try {
        auto& share = *Current_nid_no_patch().share;
        auto name = ShaderName(share);
        auto shader = std::make_shared<Slim::Shader>();
        shader->type = type;
        share.shaders.emplace(name, std::move(shader));
        return name;
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); return 0; }
}

GLuint APS5_VABI glCreateProgram_nid_postfix() {
    std::lock_guard lock(mutex);
    try {
        auto& share = *Current_nid_no_patch().share;
        auto name = ShaderName(share);
        share.programs.emplace(name, std::make_shared<Program>());
        return name;
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); return 0; }
}

GLboolean APS5_VABI glIsShader_nid_postfix(GLuint name) {
    std::lock_guard lock(mutex);
    return Current_nid_no_patch().share->shaders.contains(name);
}

GLboolean APS5_VABI glIsProgram_nid_postfix(GLuint name) {
    std::lock_guard lock(mutex);
    return Current_nid_no_patch().share->programs.contains(name);
}

void APS5_VABI glDeleteShader_nid_postfix(GLuint name) {
    std::lock_guard lock(mutex);
    if (!name) return;
    if (auto* shader = FindShader(name)) shader->deleted = true;
    CollectShaders(*Current_nid_no_patch().share);
}

void APS5_VABI glDeleteProgram_nid_postfix(GLuint name) {
    std::lock_guard lock(mutex);
    if (!name || !FindProgram(name)) return;
    auto& share = *Current_nid_no_patch().share;
    share.programs.erase(name);
    CollectShaders(share);
}

void APS5_VABI glAttachShader_nid_postfix(GLuint program, GLuint shader) {
    std::lock_guard lock(mutex);
    auto* p = FindProgram(program);
    if (!p) return;
    auto* object = FindShader(shader);
    if (!object) return;
    for (auto& entry : p->shaders) if (entry.second->type == object->type) { Error_nid_no_patch(0x0502); return; }
    try { p->shaders.emplace(shader, Current_nid_no_patch().share->shaders.at(shader)); }
    catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void APS5_VABI glDetachShader_nid_postfix(GLuint program, GLuint shader) {
    std::lock_guard lock(mutex);
    auto* p = FindProgram(program);
    if (!p || !FindShader(shader)) return;
    if (!p->shaders.erase(shader)) { Error_nid_no_patch(0x0502); return; }
    CollectShaders(*Current_nid_no_patch().share);
}

void APS5_VABI glShaderSource_nid_postfix(GLuint shader, GLsizei count, const GLchar* const* strings, const GLint* lengths) {
    std::lock_guard lock(mutex);
    auto* object = FindShader(shader);
    if (!object) return;
    if (count < 0) { Error_nid_no_patch(0x0501); return; }
    try {
        std::string source;
        for (GLsizei i = 0; i < count; ++i) {
            if (lengths && lengths[i] >= 0) source.append(strings[i], lengths[i]);
            else source.append(strings[i]);
        }
        object->source.swap(source);
    } catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void APS5_VABI glGetShaderiv_nid_postfix(GLuint shader, GLenum name, GLint* value) {
    std::lock_guard lock(mutex);
    auto* object = FindShader(shader);
    if (!object) return;
    switch (name) {
    case 0x8b4f: *value = object->type; break;
    case 0x8b80: *value = object->deleted; break;
    case 0x8b81: case 0x8b84: *value = 0; break;
    case 0x8b88: *value = object->source.empty() ? 0 : static_cast<GLint>(std::min<std::size_t>(object->source.size() + 1, std::numeric_limits<GLint>::max())); break;
    default: Error_nid_no_patch(0x0500); break;
    }
}

void APS5_VABI glGetProgramiv_nid_postfix(GLuint program, GLenum name, GLint* value) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram(program);
    if (!object) return;
    switch (name) {
    case 0x8b80: *value = object->deleted; break;
    case 0x8b85: *value = static_cast<GLint>(object->shaders.size()); break;
    case 0x8b82: case 0x8b83: case 0x8b84: case 0x8b86: case 0x8b87: case 0x8b89: case 0x8b8a: *value = 0; break;
    default: Error_nid_no_patch(0x0500); break;
    }
}

void APS5_VABI glGetAttachedShaders_nid_postfix(GLuint program, GLsizei size, GLsizei* count, GLuint* shaders) {
    std::lock_guard lock(mutex);
    if (size < 0) { Error_nid_no_patch(0x0501); return; }
    auto* object = FindProgram(program);
    if (!object) return;
    GLsizei written = 0;
    for (auto& entry : object->shaders) {
        if (written == size) break;
        shaders[written++] = entry.first;
    }
    if (count) *count = written;
}

void APS5_VABI glGetShaderInfoLog_nid_postfix(GLuint shader, GLsizei size, GLsizei* length, GLchar* log) {
    std::lock_guard lock(mutex);
    if (size < 0) { Error_nid_no_patch(0x0501); return; }
    if (FindShader(shader)) CopyString({}, size, length, log);
}

void APS5_VABI glGetProgramInfoLog_nid_postfix(GLuint program, GLsizei size, GLsizei* length, GLchar* log) {
    std::lock_guard lock(mutex);
    if (size < 0) { Error_nid_no_patch(0x0501); return; }
    if (FindProgram(program)) CopyString({}, size, length, log);
}

void APS5_VABI glBindAttribLocation_nid_postfix(GLuint program, GLuint index, const GLchar* name) {
    std::lock_guard lock(mutex);
    auto* object = FindProgram(program);
    if (!object) return;
    if (index >= 8) { Error_nid_no_patch(0x0501); return; }
    if (std::strncmp(name, "gl_", 3) == 0) { Error_nid_no_patch(0x0502); return; }
    try { object->attributes[name] = index; }
    catch (const std::bad_alloc&) { Error_nid_no_patch(0x0505); }
}

void APS5_VABI glUseProgram_nid_postfix(GLuint program) {
    std::lock_guard lock(mutex);
    if (program && FindProgram(program)) Error_nid_no_patch(0x0502);
}

void APS5_VABI glReleaseShaderCompiler_nid_postfix() { std::lock_guard lock(mutex); Current_nid_no_patch(); }

}
