# 04 — Iluminação

Depois que o sphere tracing encontra o ponto `p` e a normal `n`, falta decidir a **cor**.
Usamos modelos locais: a cor depende só do ponto, da normal, da luz e da câmera.

## Luz direcional
Uma luz muito distante, como o Sol, chega com raios paralelos. Ela é descrita por uma
direção `l` (unitária, apontando **do ponto para a luz**) e uma cor/intensidade `c_luz`.
Na GUI, `l` vem de azimute e elevação, nas mesmas coordenadas esféricas da câmera.

## Difusa de Lambert
Uma superfície fosca espalha a luz igualmente em todas as direções, então o brilho não
depende de onde a câmera está. Depende só de **quanta luz chega por unidade de área**.

Um feixe de seção transversal `A` que chega inclinado de um ângulo `θ` em relação à normal
se espalha sobre uma área `A / cos θ` da superfície. A energia por unidade de área cai com
`cos θ`, que é a **lei do cosseno de Lambert**. Como `n` e `l` são unitários:

```
cos θ = n · l
```

- Se `n · l < 0`, a face está de costas para a luz e não recebe nada. Por isso usamos
  `max(n · l, 0)`.
- `n · l = 1` quando a luz chega perpendicular à superfície (brilho máximo).

## Luz ambiente
Na realidade, a luz quica entre as superfícies (iluminação indireta), e mesmo o lado de
sombra recebe alguma luz. A aproximação mais simples é um termo constante `k_amb`.

## Modelo do M3
Com `albedo` sendo a cor própria do objeto (a fração de luz que ele reflete por canal):

```
cor = albedo · (k_amb + c_luz · max(n · l, 0))
```

A multiplicação é componente a componente: um objeto laranja sob luz branca reflete
laranja; sob luz azul, quase nada.

## Modelo do M7
```
cor = albedo · (k_amb · AO + c_luz · max(n · l, 0) · sombra)  [+ especular]
cor = mix(cor, céu, 1 − e^(−ρ t))                              (névoa)
saída = cor^(1/2.2)                                             (gama)
```

A sombra multiplica só a parte **direta**: ela diz se a luz direcional chega ao ponto. O
ambiente representa luz que vem de todas as direções, então quem o atenua é a oclusão ambiente.

## Oclusão ambiente (AO)
A luz ambiente chega de todo o hemisfério acima da superfície. Num canto ou numa fresta,
parte desse hemisfério está tampada por outras superfícies, e o ponto recebe menos luz.

A SDF dá uma estimativa barata disso. Andando ao longo da normal, num lugar aberto,
`map(p + h·n) = h`: o ponto mais próximo da cena continua sendo `p`, de onde saímos. Se outra
superfície estiver perto, `map(p + h·n) < h`. A diferença `h − map(p + h·n)` mede quanto a
vizinhança está "fechada":

```
oclusão = Σᵢ wᵢ · (hᵢ − map(p + hᵢ n)),     hᵢ = 0.01 … 0.13,  wᵢ = 0.95ⁱ
AO = clamp(1 − 3 · oclusão, 0, 1)
```

- São só 5 avaliações de `map`, sem raios aleatórios. É uma aproximação, não uma integral
  sobre o hemisfério (como no *ray tracing*), mas captura bem o **contato**: o escurecimento
  onde um objeto encosta no chão.
- O alcance (até 0.13) define a escala da oclusão. Com valores maiores, objetos distantes
  também escurecem o ponto.
- O fator 3 e os pesos são ajustes visuais (Quilez).

## Névoa
Num meio uniforme (ar com partículas), a fração de luz que atravessa uma distância `t` sem
ser espalhada cai exponencialmente, pela **lei de Beer-Lambert**:

```
transmitância T(t) = e^(−ρ t)          ρ = densidade
cor = T · cor_objeto + (1 − T) · cor_névoa
```

