#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "scene.hpp"
#include "shader.hpp"

static const char* VERT_PATH = SHADER_DIR "fullscreen.vert";
static const char* FRAG_PATH = SHADER_DIR "raymarch.frag";
static const float DEG_TO_RAD = 3.14159265f / 180.0f;

static bool gReloadRequested = false;
static bool gShowGui = true; // H alterna: útil para apresentar e para tirar figuras do relatório

// Câmera orbital: gira em torno de um alvo numa esfera de raio "distance".
// yaw = ângulo horizontal, pitch = ângulo de elevação (radianos).
struct OrbitCamera {
    float yaw = 0.6f, pitch = 0.45f, distance = 9.0f;
    float target[3] = {0.0f, 0.6f, 0.0f};
    float fovDegrees = 60.0f;

    // Coordenadas esféricas -> cartesianas, relativas ao alvo.
    void position(float out[3]) const {
        float cp = std::cos(pitch);
        out[0] = target[0] + distance * cp * std::sin(yaw);
        out[1] = target[1] + distance * std::sin(pitch);
        out[2] = target[2] + distance * cp * std::cos(yaw);
    }
};

// Tudo que a GUI controla e o shader recebe como uniform.
struct RenderSettings {
    float resolutionScale = 1.0f; // fração da resolução da janela usada pelo raymarcher
    bool vsync = true;
    int maxSteps = 128;
    float maxDist = 100.0f;
    float surfEps = 0.001f;
    float lightAzimuth = 40.0f, lightElevation = 50.0f; // graus
    float lightColor[3] = {1.0f, 0.95f, 0.85f};
    float ambient = 0.04f;  // em espaço linear: 0.04 já aparece como ~0.23 na tela com gama
    float specular = 0.5f;    // intensidade do brilho especular (0 = superfície fosca)
    float shininess = 32.0f;  // expoente de Blinn-Phong: maior = brilho menor e mais concentrado
    bool shadows = true;
    float shadowK = 8.0f;     // nitidez da penumbra: maior = sombra mais dura
    bool ao = true;           // oclusão ambiente
    float fogDensity = 0.005f; // 0 = sem névoa
    bool gamma = true;        // iluminação em espaço linear + codificação de gama na saída
    int debugView = 0; // 0 = iluminado, 1 = normais, 2 = heatmap de passos, 3 = sombra, 4 = AO
    bool animPaused = false;
    float animSpeed = 1.0f; // multiplica o passo do tempo de animação
};

static OrbitCamera gCamera;
static RenderSettings gSettings;
static std::vector<SceneObject> gObjects = galleryPreset();

static void onKey(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS || ImGui::GetIO().WantCaptureKeyboard) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_R) gReloadRequested = true;
    if (key == GLFW_KEY_H) gShowGui = !gShowGui;
    if (key == GLFW_KEY_SPACE) gSettings.animPaused = !gSettings.animPaused;
}

static void onScroll(GLFWwindow* /*window*/, double /*dx*/, double dy) {
    if (ImGui::GetIO().WantCaptureMouse) return; // rolando sobre a GUI, não sobre a cena
    // Zoom multiplicativo: cada "clique" da roda aproxima 10%, perto ou longe.
    gCamera.distance = std::clamp(gCamera.distance * std::pow(0.9f, (float)dy), 1.5f, 50.0f);
}

