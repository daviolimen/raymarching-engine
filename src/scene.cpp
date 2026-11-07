#include "scene.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <utility>

static const char* TYPE_NAMES[OBJ_TYPE_COUNT] = {
    "Esfera", "Caixa", "Caixa arredondada", "Toro", "Cápsula", "Cilindro", "Cone", "Octaedro",
};
static const char* OP_NAMES[] = {"União", "Subtração", "Interseção"};

// O que cada parâmetro significa para cada tipo (nullptr = não usado).
// Tem que bater com sdObject() em shaders/raymarch.frag.
static const char* PARAM_LABELS[OBJ_TYPE_COUNT][4] = {
    {"Raio", nullptr, nullptr, nullptr},
    {"Meia largura (x)", "Meia altura (y)", "Meia profundidade (z)", nullptr},
    {"Meia largura (x)", "Meia altura (y)", "Meia profundidade (z)", "Raio do arredondamento"},
    {"Raio maior", "Raio do tubo", nullptr, nullptr},
    {"Meio comprimento", "Raio", nullptr, nullptr},
    {"Raio", "Meia altura", nullptr, nullptr},
    {"Raio da base", "Meia altura", "Raio do topo", nullptr},
    {"Tamanho", nullptr, nullptr, nullptr},
};

std::vector<SceneObject> galleryPreset() {
    return {
        // Fila de trás
        {.type = OBJ_SPHERE, .pos = {-3.0f, 0.7f, -1.5f}, .params = {0.7f}, .color = {0.9f, 0.5f, 0.2f}},
        {.type = OBJ_BOX, .pos = {-1.0f, 0.6f, -1.5f}, .rotDegrees = {0.0f, 30.0f, 0.0f}, .params = {0.6f, 0.6f, 0.6f}, .color = {0.2f, 0.6f, 0.9f}, .spin = 30.0f},
        {.type = OBJ_ROUND_BOX, .pos = {1.0f, 0.6f, -1.5f}, .params = {0.6f, 0.6f, 0.6f, 0.15f}, .color = {0.3f, 0.8f, 0.4f}},
        {.type = OBJ_TORUS, .pos = {3.0f, 0.8f, -1.5f}, .rotDegrees = {90.0f, 0.0f, 0.0f}, .params = {0.6f, 0.2f}, .color = {0.9f, 0.8f, 0.2f}, .spin = 30.0f},
        // Fila da frente
        {.type = OBJ_CAPSULE, .pos = {-3.0f, 0.8f, 1.5f}, .params = {0.5f, 0.3f}, .color = {0.8f, 0.3f, 0.6f}},
        {.type = OBJ_CYLINDER, .pos = {-1.0f, 0.6f, 1.5f}, .params = {0.5f, 0.6f}, .color = {0.4f, 0.4f, 0.9f}},
        {.type = OBJ_CONE, .pos = {1.0f, 0.7f, 1.5f}, .params = {0.6f, 0.7f, 0.0f}, .color = {0.9f, 0.3f, 0.3f}},
        {.type = OBJ_OCTAHEDRON, .pos = {3.0f, 0.8f, 1.5f}, .rotDegrees = {0.0f, 20.0f, 0.0f}, .params = {0.8f}, .color = {0.3f, 0.8f, 0.8f}, .spin = -30.0f},
    };
}

std::vector<SceneObject> fusionPreset() {
    return {
        {.type = OBJ_SPHERE, .pos = {-1.0f, 1.0f, 0.0f}, .params = {0.8f}, .color = {0.9f, 0.4f, 0.2f}},
        {.type = OBJ_ROUND_BOX, .k = 0.7f, .pos = {1.0f, 0.9f, 0.0f}, .rotDegrees = {0.0f, 35.0f, 0.0f}, .params = {0.55f, 0.55f, 0.55f, 0.1f}, .color = {0.2f, 0.5f, 0.9f}},
        {.type = OBJ_TORUS, .k = 0.5f, .pos = {0.0f, 1.0f, 0.0f}, .params = {1.6f, 0.18f}, .color = {0.3f, 0.8f, 0.4f}},
        // Esfera subtraída: "cava" o topo da esfera laranja.
        {.type = OBJ_SPHERE, .op = OP_SUBTRACT, .k = 0.2f, .pos = {-1.0f, 1.7f, 0.2f}, .params = {0.45f}, .color = {0.9f, 0.9f, 0.9f}},
    };
}

