#pragma once
#include <glad/gl.h>
#include <string>

// Lê, compila e linka um programa (vertex + fragment shader).
// Retorna 0 em caso de erro e imprime o log do driver no stderr.
GLuint loadProgram(const std::string& vertPath, const std::string& fragPath);
