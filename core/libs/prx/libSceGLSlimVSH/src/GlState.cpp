#include "State.hpp"
#include <limits>

using namespace Slim;

namespace {

bool Compare(GLenum value) { return value >= 0x0200 && value <= 0x0207; }
bool Face(GLenum value) { return value == 0x0404 || value == 0x0405 || value == 0x0408; }
bool Equation(GLenum value) { return value == 0x8006 || value == 0x800a || value == 0x800b; }
bool Factor(GLenum value) {
    return value <= 1 || (value >= 0x0300 && value <= 0x0308) || (value >= 0x8001 && value <= 0x8004);
}
bool Operation(GLenum value) {
    return value == 0 || value == 0x1e00 || value == 0x1e01 || value == 0x1e02 || value == 0x1e03 || value == 0x150a || value == 0x8507 || value == 0x8508;
}

GLint Integer(double value) {
    if (std::isnan(value)) return 0;
    return static_cast<GLint>(std::clamp(std::round(value), double(std::numeric_limits<GLint>::min()), double(std::numeric_limits<GLint>::max())));
}

Attribute* Attrib(GLuint index) {
    auto& c = Current_nid_no_patch();
    if (index >= c.attributes.size()) { Error_nid_no_patch(0x0501); return nullptr; }
    return &c.attributes[index];
}

bool AttribQuery(GLuint index, GLenum name, std::vector<double>& result) {
    auto* a = Attrib(index);
    if (!a) return false;
    switch (name) {
    case 0x8626: result.assign(a->value.begin(), a->value.end()); break;
    case 0x8622: result = {double(a->enabled)}; break;
    case 0x8623: result = {double(a->size)}; break;
    case 0x8624: result = {double(a->stride)}; break;
    case 0x8625: result = {double(a->type)}; break;
    case 0x886a: result = {double(a->normalized)}; break;
    case 0x889f: result = {double(a->buffer ? a->buffer->name : 0)}; break;
    default: Error_nid_no_patch(0x0500); return false;
    }
    return true;
}

}

extern "C" {

GLenum APS5_VABI glGetError_nid_postfix() {
    std::lock_guard lock(mutex);
    auto& c = Current_nid_no_patch();
    auto error = c.error;
    c.error = 0;
    return error;
}

void APS5_VABI glGetIntegerv_nid_postfix(GLenum name, GLint* values) {
    std::lock_guard lock(mutex);
    std::vector<double> data;
    if (!Query_nid_no_patch(name, data)) return;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (name == 0x0b93 || name == 0x0b98 || name == 0x8ca4 || name == 0x8ca5) values[i] = static_cast<GLint>(static_cast<GLuint>(data[i]));
        else if (name == 0x0b70 || name == 0x0b73 || name == 0x0c22 || name == 0x8005) values[i] = Integer(std::floor((data[i] * 4294967295.0 - 1.0) / 2.0 + 0.5));
        else values[i] = Integer(data[i]);
    }
}

void APS5_VABI glGetFloatv_nid_postfix(GLenum name, GLfloat* values) {
    std::lock_guard lock(mutex);
    std::vector<double> data;
    if (Query_nid_no_patch(name, data)) std::copy(data.begin(), data.end(), values);
}

void APS5_VABI glGetBooleanv_nid_postfix(GLenum name, GLboolean* values) {
    std::lock_guard lock(mutex);
    std::vector<double> data;
    if (Query_nid_no_patch(name, data)) for (std::size_t i = 0; i < data.size(); ++i) values[i] = data[i] != 0;
}

void APS5_VABI glEnable_nid_postfix(GLenum cap) {
    std::lock_guard lock(mutex);
    if (!Capability_nid_no_patch(cap)) { Error_nid_no_patch(0x0500); return; }
    Current_nid_no_patch().enabled.insert(cap);
}

void APS5_VABI glDisable_nid_postfix(GLenum cap) {
    std::lock_guard lock(mutex);
    if (!Capability_nid_no_patch(cap)) { Error_nid_no_patch(0x0500); return; }
    Current_nid_no_patch().enabled.erase(cap);
}

GLboolean APS5_VABI glIsEnabled_nid_postfix(GLenum cap) {
    std::lock_guard lock(mutex);
    if (!Capability_nid_no_patch(cap)) { Error_nid_no_patch(0x0500); return 0; }
    return Current_nid_no_patch().enabled.contains(cap);
}