// Objetos que se aproximam, se fundem e se separam ciclicamente.
std::vector<SceneObject> dancePreset() {
    const float pi = 3.14159265f;
    return {
        // Núcleo parado e duas esferas que oscilam em direção a ele, em oposição de fase.
        {.type = OBJ_SPHERE, .pos = {0.0f, 1.2f, 0.0f}, .params = {0.6f}, .color = {0.6f, 0.3f, 0.8f}},
        {.type = OBJ_SPHERE, .k = 0.6f, .pos = {-1.4f, 1.2f, 0.0f}, .params = {0.4f}, .color = {0.95f, 0.5f, 0.2f},
         .anim = ANIM_OSCILLATE, .animSpeed = 1.5f, .animAmp = 0.9f, .animDir = {1.0f, 0.0f, 0.0f}},
        {.type = OBJ_SPHERE, .k = 0.6f, .pos = {1.4f, 1.2f, 0.0f}, .params = {0.4f}, .color = {0.2f, 0.5f, 0.95f},
         .anim = ANIM_OSCILLATE, .animSpeed = 1.5f, .animAmp = 0.9f, .animPhase = pi, .animDir = {1.0f, 0.0f, 0.0f}},
        // Satélite que orbita o núcleo e atravessa as outras esferas.
        {.type = OBJ_SPHERE, .k = 0.5f, .pos = {0.0f, 1.2f, 0.0f}, .params = {0.22f}, .color = {0.3f, 0.85f, 0.4f},
         .anim = ANIM_ORBIT, .animSpeed = 1.0f, .animAmp = 1.3f},
        // Poça no chão e uma gota que quica e se funde com ela no contato.
        {.type = OBJ_CYLINDER, .pos = {0.0f, 0.05f, 2.2f}, .params = {0.8f, 0.05f}, .color = {0.2f, 0.75f, 0.8f}},
        {.type = OBJ_SPHERE, .k = 0.4f, .pos = {0.0f, 0.3f, 2.2f}, .params = {0.3f}, .color = {0.2f, 0.75f, 0.8f},
         .anim = ANIM_BOUNCE, .animSpeed = 3.0f, .animAmp = 1.5f},
        // Caixa arredondada girando no fundo, para mostrar rotação contínua.
        {.type = OBJ_ROUND_BOX, .pos = {-2.6f, 0.6f, -1.8f}, .params = {0.5f, 0.5f, 0.5f, 0.1f}, .color = {0.9f, 0.8f, 0.2f}, .spin = 45.0f},
    };
}

// Deslocamento do objeto em relação à posição base, no instante t (segundos).
// Notação: ω = animSpeed (rad/s), A = animAmp, φ = animPhase. Teoria em docs/06-animacao.md.
static void animationOffset(const SceneObject& o, float t, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    float angle = o.animSpeed * t + o.animPhase; // ωt + φ
    switch (o.anim) {
    case ANIM_ORBIT: // movimento circular uniforme de raio A no plano xz, em torno da posição base
        out[0] = o.animAmp * std::cos(angle);
        out[2] = o.animAmp * std::sin(angle);
        break;
    case ANIM_OSCILLATE: { // MHS ao longo de animDir normalizada, até A de cada lado da posição base
        float len = std::sqrt(o.animDir[0]*o.animDir[0] + o.animDir[1]*o.animDir[1] + o.animDir[2]*o.animDir[2]);
        if (len == 0) return;
        out[0] = o.animAmp * std::sin(angle) * o.animDir[0] / len;
        out[1] = o.animAmp * std::sin(angle) * o.animDir[1] / len;
        out[2] = o.animAmp * std::sin(angle) * o.animDir[2] / len;
        break;
    }
    case ANIM_BOUNCE: { // uma parábola por ciclo (gravidade constante), da base até a altura A
        float tmp = angle / (2.0f * std::numbers::pi_v<float>);
        float frac = tmp - std::floor(tmp); // fração do ciclo em [0,1), também para ângulo negativo
        out[1] = 4 * o.animAmp * frac * (1 - frac);
        break;
    }
    }
}

