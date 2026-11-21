#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* file, int line, const char* expr) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}

#define CHECK(expr) Require((expr), __FILE__, __LINE__, #expr)

void ExpectError(GLenum expected, const char* file, int line) {
    const GLenum actual = glGetError_nid_postfix();
    if (actual != expected) {
        std::fprintf(stderr, "FAIL %s:%d: expected error 0x%x, got 0x%x\n", file, line, expected, actual);
        std::exit(1);
    }
}

#define CHECK_ERROR(expected) ExpectError((expected), __FILE__, __LINE__)

GLuint CompileShader(GLenum type, const char* source, bool expectSuccess = true) {
    const GLuint shader = glCreateShader_nid_postfix(type);
    CHECK(shader != 0);
    glShaderSource_nid_postfix(shader, 1, &source, nullptr);
    glCompileShader_nid_postfix(shader);
    CHECK_ERROR(0);
    GLint status = -1;
    glGetShaderiv_nid_postfix(shader, 0x8b81, &status);
    CHECK_ERROR(0);
    if (status != (expectSuccess ? 1 : 0)) {
        GLint len = 0;
        glGetShaderiv_nid_postfix(shader, 0x8b84, &len);
        std::string log(static_cast<std::size_t>(std::max(len, 1)), '\0');
        if (len > 0) glGetShaderInfoLog_nid_postfix(shader, len, nullptr, log.data());
        std::fprintf(stderr, "CompileShader mismatch (expected %d, got %d) for:\n%s\nlog:\n%s\n",
                     expectSuccess ? 1 : 0, status, source, log.c_str());
        std::exit(1);
    }
    return shader;
}

GLuint LinkProgramFromSources(const char* vertexSource, const char* fragmentSource, bool expectSuccess = true) {
    const GLuint vs = CompileShader(0x8b31, vertexSource, true);
    const GLuint fs = CompileShader(0x8b30, fragmentSource, true);
    const GLuint program = glCreateProgram_nid_postfix();
    CHECK(program != 0);
    glAttachShader_nid_postfix(program, vs);
    glAttachShader_nid_postfix(program, fs);
    glLinkProgram_nid_postfix(program);
    CHECK_ERROR(0);
    GLint status = -1;
    glGetProgramiv_nid_postfix(program, 0x8b82, &status);
    CHECK(status == (expectSuccess ? 1 : 0));
    glDeleteShader_nid_postfix(vs);
    glDeleteShader_nid_postfix(fs);
    return program;
}

std::string ShaderInfoLog(GLuint shader) {
    GLint length = 0;
    glGetShaderiv_nid_postfix(shader, 0x8b84, &length);
    CHECK_ERROR(0);
    if (length == 0) return {};
    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = -1;
    glGetShaderInfoLog_nid_postfix(shader, length, &written, log.data());
    CHECK_ERROR(0);
    CHECK(written == length - 1);
    CHECK(log[static_cast<std::size_t>(written)] == '\0');
    log.resize(static_cast<std::size_t>(written));
    return log;
}

std::string ProgramInfoLog(GLuint program) {
    GLint length = 0;
    glGetProgramiv_nid_postfix(program, 0x8b84, &length);
    CHECK_ERROR(0);
    if (length == 0) return {};
    std::string log(static_cast<std::size_t>(length), '\0');
    GLsizei written = -1;
    glGetProgramInfoLog_nid_postfix(program, length, &written, log.data());
    CHECK_ERROR(0);
    CHECK(written == length - 1);
    CHECK(log[static_cast<std::size_t>(written)] == '\0');
    log.resize(static_cast<std::size_t>(written));
    return log;
}

void TestCompilerAndPrecisionQueries() {
    GLint compilerInt = 0;
    glGetIntegerv_nid_postfix(0x8dfa, &compilerInt);
    CHECK_ERROR(0);
    CHECK(compilerInt == 1);

    GLboolean compilerBool = 0;
    glGetBooleanv_nid_postfix(0x8dfa, &compilerBool);
    CHECK_ERROR(0);
    CHECK(compilerBool == 1);

    GLfloat compilerFloat = 0.0f;
    glGetFloatv_nid_postfix(0x8dfa, &compilerFloat);
    CHECK_ERROR(0);
    CHECK(compilerFloat == 1.0f);

    GLint currentProgram = -1;
    glGetIntegerv_nid_postfix(0x8b8d, &currentProgram);
    CHECK_ERROR(0);
    CHECK(currentProgram == 0);

    const GLenum shaderTypes[]{0x8b31, 0x8b30};
    const GLenum floatPrecisions[]{0x8df0, 0x8df1, 0x8df2};
    const GLenum intPrecisions[]{0x8df3, 0x8df4, 0x8df5};
    for (GLenum stage : shaderTypes) {
        for (GLenum prec : floatPrecisions) {
            GLint range[2]{-1, -1};
            GLint precision = -1;
            glGetShaderPrecisionFormat_nid_postfix(stage, prec, range, &precision);
            CHECK_ERROR(0);
            CHECK(range[0] == 127 && range[1] == 127 && precision == 23);
        }
        for (GLenum prec : intPrecisions) {
            GLint range[2]{-1, -1};
            GLint precision = -1;
            glGetShaderPrecisionFormat_nid_postfix(stage, prec, range, &precision);
            CHECK_ERROR(0);
            CHECK(range[0] == 31 && range[1] == 30 && precision == 0);
        }
    }

    GLint range[2]{9, 9};
    GLint precision = 9;
    glGetShaderPrecisionFormat_nid_postfix(0x1234, 0x8df2, range, &precision);
    CHECK_ERROR(0x0500);
    CHECK(range[0] == 9 && range[1] == 9 && precision == 9);
    glGetShaderPrecisionFormat_nid_postfix(0x8b31, 0x1234, range, &precision);
    CHECK_ERROR(0x0500);
    CHECK(range[0] == 9 && range[1] == 9 && precision == 9);
}

