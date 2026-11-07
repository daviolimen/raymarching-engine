# 03 — Primitivas e transformações

Referência principal: I. Quilez, *distance functions*
(https://iquilezles.org/articles/distfunctions/).

## Espaço do objeto
Cada SDF é escrita para a forma **na origem e sem rotação**. Para posicionar um objeto,
levamos o ponto do mundo para o referencial do objeto aplicando a transformação **inversa**:

```
p_local = Rᵀ · (p − posição)
```

- **Translação:** subtrair a posição.
- **Rotação:** para matrizes de rotação, `R⁻¹ = Rᵀ` (são ortogonais). No GLSL: `transpose(R) * v`.
- A ordem importa: primeiro desfazemos a translação, depois a rotação (o inverso da ordem
  em que o objeto foi transformado).
- Rotação e translação **preservam distâncias** (isometrias), então a SDF continua exata.
  Uma escala **não** preserva: escalar por `s` exige multiplicar o resultado por `s`.

### Matrizes de rotação
Rotação de ângulo `a` em torno do eixo y:

```
       ⎡  cos a   0   sin a ⎤
Ry  =  ⎢   0      1    0    ⎥
       ⎣ −sin a   0   cos a ⎦
```

O GLSL monta `mat3(...)` **por colunas**: os três primeiros números são a primeira coluna.
Composição: `Ry(spin) * Rx(90°)` primeiro deita o toro em pé (Rx) e depois o gira em torno
do eixo vertical (Ry).

## SDF exata × limite inferior (*bound*)
- **Exata:** `f(p)` é exatamente a distância até a superfície. É o melhor caso para o
  sphere tracing.
- **Limite inferior:** `f(p) ≤ distância real`. O sphere tracing continua **correto** (nunca
  atravessa a superfície), mas dá passos menores que o possível e gasta mais iterações.
- Se `f(p)` **superestimar** a distância, o raio pode atravessar objetos, o que causa buracos
  e artefatos.

O heatmap de passos mostra a diferença: compare o octaedro (limite inferior) com o cone (exata).

## Plano
Plano horizontal `y = 0`: a distância é a altura, `f(p) = p.y`.
Um plano qualquer com normal unitária `n` a distância `h` da origem: `f(p) = n·p + h`.

## Caixa
Caixa centrada na origem com semi-dimensões `b = (bx, by, bz)`.

1. **Simetria:** a caixa é simétrica nos três eixos, então `abs(p)` leva qualquer ponto para o
   primeiro octante sem mudar a distância.
2. **Comparar com a borda:** `q = abs(p) − b` diz, eixo a eixo, quanto o ponto passa da face
   (positivo) ou quanto falta para chegar nela (negativo).
3. **Fora:** um componente negativo de `q` quer dizer que, naquele eixo, o ponto já está na
   faixa da caixa e não precisa se mover nessa direção. Os positivos dão o deslocamento até o
   ponto mais próximo: `‖max(q, 0)‖`. Isso vale nos três casos: em frente a uma face (1 eixo
   positivo), a uma aresta (2) ou a um vértice (3).
4. **Dentro:** todos os componentes são negativos, e a borda mais próxima é a face com o
   componente **menos negativo**: `max(qx, qy, qz)`.
5. **Juntando sem `if`:** cada termo se anula no caso em que não vale. Dentro,
   `max(q, 0) = 0`; fora, `min(max(qx, qy, qz), 0) = 0`:

```
sdBox(p, b) = ‖max(q, 0)‖ + min(max(qx, qy, qz), 0),   q = |p| − b
```

| ponto (b = 1) | q | resultado |
|---|---|---|
| (3, 0, 0), face | (2, −1, −1) | 2 |
| (3, 3, 0), aresta | (2, 2, −1) | √8 ≈ 2,83 |
| (3, 3, 3), vértice | (2, 2, 2) | √12 ≈ 3,46 |
| (0,5, 0, 0), dentro | (−0,5, −1, −1) | −0,5 |

**Dois erros instrutivos:**
- Usar só `max(qx, qy, qz)` em todo lugar dá uma SDF que é um **limite inferior** fora (2 em
  vez de √8 no ponto da aresta): a mesma ideia do octaedro, a distância até o plano da face.
- `‖q‖` (sem separar os casos) é a distância até o **vértice** mais próximo: a caixa vira 8
  pontos, e a caixa arredondada vira 8 esferas.

**Por que evitar `if` em shaders:** a GPU executa grupos de pixels vizinhos em conjunto
(*warps*/*wavefronts*). Se pixels do mesmo grupo vão para ramos diferentes de um `if`, o grupo
executa os dois ramos (*divergência*).

## Cilindro
Por simetria de revolução, no plano `(‖p.xz‖, p.y)` o cilindro de raio `r` e altura `2h` é o
retângulo `[0, r] × [−h, h]`. É a fórmula da caixa em 2D:

```
q = (‖p.xz‖, |p.y|) − (r, h)
sdCylinder = ‖max(q, 0)‖ + min(max(qx, qy), 0)
```

## Caixa arredondada (*rounding*)
Para qualquer SDF `f`, a função `f(p) − r` tem como superfície o conjunto dos pontos a
distância `r` da superfície original, o que "infla" a forma em `r`. Cantos e arestas viram
arcos de raio `r`. Encolhendo antes a caixa em `r`, o tamanho total se mantém:

```
sdRoundBox(p, b, r) = sdBox(p, b − r) − r
```

## Cone (tronco de cone)
O tronco vai de `y = −h` (raio `r1`) a `y = +h` (raio `r2`), e com `r2 = 0` é um cone.

**Redução a 2D:** o sólido é de revolução em torno do eixo y. A distância depende só de
quão longe o ponto está do eixo e da sua altura:

```
q = (‖p.xz‖, p.y)
```

No plano `q`, a forma é um trapézio: o eixo, a tampa de baixo, a lateral inclinada e a tampa
de cima. A distância até ele é a menor entre:

- **`ca`**, o vetor até a tampa mais próxima (a de baixo se `q.y < 0`, senão a de cima),
  tratando a tampa como o segmento de `0` até o seu raio;
- **`cb`**, o vetor até a **lateral**, o segmento de `(r1, −h)` a `(r2, h)`. É a projeção de
  ponto sobre segmento, a mesma ideia da cápsula (`clamp` do parâmetro em `[0, 1]`).

Sinal: o ponto está dentro se está do lado de dentro da lateral (`cb.x < 0`) **e** entre as
duas tampas (`ca.y < 0`).

## Octaedro (limite inferior)
O octaedro regular é `|x| + |y| + |z| ≤ s`. Por simetria (`abs(p)`) basta o primeiro octante,
onde a face é o plano `x + y + z = s`, de normal `(1,1,1)/√3`. A distância até esse **plano**
é

```
(x + y + z − s) / √3
```

Ela é **exata dentro** e um **limite inferior fora**: o ponto mais próximo do plano pode cair
fora do triângulo da face, e então o ponto real mais próximo (numa aresta ou num vértice) está
mais longe. É um exemplo de SDF que é segura sem ser exata.

## Toro
Toro deitado no plano xz, de raio maior `R` (do centro até o meio do tubo) e raio do tubo `r`.
É o conjunto dos pontos a distância `r` do **círculo central** (raio `R`, em y = 0).

Distância de `p` até esse círculo, reduzindo a 2D:
- na horizontal, `p` está a `‖p.xz‖` do eixo y, então a `‖p.xz‖ − R` do círculo;
- na vertical, o círculo está em y = 0, então a diferença é `p.y`.

O vetor 2D `(‖p.xz‖ − R, p.y)` liga o ponto mais próximo do círculo até `p`. Assim, como na
esfera:

```
sdTorus(p, R, r) = ‖(‖p.xz‖ − R, p.y)‖ − r
```

| ponto (R = 1, r = 0,25) | vetor 2D | resultado |
|---|---|---|
| (1, 0, 0), centro do tubo | (0, 0) | −0,25 |
| (0, 0, 0), buraco | (−1, 0) | 0,75 |
| (2, 0, 0), fora | (1, 0) | 0,75 |
| (1, 1, 0), acima | (0, 1) | 0,75 |

No eixo y (o centro do buraco), todos os pontos do círculo estão à mesma distância: o ponto
mais próximo não é único e a SDF tem um vinco ali. Isso não atrapalha, porque essa região
está longe da superfície.

## Cápsula
A cápsula é o conjunto dos pontos a distância `r` do **segmento** de `a` até `b`, isto é, um
cilindro com pontas hemisféricas.

O segmento é `a + h·(b − a)`, com `h ∈ [0, 1]`. Com `pa = p − a` e `ba = b − a`:

1. **Projeção na reta:** o ponto da reta mais próximo de `p` tem parâmetro
   `h = (pa · ba) / (ba · ba)`.
2. **Limitar ao segmento:** `h = clamp(h, 0, 1)`. Se a projeção cai antes de `a` ou depois de
   `b`, o mais próximo é a ponta.
3. **Distância:** `sdCapsule = ‖pa − h·ba‖ − r`.

**Por que dividir por `|ba|²` e não por `|ba|`:** análise de unidades. `pa · ba` tem unidade L²;
dividido por `|ba|` (L) dá a *projeção escalar* (L, uma distância ao longo do segmento), mas
`h` precisa ser **adimensional** (uma fração do segmento). Dividindo por `|ba|` em vez de
`|ba|²`, o resultado só fica certo quando o segmento tem comprimento 1, o que pode esconder
o bug em testes com valores "redondos".

| ponto (a = (0,−1,0), b = (0,1,0), r = 0,3) | h | resultado |
|---|---|---|
| (1; 0,5; 0) | 0,75 | 0,7 |
| (0, 2, 0) | 1 (clamp) | 0,7 |
| (0, 0, 0) | 0,5 | −0,3 |

O mesmo cálculo (ponto mais próximo num segmento) aparece na lateral do tronco de cone.
