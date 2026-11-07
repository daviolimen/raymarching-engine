#pragma once
#include <glad/gl.h>

#include <vector>

// Tem que bater com MAX_OBJECTS em shaders/raymarch.frag (tamanho dos arrays de uniforms).
constexpr int MAX_OBJECTS = 32;

// Tem que bater com as constantes de tipo e operação em shaders/raymarch.frag.
enum ObjectType { OBJ_SPHERE, OBJ_BOX, OBJ_ROUND_BOX, OBJ_TORUS, OBJ_CAPSULE, OBJ_CYLINDER, OBJ_CONE, OBJ_OCTAHEDRON, OBJ_TYPE_COUNT };
enum ObjectOp { OP_UNION, OP_SUBTRACT, OP_INTERSECT };
enum AnimMode { ANIM_NONE, ANIM_ORBIT, ANIM_OSCILLATE, ANIM_BOUNCE };

struct SceneObject {
    int type = OBJ_SPHERE;
    int op = OP_UNION;
    float k = 0.0f; // suavidade da operação com o resultado dos objetos anteriores (0 = aresta viva)
    float pos[3] = {0.0f, 1.0f, 0.0f};
    float rotDegrees[3] = {0.0f, 0.0f, 0.0f}; // em torno de x, depois y, depois z
    float params[4] = {0.5f, 0.5f, 0.5f, 0.1f}; // significado depende do tipo (ver PARAM_LABELS em scene.cpp)
    float color[3] = {0.8f, 0.8f, 0.8f};

    // Animação: a posição desenhada é pos + deslocamento(t); ver animationOffset em scene.cpp.
    int anim = ANIM_NONE;
    float animSpeed = 1.0f;  // velocidade angular ω (rad/s): período = 2π/ω
    float animAmp = 0.5f;    // amplitude A (unidades de distância)
    float animPhase = 0.0f;  // fase φ (rad): defasa objetos com o mesmo movimento
    float animDir[3] = {1.0f, 0.0f, 0.0f}; // direção da oscilação
    float spin = 0.0f;       // giro contínuo em torno do eixo y (graus/s)
};

std::vector<SceneObject> galleryPreset();
std::vector<SceneObject> fusionPreset();
std::vector<SceneObject> dancePreset();

// Envia a lista de objetos, animada no instante t (segundos), para os arrays de uniforms
// do programa atualmente em uso.
void uploadScene(GLuint program, const std::vector<SceneObject>& objects, float t);

// Janela da GUI para escolher um preset e adicionar, remover, reordenar e editar objetos.
void drawSceneEditor(std::vector<SceneObject>& objects);