void TestShaderCompilation() {
    const char* validVertex =
        "attribute vec4 aPos;\n"
        "void main() {\n"
        "    gl_Position = aPos;\n"
        "}\n";
    const GLuint vs = CompileShader(0x8b31, validVertex, true);
    CHECK(ShaderInfoLog(vs).empty());

    const char* version100Vertex =
        "// leading comment\n"
        "/* block comment */\n"
        "#version 100\n"
        "attribute vec3 aPos;\n"
        "void main() {\n"
        "    float limitCheck = float(gl_MaxVertexAttribs + gl_MaxVertexUniformVectors + gl_MaxVaryingVectors + gl_MaxFragmentUniformVectors + gl_MaxTextureImageUnits + gl_MaxCombinedTextureImageUnits + gl_MaxVertexTextureImageUnits);\n"
        "    gl_Position = vec4(aPos, limitCheck);\n"
        "}\n";
    const GLuint vs100 = CompileShader(0x8b31, version100Vertex, true);
    glDeleteShader_nid_postfix(vs100);

    const char* syntaxError = "void main( { gl_Position = vec4(0.0); }";
    const GLuint badSyntax = CompileShader(0x8b31, syntaxError, false);
    CHECK(!ShaderInfoLog(badSyntax).empty());

    const char* attribArray =
        "attribute vec4 aArr[2];\n"
        "void main() { gl_Position = aArr[0] + aArr[1]; }\n";
    const GLuint badAttribArray = CompileShader(0x8b31, attribArray, false);
    CHECK(!ShaderInfoLog(badAttribArray).empty());
    glDeleteShader_nid_postfix(badAttribArray);

    const char* es300 =
        "#version 300 es\n"
        "in vec4 aPos;\n"
        "void main() { gl_Position = aPos; }\n";
    const GLuint badVersion = CompileShader(0x8b31, es300, false);
    CHECK(!ShaderInfoLog(badVersion).empty());

    const char* es100WithProfile =
        "#version 100 es\n"
        "void main() { gl_Position = vec4(0.0); }\n";
    const GLuint badProfile = CompileShader(0x8b31, es100WithProfile, false);
    CHECK(!ShaderInfoLog(badProfile).empty());

    const char* desktop110 =
        "#version 110\n"
        "void main() { gl_Position = vec4(0.0); }\n";
    const GLuint badDesktop = CompileShader(0x8b31, desktop110, false);
    CHECK(!ShaderInfoLog(badDesktop).empty());

    const char* version100Commented =
        "#version 100 /* trailing */\n"
        "void main() { gl_Position = vec4(0.0); }\n";
    const GLuint vs100Commented = CompileShader(0x8b31, version100Commented, true);
    glDeleteShader_nid_postfix(vs100Commented);

    const char* badVersionCommented =
        "#version /* inline */ 300 es\n"
        "void main() { gl_Position = vec4(0.0); }\n";
    const GLuint badVerC = CompileShader(0x8b31, badVersionCommented, false);
    glDeleteShader_nid_postfix(badVerC);

    const char* badProfileCommented =
        "#version 100 /* inline */ core\n"
        "void main() { gl_Position = vec4(0.0); }\n";
    const GLuint badProfC = CompileShader(0x8b31, badProfileCommented, false);
    glDeleteShader_nid_postfix(badProfC);

    const char* lateVersion =
        "void main() { gl_Position = vec4(0.0); }\n"
        "#version 300 es\n";
    const GLuint badLate = CompileShader(0x8b31, lateVersion, false);
    CHECK(!ShaderInfoLog(badLate).empty());

    const GLuint nullByteShader = glCreateShader_nid_postfix(0x8b31);
    const char withNull[]{'v', 'o', 'i', 'd', ' ', 'm', 'a', 'i', 'n', '(', ')', '{', '}', '\0', 'x'};
    const char* nullPtr = withNull;
    const GLint nullLen = static_cast<GLint>(sizeof(withNull));
    glShaderSource_nid_postfix(nullByteShader, 1, &nullPtr, &nullLen);
    glCompileShader_nid_postfix(nullByteShader);
    CHECK_ERROR(0);
    GLint status = 1;
    glGetShaderiv_nid_postfix(nullByteShader, 0x8b81, &status);
    CHECK(status == 0);
    CHECK(!ShaderInfoLog(nullByteShader).empty());
    glDeleteShader_nid_postfix(nullByteShader);

    const char* badSourceAfterCompile = "broken syntax !!!";
    glShaderSource_nid_postfix(vs, 1, &badSourceAfterCompile, nullptr);
    glGetShaderiv_nid_postfix(vs, 0x8b81, &status);
    CHECK(status == 1);

    const char* validFragment =
        "precision mediump float;\n"
        "void main() {\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";
    const GLuint fs = CompileShader(0x8b30, validFragment, true);
    const GLuint prog = glCreateProgram_nid_postfix();
    glAttachShader_nid_postfix(prog, vs);
    glAttachShader_nid_postfix(prog, fs);
    glLinkProgram_nid_postfix(prog);
    CHECK_ERROR(0);
    glGetProgramiv_nid_postfix(prog, 0x8b82, &status);
    CHECK(status == 1);

    glCompileShader_nid_postfix(vs);
    glGetShaderiv_nid_postfix(vs, 0x8b81, &status);
    CHECK(status == 0);

    const GLuint emptyShader = CompileShader(0x8b31, "", true);
    const GLuint validFsForEmpty = CompileShader(0x8b30, validFragment, true);
    const GLuint emptyProg = glCreateProgram_nid_postfix();
    glAttachShader_nid_postfix(emptyProg, emptyShader);
    glAttachShader_nid_postfix(emptyProg, validFsForEmpty);
    glLinkProgram_nid_postfix(emptyProg);
    CHECK_ERROR(0);
    glGetProgramiv_nid_postfix(emptyProg, 0x8b82, &status);
    CHECK(status == 0);
    glDeleteProgram_nid_postfix(emptyProg);
    glDeleteShader_nid_postfix(emptyShader);
    glDeleteShader_nid_postfix(validFsForEmpty);

    glCompileShader_nid_postfix(prog);
    CHECK_ERROR(0x0502);
    glCompileShader_nid_postfix(99999);
    CHECK_ERROR(0x0501);

    glDeleteProgram_nid_postfix(prog);
    glDeleteShader_nid_postfix(vs);
    glDeleteShader_nid_postfix(fs);
    glDeleteShader_nid_postfix(badSyntax);
    glDeleteShader_nid_postfix(badVersion);
    glDeleteShader_nid_postfix(badProfile);
    glDeleteShader_nid_postfix(badDesktop);
    glDeleteShader_nid_postfix(badLate);
    CHECK_ERROR(0);
}