void APS5_VABI glBlendEquationSeparate_nid_postfix(GLenum rgb, GLenum alpha) {
    std::lock_guard lock(mutex);
    if (!Equation(rgb) || !Equation(alpha)) { Error_nid_no_patch(0x0500); return; }
    auto& s = Current_nid_no_patch().state;
    s[0x8009] = {double(rgb)}; s[0x883d] = {double(alpha)};
}

void APS5_VABI glBlendEquation_nid_postfix(GLenum mode) { glBlendEquationSeparate_nid_postfix(mode, mode); }

void APS5_VABI glBlendFuncSeparate_nid_postfix(GLenum srcRgb, GLenum dstRgb, GLenum srcAlpha, GLenum dstAlpha) {
    std::lock_guard lock(mutex);
    if (!Factor(srcRgb) || !Factor(dstRgb) || !Factor(srcAlpha) || !Factor(dstAlpha) || dstRgb == 0x0308 || dstAlpha == 0x0308) { Error_nid_no_patch(0x0500); return; }
    if (((srcRgb == 0x8001 || srcRgb == 0x8002) && (dstRgb == 0x8003 || dstRgb == 0x8004)) ||
        ((dstRgb == 0x8001 || dstRgb == 0x8002) && (srcRgb == 0x8003 || srcRgb == 0x8004))) { Error_nid_no_patch(0x0502); return; }
    auto& s = Current_nid_no_patch().state;
    s[0x80c9] = {double(srcRgb)}; s[0x80c8] = {double(dstRgb)};
    s[0x80cb] = {double(srcAlpha)}; s[0x80ca] = {double(dstAlpha)};
}

void APS5_VABI glBlendFunc_nid_postfix(GLenum source, GLenum destination) {
    glBlendFuncSeparate_nid_postfix(source, destination, source, destination);
}

void APS5_VABI glStencilFuncSeparate_nid_postfix(GLenum face, GLenum func, GLint reference, GLuint mask) {
    std::lock_guard lock(mutex);
    if (!Face(face) || !Compare(func)) { Error_nid_no_patch(0x0500); return; }
    auto& s = Current_nid_no_patch().state;
    if (face != 0x0405) { s[0x0b92] = {double(func)}; s[0x0b97] = {double(std::max(reference, 0))}; s[0x0b93] = {double(mask)}; }
    if (face != 0x0404) { s[0x8800] = {double(func)}; s[0x8ca3] = {double(std::max(reference, 0))}; s[0x8ca4] = {double(mask)}; }
}

void APS5_VABI glStencilFunc_nid_postfix(GLenum func, GLint reference, GLuint mask) { glStencilFuncSeparate_nid_postfix(0x0408, func, reference, mask); }

void APS5_VABI glStencilMaskSeparate_nid_postfix(GLenum face, GLuint mask) {
    std::lock_guard lock(mutex);
    if (!Face(face)) { Error_nid_no_patch(0x0500); return; }
    auto& s = Current_nid_no_patch().state;
    if (face != 0x0405) s[0x0b98] = {double(mask)};
    if (face != 0x0404) s[0x8ca5] = {double(mask)};
}

void APS5_VABI glStencilMask_nid_postfix(GLuint mask) { glStencilMaskSeparate_nid_postfix(0x0408, mask); }

void APS5_VABI glStencilOpSeparate_nid_postfix(GLenum face, GLenum fail, GLenum depthFail, GLenum pass) {
    std::lock_guard lock(mutex);
    if (!Face(face) || !Operation(fail) || !Operation(depthFail) || !Operation(pass)) { Error_nid_no_patch(0x0500); return; }
    auto& s = Current_nid_no_patch().state;
    if (face != 0x0405) { s[0x0b94] = {double(fail)}; s[0x0b95] = {double(depthFail)}; s[0x0b96] = {double(pass)}; }
    if (face != 0x0404) { s[0x8801] = {double(fail)}; s[0x8802] = {double(depthFail)}; s[0x8803] = {double(pass)}; }
}

void APS5_VABI glStencilOp_nid_postfix(GLenum fail, GLenum depthFail, GLenum pass) { glStencilOpSeparate_nid_postfix(0x0408, fail, depthFail, pass); }

