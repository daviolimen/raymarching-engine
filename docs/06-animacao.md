# 06 — Animação: trajetórias paramétricas

## Onde a animação acontece
Cada objeto tem uma posição **base** `p₀` e um modo de movimento. A cada quadro, a CPU calcula

```
p(t) = p₀ + Δ(t)
```

e envia `p(t)` nos mesmos uniforms do M5. O shader não muda; ele só vê posições novas.

### Tempo de animação
O tempo não é o relógio (`glfwGetTime`), mas um acumulador:

```
t ← t + Δt_quadro · velocidade_global      (só se não estiver pausado)
```

Assim dá para pausar (tecla espaço), acelerar ou desacelerar sem saltos.

## Parâmetros comuns
| símbolo | campo | significado |
|---|---|---|
| ω | `animSpeed` | velocidade angular (rad/s). Período `T = 2π/ω` |
| A | `animAmp` | amplitude (unidades de distância) |
| φ | `animPhase` | fase (rad): onde o objeto está no ciclo em t = 0 |

O **ângulo** `θ(t) = ωt + φ` cresce linearmente com o tempo. Dois objetos com o mesmo ω e
fases diferentes fazem o mesmo movimento, defasado no tempo. Com `φ = π` (meio ciclo), eles
ficam em **oposição de fase**: quando um vai para +, o outro vai para −.

## Órbita: movimento circular uniforme
```
Δ(t) = (A cos θ, 0, A sin θ)
```

- `‖Δ‖ = A` sempre (pois `cos² + sin² = 1`): o objeto fica numa circunferência de raio A.
- Velocidade: `Δ′(t) = Aω(−sin θ, 0, cos θ)`, com módulo constante `Aω`, tangente à circunferência.
- Aceleração: `Δ″(t) = −ω² Δ(t)`, que aponta para o centro (aceleração centrípeta `Aω²`).

## Oscilação: movimento harmônico simples
Vai e volta ao longo de uma direção `d`, até a distância `A` de cada lado da posição base:

```
Δ(t) = A · d̂ · sin θ,       d̂ = d / ‖d‖
```

- É a **projeção da órbita** sobre um eixo: a órbita são duas oscilações (x e z) defasadas de 90°.
- **Normalizar `d`** separa as responsabilidades: `d` diz *para onde*, `A` diz *quanto*. Sem
  normalizar, a amplitude real seria `A·‖d‖`.
- **Direção nula:** `d/‖d‖ = 0/0 = NaN`, que contaminaria a posição enviada ao shader. Com
  `‖d‖ = 0`, o deslocamento é zero.
- **`sin` × `cos`:** o mesmo movimento, defasado de 90°. Com `sin`, o objeto começa (φ = 0) na
  posição base com velocidade máxima; com `cos`, começa parado no extremo.
- Velocidade `Aω cos θ`: máxima no centro e nula nos extremos, onde o movimento inverte.

## Quique: uma parábola por ciclo
Com gravidade constante, a altura num voo é uma parábola no tempo. Com a **fração do ciclo**

```
u = θ/2π − ⌊θ/2π⌋ ∈ [0, 1)
Δ_y(t) = 4A · u(1 − u),     Δ_x = Δ_z = 0
```

- `u = 0` e `u = 1` dão 0 (contato com a base); `u = 1/2` dá `A` (topo).
- `d²Δ_y/du² = −8A` é constante, ou seja, aceleração constante (gravidade).
  Em unidades de tempo, `g = 8A/T²` com `T = 2π/ω`. No preset "Dança" (A = 1,5, ω = 3),
  `g ≈ 2,7`: um quique "lunar".
- Usar `θ/2π` em vez de `t/T` evita dividir por ω (que pode ser 0).
- `x − ⌊x⌋` dá sempre um valor em `[0, 1)`. Já `fmod(x, 1)` dá um valor negativo para `x < 0`.
- No contato, a velocidade inverte instantaneamente (de `−4A/T` para `+4A/T`), o "bico" da
  colisão elástica. Com smin entre a gota e a poça, a fusão suaviza o contato visualmente.
- O eixo vertical da engine é **y** (o chão é `y = 0`).

## Fundir e separar
Pelo `docs/05`, o smin afunda no máximo `k/4`, no ponto onde `a = b`. Para duas superfícies
separadas por um vão `g`, o ponto médio tem `a = b = g/2`, e

```
smin = g/2 − k/4 < 0   ⟺   g < k/2
```

A **ponte** entre os objetos aparece quando o vão fica menor que `k/2`. Animar o vão através
desse limite faz os objetos se juntarem e se separarem. No preset "Dança", as esferas laterais
(raio 0,4) oscilam com A = 0,9 em torno de x = ±1,4, e o núcleo tem raio 0,6. O vão vai de
cerca de 1,3 até **negativo** (sobreposição), cruzando o limite `k/2 = 0,3` duas vezes por ciclo.