void TestLinkingErrorsAndLimits() {
    const char* simpleVs =
        "attribute vec4 aPos;\n"
        "void main() { gl_Position = aPos; }\n";
    const char* simpleFs =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(1.0); }\n";

    const GLuint emptyProgram = glCreateProgram_nid_postfix();
    glLinkProgram_nid_postfix(emptyProgram);
    CHECK_ERROR(0);
    GLint linked = 1;
    glGetProgramiv_nid_postfix(emptyProgram, 0x8b82, &linked);
    CHECK(linked == 0);
    CHECK(!ProgramInfoLog(emptyProgram).empty());

    const GLuint onlyVs = CompileShader(0x8b31, simpleVs, true);
    glAttachShader_nid_postfix(emptyProgram, onlyVs);
    glLinkProgram_nid_postfix(emptyProgram);
    glGetProgramiv_nid_postfix(emptyProgram, 0x8b82, &linked);
    CHECK(linked == 0);
    CHECK(!ProgramInfoLog(emptyProgram).empty());
    glDeleteProgram_nid_postfix(emptyProgram);
    glDeleteShader_nid_postfix(onlyVs);

    const GLuint uncompiledVs = glCreateShader_nid_postfix(0x8b31);
    const GLuint compiledFs = CompileShader(0x8b30, simpleFs, true);
    const GLuint uncompiledProgram = glCreateProgram_nid_postfix();
    glShaderSource_nid_postfix(uncompiledVs, 1, &simpleVs, nullptr);
    glAttachShader_nid_postfix(uncompiledProgram, uncompiledVs);
    glAttachShader_nid_postfix(uncompiledProgram, compiledFs);
    glLinkProgram_nid_postfix(uncompiledProgram);
    glGetProgramiv_nid_postfix(uncompiledProgram, 0x8b82, &linked);
    CHECK(linked == 0);
    glDeleteProgram_nid_postfix(uncompiledProgram);
    glDeleteShader_nid_postfix(uncompiledVs);
    glDeleteShader_nid_postfix(compiledFs);

    const GLuint missingVarying = LinkProgramFromSources(
        simpleVs,
        "precision mediump float;\n"
        "varying vec2 vMissing;\n"
        "void main() { gl_FragColor = vec4(vMissing, 0.0, 1.0); }\n",
        false);
    CHECK(!ProgramInfoLog(missingVarying).empty());
    glDeleteProgram_nid_postfix(missingVarying);

    const GLuint unusedVaryingOk = LinkProgramFromSources(
        simpleVs,
        "precision mediump float;\n"
        "varying vec2 vUnused;\n"
        "void main() { gl_FragColor = vec4(1.0); }\n",
        true);
    glDeleteProgram_nid_postfix(unusedVaryingOk);

    const GLuint varyingMismatch = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "varying vec2 vVal;\n"
        "void main() { vVal = aPos.xy; gl_Position = aPos; }\n",
        "precision mediump float;\n"
        "varying vec3 vVal;\n"
        "void main() { gl_FragColor = vec4(vVal, 1.0); }\n",
        false);
    CHECK(!ProgramInfoLog(varyingMismatch).empty());
    glDeleteProgram_nid_postfix(varyingMismatch);

    const GLuint uniformTypeMismatch = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "uniform vec2 uShared;\n"
        "void main() { gl_Position = aPos + vec4(uShared, 0.0, 0.0); }\n",
        "precision mediump float;\n"
        "uniform vec4 uShared;\n"
        "void main() { gl_FragColor = uShared; }\n",
        false);
    CHECK(!ProgramInfoLog(uniformTypeMismatch).empty());
    glDeleteProgram_nid_postfix(uniformTypeMismatch);

    const GLuint tooManyAttribs = LinkProgramFromSources(
        "attribute vec4 a0; attribute vec4 a1; attribute vec4 a2; attribute vec4 a3;\n"
        "attribute vec4 a4; attribute vec4 a5; attribute vec4 a6; attribute vec4 a7;\n"
        "attribute vec4 a8;\n"
        "void main() { gl_Position = a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8; }\n",
        simpleFs,
        false);
    CHECK(!ProgramInfoLog(tooManyAttribs).empty());
    glDeleteProgram_nid_postfix(tooManyAttribs);

    const GLuint vertexSamplerDisallowed = LinkProgramFromSources(
        "attribute vec2 aUv;\n"
        "uniform sampler2D uTex;\n"
        "void main() { gl_Position = texture2D(uTex, aUv); }\n",
        simpleFs,
        false);
    CHECK(!ProgramInfoLog(vertexSamplerDisallowed).empty());
    glDeleteProgram_nid_postfix(vertexSamplerDisallowed);

    const GLuint tooManyFragUniforms = LinkProgramFromSources(
        simpleVs,
        "precision mediump float;\n"
        "uniform vec4 uTooMany[17];\n"
        "void main() { gl_FragColor = uTooMany[0] + uTooMany[16]; }\n",
        false);
    CHECK(!ProgramInfoLog(tooManyFragUniforms).empty());
    glDeleteProgram_nid_postfix(tooManyFragUniforms);

    const GLuint tooManyVaryings = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "varying vec4 vArr[9];\n"
        "void main() { for (int i = 0; i < 9; ++i) vArr[i] = aPos; gl_Position = aPos; }\n",
        "precision mediump float;\n"
        "varying vec4 vArr[9];\n"
        "void main() { gl_FragColor = vArr[0] + vArr[8]; }\n",
        false);
    CHECK(!ProgramInfoLog(tooManyVaryings).empty());
    glDeleteProgram_nid_postfix(tooManyVaryings);

    const GLuint missingMain = LinkProgramFromSources(
        "attribute vec4 aPos;\n",
        simpleFs,
        false);
    CHECK(!ProgramInfoLog(missingMain).empty());
    glDeleteProgram_nid_postfix(missingMain);

    const GLuint uniformPrecisionMismatch = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "uniform highp float uPrec;\n"
        "void main() { gl_Position = aPos * uPrec; }\n",
        "precision mediump float;\n"
        "uniform mediump float uPrec;\n"
        "void main() { gl_FragColor = vec4(uPrec); }\n",
        false);
    CHECK(!ProgramInfoLog(uniformPrecisionMismatch).empty());
    glDeleteProgram_nid_postfix(uniformPrecisionMismatch);

    const GLuint unwrittenDeclaredVaryingOk = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "varying vec4 vDeclaredOnly;\n"
        "void main() { gl_Position = aPos; }\n",
        "precision mediump float;\n"
        "varying vec4 vDeclaredOnly;\n"
        "void main() { gl_FragColor = vDeclaredOnly; }\n",
        true);
    glDeleteProgram_nid_postfix(unwrittenDeclaredVaryingOk);

    const GLuint hugeUniformArray = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "uniform vec4 uHuge[100000000];\n"
        "void main() { gl_Position = uHuge[int(aPos.x)]; }\n",
        simpleFs,
        false);
    CHECK_ERROR(0);
    CHECK(!ProgramInfoLog(hugeUniformArray).empty());
    glDeleteProgram_nid_postfix(hugeUniformArray);

    const GLuint appendixA7Varyings = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "varying vec3 v3[4];\n"
        "varying vec2 v2a[4];\n"
        "varying vec2 v2b[2];\n"
        "varying float vf[6];\n"
        "void main() {\n"
        "    for (int i = 0; i < 4; ++i) { v3[i] = aPos.xyz; v2a[i] = aPos.xy; }\n"
        "    for (int i = 0; i < 2; ++i) v2b[i] = aPos.zw;\n"
        "    for (int i = 0; i < 6; ++i) vf[i] = aPos.x;\n"
        "    gl_Position = aPos;\n"
        "}\n",
        "precision mediump float;\n"
        "varying vec3 v3[4];\n"
        "varying vec2 v2a[4];\n"
        "varying vec2 v2b[2];\n"
        "varying float vf[6];\n"
        "void main() {\n"
        "    gl_FragColor = vec4(v3[0] + v3[3], vf[0] + vf[5]) + vec4(v2a[0] + v2a[3], v2b[0] + v2b[1]);\n"
        "}\n",
        true);
    glDeleteProgram_nid_postfix(appendixA7Varyings);

    const GLuint appendixA7FragmentUniforms = LinkProgramFromSources(
        simpleVs,
        "precision mediump float;\n"
        "uniform vec3 u3[8];\n"
        "uniform vec2 u2a[8];\n"
        "uniform vec2 u2b[4];\n"
        "uniform float uf[12];\n"
        "void main() {\n"
        "    gl_FragColor = vec4(u3[0] + u3[7], uf[0] + uf[11]) + vec4(u2a[0] + u2a[7], u2b[0] + u2b[3]);\n"
        "}\n",
        true);
    glDeleteProgram_nid_postfix(appendixA7FragmentUniforms);

    const GLuint appendixA7BestFitSingleCol = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "varying vec4 va; varying vec4 vb; varying vec4 vc;\n"
        "varying vec3 vd;\n"
        "varying vec2 ve[4];\n"
        "varying float vi[4]; varying float vj[3]; varying float vk[2];\n"
        "void main() {\n"
        "    va = aPos; vb = aPos; vc = aPos; vd = aPos.xyz;\n"
        "    for (int i = 0; i < 4; ++i) { ve[i] = aPos.xy; vi[i] = aPos.x; }\n"
        "    for (int i = 0; i < 3; ++i) vj[i] = aPos.y;\n"
        "    for (int i = 0; i < 2; ++i) vk[i] = aPos.z;\n"
        "    gl_Position = aPos;\n"
        "}\n",
        "precision mediump float;\n"
        "varying vec4 va; varying vec4 vb; varying vec4 vc;\n"
        "varying vec3 vd;\n"
        "varying vec2 ve[4];\n"
        "varying float vi[4]; varying float vj[3]; varying float vk[2];\n"
        "void main() {\n"
        "    gl_FragColor = va + vb + vc + vec4(vd, vi[3] + vj[2] + vk[1]) + vec4(ve[3], 0.0, 0.0);\n"
        "}\n",
        true);
    glDeleteProgram_nid_postfix(appendixA7BestFitSingleCol);

    const GLuint appendixA7Mat2FullRowsFit = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "varying mat2 vm[4];\n"
        "void main() {\n"
        "    for (int i = 0; i < 4; ++i) vm[i] = mat2(aPos.x, aPos.y, aPos.z, aPos.w);\n"
        "    gl_Position = aPos;\n"
        "}\n",
        "precision mediump float;\n"
        "varying mat2 vm[4];\n"
        "void main() { gl_FragColor = vec4(vm[0][0], vm[3][1]); }\n",
        true);
    glDeleteProgram_nid_postfix(appendixA7Mat2FullRowsFit);

    const GLuint appendixA7Mat2FullRowsOverflow = LinkProgramFromSources(
        "attribute vec4 aPos;\n"
        "varying mat2 vm[4];\n"
        "varying float vExtra;\n"
        "void main() {\n"
        "    for (int i = 0; i < 4; ++i) vm[i] = mat2(aPos.x, aPos.y, aPos.z, aPos.w);\n"
        "    vExtra = aPos.x;\n"
        "    gl_Position = aPos;\n"
        "}\n",
        "precision mediump float;\n"
        "varying mat2 vm[4];\n"
        "varying float vExtra;\n"
        "void main() { gl_FragColor = vec4(vm[0][0], vm[3][1]) + vec4(vExtra); }\n",
        false);
    CHECK(!ProgramInfoLog(appendixA7Mat2FullRowsOverflow).empty());
    glDeleteProgram_nid_postfix(appendixA7Mat2FullRowsOverflow);

    const GLuint vsObj = CompileShader(0x8b31, simpleVs, true);
    glLinkProgram_nid_postfix(vsObj);
    CHECK_ERROR(0x0502);
    glLinkProgram_nid_postfix(99999);
    CHECK_ERROR(0x0501);
    glDeleteShader_nid_postfix(vsObj);
    CHECK_ERROR(0);
}

