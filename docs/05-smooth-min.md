# 05 — Operações CSG e smooth minimum

## CSG com SDFs
*Constructive Solid Geometry*: montar sólidos combinando outros por operações de conjuntos.
Com SDFs, cada operação vira uma conta simples sobre as distâncias `a = f_A(p)` e `b = f_B(p)`:

| operação | conjunto | SDF | por quê |
|---|---|---|---|
| união | A ∪ B | `min(a, b)` | dentro de A ∪ B ⇔ dentro de algum ⇔ o menor é negativo |
| interseção | A ∩ B | `max(a, b)` | dentro de ambos ⇔ o maior é negativo |
| complemento | fora de A | `−a` | troca dentro por fora |
| subtração | A − B = A ∩ (fora de B) | `max(a, −b)` | interseção com o complemento |

Sobre a **exatidão**: `min` é exato fora dos objetos. `max` e a subtração dão, em geral, um
**limite inferior**: são seguros para o sphere tracing, mas não exatos. Em todos os casos o
resultado continua 1-Lipschitz.

### A cena como cadeia
Na engine, os objetos são avaliados **em ordem**, e cada um opera sobre o resultado acumulado:

```
d = +∞
para cada objeto i:  d = operação_i(d, d_i)
d = min(d, chão)
```

Por isso "Esfera (União), Caixa (Subtração)" cava a caixa na esfera, enquanto a ordem inversa
cava algo que ainda não existe (resultado vazio). O chão entra por último com `min` comum, para
que os objetos não se fundam nele.

## O problema do `min`
Na união `min(a, b)`, a troca entre "o mais próximo é A" e "o mais próximo é B" acontece
bruscamente onde `a = b`. Ali a função tem um **bico** (é contínua, mas a derivada salta) e a
superfície tem uma **aresta viva** onde os objetos se encontram.

Queremos uma função que:
1. seja igual a `min(a, b)` quando os objetos estão longe um do outro (`|a − b| ≥ k`);
2. faça uma transição **suave** (derivada contínua, C¹) quando `|a − b| < k`;
3. nunca fique acima de `min(a, b)`, para "preencher" o vão entre os objetos.

O parâmetro `k` é a **largura da região de transição**, em unidades de distância.

## Dedução do smin polinomial (quadrático)
Escreva `x = a − b`. Então `min(a, b) = b + min(x, 0)`, e basta suavizar a função de uma
variável `m(x) = min(x, 0)`, que tem o bico em `x = 0`.

Mantemos `m(x) = min(x, 0)` fora de `[−k, k]` e somamos uma **correção** `c(x) ≤ 0` dentro.
As condições para a junção ser C¹ em `x = ±k`:

- `c(±k) = 0` (a função não salta);
- `c′(±k) = 0` (a derivada não salta, porque fora a correção é zero).

Por simetria, procuramos um `c` que dependa de `|x|`. A parábola mais simples com raiz
**dupla** em `|x| = k` (raiz dupla = valor e derivada nulos) é:

```
c(x) = −α (k − |x|)²
```

Para escolher `α`, impomos que a derivada de `m + c` também seja contínua em `x = 0`, onde
`min(x, 0)` tem derivada 1 à esquerda e 0 à direita:

```
x → 0⁻:  m′ = 1 + c′ = 1 − 2αk
x → 0⁺:  m′ = 0 + c′ = 2αk
1 − 2αk = 2αk   ⟹   α = 1 / (4k)
```

Definindo o **fator de transição**

```
h = max(k − |a − b|, 0) / k         (1 no meio da fusão, 0 fora dela)
```

a correção vira `c = −(k − |x|)² / (4k) = −h²·k/4`. Assim:

```
smin(a, b, k) = min(a, b) − h² · k / 4
```

Propriedades:
- **Fora da fusão** (`|a − b| ≥ k`): `h = 0` e o smin é igual ao `min`.
- **Afundamento máximo** em `a = b`: `h = 1`, então o smin fica `k/4` abaixo do `min`. É
  essa "sobra" negativa que cria a ponte entre os objetos.
- **`k = 0`:** a fórmula divide por zero, mas o limite matemático é o `min` comum. No código,
  esse caso precisa ser tratado à parte.

## O smin é seguro para o sphere tracing?
Dentro da fusão, as derivadas parciais são

```
∂smin/∂a = m′(x) ∈ [0, 1]         ∂smin/∂b = 1 − m′(x) ∈ [0, 1]
```

e somam 1. O gradiente do smin é então uma **média ponderada** de `∇a` e `∇b`. Como ambos têm
norma ≤ 1, o resultado também tem:

```
‖∇smin‖ ≤ (1 − s)‖∇b‖ + s‖∇a‖ ≤ 1
```

O smin continua **1-Lipschitz** e nunca superestima a distância, então a marcha é segura.
Mas ele **subestima** a distância na região de fusão: o raio dá passos menores do que poderia,
e o heatmap mostra essa região mais "quente".

## Máximo suave
Da identidade `max(a, b) = −min(−a, −b)`:

```
smax(a, b, k) = −smin(−a, −b, k)
```

Com ele vêm a interseção suave `smax(a, b, k)` e a subtração suave `smax(a, −b, k)`, que
arredondam as arestas do recorte.

## Mistura de cor
Na região de fusão, o ponto não pertence nem só a A nem só a B. Os pesos naturais são as
**derivadas parciais** do smin, que dizem quanto cada objeto influencia o resultado e somam 1.

Com `x = a − b`, derivando em relação a `b` (atenção: `∂x/∂b = −1`):

```
x > 0 (b mais perto):  ∂smin/∂b = 1 − 2α(k − |x|)
x < 0 (a mais perto):  ∂smin/∂b = 2α(k − |x|)          α = 1/(4k)
```

(Com a convenção `x = b − a`, os dois casos trocam de lugar, porque `∂x/∂b = +1`.)

Substituindo `α` e o valor de `|x|` em cada caso, os dois trechos viram **a mesma reta**:

```
w_b = clamp(0.5 + (a − b) / (2k), 0, 1)
cor = mix(cor_a, cor_b, w_b)
```

- No meio da fusão (`a = b`), `w_b = 0,5`: metade de cada cor.
- Nas bordas (`a − b = ±k`), `w_b` chega a 0 ou 1 e, fora delas, o `clamp` mantém esses
  valores. Sem o `clamp`, o `mix` extrapola a cor longe da fusão.
- `k = 0` precisa de tratamento explícito: em GLSL, dividir por zero tem resultado **não
  especificado**, e mesmo com IEEE `∞ · 0 = NaN`.

Testes: `w(1, 1, 0,5) = 0,5`, `w(0,2, 0,4, 0,5) = 0,3`, `w(2, 0, 0,5) = 1`, `w(0,3, 0,5, 0) = 0`,
e a simetria `w(a, b, k) + w(b, a, k) = 1`.

## Referência
- I. Quilez, *smooth minimum* — https://iquilezles.org/articles/smin/
