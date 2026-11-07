#include "prx/libScePigletv2VSH/include/Egl.hpp"
#include "prx/libc/include/General.hpp"

using namespace Piglet;

extern "C" {

bool APS5_VABI scePigletSetConfigurationVSH(const PigletConfiguration* configuration) {
    return SetConfiguration(configuration);
}

void* APS5_VABI eglGetDisplay(void* native) {
    return EglGetDisplay(native);
}

EGLBoolean APS5_VABI eglInitialize(void* display, EGLint* major, EGLint* minor) {
    return EglInitialize(display, major, minor);
}

EGLBoolean APS5_VABI eglTerminate(void* display) {
    return EglTerminate(display);
}

EGLBoolean APS5_VABI eglBindAPI(EGLenum api) {
    return EglBindAPI(api);
}

EGLBoolean APS5_VABI eglChooseConfig(void* display, const EGLint* attributes, void** configs, EGLint size, EGLint* count) {
    return EglChooseConfig(display, attributes, configs, size, count);
}

void* APS5_VABI eglCreateWindowSurface(void* display, void* config, const NativeWindow* window, const EGLint* attributes) {
    return EglCreateWindowSurface(display, config, window, attributes);
}

void* APS5_VABI eglCreateContext(void* display, void* config, void* share, const EGLint* attributes) {
    return EglCreateContext(display, config, share, attributes);
}

EGLBoolean APS5_VABI eglMakeCurrent(void* display, void* draw, void* read, void* context) {
    return EglMakeCurrent(display, draw, read, context);
}

EGLBoolean APS5_VABI eglSwapInterval(void* display, EGLint interval) {
    return EglSwapInterval(display, interval);
}

EGLBoolean APS5_VABI eglSwapBuffers(void* display, void* surface) {
    return EglSwapBuffers(display, surface);
}

EGLBoolean APS5_VABI eglDestroySurface(void* display, void* surface) {
    return EglDestroySurface(display, surface);
}

EGLBoolean APS5_VABI eglDestroyContext(void* display, void* context) {
    return EglDestroyContext(display, context);
}

EGLint APS5_VABI eglGetError() {
    return EglGetError();
}

void APS5_VABI glActiveTexture(GLenum texture) {
    Current()->ActiveTexture(texture);
}

void APS5_VABI glAttachShader(GLuint program, GLuint shader) {
    Current()->AttachShader(program, shader);
}

void APS5_VABI glBindTexture(GLenum target, GLuint texture) {
    Current()->BindTexture(target, texture);
}

void APS5_VABI glBlendFunc(GLenum source, GLenum destination) {
    Current()->BlendFunc(source, destination);
}

void APS5_VABI glClear(GLbitfield mask) {
    Current()->Clear(mask);
}

void APS5_VABI glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    Current()->ClearColor(red, green, blue, alpha);
}

GLuint APS5_VABI glCreateProgram() {
    return Current()->CreateProgram();
}

GLuint APS5_VABI glCreateShader(GLenum type) {
    return Current()->CreateShader(type);
}

void APS5_VABI glDeleteProgram(GLuint program) {
    Current()->DeleteProgram(program);
}

void APS5_VABI glDeleteShader(GLuint shader) {
    Current()->DeleteShader(shader);
}

void APS5_VABI glDeleteTextures(GLsizei count, const GLuint* textures) {
    Current()->DeleteTextures(count, textures);
}

void APS5_VABI glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    Current()->DrawArrays(mode, first, count);
}

void APS5_VABI glEnable(GLenum capability) {
    Current()->Enable(capability);
}

void APS5_VABI glEnableVertexAttribArray(GLuint index) {
    Current()->EnableVertexAttribArray(index);
}

void APS5_VABI glGenTextures(GLsizei count, GLuint* textures) {
    Current()->GenTextures(count, textures);
}

void APS5_VABI glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufferSize, GLsizei* length, GLint* size, GLenum* type, char* name) {
    Current()->GetActiveAttrib(program, index, bufferSize, length, size, type, name);
}

void APS5_VABI glGetActiveUniform(GLuint program, GLuint index, GLsizei bufferSize, GLsizei* length, GLint* size, GLenum* type, char* name) {
    Current()->GetActiveUniform(program, index, bufferSize, length, size, type, name);
}

GLint APS5_VABI glGetAttribLocation(GLuint program, const char* name) {
    return Current()->GetAttribLocation(program, name);
}

GLenum APS5_VABI glGetError() {
    return Current()->GetError();
}

void APS5_VABI glGetProgramiv(GLuint program, GLenum name, GLint* value) {
    Current()->GetProgramiv(program, name, value);
}

const char* APS5_VABI glGetString(GLenum name) {
    return Current()->GetString(name);
}

GLint APS5_VABI glGetUniformLocation(GLuint program, const char* name) {
    return Current()->GetUniformLocation(program, name);
}

void APS5_VABI glLinkProgram(GLuint program) {
    Current()->LinkProgram(program);
}

void APS5_VABI glPixelStorei(GLenum name, GLint value) {
    Current()->PixelStorei(name, value);
}

void APS5_VABI glShaderBinary(GLsizei count, const GLuint* shaders, GLenum format, const void* binary, GLsizei length) {
    Current()->ShaderBinary(count, shaders, format, binary, length);
}

void APS5_VABI glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    Current()->TexImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
}

void APS5_VABI glTexParameteri(GLenum target, GLenum name, GLint value) {
    Current()->TexParameteri(target, name, value);
}

void APS5_VABI glUniform1f(GLint location, GLfloat value) {
    Current()->Uniform1f(location, value);
}

void APS5_VABI glUniform1i(GLint location, GLint value) {
    Current()->Uniform1i(location, value);
}

void APS5_VABI glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    Current()->UniformMatrix4fv(location, count, transpose, value);
}

void APS5_VABI glUseProgram(GLuint program) {
    Current()->UseProgram(program);
}

void APS5_VABI glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {
    Current()->VertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void APS5_VABI glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    Current()->Viewport(x, y, width, height);
}

}
