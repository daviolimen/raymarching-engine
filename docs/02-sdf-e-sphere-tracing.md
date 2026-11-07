# 02 — SDFs e sphere tracing

## Rasterização × raymarching
- **Rasterização** (o que a GPU faz normalmente): para cada *triângulo*, descobre quais pixels
  ele cobre. A cena é uma lista de triângulos.
- **Raymarching**: para cada *pixel*, lança um raio e procura onde ele encontra a superfície.
  A cena é uma **função** `f: ℝ³ → ℝ` que, para qualquer ponto, responde "quão longe estou
  da superfície mais próxima?".

## Signed Distance Function (SDF)
Uma SDF de uma forma `S` é a função

```
f(p) = ± min_{q ∈ ∂S} ‖p − q‖
```

ou seja, a distância do ponto `p` até o ponto mais próximo da borda `∂S`, com sinal:

| `f(p)` | significado |
|--------|-------------|
| `> 0`  | `p` está fora; existe uma bola de raio `f(p)` ao redor de `p` que não toca a forma |
| `= 0`  | `p` está na superfície |
| `< 0`  | `p` está dentro |

A superfície é o **conjunto de nível zero**: `{p : f(p) = 0}`.

### Exemplo: esfera
A esfera de raio `r` centrada na origem é o conjunto dos pontos a distância `r` da origem.
Um ponto `p` está a `‖p‖` da origem, então está a `‖p‖ − r` da casca:

```
sdSphere(p, r) = ‖p‖ − r          →  length(p) - r
```

### Transladar um objeto = transladar o espaço ao contrário
Para uma esfera centrada em `c`, avaliamos a SDF da esfera da origem no ponto `p − c`.
Em vez de mover o objeto, levamos o ponto para o "referencial local" do objeto. O mesmo
vale para rotação (M4): aplica-se a **inversa** da transformação ao ponto.

### União de objetos
A distância até o objeto mais próximo de um conjunto é a menor das distâncias:

```
f_cena(p) = min(f_A(p), f_B(p), …)
```

No M5 o `min` é trocado pelo **smooth min**, o que faz os objetos se fundirem.

## A propriedade que torna tudo seguro: Lipschitz
Uma SDF exata satisfaz `|f(a) − f(b)| ≤ ‖a − b‖`, isto é, ela é **1-Lipschitz**: a
distância não pode variar mais rápido do que o quanto andamos. Consequência direta:

> se `f(p) = d`, a bola de raio `d` centrada em `p` está **vazia** e não intersecta
> nenhuma superfície.

Portanto, a partir de `p`, podemos andar `d` em **qualquer** direção, inclusive na do raio,
sem atravessar nada. Essa bola vazia é a "esfera" que dá nome ao *sphere tracing*
(Hart, 1996).

## Sphere tracing
Um raio é a semirreta `r(t) = ro + t·rd`, com origem `ro` (a câmera), direção unitária
`rd` e parâmetro `t ≥ 0`, que é a distância percorrida, já que `‖rd‖ = 1`.

Começando em `t₀ = 0`, repetimos:

```
dᵢ   = f(ro + tᵢ·rd)        distância segura a partir do ponto atual
tᵢ₊₁ = tᵢ + dᵢ              avança exatamente essa distância
```

Critérios de parada:

| condição | interpretação | resultado |
|----------|---------------|-----------|
| `dᵢ < ε` (`SURF_EPS`) | chegamos "colados" na superfície | **acerto** em `t = tᵢ` |
| `tᵢ > MAX_DIST` | o raio saiu da cena | **errou** (céu) |
| `i = MAX_STEPS` | esgotou o orçamento de iterações | **errou** (aproximação) |

Por que não basta `d == 0`? Porque `tᵢ` só **se aproxima** da superfície e nunca chega
exatamente nela. Perto da superfície, `d` diminui geometricamente. O `ε` define a
precisão aceitável.

### Custo e o pior caso
- Raio que vai direto a uma parede: poucos passos, porque `d` é grande no início.
- Raio **rasante**, que passa quase tangente a uma superfície: `d` fica pequeno por muito
  tempo, mesmo sem acertar, e o raio dá muitos passos minúsculos. É por isso que as bordas
  dos objetos são as regiões mais caras. O *heatmap* de passos do M3 mostra isso.