// Matriz de rotação R = Rz · Ry · Rx (gira primeiro em torno de x, depois y, depois z),
// guardada por LINHAS. Ângulos em graus.
static void rotationMatrix(const float degrees[3], float out[9]) {
    const float toRad = 3.14159265f / 180.0f;
    float cx = std::cos(degrees[0] * toRad), sx = std::sin(degrees[0] * toRad);
    float cy = std::cos(degrees[1] * toRad), sy = std::sin(degrees[1] * toRad);
    float cz = std::cos(degrees[2] * toRad), sz = std::sin(degrees[2] * toRad);
    // Produto Rz·Ry·Rx já expandido.
    const float m[9] = {
        cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
        sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
        -sy,     cy * sx,                cy * cx,
    };
    std::copy(m, m + 9, out);
}

// Raio da menor esfera centrada na origem do objeto que o contém inteiro (independe da rotação).
// O shader usa |p - pos| - raio como limite inferior barato da distância até o objeto.
static float boundingRadius(const SceneObject& o) {
    const float* s = o.params;
    switch (o.type) {
    case OBJ_SPHERE:     return s[0];
    case OBJ_BOX:
    case OBJ_ROUND_BOX:  return std::sqrt(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]); // canto da caixa
    case OBJ_TORUS:      return s[0] + s[1];                    // raio maior + tubo
    case OBJ_CAPSULE:    return s[0] + s[1];                    // ponta do segmento + raio
    case OBJ_CYLINDER:   return std::sqrt(s[0]*s[0] + s[1]*s[1]); // borda da tampa
    case OBJ_CONE:       return std::sqrt(std::max(s[0], s[2]) * std::max(s[0], s[2]) + s[1]*s[1]);
    default:             return s[0];                           // octaedro: vértice a s do centro
    }
}

void uploadScene(GLuint program, const std::vector<SceneObject>& objects, float t) {
    auto loc = [program](const char* name) { return glGetUniformLocation(program, name); };
    int count = std::min((int)objects.size(), MAX_OBJECTS);
    glUniform1i(loc("uObjectCount"), count);
    if (count == 0) return;

    // Arrays contíguos: glUniform*v envia "count" elementos de uma vez, a partir do índice 0.
    int types[MAX_OBJECTS], ops[MAX_OBJECTS];
    float ks[MAX_OBJECTS], bounds[MAX_OBJECTS], pos[MAX_OBJECTS][3], rot[MAX_OBJECTS][9], params[MAX_OBJECTS][4], colors[MAX_OBJECTS][3];
    for (int i = 0; i < count; i++) {
        const SceneObject& o = objects[i];
        types[i] = o.type;
        ops[i] = o.op;
        ks[i] = o.k;
        bounds[i] = boundingRadius(o);
        float offset[3];
        animationOffset(o, t, offset);
        for (int c = 0; c < 3; c++) pos[i][c] = o.pos[c] + offset[c];
        float rotNow[3] = {o.rotDegrees[0], std::fmod(o.rotDegrees[1] + o.spin * t, 360.0f), o.rotDegrees[2]};
        rotationMatrix(rotNow, rot[i]);
        std::copy(o.params, o.params + 4, params[i]);
        std::copy(o.color, o.color + 3, colors[i]);
    }
    glUniform1iv(loc("uObjType"), count, types);
    glUniform1iv(loc("uObjOp"), count, ops);
    glUniform1fv(loc("uObjK"), count, ks);
    glUniform3fv(loc("uObjPos"), count, &pos[0][0]);
    glUniform1fv(loc("uObjBound"), count, bounds);
    // O GLSL guarda matrizes por colunas; GL_TRUE avisa que as nossas estão por linhas.
    glUniformMatrix3fv(loc("uObjRot"), count, GL_TRUE, &rot[0][0]);
    glUniform4fv(loc("uObjParams"), count, &params[0][0]);
    glUniform3fv(loc("uObjColor"), count, &colors[0][0]);
}

