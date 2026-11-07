# 01 — OpenGL: o mínimo para um raymarcher

## O que o OpenGL é (e não é)
OpenGL é uma **especificação** de API para conversar com a GPU. Quem a implementa é o
driver (Mesa, NVIDIA...). Ele não cria janelas nem lê o teclado; para isso usamos o **GLFW**.

| Peça | Papel |
|------|-------|
| GLFW | cria a janela, o **contexto OpenGL** e entrega eventos de teclado/mouse |
| glad | busca no driver, em tempo de execução, os ponteiros das funções `gl*` modernas |
| OpenGL | envia comandos e dados à GPU |
| GLSL | linguagem dos programas (*shaders*) que rodam **na** GPU |

**Contexto** é o "estado global" do OpenGL associado a uma janela: todos os objetos
(shaders, buffers, texturas) vivem dentro dele. `glfwMakeContextCurrent` diz que as chamadas
`gl*` desta thread vão para aquele contexto.

**Core profile 3.3:** pedimos uma versão sem as funções legadas (`glBegin/glEnd`, luz fixa).
Tudo o que é desenhado passa por shaders escritos por nós.

## O pipeline, na versão que usamos
```
glDrawArrays(GL_TRIANGLES, 0, 3)
   │
   ▼
Vertex shader (3×)      → posiciona os 3 vértices em "clip space" [-1,1]²
   │
   ▼
Rasterização            → descobre quais pixels o triângulo cobre
   │
   ▼
Fragment shader (1×/px) → calcula a cor de cada pixel   ← TODO o raymarching acontece aqui
   │
   ▼
Framebuffer (back buffer) → glfwSwapBuffers mostra na tela
```

Num renderizador tradicional, a geometria (malhas de triângulos) entra no vertex shader.
No raymarching, a geometria é **uma função matemática** avaliada no fragment shader. Por isso
desenhamos só um triângulo gigante que cobre a tela: ele existe apenas para disparar o
fragment shader em todos os pixels.

### O triângulo de tela cheia (`shaders/fullscreen.vert`)
Sem nenhum buffer de vértices, usamos `gl_VertexID` (0, 1, 2) para gerar
`(-1,-1), (3,-1), (-1,3)`. Esse triângulo contém o quadrado `[-1,1]²` e o excesso é
recortado (*clipping*). Comparado a um quad com 2 triângulos, evita a diagonal, onde
pixels vizinhos ficam em triângulos diferentes, o que é levemente menos eficiente.

### VAO
Um *Vertex Array Object* guarda **como** ler atributos de vértice dos buffers. Não temos
atributos, mas o core profile exige um VAO ligado em `glDrawArrays`, então criamos um vazio.

## Shaders e programas
1. `glCreateShader` + `glShaderSource` + `glCompileShader`: o **driver** compila o GLSL na
   hora em que o programa roda. Erros de sintaxe aparecem só aí (veja `src/shader.cpp`).
2. `glCreateProgram` + `glAttachShader` + `glLinkProgram`: liga o vertex ao fragment.
3. `glUseProgram`: escolhe o programa para os próximos desenhos.

## Uniforms
Variáveis `uniform` são parâmetros que a CPU envia ao shader e que são **iguais para
todos os pixels** de um desenho (tempo, resolução, posição da câmera, lista de objetos...).

```cpp
glUniform1f(glGetUniformLocation(program, "uTime"), t);
```
`glGetUniformLocation` devolve -1 se o nome não existir **ou se o compilador o removeu por
não ser usado**. Um `glUniform*` com -1 é ignorado em silêncio, uma fonte clássica de
"mudei o valor e nada aconteceu".

## Double buffering e vsync
Desenhamos no *back buffer* enquanto o monitor mostra o *front buffer*;
`glfwSwapBuffers` os troca. Com `glfwSwapInterval(1)` a troca espera o monitor (vsync),
o que limita o FPS à taxa de atualização dele.

## `gl_FragCoord` e coordenadas de tela
No fragment shader, `gl_FragCoord.xy` é a posição do pixel **em pixels**, com origem no
canto inferior esquerdo. Para a câmera precisamos de coordenadas normalizadas e
independentes da resolução; essa conversão é o primeiro passo do raymarcher.

## Framebuffers e a escala de resolução
Um **framebuffer** é um destino de desenho: um conjunto de imagens (cor, profundidade...).
O framebuffer `0` é a janela. Podemos criar outros (**FBOs**) que desenham em imagens fora
da tela.

O custo do raymarching é proporcional ao **número de pixels**: o fragment shader roda uma
vez por pixel. Para controlar esse custo:

1. criamos um FBO com um *renderbuffer* de cor de tamanho `escala × janela`;
2. o raymarcher desenha nele (`glViewport` com o tamanho reduzido);
3. `glBlitFramebuffer` copia a imagem para a janela, ampliando (`GL_LINEAR` interpola);
4. a GUI é desenhada **depois**, direto na janela, em resolução cheia.

Com escala 0,5 há ¼ dos pixels, então o shader roda cerca de 4× menos vezes.
Usamos *renderbuffer* em vez de *textura* porque só copiamos a imagem, sem amostrá-la num
shader.

## Dear ImGui: interface em "modo imediato"
Numa GUI tradicional (*retained mode*: Qt, GTK), criamos objetos "botão" que guardam estado
e chamam callbacks. No **modo imediato**, a interface é redesenhada a cada quadro a partir do
código:

```cpp
ImGui::SliderFloat("FOV", &camera.fov, 20.0f, 120.0f); // desenha E altera camera.fov
```

O estado é a própria variável C++. A cada quadro:
`NewFrame()` → chamadas `ImGui::*` descrevem a interface → `Render()` gera triângulos
→ o backend OpenGL os desenha.

O ImGui também avisa quando o mouse ou o teclado estão "sobre a GUI"
(`io.WantCaptureMouse` / `WantCaptureKeyboard`). Usamos isso para não girar a câmera
enquanto o usuário arrasta um controle deslizante.

## Arrays de uniforms: a cena como dado
A partir do M5, a cena não está escrita no shader: cada propriedade dos objetos é um
**array de uniforms** (`uniform vec3 uObjPos[32];`...), e o `map` percorre os objetos num laço.

- `glGetUniformLocation(program, "uObjPos")` devolve a posição do elemento `[0]`.
- `glUniform3fv(loc, n, dados)` envia `n` elementos consecutivos de uma vez.
- Matrizes: o GLSL as guarda **por colunas**. Montamos as rotações por linhas no C++ e passamos
  `transpose = GL_TRUE` em `glUniformMatrix3fv` para o OpenGL converter.
- Há um limite de uniforms por estágio (`GL_MAX_FRAGMENT_UNIFORM_COMPONENTS`, no mínimo 1024
  componentes em GL 3.3). Por isso fixamos um máximo de objetos (`MAX_OBJECTS = 32`).

Trocar código por dado tem um custo: o compilador não pode mais otimizar a cena específica
(desenrolar o laço, eliminar ramos), e o `map` fica mais caro por avaliação.