- `ρ = 0` desliga a névoa. Com `ρ = 0.005`, a 10 unidades restam 95% da cor do objeto e, a
  100 unidades, 61%.
- A cor da névoa é a **cor do céu naquela direção**. Assim, o chão se dissolve no céu no
  horizonte e esconde o corte feito pela distância máxima e pelo limite de passos (visível no
  heatmap).

## Correção de gama
Os valores 0–255 de uma imagem (e das cores da GUI) não são proporcionais à energia da luz.
Eles estão codificados em **sRGB**, aproximadamente `valor = energia^(1/2.2)`. A codificação
gasta mais níveis nos tons escuros, onde o olho distingue melhor.

A iluminação soma e multiplica **energia**: duas luzes iguais dão o dobro de energia, não o
dobro do valor sRGB. Então:

1. **Decodificar** as cores de entrada (albedo, cor da luz, céu): `linear = srgb^2.2`
2. Iluminar em espaço linear.
3. **Codificar** o resultado para a tela: `srgb = linear^(1/2.2)`

Sem isso, a transição de luz para sombra fica escura e abrupta demais, e as cores saturadas
"queimam". Consequências práticas:

- Parâmetros de energia (ambiente, névoa) ficam bem menores no espaço linear. Um ambiente de
  `0.04` aparece como `0.04^(1/2.2) ≈ 0.23` na tela.
- Misturas também acontecem em espaço linear. Uma névoa forte "lava" os tons escuros, porque
  um pouco de energia somada a um canal escuro sobe muito depois de codificada.
- O checkbox "Correção de gama" na GUI desliga as duas conversões, o que permite comparar.

## Sombras suaves
**Sombra dura:** de `p`, marcha-se um raio **em direção à luz** (`rd = l`). Se ele acertar
alguma superfície antes de `tmax`, a luz direta não chega a `p`. É o mesmo sphere tracing do
raio da câmera, mas agora interessa *se* acertou, não *onde*.

**Penumbra:** a cada passo, `h = map(x)` é a folga entre o raio e a superfície mais próxima, e
`t` é a distância percorrida. Vista de `p`, essa folga forma um ângulo de aproximadamente
`h/t`. Uma luz real tem tamanho angular. Se a folga for menor que ele, parte do disco da luz
fica tampada. Com `k ≈ 1/(raio angular da luz)`:

```
sombra = clamp( min ao longo do raio de k · h / t,  0, 1 )
```

- Raio que passa raspando (h pequeno) dá penumbra escura. Raio que passa longe tem
  `k·h/t ≥ 1` e o ponto fica totalmente iluminado.
- **k grande** = luz pequena, sombra dura. **k pequeno** = luz grande, sombra macia.
- A penumbra **alarga com a distância** entre o objeto e o chão (o `t` no denominador), como
  numa sombra real.
- Num ray tracer tradicional, o raio de sombra só diz sim ou não, e a penumbra exige vários
  raios para pontos diferentes da luz. Aqui, um raio só estima a penumbra porque a SDF diz
  *quão perto* ele passou. É uma aproximação, mas muito mais barata.

Detalhes de implementação (`softShadow` em `raymarch.frag`):
- **`tmin > 0`:** `p` está colado na superfície (`map(p) ≈ 0`), e em `t = 0` o `h/t` divide por
  zero.
- **Saída antecipada:** quando o raio encosta no obstáculo, `h → 0` e o laço "para" na
  superfície. Sem sair quando `sombra < 0.001`, todo pixel de umbra gasta o limite de
  iterações (medido: 64 de 64). Não é preciso o teste `h < ε` do `march`, porque `h → 0` já
  leva `k·h/t → 0`.
- **`clamp` no retorno:** parado na superfície, o raio pode cair um fio *dentro* dela por erro
  de ponto flutuante (`h < 0`). Uma sombra negativa deixaria a difusa negativa, e `pow` de um
  valor negativo na correção de gama é **indefinido** em GLSL.