void drawSceneEditor(std::vector<SceneObject>& objects) {
    static int selected = 0;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 390.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(390.0f, 600.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Cena");

    ImGui::TextUnformatted("Presets:");
    ImGui::SameLine();
    if (ImGui::Button("Galeria")) objects = galleryPreset();
    ImGui::SameLine();
    if (ImGui::Button("Fusão")) objects = fusionPreset();
    ImGui::SameLine();
    if (ImGui::Button("Dança")) objects = dancePreset();
    ImGui::SameLine();
    ImGui::BeginDisabled(objects.size() >= (size_t)MAX_OBJECTS);
    if (ImGui::Button("+ Objeto")) {
        objects.push_back({});
        selected = (int)objects.size() - 1;
    }
    ImGui::EndDisabled();

    // A ordem importa: cada objeto opera sobre o resultado acumulado dos anteriores.
    ImGui::TextDisabled("Ordem de avaliação (de cima para baixo):");
    ImGui::BeginChild("lista", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 7.0f), ImGuiChildFlags_Borders);
    for (int i = 0; i < (int)objects.size(); i++) {
        char label[96];
        std::snprintf(label, sizeof label, "%d. %s (%s)", i + 1, TYPE_NAMES[objects[i].type], OP_NAMES[objects[i].op]);
        if (ImGui::Selectable(label, selected == i)) selected = i;
    }
    ImGui::EndChild();

    selected = std::min(selected, (int)objects.size() - 1);
    if (selected < 0) {
        ImGui::TextDisabled("Cena vazia: use \"+ Objeto\" ou um preset.");
        ImGui::End();
        return;
    }

    SceneObject& o = objects[selected];
    ImGui::PushItemWidth(ImGui::GetFontSize() * 11.0f);
    ImGui::Combo("Tipo", &o.type, TYPE_NAMES, OBJ_TYPE_COUNT);
    ImGui::Combo("Operação", &o.op, OP_NAMES, 3);
    ImGui::SliderFloat("Suavidade k", &o.k, 0.0f, 2.0f, "%.2f");
    ImGui::DragFloat3("Posição", o.pos, 0.01f);
    ImGui::DragFloat3("Rotação (graus)", o.rotDegrees, 0.5f, -180.0f, 180.0f, "%.0f");
    for (int j = 0; j < 4; j++) {
        if (PARAM_LABELS[o.type][j]) ImGui::SliderFloat(PARAM_LABELS[o.type][j], &o.params[j], 0.0f, 3.0f, "%.2f");
    }
    ImGui::ColorEdit3("Cor", o.color);

    ImGui::SeparatorText("Animação");
    ImGui::Combo("Movimento", &o.anim, "Parado\0Órbita\0Oscilação\0Quique\0");
    if (o.anim != ANIM_NONE) {
        ImGui::SliderFloat("Velocidade ω (rad/s)", &o.animSpeed, 0.0f, 6.0f, "%.2f");
        ImGui::SliderFloat("Amplitude A", &o.animAmp, 0.0f, 3.0f, "%.2f");
        ImGui::SliderAngle("Fase φ", &o.animPhase, 0.0f, 360.0f);
        if (o.anim == ANIM_OSCILLATE) ImGui::DragFloat3("Direção", o.animDir, 0.01f, -1.0f, 1.0f);
    }
    ImGui::SliderFloat("Giro (graus/s)", &o.spin, -180.0f, 180.0f, "%.0f");
    ImGui::PopItemWidth();

    if (ImGui::Button("Subir") && selected > 0) {
        std::swap(objects[selected], objects[selected - 1]);
        selected--;
    }
    ImGui::SameLine();
    if (ImGui::Button("Descer") && selected + 1 < (int)objects.size()) {
        std::swap(objects[selected], objects[selected + 1]);
        selected++;
    }
    ImGui::SameLine();
    if (ImGui::Button("Remover")) objects.erase(objects.begin() + selected);
    ImGui::End();
}