void TestAttributeBindingAndReflection() {
    const char* vsSrc =
        "attribute vec3 aPos;\n"
        "attribute mat3 aBasis;\n"
        "attribute vec2 aUv;\n"
        "attribute vec4 aUnused;\n"
        "varying vec2 vUv;\n"
        "void main() {\n"
        "    vUv = aUv;\n"
        "    gl_Position = vec4(aBasis * aPos, 1.0);\n"
        "}\n";
    const char* fsSrc =
        "precision mediump float;\n"
        "varying vec2 vUv;\n"
        "void main() { gl_FragColor = vec4(vUv, 0.0, 1.0); }\n";

    const GLuint vs = CompileShader(0x8b31, vsSrc, true);
    const GLuint fs = CompileShader(0x8b30, fsSrc, true);
    const GLuint program = glCreateProgram_nid_postfix();
    glAttachShader_nid_postfix(program, vs);
    glAttachShader_nid_postfix(program, fs);

    glBindAttribLocation_nid_postfix(program, 2, "aBasis");
    glBindAttribLocation_nid_postfix(program, 5, "aPos");
    glBindAttribLocation_nid_postfix(program, 7, "aNotInShader");
    glLinkProgram_nid_postfix(program);
    CHECK_ERROR(0);

    GLint linked = 0;
    glGetProgramiv_nid_postfix(program, 0x8b82, &linked);
    CHECK(linked == 1);

    CHECK(glGetAttribLocation_nid_postfix(program, "aBasis") == 2);
    CHECK(glGetAttribLocation_nid_postfix(program, "aPos") == 5);
    const GLint uvLoc = glGetAttribLocation_nid_postfix(program, "aUv");
    CHECK(uvLoc >= 0 && uvLoc < 8 && uvLoc != 2 && uvLoc != 3 && uvLoc != 4 && uvLoc != 5);
    CHECK(glGetAttribLocation_nid_postfix(program, "aUnused") == -1);
    CHECK(glGetAttribLocation_nid_postfix(program, "aNotInShader") == -1);
    CHECK(glGetAttribLocation_nid_postfix(program, "gl_Position") == -1);
    CHECK_ERROR(0);

    glBindAttribLocation_nid_postfix(program, 0, "aPos");
    CHECK(glGetAttribLocation_nid_postfix(program, "aPos") == 5);

    GLint activeAttribs = 0;
    GLint maxAttribLen = 0;
    glGetProgramiv_nid_postfix(program, 0x8b89, &activeAttribs);
    glGetProgramiv_nid_postfix(program, 0x8b8a, &maxAttribLen);
    CHECK(activeAttribs == 3);
    CHECK(maxAttribLen == 7);

    std::map<std::string, std::pair<GLint, GLenum>> found;
    for (GLuint i = 0; i < static_cast<GLuint>(activeAttribs); ++i) {
        char buf[32]{};
        GLsizei len = -1;
        GLint sz = -1;
        GLenum type = 0;
        glGetActiveAttrib_nid_postfix(program, i, sizeof(buf), &len, &sz, &type, buf);
        CHECK_ERROR(0);
        CHECK(len == static_cast<GLsizei>(std::strlen(buf)));
        found[buf] = {sz, type};
    }
    CHECK(found["aPos"] == std::make_pair(1, GLenum(0x8b51)));
    CHECK(found["aBasis"] == std::make_pair(1, GLenum(0x8b5b)));
    CHECK(found["aUv"] == std::make_pair(1, GLenum(0x8b50)));

    char shortBuf[4]{'z', 'z', 'z', 'z'};
    GLsizei shortLen = -1;
    GLint sz = -1;
    GLenum type = 0;
    glGetActiveAttrib_nid_postfix(program, 0, 3, &shortLen, &sz, &type, shortBuf);
    CHECK_ERROR(0);
    CHECK(shortLen == 2 && shortBuf[2] == '\0' && shortBuf[3] == 'z');

    glGetActiveAttrib_nid_postfix(program, 0, 0, &shortLen, &sz, &type, shortBuf);
    CHECK_ERROR(0);
    CHECK(shortLen == 0);

    glGetActiveAttrib_nid_postfix(program, 3, sizeof(shortBuf), &shortLen, &sz, &type, shortBuf);
    CHECK_ERROR(0x0501);
    glGetActiveAttrib_nid_postfix(program, 0, -1, &shortLen, &sz, &type, shortBuf);
    CHECK_ERROR(0x0501);

    glBindAttribLocation_nid_postfix(program, 2, "aBasis");
    glBindAttribLocation_nid_postfix(program, 4, "aPos");
    glLinkProgram_nid_postfix(program);
    glGetProgramiv_nid_postfix(program, 0x8b82, &linked);
    CHECK(linked == 0);

    glBindAttribLocation_nid_postfix(program, 6, "aBasis");
    glBindAttribLocation_nid_postfix(program, 0, "aPos");
    glLinkProgram_nid_postfix(program);
    glGetProgramiv_nid_postfix(program, 0x8b82, &linked);
    CHECK(linked == 0);

    CHECK(glGetAttribLocation_nid_postfix(program, "aPos") == -1);
    CHECK_ERROR(0x0502);

    glDeleteProgram_nid_postfix(program);
    glDeleteShader_nid_postfix(vs);
    glDeleteShader_nid_postfix(fs);
    CHECK_ERROR(0);
}