## Especular de Blinn-Phong
Uma superfície **lisa** reflete a luz preferencialmente na direção do espelho, e aparece um
ponto de brilho que **se move com a câmera**. Os vetores, todos unitários e saindo do ponto:

| vetor | significado | no shader |
|---|---|---|
| `n` | normal | `n` |
| `l` | para a luz | `uLightDir` |
| `v` | para a câmera | `-rd` (o raio vai *da* câmera *para* o ponto) |
| `h` | meio-termo | `normalize(l + v)` |

Se a superfície fosse um espelho perfeito, `l` refletiria exatamente em `v` quando `n = h`.
Então `n · h` mede o quanto a superfície "espelha a luz para a câmera" (Blinn, 1977). O modelo
de Phong (1975) compara o reflexo de `l` com `v`. O de Blinn é mais barato e mais estável em
ângulos rasantes.

```
especular = max(n · l, 0) · sombra · max(n · h, 0)^s
cor += c_luz · k_esp · especular
```

- **Expoente `s`:** elevar um número < 1 a uma potência alta o faz cair rápido para 0. Com
  `s = 4`, o brilho é largo (plástico fosco); com `s = 256`, é um ponto nítido (verniz).
- **Fora do albedo:** em dielétricos (plástico, tinta), o reflexo tem a cor da **luz**.
- **As portas são as mesmas da difusa:** `max(n·l, 0)` e `sombra`. O `h` é a média entre `l` e
  `v`, então pode apontar para o lado da normal mesmo com a luz atrás da face.
  Contraexemplo: `n = (0,1,0)`, `l = (0.985, −0.174, 0)`, `v = (−0.5, 0.866, 0)` dão `n·l = −0.17`
  (face sem luz) e `n·h = 0.82`. Com `s = 4`, sem a porta, a face ganharia `0.82⁴ ≈ 0.45` de
  brilho.
- **Os três produtos escalares respondem a perguntas diferentes:** `n·l` pergunta se a luz
  chega, `n·v` se a câmera vê (quase sempre sim, para pontos visíveis), e `n·h` se a face
  espelha a luz para a câmera. Só `n·l` é porta da luz direta.

## Custo
Cada pixel que acerta a cena agora faz: a marcha da câmera (até `uMaxSteps` avaliações de
`map`), a normal (4), a marcha de sombra (até 64) e a AO (5). Na galeria, em resolução cheia,
o FPS caiu de ~50 (M6) para ~28. A escala de resolução compensa na demo.

### O chão não entra na marcha de sombra
Com a luz acima do horizonte (`l.y > 0`), um raio que sai de `y ≥ 0` subindo nunca volta ao
plano `y = 0`, então o chão **não pode bloquear** a luz. Mantê-lo no `map` da sombra causava
dois problemas com luz rasante:

- **Custo e degraus:** saindo do chão, `h ≈ y = t · sin(elevação)`. A 5°, `t` cresce só 8,7%
  por passo, e ir de 0.02 a 20 exige `ln(1000)/ln(1.087) ≈ 83` passos, mais que o limite de 64.
  Os raios paravam no meio do caminho, e as sombras longas ficavam em degraus.
- **Falso escurecimento:** `k·h/t ≥ k · sin(elevação)`, que fica < 1 abaixo de ~7° (com
  `k = 8`), e todo o chão ganhava "penumbra" sem nenhum objeto por perto.

Por isso o `softShadow` usa `mapObjects` (a cena sem o chão), e a elevação da luz vai de 0° a
90° na GUI. Além de corrigir as sombras, isso aumentou o FPS em 10–15% (`docs/07`).

## Limitações conhecidas
- A AO é local (alcance de 0.13) e não captura oclusão de objetos distantes.
- A penumbra `k·h/t` é uma aproximação. Ela é mais larga que a real logo atrás de objetos
  finos e não depende da distância até a luz (que é direcional).