## Normal da superfície: o gradiente
Para iluminar um ponto precisamos da **normal**, o vetor unitário perpendicular à superfície.

A superfície é o conjunto de nível `f(p) = 0`. Um resultado do cálculo diz que o **gradiente**

```
∇f = (∂f/∂x, ∂f/∂y, ∂f/∂z)
```

é perpendicular aos conjuntos de nível e aponta para onde `f` cresce mais rápido. Numa SDF,
`f` cresce ao se afastar da superfície, então `∇f` aponta **para fora**: é a normal.
Numa SDF exata, ainda vale `‖∇f‖ = 1`, mas normalizamos mesmo assim, porque a aproximação
abaixo e os blends do M5 quebram essa igualdade.

Exemplo analítico, para a esfera: `f(p) = ‖p‖ − r ⟹ ∇f = p / ‖p‖`, o vetor do centro até o
ponto, como esperado.

### Diferenças centrais
`map` é uma composição arbitrária de funções, então não derivamos à mão: aproximamos
numericamente. Pela expansão de Taylor em torno de `p`, com passo `h` no eixo x:

```
f(p + h·x̂) = f(p) + h·∂f/∂x + (h²/2)·∂²f/∂x² + O(h³)
f(p − h·x̂) = f(p) − h·∂f/∂x + (h²/2)·∂²f/∂x² − O(h³)
```

Subtraindo, os termos pares se cancelam:

```
∂f/∂x ≈ [ f(p + h·x̂) − f(p − h·x̂) ] / (2h)        erro O(h²)
```

O mesmo vale para y e z, num total de **6 avaliações de `map`**. Como vamos normalizar o
resultado, o fator `1/(2h)` é o mesmo nos três eixos e pode ser omitido.

| diferença | fórmula | erro |
|-----------|---------|------|
| progressiva | `[f(p+h) − f(p)] / h` | O(h) |
| **central** | `[f(p+h) − f(p−h)] / 2h` | **O(h²)** |

**Escolha de `h`:**
- grande demais: a normal "borra" detalhes e cantos;
- pequeno demais: a subtração de dois números quase iguais em `float` (≈7 dígitos) perde
  precisão e a normal fica ruidosa.

Um valor da ordem de `SURF_EPS` (por exemplo `0.001`) costuma funcionar bem.

Existe também a técnica do **tetraedro**, que usa 4 avaliações nos vértices de um tetraedro
em vez de 6 nos eixos (Quilez, *normals for an SDF*).

## Câmera: de pixel a raio
Com o `uv` do M1 (lado menor em `[-1, 1]`), construímos uma base ortonormal da câmera:

```
forward = normalize(alvo − posição)
right   = normalize(forward × (0, 1, 0))
up      = right × forward
```

e uma "tela virtual" a distância `focal` da câmera. O raio do pixel é:

```
rd = normalize(uv.x·right + uv.y·up + focal·forward)
```

Na borda do lado menor (`uv = ±1`), o ângulo entre o raio e o eixo da câmera é `fov/2`:

```
tan(fov/2) = 1 / focal   ⟹   focal = 1 / tan(fov/2)
```

Logo, quanto maior o FOV, menor a `focal` e mais "abertos" ficam os raios (grande-angular).

A câmera orbital da CPU guarda (yaw, pitch, distância) e converte em posição com
coordenadas esféricas:

```
posição = alvo + distância · (cos(pitch)·sin(yaw), sin(pitch), cos(pitch)·cos(yaw))
```

O pitch é limitado a ±1,5 rad (≈86°): olhando exatamente na vertical, `forward` fica
paralelo a `(0,1,0)` e o produto vetorial da base vale zero.

## Referências
- J. C. Hart, *Sphere Tracing: A Geometric Method for the Antialiased Ray Tracing of Implicit
  Surfaces*, The Visual Computer, 1996.
- I. Quilez, *Distance functions* — https://iquilezles.org/articles/distfunctions/