void TestUniformReflectionAndAccessors() {
    const char* vsSrc =
        "attribute vec4 aPos;\n"
        "uniform float uFloat;\n"
        "uniform vec2 uVec2;\n"
        "uniform vec3 uVec3;\n"
        "uniform vec4 uVec4;\n"
        "uniform int uInt;\n"
        "uniform ivec2 uIvec2;\n"
        "uniform ivec3 uIvec3;\n"
        "uniform ivec4 uIvec4;\n"
        "uniform bool uBool;\n"
        "uniform bvec2 uBvec2;\n"
        "uniform bvec3 uBvec3;\n"
        "uniform bvec4 uBvec4;\n"
        "uniform mat2 uMat2;\n"
        "uniform mat3 uMat3;\n"
        "uniform mat4 uMat4;\n"
        "uniform float uSingleArr[1];\n"
        "uniform vec2 uArr[3];\n"
        "struct Light { vec3 color; float intensity[2]; };\n"
        "uniform Light uLights[2];\n"
        "varying vec4 vOut;\n"
        "void main() {\n"
        "    float acc = uFloat + uVec2.x + uVec3.x + uVec4.x;\n"
        "    acc += float(uInt + uIvec2.x + uIvec3.x + uIvec4.x);\n"
        "    acc += uBool ? 1.0 : 0.0;\n"
        "    acc += uBvec2.x ? 1.0 : 0.0;\n"
        "    acc += uBvec3.x ? 1.0 : 0.0;\n"
        "    acc += uBvec4.x ? 1.0 : 0.0;\n"
        "    acc += (uMat2 * aPos.xy).x + (uMat3 * aPos.xyz).x;\n"
        "    acc += uSingleArr[0] + uArr[0].x + uArr[2].y;\n"
        "    acc += uLights[0].color.x + uLights[0].intensity[1];\n"
        "    acc += uLights[1].color.y + uLights[1].intensity[1];\n"
        "    acc += gl_DepthRange.near;\n"
        "    vOut = vec4(acc);\n"
        "    gl_Position = uMat4 * aPos;\n"
        "}\n";
    const char* fsSrc =
        "precision mediump float;\n"
        "uniform sampler2D uTex2D;\n"
        "uniform samplerCube uTexCube;\n"
        "varying vec4 vOut;\n"
        "void main() {\n"
        "    gl_FragColor = vOut + texture2D(uTex2D, vOut.xy) + textureCube(uTexCube, vOut.xyz);\n"
        "}\n";

    const GLuint program = LinkProgramFromSources(vsSrc, fsSrc, true);
    CHECK(glGetUniformLocation_nid_postfix(program, "gl_DepthRange.near") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "gl_DepthRange") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "uFloat[0]") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "uArr[00]") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "uArr[01]") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "uArr[-1]") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "uArr[+1]") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "uArr[ 1 ]") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "uArr[3]") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, " uFloat") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "uLights") == -1);
    CHECK(glGetUniformLocation_nid_postfix(program, "uLights[0]") == -1);
    CHECK_ERROR(0);

    GLint activeUniforms = 0;
    GLint maxUniformLen = 0;
    glGetProgramiv_nid_postfix(program, 0x8b86, &activeUniforms);
    glGetProgramiv_nid_postfix(program, 0x8b87, &maxUniformLen);
    CHECK(activeUniforms == 23);
    CHECK(maxUniformLen > 10);

    std::map<std::string, std::pair<GLint, GLenum>> uniforms;
    for (GLuint i = 0; i < static_cast<GLuint>(activeUniforms); ++i) {
        char buf[64]{};
        GLsizei len = -1;
        GLint sz = -1;
        GLenum type = 0;
        glGetActiveUniform_nid_postfix(program, i, sizeof(buf), &len, &sz, &type, buf);
        CHECK_ERROR(0);
        CHECK(len == static_cast<GLsizei>(std::strlen(buf)));
        CHECK(std::string(buf).rfind("gl_", 0) != 0);
        uniforms[buf] = {sz, type};
    }
    CHECK(uniforms["uFloat"] == std::make_pair(1, GLenum(0x1406)));
    CHECK(uniforms["uVec4"] == std::make_pair(1, GLenum(0x8b52)));
    CHECK(uniforms["uIvec3"] == std::make_pair(1, GLenum(0x8b54)));
    CHECK(uniforms["uBvec2"] == std::make_pair(1, GLenum(0x8b57)));
    CHECK(uniforms["uMat4"] == std::make_pair(1, GLenum(0x8b5c)));
    CHECK(uniforms["uSingleArr[0]"] == std::make_pair(1, GLenum(0x1406)));
    CHECK(uniforms["uArr[0]"] == std::make_pair(3, GLenum(0x8b50)));
    CHECK(uniforms["uLights[0].color"] == std::make_pair(1, GLenum(0x8b51)));
    CHECK(uniforms["uLights[1].intensity[0]"] == std::make_pair(2, GLenum(0x1406)));
    CHECK(uniforms["uTex2D"] == std::make_pair(1, GLenum(0x8b5e)));
    CHECK(uniforms["uTexCube"] == std::make_pair(1, GLenum(0x8b60)));

    const GLint locFloat = glGetUniformLocation_nid_postfix(program, "uFloat");
    const GLint locVec2 = glGetUniformLocation_nid_postfix(program, "uVec2");
    const GLint locVec3 = glGetUniformLocation_nid_postfix(program, "uVec3");
    const GLint locVec4 = glGetUniformLocation_nid_postfix(program, "uVec4");
    const GLint locInt = glGetUniformLocation_nid_postfix(program, "uInt");
    const GLint locIvec2 = glGetUniformLocation_nid_postfix(program, "uIvec2");
    const GLint locIvec3 = glGetUniformLocation_nid_postfix(program, "uIvec3");
    const GLint locIvec4 = glGetUniformLocation_nid_postfix(program, "uIvec4");
    const GLint locBool = glGetUniformLocation_nid_postfix(program, "uBool");
    const GLint locBvec2 = glGetUniformLocation_nid_postfix(program, "uBvec2");
    const GLint locBvec3 = glGetUniformLocation_nid_postfix(program, "uBvec3");
    const GLint locBvec4 = glGetUniformLocation_nid_postfix(program, "uBvec4");
    const GLint locMat2 = glGetUniformLocation_nid_postfix(program, "uMat2");
    const GLint locMat3 = glGetUniformLocation_nid_postfix(program, "uMat3");
    const GLint locMat4 = glGetUniformLocation_nid_postfix(program, "uMat4");
    const GLint locSingle = glGetUniformLocation_nid_postfix(program, "uSingleArr");
    const GLint locSingle0 = glGetUniformLocation_nid_postfix(program, "uSingleArr[0]");
    const GLint locArr = glGetUniformLocation_nid_postfix(program, "uArr");
    const GLint locArr0 = glGetUniformLocation_nid_postfix(program, "uArr[0]");
    const GLint locArr1 = glGetUniformLocation_nid_postfix(program, "uArr[1]");
    const GLint locArr2 = glGetUniformLocation_nid_postfix(program, "uArr[2]");
    const GLint locLightColor = glGetUniformLocation_nid_postfix(program, "uLights[1].color");
    const GLint locLightInt1 = glGetUniformLocation_nid_postfix(program, "uLights[0].intensity[1]");
    const GLint locTex2D = glGetUniformLocation_nid_postfix(program, "uTex2D");
    const GLint locTexCube = glGetUniformLocation_nid_postfix(program, "uTexCube");
    CHECK(locFloat >= 0 && locSingle == locSingle0 && locArr == locArr0 && locArr1 == locArr0 + 1 && locArr2 == locArr0 + 2);
    CHECK(locLightColor >= 0 && locLightInt1 >= 0 && locTex2D >= 0 && locTexCube >= 0);

    glUniform1f_nid_postfix(locFloat, 1.0f);
    CHECK_ERROR(0x0502);
    glUniform1f_nid_postfix(-1, 1.0f);
    CHECK_ERROR(0x0502);

    glUseProgram_nid_postfix(program);
    CHECK_ERROR(0);
    GLint boundProgram = 0;
    glGetIntegerv_nid_postfix(0x8b8d, &boundProgram);
    CHECK(boundProgram == static_cast<GLint>(program));

    GLfloat fInit = 99.0f;
    glGetUniformfv_nid_postfix(program, locFloat, &fInit);
    CHECK_ERROR(0);
    CHECK(fInit == 0.0f);

    glUniform1f_nid_postfix(-1, 42.0f);
    glUniform1fv_nid_postfix(-1, 1, reinterpret_cast<const GLfloat*>(0x10));
    glUniformMatrix4fv_nid_postfix(-1, 0, 0, reinterpret_cast<const GLfloat*>(0x10));
    CHECK_ERROR(0);

    glUniform1fv_nid_postfix(-1, -1, nullptr);
    CHECK_ERROR(0x0501);
    glUniformMatrix2fv_nid_postfix(-1, 1, 1, nullptr);
    CHECK_ERROR(0x0501);
    glUniform1f_nid_postfix(-2, 1.0f);
    CHECK_ERROR(0x0502);
    glUniform1f_nid_postfix(9999, 1.0f);
    CHECK_ERROR(0x0502);

    glUniform1f_nid_postfix(locFloat, 3.6f);
    glUniform2f_nid_postfix(locVec2, -1.6f, 2.2f);
    glUniform3f_nid_postfix(locVec3, 1.0f, 2.0f, 3.0f);
    glUniform4f_nid_postfix(locVec4, 4.0f, 5.0f, 6.0f, 7.0f);
    glUniform1i_nid_postfix(locInt, -42);
    glUniform2i_nid_postfix(locIvec2, 10, 20);
    glUniform3i_nid_postfix(locIvec3, 30, 40, 50);
    glUniform4i_nid_postfix(locIvec4, 60, 70, 80, 90);
    glUniform1f_nid_postfix(locBool, -0.25f);
    glUniform2i_nid_postfix(locBvec2, 0, 7);
    const GLfloat b3[]{0.0f, 0.0f, 9.0f};
    glUniform3fv_nid_postfix(locBvec3, 1, b3);
    const GLint b4[]{5, 0, -3, 0};
    glUniform4iv_nid_postfix(locBvec4, 1, b4);
    const GLfloat m2[]{1, 2, 3, 4};
    const GLfloat m3[]{1, 2, 3, 4, 5, 6, 7, 8, 9};
    const GLfloat m4[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    glUniformMatrix2fv_nid_postfix(locMat2, 1, 0, m2);
    glUniformMatrix3fv_nid_postfix(locMat3, 1, 0, m3);
    glUniformMatrix4fv_nid_postfix(locMat4, 1, 0, m4);
    const GLfloat singleVals[]{7.5f, 99.0f};
    glUniform1fv_nid_postfix(locSingle, 2, singleVals);
    const GLfloat arrVals[]{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    glUniform2fv_nid_postfix(locArr1, 3, arrVals);
    const GLfloat colorVals[]{0.25f, 0.5f, 0.75f};
    glUniform3fv_nid_postfix(locLightColor, 1, colorVals);
    glUniform1f_nid_postfix(locLightInt1, 11.25f);
    glUniform1i_nid_postfix(locTex2D, 2);
    const GLint cubeUnit = 3;
    glUniform1iv_nid_postfix(locTexCube, 1, &cubeUnit);
    CHECK_ERROR(0);

    GLfloat fOut[16]{};
    GLint iOut[16]{};
    glGetUniformfv_nid_postfix(program, locFloat, fOut);
    glGetUniformiv_nid_postfix(program, locFloat, iOut);
    CHECK(fOut[0] == 3.6f && iOut[0] == 4);

    glGetUniformiv_nid_postfix(program, locVec2, iOut);
    CHECK(iOut[0] == -2 && iOut[1] == 2);

    glGetUniformfv_nid_postfix(program, locVec3, fOut);
    CHECK(fOut[0] == 1.0f && fOut[1] == 2.0f && fOut[2] == 3.0f);

    glGetUniformfv_nid_postfix(program, locVec4, fOut);
    CHECK(fOut[0] == 4.0f && fOut[1] == 5.0f && fOut[2] == 6.0f && fOut[3] == 7.0f);

    glGetUniformfv_nid_postfix(program, locInt, fOut);
    glGetUniformiv_nid_postfix(program, locInt, iOut);
    CHECK(fOut[0] == -42.0f && iOut[0] == -42);

    glGetUniformiv_nid_postfix(program, locIvec2, iOut);
    CHECK(iOut[0] == 10 && iOut[1] == 20);
    glGetUniformiv_nid_postfix(program, locIvec3, iOut);
    CHECK(iOut[0] == 30 && iOut[1] == 40 && iOut[2] == 50);
    glGetUniformiv_nid_postfix(program, locIvec4, iOut);
    CHECK(iOut[0] == 60 && iOut[1] == 70 && iOut[2] == 80 && iOut[3] == 90);

    glGetUniformiv_nid_postfix(program, locBool, iOut);
    glGetUniformfv_nid_postfix(program, locBool, fOut);
    CHECK(iOut[0] == 1 && fOut[0] == 1.0f);
    glGetUniformiv_nid_postfix(program, locBvec2, iOut);
    CHECK(iOut[0] == 0 && iOut[1] == 1);
    glGetUniformiv_nid_postfix(program, locBvec3, iOut);
    CHECK(iOut[0] == 0 && iOut[1] == 0 && iOut[2] == 1);
    glGetUniformiv_nid_postfix(program, locBvec4, iOut);
    CHECK(iOut[0] == 1 && iOut[1] == 0 && iOut[2] == 1 && iOut[3] == 0);

    glGetUniformfv_nid_postfix(program, locMat2, fOut);
    CHECK(std::memcmp(fOut, m2, sizeof(m2)) == 0);
    glGetUniformfv_nid_postfix(program, locMat3, fOut);
    CHECK(std::memcmp(fOut, m3, sizeof(m3)) == 0);
    glGetUniformfv_nid_postfix(program, locMat4, fOut);
    CHECK(std::memcmp(fOut, m4, sizeof(m4)) == 0);

    glGetUniformfv_nid_postfix(program, locSingle, fOut);
    CHECK(fOut[0] == 7.5f);
    glGetUniformfv_nid_postfix(program, locArr0, fOut);
    CHECK(fOut[0] == 0.0f && fOut[1] == 0.0f);
    glGetUniformfv_nid_postfix(program, locArr1, fOut);
    CHECK(fOut[0] == 1.0f && fOut[1] == 2.0f);
    glGetUniformfv_nid_postfix(program, locArr2, fOut);
    CHECK(fOut[0] == 3.0f && fOut[1] == 4.0f);
    glGetUniformfv_nid_postfix(program, locLightColor, fOut);
    CHECK(fOut[0] == 0.25f && fOut[1] == 0.5f && fOut[2] == 0.75f);
    glGetUniformfv_nid_postfix(program, locLightInt1, fOut);
    CHECK(fOut[0] == 11.25f);
    glGetUniformiv_nid_postfix(program, locTex2D, iOut);
    CHECK(iOut[0] == 2);
    glGetUniformiv_nid_postfix(program, locTexCube, iOut);
    CHECK(iOut[0] == 3);
    CHECK_ERROR(0);

    const GLfloat hugeArrVals[]{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
    glUniform2fv_nid_postfix(locArr0, std::numeric_limits<GLsizei>::max(), hugeArrVals);
    CHECK_ERROR(0);
    glGetUniformfv_nid_postfix(program, locArr0, fOut);
    CHECK(fOut[0] == 10.0f && fOut[1] == 20.0f);
    glGetUniformfv_nid_postfix(program, locArr2, fOut);
    CHECK(fOut[0] == 50.0f && fOut[1] == 60.0f);

    glUniform2fv_nid_postfix(locArr0, 0, nullptr);
    glUniformMatrix4fv_nid_postfix(locMat4, 0, 0, nullptr);
    CHECK_ERROR(0);
    glUniform3fv_nid_postfix(locArr0, 0, nullptr);
    CHECK_ERROR(0x0502);
    glUniformMatrix3fv_nid_postfix(locMat4, 0, 0, nullptr);
    CHECK_ERROR(0x0502);

    const std::string longName(5000, 'a');
    CHECK(glGetAttribLocation_nid_postfix(program, longName.c_str()) == -1);
    CHECK_ERROR(0);
    CHECK(glGetUniformLocation_nid_postfix(program, longName.c_str()) == -1);
    CHECK_ERROR(0);
    glBindAttribLocation_nid_postfix(program, 0, longName.c_str());
    CHECK_ERROR(0);
    const std::string longGlName = std::string("gl_") + longName;
    glBindAttribLocation_nid_postfix(program, 0, longGlName.c_str());
    CHECK_ERROR(0x0502);

    const GLfloat twoFloats[]{1.0f, 2.0f};
    glUniform1fv_nid_postfix(locFloat, 2, twoFloats);
    CHECK_ERROR(0x0502);
    glUniform1i_nid_postfix(locFloat, 1);
    CHECK_ERROR(0x0502);
    glUniform1f_nid_postfix(locInt, 1.0f);
    CHECK_ERROR(0x0502);
    glUniform1f_nid_postfix(locTex2D, 0.0f);
    CHECK_ERROR(0x0502);
    glUniform2f_nid_postfix(locFloat, 1.0f, 2.0f);
    CHECK_ERROR(0x0502);
    glUniformMatrix3fv_nid_postfix(locMat4, 1, 0, m3);
    CHECK_ERROR(0x0502);
    glUniformMatrix4fv_nid_postfix(locMat4, 1, 1, m4);
    CHECK_ERROR(0x0501);
    glGetUniformfv_nid_postfix(program, -1, fOut);
    CHECK_ERROR(0x0502);
    glGetUniformiv_nid_postfix(program, 9999, iOut);
    CHECK_ERROR(0x0502);

    glValidateProgram_nid_postfix(program);
    CHECK_ERROR(0);
    GLint validated = 0;
    glGetProgramiv_nid_postfix(program, 0x8b83, &validated);
    CHECK(validated == 1);

    glUniform1i_nid_postfix(locTexCube, 2);
    glValidateProgram_nid_postfix(program);
    glGetProgramiv_nid_postfix(program, 0x8b83, &validated);
    CHECK(validated == 0);
    CHECK(!ProgramInfoLog(program).empty());

    glUniform1i_nid_postfix(locTexCube, 8);
    glValidateProgram_nid_postfix(program);
    glGetProgramiv_nid_postfix(program, 0x8b83, &validated);
    CHECK(validated == 0);

    glUniform1i_nid_postfix(locTexCube, -1);
    glValidateProgram_nid_postfix(program);
    glGetProgramiv_nid_postfix(program, 0x8b83, &validated);
    CHECK(validated == 0);

    glUniform1i_nid_postfix(locTexCube, 4);
    glValidateProgram_nid_postfix(program);
    glGetProgramiv_nid_postfix(program, 0x8b83, &validated);
    CHECK(validated == 1);
    CHECK(ProgramInfoLog(program).empty());

    glUseProgram_nid_postfix(0);
    glGetIntegerv_nid_postfix(0x8b8d, &boundProgram);
    CHECK(boundProgram == 0);
    glDeleteProgram_nid_postfix(program);
    CHECK_ERROR(0);
}

void TestRelinkAndDeferredProgramDeletion(EGLDisplay display, EGLConfig config, EGLSurface surface, EGLContext primaryContext) {
    const char* vsSrc =
        "attribute vec4 aPos;\n"
        "uniform float uScale;\n"
        "void main() { gl_Position = aPos * uScale; }\n";
    const char* fsSrc =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(1.0); }\n";

    const GLuint vs = CompileShader(0x8b31, vsSrc, true);
    const GLuint fs = CompileShader(0x8b30, fsSrc, true);
    const GLuint program = glCreateProgram_nid_postfix();
    glAttachShader_nid_postfix(program, vs);
    glAttachShader_nid_postfix(program, fs);
    glLinkProgram_nid_postfix(program);
    glUseProgram_nid_postfix(program);
    CHECK_ERROR(0);

    const GLint locScale = glGetUniformLocation_nid_postfix(program, "uScale");
    CHECK(locScale >= 0);
    glUniform1f_nid_postfix(locScale, 5.0f);
    CHECK_ERROR(0);

    glLinkProgram_nid_postfix(program);
    CHECK_ERROR(0);
    GLfloat scaleVal = 99.0f;
    glGetUniformfv_nid_postfix(program, locScale, &scaleVal);
    CHECK(scaleVal == 0.0f);
    glUniform1f_nid_postfix(locScale, 7.0f);
    CHECK_ERROR(0);

    glDetachShader_nid_postfix(program, fs);
    glLinkProgram_nid_postfix(program);
    CHECK_ERROR(0);
    GLint linked = 1;
    glGetProgramiv_nid_postfix(program, 0x8b82, &linked);
    CHECK(linked == 0);

    glUniform1f_nid_postfix(locScale, 9.0f);
    CHECK_ERROR(0);

    GLint currentProgram = 0;
    glGetIntegerv_nid_postfix(0x8b8d, &currentProgram);
    CHECK(currentProgram == static_cast<GLint>(program));

    glUseProgram_nid_postfix(program);
    CHECK_ERROR(0x0502);
    glUniform1f_nid_postfix(locScale, 11.0f);
    CHECK_ERROR(0);

    glAttachShader_nid_postfix(program, fs);
    glLinkProgram_nid_postfix(program);
    glGetProgramiv_nid_postfix(program, 0x8b82, &linked);
    CHECK(linked == 1);

    glDeleteShader_nid_postfix(vs);
    glDeleteShader_nid_postfix(fs);
    glDeleteProgram_nid_postfix(program);
    CHECK_ERROR(0);
    CHECK(glIsProgram_nid_postfix(program) == 1);
    CHECK(glIsShader_nid_postfix(vs) == 1);
    GLint deleted = 0;
    glGetProgramiv_nid_postfix(program, 0x8b80, &deleted);
    CHECK(deleted == 1);
    glGetIntegerv_nid_postfix(0x8b8d, &currentProgram);
    CHECK(currentProgram == static_cast<GLint>(program));

    glUseProgram_nid_postfix(0);
    CHECK_ERROR(0);
    CHECK(glIsProgram_nid_postfix(program) == 0);
    CHECK(glIsShader_nid_postfix(vs) == 0);
    CHECK(glIsShader_nid_postfix(fs) == 0);

    const GLuint sharedProg = LinkProgramFromSources(vsSrc, fsSrc, true);
    const EGLint ctxAttribs[]{0x3098, 2, 0x3038};
    const EGLContext sharedContext = eglCreateContext_nid_postfix(display, config, primaryContext, ctxAttribs);
    CHECK(sharedContext != nullptr);
    CHECK(eglMakeCurrent_nid_postfix(display, surface, surface, sharedContext) == 1);
    glUseProgram_nid_postfix(sharedProg);
    glDeleteProgram_nid_postfix(sharedProg);
    CHECK(glIsProgram_nid_postfix(sharedProg) == 1);
    CHECK(eglDestroyContext_nid_postfix(display, sharedContext) == 1);
    CHECK(eglMakeCurrent_nid_postfix(display, surface, surface, primaryContext) == 1);
    CHECK(glIsProgram_nid_postfix(sharedProg) == 0);
    CHECK_ERROR(0);
}

void TestShaderBinaryAndGuestPointerFaults() {
    const char* vsSrc =
        "attribute vec4 aPos;\n"
        "uniform vec2 uPair[2];\n"
        "uniform mat2 uM;\n"
        "void main() { gl_Position = vec4(uM * aPos.xy + uPair[0] + uPair[1], 0.0, 1.0); }\n";
    const char* fsSrc =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(1.0); }\n";

    const GLuint vs = CompileShader(0x8b31, vsSrc, true);
    const GLuint fs = CompileShader(0x8b30, fsSrc, true);
    const GLuint program = glCreateProgram_nid_postfix();
    glAttachShader_nid_postfix(program, vs);
    glAttachShader_nid_postfix(program, fs);
    glLinkProgram_nid_postfix(program);
    glUseProgram_nid_postfix(program);
    CHECK_ERROR(0);

    const GLuint pair[]{vs, fs};
    const unsigned char dummyBin[4]{1, 2, 3, 4};
    glShaderBinary_nid_postfix(2, pair, 0x1234, dummyBin, 4);
    CHECK_ERROR(0x0500);
    glShaderBinary_nid_postfix(0, nullptr, 0x1234, nullptr, 0);
    CHECK_ERROR(0x0500);
    glShaderBinary_nid_postfix(-1, pair, 0x1234, dummyBin, 4);
    CHECK_ERROR(0x0501);
    glShaderBinary_nid_postfix(1, pair, 0x1234, dummyBin, -1);
    CHECK_ERROR(0x0501);
    const GLuint dup[]{vs, vs};
    glShaderBinary_nid_postfix(2, dup, 0x1234, dummyBin, 4);
    CHECK_ERROR(0x0502);
    const GLuint progArr[]{program};
    glShaderBinary_nid_postfix(1, progArr, 0x1234, dummyBin, 4);
    CHECK_ERROR(0x0502);
    const GLuint badArr[]{99999};
    glShaderBinary_nid_postfix(1, badArr, 0x1234, dummyBin, 4);
    CHECK_ERROR(0x0501);

    void* unmapped = reinterpret_cast<void*>(0x10);

    errno = 71;
    glShaderBinary_nid_postfix(1, static_cast<const GLuint*>(unmapped), 0x1234, dummyBin, 4);
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);

    GLint range[2]{};
    GLint prec = 0;
    errno = 71;
    glGetShaderPrecisionFormat_nid_postfix(0x8b31, 0x8df2, static_cast<GLint*>(unmapped), &prec);
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);
    errno = 71;
    glGetShaderPrecisionFormat_nid_postfix(0x8b31, 0x8df2, range, static_cast<GLint*>(unmapped));
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);

    errno = 71;
    CHECK(glGetAttribLocation_nid_postfix(program, static_cast<const GLchar*>(unmapped)) == -1);
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);

    errno = 71;
    CHECK(glGetUniformLocation_nid_postfix(program, static_cast<const GLchar*>(unmapped)) == -1);
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);

    GLsizei len = 0;
    GLint cnt = 0;
    GLenum tp = 0;
    char nameBuf[16]{};
    errno = 71;
    glGetActiveAttrib_nid_postfix(program, 0, sizeof(nameBuf), &len, static_cast<GLint*>(unmapped), &tp, nameBuf);
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);
    errno = 71;
    glGetActiveAttrib_nid_postfix(program, 0, sizeof(nameBuf), &len, &cnt, static_cast<GLenum*>(unmapped), nameBuf);
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);
    errno = 71;
    glGetActiveAttrib_nid_postfix(program, 0, sizeof(nameBuf), &len, &cnt, &tp, static_cast<GLchar*>(unmapped));
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);
    errno = 71;
    glGetActiveUniform_nid_postfix(program, 0, sizeof(nameBuf), static_cast<GLsizei*>(unmapped), &cnt, &tp, nameBuf);
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);

    const GLint locPair = glGetUniformLocation_nid_postfix(program, "uPair");
    const GLint locM = glGetUniformLocation_nid_postfix(program, "uM");
    CHECK(locPair >= 0 && locM >= 0);

    errno = 71;
    glUniform2fv_nid_postfix(locPair, 1, static_cast<const GLfloat*>(unmapped));
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);

    errno = 71;
    glUniformMatrix2fv_nid_postfix(locM, 1, 0, static_cast<const GLfloat*>(unmapped));
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);

    errno = 71;
    glGetUniformfv_nid_postfix(program, locPair, static_cast<GLfloat*>(unmapped));
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);

    errno = 71;
    glGetUniformiv_nid_postfix(program, locPair, static_cast<GLint*>(unmapped));
    CHECK_ERROR(0x0501);
    CHECK(errno == 71);

    glUseProgram_nid_postfix(0);
    glDeleteProgram_nid_postfix(program);
    glDeleteShader_nid_postfix(vs);
    glDeleteShader_nid_postfix(fs);
    CHECK_ERROR(0);
}

}

