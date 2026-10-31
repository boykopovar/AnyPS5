#include "prx/libSceGLSlimVSH/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceGLSlimVSH/include/Api.hpp"

extern "C" {

EGLBoolean APS5_VABI eglBindTexImage_nid_postfix(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
    (void)dpy;
    (void)surface;
    (void)buffer;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLSurface APS5_VABI eglCreatePbufferFromClientBuffer_nid_postfix(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list) {
    (void)dpy;
    (void)buftype;
    (void)buffer;
    (void)config;
    (void)attrib_list;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLSurface APS5_VABI eglCreatePbufferSurface_nid_postfix(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list) {
    (void)dpy;
    (void)config;
    (void)attrib_list;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglGetConfigAttrib_nid_postfix(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value) {
    (void)dpy;
    (void)config;
    (void)attribute;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglGetConfigs_nid_postfix(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config) {
    (void)dpy;
    (void)configs;
    (void)config_size;
    (void)num_config;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLContext APS5_VABI eglGetCurrentContext_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLDisplay APS5_VABI eglGetCurrentDisplay_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLSurface APS5_VABI eglGetCurrentSurface_nid_postfix(EGLint readdraw) {
    (void)readdraw;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

__eglMustCastToProperFunctionPointerType APS5_VABI eglGetProcAddress_nid_postfix(const char *procname) {
    (void)procname;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLenum APS5_VABI eglQueryAPI_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglQueryContext_nid_postfix(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value) {
    (void)dpy;
    (void)ctx;
    (void)attribute;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

const char * APS5_VABI eglQueryString_nid_postfix(EGLDisplay dpy, EGLint name) {
    (void)dpy;
    (void)name;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglQuerySurface_nid_postfix(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value) {
    (void)dpy;
    (void)surface;
    (void)attribute;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglReleaseTexImage_nid_postfix(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
    (void)dpy;
    (void)surface;
    (void)buffer;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglReleaseThread_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglSurfaceAttrib_nid_postfix(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value) {
    (void)dpy;
    (void)surface;
    (void)attribute;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglWaitClient_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglWaitGL_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return {};
}

EGLBoolean APS5_VABI eglWaitNative_nid_postfix(EGLint engine) {
    (void)engine;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

void APS5_VABI glBindAttribLocation_nid_postfix(GLuint program, GLuint index, const GLchar *name) {
    (void)program;
    (void)index;
    (void)name;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glBindBuffer_nid_postfix(GLenum target, GLuint buffer) {
    (void)target;
    (void)buffer;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glBindFramebuffer_nid_postfix(GLenum target, GLuint framebuffer) {
    (void)target;
    (void)framebuffer;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glBindRenderbuffer_nid_postfix(GLenum target, GLuint renderbuffer) {
    (void)target;
    (void)renderbuffer;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glBlendColor_nid_postfix(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    (void)red;
    (void)green;
    (void)blue;
    (void)alpha;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glBlendEquation_nid_postfix(GLenum mode) {
    (void)mode;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glBlendEquationSeparate_nid_postfix(GLenum modeRGB, GLenum modeAlpha) {
    (void)modeRGB;
    (void)modeAlpha;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glBlendFuncSeparate_nid_postfix(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    (void)sfactorRGB;
    (void)dfactorRGB;
    (void)sfactorAlpha;
    (void)dfactorAlpha;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glBufferData_nid_postfix(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    (void)target;
    (void)size;
    (void)data;
    (void)usage;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glBufferSubData_nid_postfix(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    (void)target;
    (void)offset;
    (void)size;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
}

GLenum APS5_VABI glCheckFramebufferStatus_nid_postfix(GLenum target) {
    (void)target;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

void APS5_VABI glClearDepthf_nid_postfix(GLfloat d) {
    (void)d;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glClearStencil_nid_postfix(GLint s) {
    (void)s;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glColorMask_nid_postfix(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    (void)red;
    (void)green;
    (void)blue;
    (void)alpha;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glCompileShader_nid_postfix(GLuint shader) {
    (void)shader;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glCompressedTexImage2D_nid_postfix(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)border;
    (void)imageSize;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glCompressedTexSubImage2D_nid_postfix(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)width;
    (void)height;
    (void)format;
    (void)imageSize;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glCopyTexImage2D_nid_postfix(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    (void)target;
    (void)level;
    (void)internalformat;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)border;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glCopyTexSubImage2D_nid_postfix(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glCullFace_nid_postfix(GLenum mode) {
    (void)mode;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDeleteBuffers_nid_postfix(GLsizei n, const GLuint *buffers) {
    (void)n;
    (void)buffers;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDeleteFramebuffers_nid_postfix(GLsizei n, const GLuint *framebuffers) {
    (void)n;
    (void)framebuffers;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDeleteRenderbuffers_nid_postfix(GLsizei n, const GLuint *renderbuffers) {
    (void)n;
    (void)renderbuffers;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDepthFunc_nid_postfix(GLenum func) {
    (void)func;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDepthMask_nid_postfix(GLboolean flag) {
    (void)flag;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDepthRangef_nid_postfix(GLfloat n, GLfloat f) {
    (void)n;
    (void)f;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDetachShader_nid_postfix(GLuint program, GLuint shader) {
    (void)program;
    (void)shader;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDisable_nid_postfix(GLenum cap) {
    (void)cap;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDisableVertexAttribArray_nid_postfix(GLuint index) {
    (void)index;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glDrawElements_nid_postfix(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    (void)mode;
    (void)count;
    (void)type;
    (void)indices;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glFinish_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glFlush_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glFramebufferRenderbuffer_nid_postfix(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    (void)target;
    (void)attachment;
    (void)renderbuffertarget;
    (void)renderbuffer;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glFramebufferTexture2D_nid_postfix(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    (void)target;
    (void)attachment;
    (void)textarget;
    (void)texture;
    (void)level;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glFrontFace_nid_postfix(GLenum mode) {
    (void)mode;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGenBuffers_nid_postfix(GLsizei n, GLuint *buffers) {
    (void)n;
    (void)buffers;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGenFramebuffers_nid_postfix(GLsizei n, GLuint *framebuffers) {
    (void)n;
    (void)framebuffers;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGenRenderbuffers_nid_postfix(GLsizei n, GLuint *renderbuffers) {
    (void)n;
    (void)renderbuffers;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGenerateMipmap_nid_postfix(GLenum target) {
    (void)target;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetAttachedShaders_nid_postfix(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders) {
    (void)program;
    (void)maxCount;
    (void)count;
    (void)shaders;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetBooleanv_nid_postfix(GLenum pname, GLboolean *data) {
    (void)pname;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetBufferParameteriv_nid_postfix(GLenum target, GLenum pname, GLint *params) {
    (void)target;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetFloatv_nid_postfix(GLenum pname, GLfloat *data) {
    (void)pname;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetFramebufferAttachmentParameteriv_nid_postfix(GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    (void)target;
    (void)attachment;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetIntegerv_nid_postfix(GLenum pname, GLint *data) {
    (void)pname;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetProgramInfoLog_nid_postfix(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    (void)program;
    (void)bufSize;
    (void)length;
    (void)infoLog;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetRenderbufferParameteriv_nid_postfix(GLenum target, GLenum pname, GLint *params) {
    (void)target;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetShaderInfoLog_nid_postfix(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    (void)shader;
    (void)bufSize;
    (void)length;
    (void)infoLog;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetShaderPrecisionFormat_nid_postfix(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision) {
    (void)shadertype;
    (void)precisiontype;
    (void)range;
    (void)precision;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetShaderiv_nid_postfix(GLuint shader, GLenum pname, GLint *params) {
    (void)shader;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetTexParameterfv_nid_postfix(GLenum target, GLenum pname, GLfloat *params) {
    (void)target;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetTexParameteriv_nid_postfix(GLenum target, GLenum pname, GLint *params) {
    (void)target;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetUniformfv_nid_postfix(GLuint program, GLint location, GLfloat *params) {
    (void)program;
    (void)location;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetUniformiv_nid_postfix(GLuint program, GLint location, GLint *params) {
    (void)program;
    (void)location;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetVertexAttribPointerv_nid_postfix(GLuint index, GLenum pname, void **pointer) {
    (void)index;
    (void)pname;
    (void)pointer;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetVertexAttribfv_nid_postfix(GLuint index, GLenum pname, GLfloat *params) {
    (void)index;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glGetVertexAttribiv_nid_postfix(GLuint index, GLenum pname, GLint *params) {
    (void)index;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glHint_nid_postfix(GLenum target, GLenum mode) {
    (void)target;
    (void)mode;
    NotImplemented_nid_no_patch(__func__);
}

GLboolean APS5_VABI glIsBuffer_nid_postfix(GLuint buffer) {
    (void)buffer;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

GLboolean APS5_VABI glIsEnabled_nid_postfix(GLenum cap) {
    (void)cap;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

GLboolean APS5_VABI glIsFramebuffer_nid_postfix(GLuint framebuffer) {
    (void)framebuffer;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

GLboolean APS5_VABI glIsProgram_nid_postfix(GLuint program) {
    (void)program;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

GLboolean APS5_VABI glIsRenderbuffer_nid_postfix(GLuint renderbuffer) {
    (void)renderbuffer;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

GLboolean APS5_VABI glIsShader_nid_postfix(GLuint shader) {
    (void)shader;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

GLboolean APS5_VABI glIsTexture_nid_postfix(GLuint texture) {
    (void)texture;
    NotImplemented_nid_no_patch(__func__);
    return {};
}

void APS5_VABI glLineWidth_nid_postfix(GLfloat width) {
    (void)width;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glPolygonOffset_nid_postfix(GLfloat factor, GLfloat units) {
    (void)factor;
    (void)units;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glReadPixels_nid_postfix(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    (void)pixels;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glReleaseShaderCompiler_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glRenderbufferStorage_nid_postfix(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    (void)target;
    (void)internalformat;
    (void)width;
    (void)height;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glSampleCoverage_nid_postfix(GLfloat value, GLboolean invert) {
    (void)value;
    (void)invert;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glScissor_nid_postfix(GLint x, GLint y, GLsizei width, GLsizei height) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glShaderSource_nid_postfix(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) {
    (void)shader;
    (void)count;
    (void)string;
    (void)length;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glStencilFunc_nid_postfix(GLenum func, GLint ref, GLuint mask) {
    (void)func;
    (void)ref;
    (void)mask;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glStencilFuncSeparate_nid_postfix(GLenum face, GLenum func, GLint ref, GLuint mask) {
    (void)face;
    (void)func;
    (void)ref;
    (void)mask;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glStencilMask_nid_postfix(GLuint mask) {
    (void)mask;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glStencilMaskSeparate_nid_postfix(GLenum face, GLuint mask) {
    (void)face;
    (void)mask;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glStencilOp_nid_postfix(GLenum fail, GLenum zfail, GLenum zpass) {
    (void)fail;
    (void)zfail;
    (void)zpass;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glStencilOpSeparate_nid_postfix(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    (void)face;
    (void)sfail;
    (void)dpfail;
    (void)dppass;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glTexParameterf_nid_postfix(GLenum target, GLenum pname, GLfloat param) {
    (void)target;
    (void)pname;
    (void)param;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glTexParameterfv_nid_postfix(GLenum target, GLenum pname, const GLfloat *params) {
    (void)target;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glTexParameteriv_nid_postfix(GLenum target, GLenum pname, const GLint *params) {
    (void)target;
    (void)pname;
    (void)params;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glTexSubImage2D_nid_postfix(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)width;
    (void)height;
    (void)format;
    (void)type;
    (void)pixels;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform1fv_nid_postfix(GLint location, GLsizei count, const GLfloat *value) {
    (void)location;
    (void)count;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform1iv_nid_postfix(GLint location, GLsizei count, const GLint *value) {
    (void)location;
    (void)count;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform2f_nid_postfix(GLint location, GLfloat v0, GLfloat v1) {
    (void)location;
    (void)v0;
    (void)v1;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform2fv_nid_postfix(GLint location, GLsizei count, const GLfloat *value) {
    (void)location;
    (void)count;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform2i_nid_postfix(GLint location, GLint v0, GLint v1) {
    (void)location;
    (void)v0;
    (void)v1;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform2iv_nid_postfix(GLint location, GLsizei count, const GLint *value) {
    (void)location;
    (void)count;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform3f_nid_postfix(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    (void)location;
    (void)v0;
    (void)v1;
    (void)v2;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform3fv_nid_postfix(GLint location, GLsizei count, const GLfloat *value) {
    (void)location;
    (void)count;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform3i_nid_postfix(GLint location, GLint v0, GLint v1, GLint v2) {
    (void)location;
    (void)v0;
    (void)v1;
    (void)v2;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform3iv_nid_postfix(GLint location, GLsizei count, const GLint *value) {
    (void)location;
    (void)count;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform4f_nid_postfix(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    (void)location;
    (void)v0;
    (void)v1;
    (void)v2;
    (void)v3;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform4fv_nid_postfix(GLint location, GLsizei count, const GLfloat *value) {
    (void)location;
    (void)count;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform4i_nid_postfix(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    (void)location;
    (void)v0;
    (void)v1;
    (void)v2;
    (void)v3;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniform4iv_nid_postfix(GLint location, GLsizei count, const GLint *value) {
    (void)location;
    (void)count;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniformMatrix2fv_nid_postfix(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glUniformMatrix3fv_nid_postfix(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    (void)location;
    (void)count;
    (void)transpose;
    (void)value;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glValidateProgram_nid_postfix(GLuint program) {
    (void)program;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glVertexAttrib1f_nid_postfix(GLuint index, GLfloat x) {
    (void)index;
    (void)x;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glVertexAttrib1fv_nid_postfix(GLuint index, const GLfloat *v) {
    (void)index;
    (void)v;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glVertexAttrib2f_nid_postfix(GLuint index, GLfloat x, GLfloat y) {
    (void)index;
    (void)x;
    (void)y;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glVertexAttrib2fv_nid_postfix(GLuint index, const GLfloat *v) {
    (void)index;
    (void)v;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glVertexAttrib3f_nid_postfix(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    (void)index;
    (void)x;
    (void)y;
    (void)z;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glVertexAttrib3fv_nid_postfix(GLuint index, const GLfloat *v) {
    (void)index;
    (void)v;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glVertexAttrib4f_nid_postfix(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    (void)index;
    (void)x;
    (void)y;
    (void)z;
    (void)w;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI glVertexAttrib4fv_nid_postfix(GLuint index, const GLfloat *v) {
    (void)index;
    (void)v;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceGLSlimVSHUnknown_iIsF4NzYgD4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("iIsF4NzYgD4", sceGLSlimVSHUnknown_iIsF4NzYgD4_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_hA8TWZkwPRU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("hA8TWZkwPRU", sceGLSlimVSHUnknown_hA8TWZkwPRU_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_LuP1plTAR7g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("LuP1plTAR7g", sceGLSlimVSHUnknown_LuP1plTAR7g_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_J5IG3lL56FI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("J5IG3lL56FI", sceGLSlimVSHUnknown_J5IG3lL56FI_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_oXkQEVitkCs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("oXkQEVitkCs", sceGLSlimVSHUnknown_oXkQEVitkCs_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_sDotUt1uEhA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("sDotUt1uEhA", sceGLSlimVSHUnknown_sDotUt1uEhA_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_3gtNzvkq_XY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("3gtNzvkq-XY", sceGLSlimVSHUnknown_3gtNzvkq_XY_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_T__xooU3VvQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("T-+xooU3VvQ", sceGLSlimVSHUnknown_T__xooU3VvQ_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_AcruVcmKz78_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("AcruVcmKz78", sceGLSlimVSHUnknown_AcruVcmKz78_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_QvXbiLqpn6c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("QvXbiLqpn6c", sceGLSlimVSHUnknown_QvXbiLqpn6c_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_mgKTS0PIvd8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("mgKTS0PIvd8", sceGLSlimVSHUnknown_mgKTS0PIvd8_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_RP5cXUjm48U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("RP5cXUjm48U", sceGLSlimVSHUnknown_RP5cXUjm48U_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_srJHx3p48v0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("srJHx3p48v0", sceGLSlimVSHUnknown_srJHx3p48v0_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_6ZQXcSKKjyo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("6ZQXcSKKjyo", sceGLSlimVSHUnknown_6ZQXcSKKjyo_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_wdRBDMIxI7I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("wdRBDMIxI7I", sceGLSlimVSHUnknown_wdRBDMIxI7I_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_CzmA_OALrH8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("CzmA-OALrH8", sceGLSlimVSHUnknown_CzmA_OALrH8_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_aRqvgeH_oe0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("aRqvgeH+oe0", sceGLSlimVSHUnknown_aRqvgeH_oe0_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_gI_XEd5mWPU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("gI+XEd5mWPU", sceGLSlimVSHUnknown_gI_XEd5mWPU_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_1TFdFqQBGqY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("1TFdFqQBGqY", sceGLSlimVSHUnknown_1TFdFqQBGqY_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_7Mp6qE235Qc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("7Mp6qE235Qc", sceGLSlimVSHUnknown_7Mp6qE235Qc_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_BrNgn7z6A40_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("BrNgn7z6A40", sceGLSlimVSHUnknown_BrNgn7z6A40_nid_no_patch);

int APS5_VABI sceGLSlimVSHUnknown_iyTnezpw1jE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("iyTnezpw1jE", sceGLSlimVSHUnknown_iyTnezpw1jE_nid_no_patch);

}

extern "C" {

int APS5_VABI sceSlimglCompositorSetInvisibleCanvasCommand() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglCompositorSetMemoryCommand() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglCompositorSetPostEventCommand() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglCompositorWaitEndOfRendering() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglCompositorWaitPostEvent() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglIPCEnd() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSlimglIPCSetup() {
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceGLSlimVSHInventory_9iW854bwvmc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9iW854bwvmc", libSceGLSlimVSHInventory_9iW854bwvmc_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_DVNKmWX1jOU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DVNKmWX1jOU", libSceGLSlimVSHInventory_DVNKmWX1jOU_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_JjyNMbSAgLk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JjyNMbSAgLk", libSceGLSlimVSHInventory_JjyNMbSAgLk_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_LLDLZ_minus_mQK_plus_Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LLDLZ-mQK+Q", libSceGLSlimVSHInventory_LLDLZ_minus_mQK_plus_Q_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_Yf0LATiXPsg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Yf0LATiXPsg", libSceGLSlimVSHInventory_Yf0LATiXPsg_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_ZLvm5korFRA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZLvm5korFRA", libSceGLSlimVSHInventory_ZLvm5korFRA_nid_no_patch);

int APS5_VABI libSceGLSlimVSHInventory_qSuVNKHENqs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qSuVNKHENqs", libSceGLSlimVSHInventory_qSuVNKHENqs_nid_no_patch);

}
