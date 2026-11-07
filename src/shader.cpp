#include "shader.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "[shader] não consegui abrir %s\n", path.c_str());
        return {};
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Compila um estágio (vertex ou fragment). O código GLSL é compilado pelo
// driver da GPU em tempo de execução, então erros só aparecem aqui.
static GLuint compileStage(GLenum type, const std::string& src, const std::string& path) {
    GLuint shader = glCreateShader(type);
    const char* code = src.c_str();
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        std::fprintf(stderr, "[shader] erro de compilação em %s:\n%s\n", path.c_str(), log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint loadProgram(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSrc = readFile(vertPath);
    std::string fragSrc = readFile(fragPath);
    if (vertSrc.empty() || fragSrc.empty()) return 0;

    GLuint vert = compileStage(GL_VERTEX_SHADER, vertSrc, vertPath);
    GLuint frag = compileStage(GL_FRAGMENT_SHADER, fragSrc, fragPath);

    GLuint program = 0;
    if (vert && frag) {
        // Linkar = conectar as saídas do vertex shader às entradas do fragment shader.
        program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glLinkProgram(program);

        GLint ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[4096];
            glGetProgramInfoLog(program, sizeof log, nullptr, log);
            std::fprintf(stderr, "[shader] erro de link:\n%s\n", log);
            glDeleteProgram(program);
            program = 0;
        }
    }
    // Depois de linkados, os estágios não são mais necessários (glDeleteShader(0) é ignorado).
    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}