int main() {
    const EGLDisplay display = eglGetDisplay_nid_postfix(nullptr);
    CHECK(display != nullptr);
    CHECK(eglInitialize_nid_postfix(display, nullptr, nullptr) == 1);
    EGLConfig config = nullptr;
    EGLint count = 0;
    CHECK(eglGetConfigs_nid_postfix(display, &config, 1, &count) == 1 && count == 1);
    const EGLint surfaceAttribs[]{0x3057, 8, 0x3056, 8, 0x3038};
    const EGLSurface surface = eglCreatePbufferSurface_nid_postfix(display, config, surfaceAttribs);
    CHECK(surface != nullptr);
    const EGLint contextAttribs[]{0x3098, 2, 0x3038};
    const EGLContext context = eglCreateContext_nid_postfix(display, config, nullptr, contextAttribs);
    CHECK(context != nullptr);
    CHECK(eglMakeCurrent_nid_postfix(display, surface, surface, context) == 1);

    TestCompilerAndPrecisionQueries();
    TestShaderCompilation();
    TestLinkingErrorsAndLimits();
    TestAttributeBindingAndReflection();
    TestUniformReflectionAndAccessors();
    TestRelinkAndDeferredProgramDeletion(display, config, surface, context);
    TestShaderBinaryAndGuestPointerFaults();

    CHECK(eglMakeCurrent_nid_postfix(display, nullptr, nullptr, nullptr) == 1);
    CHECK(eglDestroyContext_nid_postfix(display, context) == 1);
    CHECK(eglDestroySurface_nid_postfix(display, surface) == 1);
    CHECK(eglTerminate_nid_postfix(display) == 1);
    return 0;
}
