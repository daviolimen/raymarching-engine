# Raymarcher — PSI3572

Engine de **raymarching em tempo real** (C++20 + OpenGL 3.3) feita como projeto da disciplina
PSI3572 — Computação Visual. Em vez de rasterizar triângulos, cada pixel lança um raio e avança
por *sphere tracing* sobre uma **função de distância com sinal (SDF)** que descreve a cena
inteira. Os objetos se fundem e se separam com **smooth minimum**.

## Funcionalidades
- 8 primitivas: esfera, caixa, caixa arredondada, toro, cápsula, cilindro, cone e octaedro.
- Operações CSG suaves (união, subtração e interseção com `smin`/`smax`) e mistura de cor na
  região de fusão.
- Cena editável pela GUI: adicionar, remover e reordenar objetos, e editar posição, rotação,
  dimensões, cor e animação.
- Animação por objeto: órbita, oscilação (MHS), quique parabólico e giro.
- Iluminação: Lambert, especular de Blinn-Phong, sombras suaves com penumbra, oclusão ambiente,
  névoa e correção de gama.
- Visualizações de depuração: normais, heatmap de passos, fator de sombra e AO.
- Escala de resolução e culling por esfera envolvente (`docs/07`).

## Compilar e rodar
Dependências: CMake ≥ 3.20, um compilador C++20 e GLFW 3.3+. O glad já está em `external/`,
e o Dear ImGui é baixado pelo CMake na primeira configuração.

```sh
# Arch
sudo pacman -S cmake gcc glfw
# Ubuntu/Debian
sudo apt install cmake g++ libglfw3-dev

cmake -B build
cmake --build build
./build/raymarcher
```

## Controles
| entrada | ação |
|---|---|
| arrastar com o botão esquerdo | girar a câmera |
| roda do mouse | zoom |
| espaço | pausar ou retomar a animação |
| R | recarregar os shaders do disco (os erros aparecem no terminal) |
| H | esconder ou mostrar a GUI |
| Esc | sair |

Os shaders são lidos de `shaders/` em tempo de execução: edite, salve e aperte **R**, sem
recompilar o C++. Se o shader novo tiver erro, o anterior continua rodando.

## Estrutura
```
src/main.cpp            janela, laço principal, câmera orbital, GUI de renderização, FBO de resolução
src/scene.{hpp,cpp}     objetos da cena, presets, animação, envio dos uniforms, editor da cena
src/shader.{hpp,cpp}    compilação e linkagem dos shaders
shaders/fullscreen.vert triângulo de tela cheia (gl_VertexID, sem VBO)
shaders/raymarch.frag   SDFs, smooth min, sphere tracing, iluminação
docs/                   teoria e decisões, por tema
```

## Documentação
1. [OpenGL: o mínimo para um raymarcher](docs/01-opengl.md)
2. [SDFs e sphere tracing](docs/02-sdf-e-sphere-tracing.md)
3. [Primitivas e transformações](docs/03-primitivas.md)
4. [Iluminação](docs/04-iluminacao.md)
5. [Operações CSG e smooth minimum](docs/05-smooth-min.md)
6. [Animação](docs/06-animacao.md)
7. [Desempenho](docs/07-desempenho.md)

## Referências
- J. C. Hart, *Sphere tracing: a geometric method for the antialiased ray tracing of implicit
  surfaces*, The Visual Computer, 1996.
- I. Quilez, artigos sobre SDFs, smooth minimum, sombras suaves e AO — https://iquilezles.org/articles/
- J. F. Blinn, *Models of light reflection for computer synthesized pictures*, SIGGRAPH 1977.