void APS5_VABI glPixelStorei_nid_postfix(GLenum name, GLint value) {
    std::lock_guard lock(mutex);
    if (name != 0x0d05 && name != 0x0cf5) { Error_nid_no_patch(0x0500); return; }
    if (value != 1 && value != 2 && value != 4 && value != 8) { Error_nid_no_patch(0x0501); return; }
    Current_nid_no_patch().state[name] = {double(value)};
}

void APS5_VABI glHint_nid_postfix(GLenum target, GLenum mode) {
    std::lock_guard lock(mutex);
    if (target != 0x8192 || mode < 0x1100 || mode > 0x1102) { Error_nid_no_patch(0x0500); return; }
    Current_nid_no_patch().state[target] = {double(mode)};
}

void APS5_VABI glVertexAttribPointer_nid_postfix(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {
    std::lock_guard lock(mutex);
    auto* a = Attrib(index);
    if (!a) return;
    if (size < 1 || size > 4 || stride < 0) { Error_nid_no_patch(0x0501); return; }
    if (type != 0x1400 && type != 0x1401 && type != 0x1402 && type != 0x1403 && type != 0x1406 && type != 0x140c) { Error_nid_no_patch(0x0500); return; }
    a->size = size; a->type = type; a->normalized = normalized != 0; a->stride = stride; a->pointer = pointer;
    a->buffer = Current_nid_no_patch().arrayBuffer;
}

void APS5_VABI glEnableVertexAttribArray_nid_postfix(GLuint index) {
    std::lock_guard lock(mutex);
    if (auto* a = Attrib(index)) a->enabled = true;
}

void APS5_VABI glDisableVertexAttribArray_nid_postfix(GLuint index) {
    std::lock_guard lock(mutex);
    if (auto* a = Attrib(index)) a->enabled = false;
}

void APS5_VABI glGetVertexAttribPointerv_nid_postfix(GLuint index, GLenum name, void** pointer) {
    std::lock_guard lock(mutex);
    auto* a = Attrib(index);
    if (!a) return;
    if (name != 0x8645) { Error_nid_no_patch(0x0500); return; }
    *pointer = const_cast<void*>(a->pointer);
}

void APS5_VABI glGetVertexAttribfv_nid_postfix(GLuint index, GLenum name, GLfloat* values) {
    std::lock_guard lock(mutex);
    std::vector<double> data;
    if (AttribQuery(index, name, data)) std::copy(data.begin(), data.end(), values);
}

void APS5_VABI glGetVertexAttribiv_nid_postfix(GLuint index, GLenum name, GLint* values) {
    std::lock_guard lock(mutex);
    std::vector<double> data;
    if (AttribQuery(index, name, data)) for (std::size_t i = 0; i < data.size(); ++i) values[i] = Integer(data[i]);
}

void APS5_VABI glFinish_nid_postfix() { std::lock_guard lock(mutex); Current_nid_no_patch(); }
void APS5_VABI glFlush_nid_postfix() { std::lock_guard lock(mutex); Current_nid_no_patch(); }


void APS5_VABI glClearColor_nid_postfix(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    std::lock_guard lock(mutex);
    Current_nid_no_patch().state[0x0c22] = {Clamp_nid_no_patch(red), Clamp_nid_no_patch(green), Clamp_nid_no_patch(blue), Clamp_nid_no_patch(alpha)};
}

void APS5_VABI glBlendColor_nid_postfix(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    std::lock_guard lock(mutex);
    Current_nid_no_patch().state[0x8005] = {Clamp_nid_no_patch(red), Clamp_nid_no_patch(green), Clamp_nid_no_patch(blue), Clamp_nid_no_patch(alpha)};
}

void APS5_VABI glClearDepthf_nid_postfix(GLfloat depth) {
    std::lock_guard lock(mutex);
    Current_nid_no_patch().state[0x0b73] = {Clamp_nid_no_patch(depth)};
}

void APS5_VABI glClearStencil_nid_postfix(GLint value) {
    std::lock_guard lock(mutex);
    Current_nid_no_patch().state[0x0b91] = {double(value)};
}

void APS5_VABI glDepthMask_nid_postfix(GLboolean value) {
    std::lock_guard lock(mutex);
    Current_nid_no_patch().state[0x0b72] = {double(value != 0)};
}

void APS5_VABI glDepthRangef_nid_postfix(GLfloat nearValue, GLfloat farValue) {
    std::lock_guard lock(mutex);
    Current_nid_no_patch().state[0x0b70] = {Clamp_nid_no_patch(nearValue), Clamp_nid_no_patch(farValue)};
}

void APS5_VABI glColorMask_nid_postfix(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    std::lock_guard lock(mutex);
    Current_nid_no_patch().state[0x0c23] = {double(red != 0), double(green != 0), double(blue != 0), double(alpha != 0)};
}

void APS5_VABI glDepthFunc_nid_postfix(GLenum value) {
    std::lock_guard lock(mutex);
    if (!Compare(value)) { Error_nid_no_patch(0x0500); return; }
    Current_nid_no_patch().state[0x0b74] = {double(value)};
}

void APS5_VABI glCullFace_nid_postfix(GLenum value) {
    std::lock_guard lock(mutex);
    if (!Face(value)) { Error_nid_no_patch(0x0500); return; }
    Current_nid_no_patch().state[0x0b45] = {double(value)};
}

void APS5_VABI glFrontFace_nid_postfix(GLenum value) {
    std::lock_guard lock(mutex);
    if (value != 0x0900 && value != 0x0901) { Error_nid_no_patch(0x0500); return; }
    Current_nid_no_patch().state[0x0b46] = {double(value)};
}

void APS5_VABI glLineWidth_nid_postfix(GLfloat value) {
    std::lock_guard lock(mutex);
    if (value <= 0) { Error_nid_no_patch(0x0501); return; }
    Current_nid_no_patch().state[0x0b21] = {value};
}

void APS5_VABI glScissor_nid_postfix(GLint x, GLint y, GLsizei width, GLsizei height) {
    std::lock_guard lock(mutex);
    if (width < 0 || height < 0) { Error_nid_no_patch(0x0501); return; }
    Current_nid_no_patch().state[0x0c10] = {double(x), double(y), double(width), double(height)};
}

void APS5_VABI glViewport_nid_postfix(GLint x, GLint y, GLsizei width, GLsizei height) {
    std::lock_guard lock(mutex);
    if (width < 0 || height < 0) { Error_nid_no_patch(0x0501); return; }
    Current_nid_no_patch().state[0x0ba2] = {double(x), double(y), double(std::min(width, 2048)), double(std::min(height, 2048))};
}

void APS5_VABI glPolygonOffset_nid_postfix(GLfloat factor, GLfloat units) {
    std::lock_guard lock(mutex);
    auto& state = Current_nid_no_patch().state;
    state[0x8038] = {factor}; state[0x2a00] = {units};
}

void APS5_VABI glSampleCoverage_nid_postfix(GLfloat value, GLboolean invert) {
    std::lock_guard lock(mutex);
    auto& state = Current_nid_no_patch().state;
    state[0x80aa] = {Clamp_nid_no_patch(value)}; state[0x80ab] = {double(invert != 0)};
}

void APS5_VABI glVertexAttrib1f_nid_postfix(GLuint index, GLfloat x) {
    std::lock_guard lock(mutex);
    if (auto* a = Attrib(index)) a->value = {x, 0, 0, 1};
}

void APS5_VABI glVertexAttrib1fv_nid_postfix(GLuint index, const GLfloat* values) {
    glVertexAttrib1f_nid_postfix(index, values[0]);
}

void APS5_VABI glVertexAttrib2f_nid_postfix(GLuint index, GLfloat x, GLfloat y) {
    std::lock_guard lock(mutex);
    if (auto* a = Attrib(index)) a->value = {x, y, 0, 1};
}

void APS5_VABI glVertexAttrib2fv_nid_postfix(GLuint index, const GLfloat* values) {
    glVertexAttrib2f_nid_postfix(index, values[0], values[1]);
}

void APS5_VABI glVertexAttrib3f_nid_postfix(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    std::lock_guard lock(mutex);
    if (auto* a = Attrib(index)) a->value = {x, y, z, 1};
}

void APS5_VABI glVertexAttrib3fv_nid_postfix(GLuint index, const GLfloat* values) {
    glVertexAttrib3f_nid_postfix(index, values[0], values[1], values[2]);
}

void APS5_VABI glVertexAttrib4f_nid_postfix(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    std::lock_guard lock(mutex);
    if (auto* a = Attrib(index)) a->value = {x, y, z, w};
}

void APS5_VABI glVertexAttrib4fv_nid_postfix(GLuint index, const GLfloat* values) {
    glVertexAttrib4f_nid_postfix(index, values[0], values[1], values[2], values[3]);
}

}
