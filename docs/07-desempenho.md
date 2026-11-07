# 07 — Desempenho

## Onde está o custo
No raymarching, o custo é **por pixel** e depende do conteúdo: cada pixel que acerta a cena
faz a marcha da câmera (até `uMaxSteps` avaliações de `map`), a normal (4), a marcha de sombra
(até 64) e a AO (5). Cada avaliação de `map` percorre **todos** os objetos.

### Metodologia de medição
FPS médio de 600 quadros (após 200 de aquecimento), VSync desligado, GUI escondida, resolução
interna 936 × 1018. Medições repetidas deram o mesmo valor com diferença de 0,1 FPS.

### Divisão do tempo (Galeria, sem culling, 36,2 ms/quadro)
| configuração | FPS | ms/quadro | custo atribuído |
|---|---|---|---|
| tudo ligado | 27,6 | 36,2 | |
| sem sombras | 34,4 | 29,1 | sombras ≈ 7,1 ms |
| sem AO | 29,9 | 33,4 | AO ≈ 2,8 ms |
| sem sombras e sem AO | 37,8 | 26,5 | marcha da câmera + normal ≈ 26 ms |
| escala de resolução 0,5 | 98,8 | 10,1 | ¼ dos pixels → ~3,6× mais rápido |

O maior custo é o **sphere tracing principal**. Os raios rasantes ao chão, perto do horizonte,
gastam até 128 passos, e cada passo avalia todos os objetos (ver o heatmap). Por isso a
otimização que mais rende é **baratear o `map`**, já que ela acelera todas as etapas de uma vez.

## Culling por esfera envolvente
Cada objeto ganha uma **esfera envolvente**: centrada na origem do objeto e com o menor raio
que o contém (calculado na CPU, `boundingRadius` em `scene.cpp`). Ela não depende da rotação.

| tipo | raio envolvente |
|---|---|
| esfera | `r` |
| caixa / caixa arredondada | `‖b‖` (o canto) |
| toro | `R + r` |
| cápsula | meio comprimento + raio |
| cilindro | `√(r² + h²)` (a borda da tampa) |
| cone | `√(max(r₁, r₂)² + h²)` |
| octaedro | `s` (o vértice) |

A distância até a esfera, `dBound = ‖p − centro‖ − R`, custa um `length`. Como a esfera contém
o objeto, ela é um **limite inferior da distância real** até ele.

### Quando é seguro pular?
O smin/smax só difere do min/max quando `|a − b| < k` (`docs/05`). Para cada operação, a
condição exata para o resultado continuar `d` (o acumulado dos objetos anteriores) é:

| operação | resultado fica `d` quando | tipo de condição | com `dBound` |
|---|---|---|---|
| união `smin(d, dᵢ, k)` | `dᵢ − d ≥ k` | `dᵢ` grande | `dBound − d ≥ k` ✅ |
| subtração `smax(d, −dᵢ, k)` | `−dᵢ ≤ d − k` ⟺ `dᵢ ≥ k − d` | `dᵢ` grande | `dBound ≥ k − d` ✅ |
| interseção `smax(d, dᵢ, k)` | `dᵢ ≤ d − k` | `dᵢ` pequeno | ❌ nunca pula |

Um limite **inferior** só prova afirmações do tipo "`dᵢ` é grande". União e subtração ignoram
objetos distantes. Na interseção, ao contrário, o objeto distante **domina** o `max` (ela
recorta tudo fora dele). A cor também se mantém: na união, nessas condições, `sminBlend` dá
0. A subtração e a interseção não mudam a cor.

**Erro comum:** comparar `dBound` só com `k` ("`dBound > k`"). O `k` se compara com a
*diferença* entre os argumentos. Com `d = 5`, `dᵢ = 1` e `k = 0.5`, isso pularia um objeto que é
o mais próximo, e o `map` devolveria 5 em vez de 1. O raio daria um passo grande demais e
**atravessaria o objeto**.

### Verificação pixel a pixel
Quadro fixo (animação pausada em t = 0), lido com `glReadPixels` e comparado com a versão sem
culling:

| preset | pixels diferentes |
|---|---|
| Fusão (tem subtração) | 0 |
| Dança | 0 |
| Galeria | 75 (0,008%), diferença máxima 2/255 |

Investigação das diferenças na Galeria:
1. Duas execuções iguais dão 0 diferenças: a renderização é determinística.
2. Mesma estrutura de código, mas sem nunca pular: 0 diferenças. Não é o compilador.
3. Raio envolvente inflado em 0,001: as mesmas 75. Não é arredondamento.
4. Excluir **só o octaedro** do culling: 0 diferenças. **É o octaedro.**

O octaedro usa uma SDF que é **limite inferior** (`docs/03`): `(|x|+|y|+|z| − s)/√3`. Ao longo
de um eixo, ela vale `(x − s)/√3`, enquanto `dBound = x − s` é a distância real. Então
`dBound > dᵢ`, e o culling troca uma subestimativa pela distância correta. O resultado continua
≤ distância real (seguro para o sphere tracing) e fica **mais preciso**. A diferença de 2/255
é a penumbra e a AO perto do octaedro.

> Com SDFs exatas, o culling é exato. Com limites inferiores, ele continua seguro, mas o
> resultado pode mudar (para melhor).

### Resultado
| preset | sem culling | com culling | ganho |
|---|---|---|---|
| Galeria (8 objetos espalhados) | 27,7 | 58,3 | 2,1× |
| Dança (7 objetos, vários afastados) | 38,6 | 70,4 | 1,8× |
| Fusão (4 objetos sobrepostos) | 118,3 | 114,2 | −3% |

Depois, tirar o chão da marcha de sombra (`docs/04`) deu mais 10–15%: Galeria 64,5, Fusão
132,1 e Dança 79,3 FPS. Com luz rasante, os raios de sombra que saíam do chão davam passos
pequenos (`h ≈ t·sin(elevação)`) por dezenas de iterações.

O culling compensa em cenas **espalhadas**. Quando todos os objetos se sobrepõem, quase nada é
pulado e o teste vira custo extra. É o comportamento típico de estruturas de aceleração: elas
custam um pouco no pior caso para ganhar muito no caso comum.