static void drawGui(int internalWidth, int internalHeight) {
    RenderSettings& s = gSettings;
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("Raymarcher", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushItemWidth(ImGui::GetFontSize() * 11); // largura fixa dos controles: janela compacta
    ImGui::Text("%.1f FPS (%.2f ms/quadro)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::Text("Resolução interna: %d x %d", internalWidth, internalHeight);
    if (ImGui::Checkbox("VSync (limita o FPS ao monitor)", &s.vsync)) glfwSwapInterval(s.vsync ? 1 : 0);
    ImGui::SliderFloat("Escala de resolução", &s.resolutionScale, 0.25f, 1.0f, "%.2f");

    ImGui::SeparatorText("Sphere tracing");
    ImGui::SliderInt("Passos máximos", &s.maxSteps, 8, 512);
    ImGui::SliderFloat("Distância máxima", &s.maxDist, 5.0f, 300.0f, "%.0f");
    ImGui::SliderFloat("Epsilon", &s.surfEps, 0.00001f, 0.1f, "%.5f", ImGuiSliderFlags_Logarithmic);

    ImGui::SeparatorText("Câmera");
    ImGui::SliderFloat("FOV (graus)", &gCamera.fovDegrees, 20.0f, 120.0f, "%.0f");

    ImGui::SeparatorText("Iluminação");
    ImGui::SliderFloat("Azimute da luz", &s.lightAzimuth, -180.0f, 180.0f, "%.0f°");
    // Mínimo 0°: abaixo do horizonte o chão bloquearia a luz, e as sombras ignoram o chão (mapObjects).
    ImGui::SliderFloat("Elevação da luz", &s.lightElevation, 0.0f, 90.0f, "%.0f°");
    ImGui::ColorEdit3("Cor da luz", s.lightColor);
    ImGui::SliderFloat("Ambiente", &s.ambient, 0.0f, 1.0f);
    ImGui::SliderFloat("Especular", &s.specular, 0.0f, 1.0f);
    ImGui::SliderFloat("Expoente do brilho", &s.shininess, 1.0f, 512.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::Checkbox("Sombras", &s.shadows);
    ImGui::SameLine();
    ImGui::SliderFloat("Nitidez (k)", &s.shadowK, 1.0f, 128.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
    ImGui::Checkbox("Oclusão ambiente", &s.ao);
    ImGui::Checkbox("Correção de gama", &s.gamma);
    ImGui::SliderFloat("Névoa", &s.fogDensity, 0.0f, 0.05f, "%.4f");

    ImGui::SeparatorText("Animação");
    ImGui::Checkbox("Pausar (espaço)", &s.animPaused);
    ImGui::SameLine();
    ImGui::SliderFloat("Velocidade", &s.animSpeed, 0.0f, 3.0f, "%.2fx");

    ImGui::SeparatorText("Depuração");
    ImGui::Combo("Visualização", &s.debugView, "Iluminado\0Normais\0Heatmap de passos\0Sombra\0Oclusão ambiente\0");
    ImGui::TextDisabled("Arrastar: girar | Roda: zoom | R: recarregar shader | H: esconder GUI");
    ImGui::PopItemWidth();
    ImGui::End();
}

int main() {
    glfwSetErrorCallback([](int code, const char* msg) {
        std::fprintf(stderr, "[glfw] erro %d: %s\n", code, msg);
    });
    if (!glfwInit()) return 1;

    // Pedimos um contexto OpenGL 3.3 "core": sem as funções legadas do pipeline fixo.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Raymarcher - PSI3572", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window); // as chamadas gl* desta thread passam a valer para esta janela
    glfwSwapInterval(1);            // vsync: troca os buffers no ritmo do monitor
    // Nossos callbacks vêm antes do ImGui: ele os encadeia e repassa os eventos para nós.
    glfwSetKeyCallback(window, onKey);
    glfwSetScrollCallback(window, onScroll);

    // Só depois de ter um contexto dá para buscar os ponteiros das funções OpenGL.
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::fprintf(stderr, "falha ao carregar OpenGL\n");
        return 1;
    }
    std::printf("OpenGL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // não grava imgui.ini na pasta atual
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // O core profile exige um VAO ligado para desenhar, mesmo sem atributos de vértice:
    // o triângulo de tela cheia é gerado no vertex shader a partir de gl_VertexID.
    GLuint vao;
    glGenVertexArrays(1, &vao);

    // Framebuffer intermediário onde o raymarcher desenha em resolução reduzida.
    // Um renderbuffer basta como "imagem" porque só copiamos (blit), nunca amostramos.
    GLuint sceneFbo, sceneColor;
    glGenFramebuffers(1, &sceneFbo);
    glGenRenderbuffers(1, &sceneColor);
    int sceneWidth = 0, sceneHeight = 0;

    GLuint program = loadProgram(VERT_PATH, FRAG_PATH);

    // Tempo de animação: só avança quando não está pausado, na velocidade escolhida.
    float animTime = 0.0f;
    double lastFrameTime = glfwGetTime();

    double lastMouseX, lastMouseY;
    glfwGetCursorPos(window, &lastMouseX, &lastMouseY);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        double now = glfwGetTime();
        if (!gSettings.animPaused) animTime += (float)(now - lastFrameTime) * gSettings.animSpeed;
        lastFrameTime = now;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Arrastar com o botão esquerdo gira a câmera em torno do alvo (se não estiver sobre a GUI).
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        if (!ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            gCamera.yaw -= (float)(mouseX - lastMouseX) * 0.005f;
            gCamera.pitch += (float)(mouseY - lastMouseY) * 0.005f;
        }
        lastMouseX = mouseX;
        lastMouseY = mouseY;
        // Limites do pitch: perto de +90° a base da câmera degenera (cross(frente, cima) = 0);
        // abaixo do chão (y < 0), map(ro) < 0 e todo raio "acertaria" em t = 0.
        // Recalculado a cada quadro porque o zoom muda a distância.
        float minPitch = std::asin(std::clamp((0.05f - gCamera.target[1]) / gCamera.distance, -1.0f, 1.0f));
        gCamera.pitch = std::clamp(gCamera.pitch, minPitch, 1.5f);

        // GUI antes da cena: o que for editado neste quadro já aparece neste quadro.
        if (gShowGui) {
            drawGui(sceneWidth, sceneHeight);
            drawSceneEditor(gObjects);
        }

        if (gReloadRequested) {
            gReloadRequested = false;
            // Se o novo shader tiver erro, mantemos o antigo rodando.
            if (GLuint fresh = loadProgram(VERT_PATH, FRAG_PATH)) {
                glDeleteProgram(program);
                program = fresh;
                std::printf("[shader] recarregado\n");
            }
        }

        // Tamanho em pixels reais (pode diferir do tamanho da janela em telas HiDPI).
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        // (Re)cria a imagem intermediária quando a janela ou a escala mudam.
        int wantWidth = std::max(1, (int)(width * gSettings.resolutionScale));
        int wantHeight = std::max(1, (int)(height * gSettings.resolutionScale));
        if (wantWidth != sceneWidth || wantHeight != sceneHeight) {
            sceneWidth = wantWidth;
            sceneHeight = wantHeight;
            glBindRenderbuffer(GL_RENDERBUFFER, sceneColor);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, sceneWidth, sceneHeight);
            glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sceneColor);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::fprintf(stderr, "[fbo] framebuffer incompleto\n");
        }

        // 1) Raymarching na imagem intermediária.
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
        glViewport(0, 0, sceneWidth, sceneHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (program) {
            const RenderSettings& s = gSettings;
            auto loc = [program](const char* name) { return glGetUniformLocation(program, name); };
            glUseProgram(program);
            glUniform1f(loc("uTime"), (float)glfwGetTime());
            glUniform2f(loc("uResolution"), (float)sceneWidth, (float)sceneHeight);

            float camPos[3];
            gCamera.position(camPos);
            glUniform3fv(loc("uCamPos"), 1, camPos);
            glUniform3fv(loc("uCamTarget"), 1, gCamera.target);
            glUniform1f(loc("uFov"), gCamera.fovDegrees * DEG_TO_RAD);

            glUniform1i(loc("uMaxSteps"), s.maxSteps);
            glUniform1f(loc("uMaxDist"), s.maxDist);
            glUniform1f(loc("uSurfEps"), s.surfEps);

            // Direção PARA a luz, a partir de azimute/elevação (mesmas coordenadas esféricas da câmera).
            float az = s.lightAzimuth * DEG_TO_RAD, el = s.lightElevation * DEG_TO_RAD;
            glUniform3f(loc("uLightDir"), std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az));
            glUniform3fv(loc("uLightColor"), 1, s.lightColor);
            glUniform1f(loc("uAmbient"), s.ambient);
            glUniform1f(loc("uSpecular"), s.specular);
            glUniform1f(loc("uShininess"), s.shininess);
            glUniform1i(loc("uShadows"), s.shadows);
            glUniform1f(loc("uShadowK"), s.shadowK);
            glUniform1i(loc("uAO"), s.ao);
            glUniform1f(loc("uFogDensity"), s.fogDensity);
            glUniform1i(loc("uGamma"), s.gamma);
            glUniform1i(loc("uDebugView"), s.debugView);
            uploadScene(program, gObjects, animTime);

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        // 2) Copia ampliando para a janela (framebuffer 0). GL_LINEAR suaviza os pixels ampliados.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, sceneWidth, sceneHeight, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        // 3) GUI por cima, em resolução cheia.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window); // double buffering: mostra o quadro que acabamos de desenhar
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteProgram(program);
    glDeleteRenderbuffers(1, &sceneColor);
    glDeleteFramebuffers(1, &sceneFbo);
    glDeleteVertexArrays(1, &vao);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
