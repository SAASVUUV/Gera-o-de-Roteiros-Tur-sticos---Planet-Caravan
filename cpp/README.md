# GRASP em C++

GRASP dos autores (`src/heuristics/grasp.py` e o que ele usa) traduzido para C++, cerca de 15× mais rápido que o Python. Cada correção gera uma **versão**, e cada versão grava seus resultados com nome próprio, para não misturar nem sobrescrever os anteriores:

| Versão | O que muda | Onde está | Resultados |
|---|---|---|---|
| tradução literal | nada: idêntica ao Python, inclusive os bugs | commit `70eb95a` | `results_cpp.txt` (coluna `GRASP`) |
| `GRASP_v1` | busca local corrigida: nunca viola orçamento nem distância | commit `c09da9d` | `results_grasp_v1.txt` (coluna `GRASP_v1`) |
| `GRASP_v2` | busca local testa os candidatos em ordem, pelo efeito no trajeto, até um caber | commit `32fdc39` | `results_grasp_v2.txt` (coluna `GRASP_v2`) |
| `GRASP_v3` (atual) | o roteiro do Greedy é a solução inicial; o GRASP só o substitui com fitness ≥ | código atual | `results_grasp_v3.txt` (coluna `GRASP_v3`, 7ª coluna com a origem) |

O C++ reproduz o gerador aleatório do numpy. A tradução literal, com a semente 1, gera exatamente as mesmas 1800 linhas GRASP de `Results Analysis/My Results/results.txt`. A partir da `GRASP_v1`, as rotas mudam porque a busca local aceita e recusa trocas diferentes. Para ver uma versão anterior: `git show <commit>:cpp/src/heuristics/grasp.cpp`.

Nenhuma versão muda as regras do problema (4 locais por dia, máximo de 45), para que a comparação com os autores continue justa.

### Resultado da GRASP_v1 (parâmetros padrão, semente 1)

- **100% das soluções viáveis.** O GRASP dos autores tinha 75%: 458 de 1800 violavam distância ou orçamento.
- **Fitness médio maior que o dos autores em todos os 18 grupos**, cerca de +0,6 em média quando comparado só com as soluções viáveis deles. A verificação antiga somava o trecho novo sem descontar o antigo, o que superestimava a distância e recusava trocas válidas. A nova faz a conta exata e aceita essas trocas.
- **Ainda perde para o Greedy** com orçamento ≥ 1000 e 20 km: 44,0–44,2 contra 45,0, com 100 iterações.

### Resultado da GRASP_v2 (parâmetros padrão, semente 1)

**O que mudou na busca local.** Para cada local P, entre A (anterior) e C (próximo, ou o hotel), os candidatos de mesma categoria e nota maior são ordenados pelo efeito da troca no trajeto, `Δ = d(A→N) + d(N→C) − d(A→P) − d(P→C)`, e testados em ordem até um caber no orçamento e na distância:

- **Grupo 1 (Δ ≤ 0, a troca não aumenta o dia):** maior ganho de nota primeiro. Aqui a distância não limita, então o que importa é o fitness. A razão Δ/ganho poria os de menor ganho na frente, porque Δ é negativo.
- **Grupo 2 (Δ > 0):** menor Δ/ganho primeiro, ou seja, quem anda menos por ponto de nota.

A v1 testava só o candidato mais próximo de P e desistia se ele não coubesse. A v2 não sorteia nada na busca local: os empates vão para o menor ID.

**Resultados:**

- **100% das soluções viáveis**, inclusive no teste de estresse: 10.800 de 10.800, com 1–4 dias, 2–9 locais por dia, categoria 1–3, orçamento 150–100.000 e distância 1,5–200 km.
- **Fitness médio maior que o da v1 em todos os 18 grupos**, +1,2 em média. Com 100 iterações fica em 44,6–45,0 (o máximo é 45), e a melhor solução de cada grupo chega a 45.
- **Empata ou vence o Greedy em 12 de 18 grupos.** Perde por pouco só onde o Greedy faz 45: orçamento ≥ 1000 e 20 km, por exemplo 44,98 contra 45,0 com 100 iterações.
- **Cerca de 1,4× mais lenta por execução** que a v1, porque testa mais candidatos. Continua na casa dos milissegundos (cerca de 40 ms com 100 iterações).

### Resultado da GRASP_v3 (parâmetros padrão, semente 1)

**O que mudou.** Antes das iterações, o GRASP monta o roteiro do Greedy (tradução literal do `simple_greedy.py`, sem mudar a lógica) e o usa como melhor solução inicial. Uma iteração do GRASP substitui essa semente se tiver fitness **maior ou igual** ao dela; entre soluções do próprio GRASP vale a regra de antes (só substitui se for maior). Se nenhum hotel nota 5 couber no orçamento, o Greedy falha, como no Python, e o GRASP segue sem semente. O tempo do Greedy entra no tempo de cada execução.

**A 7ª coluna diz de onde veio a solução final.** `grasp` quer dizer que uma iteração do GRASP alcançou ou superou o Greedy por conta própria. `greedy` quer dizer que nenhuma alcançou, e ficou a semente. O `check_results` mostra isso como `do_grasp` no resumo. Use essa coluna para separar o mérito do GRASP da garantia dada pela semente: "GRASP_v3 ≥ Greedy" agora vale por construção.

**Resultados:**

- **O Greedy não usa números aleatórios**, então as iterações são exatamente as da v2: cada execução da v3 é igual à da v2 quando a v2 alcança o Greedy, e igual ao Greedy quando não alcança. Isso foi conferido linha a linha nas 1800 execuções.
- **Só muda onde o Greedy faz 45** (orçamento ≥ 1000 e 20 km), mais algumas execuções de 1000/10 km com 10 iterações. Nesses grupos a média passa a ser 45,00. Nos demais, a v3 é igual à v2.
- **`do_grasp`**: 100% em quase todos os grupos. Onde o Greedy faz 45, o GRASP sozinho chega lá em 14–22% das execuções com 10 iterações, 61–77% com 50 e 81–95% com 100.
- **100% viável**, inclusive no teste de estresse (10.800 soluções).

O Greedy em C++ também pode ser rodado sozinho com `cpp/build/greedy`. Ele reproduz exatamente as 600 linhas Greedy de `results.txt`.

## Como usar

Todos os comandos são rodados a partir da **raiz do repositório**.

### 1. Exportar os dados (uma vez, ou quando a matriz/CSV mudar)

O pré-processamento continua em Python (`prepare_data` em `src/main.py` e `build_places_parameters` em `src/opt_call.py`). O script grava o resultado em `Input/RJ_instance/`:

```
python scripts/export_instance.py
```

Se a máquina não tiver Python com numpy/pandas:

```
curl -LsSf https://astral.sh/uv/install.sh | sh
uv venv --python 3.11 .venv
uv pip install --python .venv/bin/python -r requirements.txt
.venv/bin/python scripts/export_instance.py
```

### 2. Compilar

```
make -C cpp
```

O comando gera um executável em `cpp/build/` para cada arquivo de `cpp/src/apps/`:

- `grasp`: o GRASP (versão atual, `GRASP_v3`).
- `greedy`: o Greedy dos autores (tradução literal), grava em `results_greedy_cpp.txt`.
- `check_results`: o verificador de viabilidade.

Precisa de um g++ com C++20 e não tem outras dependências: o leitor de JSON, `third_party/nlohmann/json.hpp`, já vem junto.

### 3. Rodar

```
cpp/build/grasp
```

O programa lê `Parameters/user.json`, `algorithm.json` e `experiments.json`. Ele roda todas as combinações na mesma ordem do `opt_call.py`, executando só o GRASP. A cada execução, acrescenta uma linha a `Results Analysis/My Results/results_grasp_v3.txt`, no formato do Python com uma 7ª coluna (a origem da solução: `greedy` ou `grasp`) e `GRASP_v3` na coluna do algoritmo. Antes de cada combinação de parâmetros do algoritmo, escreve um cabeçalho como `# GRASP_v3 max_iterations=10`, que o `check_results` usa para separar os grupos. Essa linha não existe na saída do Python.

Uma versão nova deve trocar `GRASP_NAME` em `heuristics/grasp.hpp` e o arquivo padrão em `apps/grasp.cpp`. Assim os resultados de cada versão ficam separados.

Opções:

| Opção | Padrão | |
|---|---|---|
| `--output ARQUIVO` | `Results Analysis/My Results/results_grasp_v3.txt` | arquivo de saída (só acrescenta) |
| `--instance PASTA` | `Input/RJ_instance` | dados exportados |
| `--seed N` | `1` | semente; 1 é a do `main.py` |

### 4. Verificar a viabilidade dos resultados

```
cpp/build/check_results "Results Analysis/My Results/results.txt"
```

O programa aceita arquivos do Python (GRASP e Greedy) e do C++. Ele faz três coisas:

- **Resumo por grupo** (seção, budget, distância): % de soluções viáveis, fitness médio de todas e só das viáveis, melhor fitness viável e quantas soluções violam cada restrição. Ele aparece no terminal e também é salvo em `<arquivo>_summary.txt` ao lado do original (ou onde `--summary` indicar), com data e uma legenda das colunas.
- **TSV com uma linha por solução**, em `<arquivo>_checked.tsv` ao lado do original (ou onde `--output` indicar). Ele traz custo real, distância de cada dia, locais por dia, a maior sequência de mesma categoria, as violações, o fitness recalculado, a origem da solução (`origem`, quando o arquivo tem a 7ª coluna) e o roteiro com os nomes dos locais (`roteiro`, ex.: `Casa 48 | Dia 1: A > B > C > D | Dia 2: ...`). Dá para abrir no pandas: `pd.read_csv(..., sep="\t")`.

- **Melhor roteiro de cada grupo, legível**, em `<arquivo>_roteiros.txt` (ou onde `--itineraries` indicar). Para cada grupo, mostra a solução viável de maior fitness: o hotel, cada dia com os locais na ordem (nome, tipo, nota, preço, ID e a distância a pé desde o local anterior), a volta ao hotel, a distância de cada dia contra o limite, o custo contra o orçamento e o fitness.

Os nomes vêm de `Input/RJ_instance/names.tsv`, gravado pelo exportador. O tipo mostrado nas atrações (ex.: "Praias", "Museus de arte") é a categoria original do `RJ.csv`, que o `main.py` reduz a "atração". Sem esse arquivo, os locais aparecem como `#ID`.

**Atenção aos dados:** 45 locais do `RJ.csv` têm "FECHADO" no nome (estabelecimentos fechados no TripAdvisor), a maioria restaurantes, e continuam disponíveis para todos os algoritmos, inclusive os dos autores. Na v3, 1393 das 3000 soluções incluem pelo menos um deles. Removê-los mudaria o problema (ver a regra de comparabilidade), então eles continuam disponíveis.

O arquivo de resultados só traz `budget` e `total_distance` em cada linha. Os demais parâmetros vêm de `Parameters/user.json`, que precisa ter um único valor para cada um. Se tiver mais de um, informe qual usar com `--amount-days`, `--daily-places` ou `--category`.

Se o fitness recalculado não bater com o registrado, o programa avisa. Isso indica que o arquivo foi gerado com outros dados. É o caso de `Results Analysis/Authors/`, que usou a matriz de distâncias dos autores.

#### Regras usadas (`model/feasibility.cpp`)

A verificação recalcula tudo a partir da rota final, sem usar nenhum valor guardado pela heurística:

| Restrição | Regra | Violação no TSV |
|---|---|---|
| Orçamento | preço do hotel × `amount_days` + preço dos locais ≤ `budget` | `orcamento(+excesso)` |
| Distância diária | hotel → locais do dia, na ordem → hotel ≤ `total_distance` | `distancia_diaN(+excesso em m)` |
| Locais por dia | locais do dia, sem o hotel, ≤ `amount_daily_places` | `locais_diaN(6>5)` |
| Categoria | no máximo `category` locais **seguidos** da mesma categoria no dia (regra do artigo) | `categoria_diaN(4>3)` |
| Repetição | nenhum local se repete na **viagem inteira** | `repetido(ID x vezes)` |
| Estrutura | começa num hotel, tem `amount_days` dias e nenhum outro hotel aparece como local | `estrutura(...)` |

Somas e limites são comparados com uma folga de 1e-6 (metros ou reais), para que erros de arredondamento não contem como violação.

A distância de cada dia é calculada por `day_route_distance`, a mesma função que a busca local usa desde a `GRASP_v1`. Por isso as duas sempre concordam.

O mesmo `check_feasibility` pode ser chamado de dentro de uma heurística nova, para garantir que ela só devolve soluções viáveis.

## Organização

Cada arquivo C++ corresponde a um arquivo Python, com as mesmas funções:

| C++ (`cpp/src/`) | Python (`src/`) |
|---|---|
| `apps/grasp.cpp` | `main.py` + `opt_call.py` (só o laço do GRASP) |
| `apps/greedy.cpp` | `main.py` + `opt_call.py` (só o laço do Greedy) |
| `apps/check_results.cpp` | — (verificador de viabilidade) |
| `model/feasibility.cpp` | — (regras de viabilidade) |
| `heuristics/grasp.cpp` | `heuristics/grasp.py` |
| `heuristics/random_greedy_procedure.cpp` | `heuristics/random_greedy_procedure.py` |
| `heuristics/simple_greedy.cpp` | `heuristics/simple_greedy.py` (Greedy, tradução literal) |
| `operations/constructive_operations.cpp` | `operations/constructive_operations.py` |
| `operations/aux.cpp` | `operations/aux.py` |
| `model/objective_function.cpp` | `model/objective_function.py` |
| `instance.cpp` | o dict `places_parameters` |
| `parameters.hpp` | os dicts `users_parameters`, `algorithm_parameters` e `route_parameters` |
| `numpy_compat.cpp` | as partes do numpy que o GRASP usa |

`numpy_compat` reproduz bit a bit:

- o gerador aleatório legado do numpy: `std::mt19937`, `randint` com máscara e rejeição, `random_sample`, e `choice` com e sem `p`;
- a soma "pairwise" de `np.sum`/`np.mean`;
- `setdiff1d` e `unique`;
- a formatação de `str(np.array)`.

O Makefile compila com `-ffp-contract=off`. Sem essa flag, o compilador poderia trocar `a*b + c` por uma instrução FMA, o que muda o arredondamento e deixaria os resultados diferentes dos do Python.

## Bugs do código original

Os que continuam no código estão marcados com `// ORIGINAL:`; os já corrigidos, com `// CORRIGIDO:`.

1. **[CORRIGIDO na GRASP_v1] `verify_local_search_changes` não atualizava direito o estado da rota.**
   - A conta que subtrai o trecho antigo é descartada: a nova distância é `daily_distance + d[before][new] + d[new][after]`, sem tirar o trecho antigo.
   - Logo depois, `daily_distance` é sobrescrita com o **custo** (`budget`).
   - `route_parameters["budget"]` nunca é atualizado. As trocas seguintes comparam o orçamento com o custo de antes da busca local.
2. **[CORRIGIDO na GRASP_v1] Os índices da busca local estavam deslocados.** O `index` vem de `enumerate(route[1:])`, mas é usado como posição em `route`:
   - `before = route[index-1]` é o local duas posições antes. Para `index = 0`, `route[-1]` é o último local do dia.
   - `after = route[index+1]` é o próprio local que está sendo trocado.
   - O ramo "último local do dia" nunca executa.

   **Correção.** `verify_local_search_changes` recebe a posição real do local. Ela recalcula a distância do dia com a troca, incluindo a volta ao hotel (`day_route_distance`), recusa a troca se passar de `total_distance` ou do orçamento, e atualiza `daily_distance` e `budget` quando aceita. O critério de escolha do candidato continua o mesmo.
3. **A volta ao hotel e os limites são verificados de forma inconsistente.**
   - `build_rcl` mede a volta ao hotel com `d[hotel][poi]`, e não com `d[poi][hotel]`. Hoje isso não faz diferença, porque a matriz gerada por `scripts/build_distance_matrix.py` é simétrica, mas faria com uma matriz assimétrica.
   - O filtro de preço é estrito (`<`), mas `verify_constructive_heuristic_constraints` usa `>`.
   - A distância diária guardada (`daily_distance`) não inclui a volta ao hotel.
4. **[MANTIDO de propósito, para comparar com os autores] Cada dia tem no máximo `amount_daily_places - 1` locais.** O dia é fechado quando `len(route) >= amount_daily_places`, e `route` inclui o hotel. Com `amount_daily_places = 5`, são 4 locais por dia.
5. **`select_alpha` usa o fitness da iteração atual, e não o melhor.** O alpha sorteado só vale para a próxima iteração.
6. **[PARCIALMENTE MELHORADO na GRASP_v2] A busca local só troca um local por outro da mesma categoria e com nota maior.** No original, ela escolhia o de menor `distância/nota` a partir do local atual, sorteando em caso de empate, e desistia se ele não coubesse. Na v2, os candidatos são ordenados pelo efeito no trajeto e testados até um caber. Continua sendo uma única passada e não acrescenta locais (não há movimento de inserção).
7. **`before_ls` é calculado e nunca usado.** Foi omitido no C++; ele não consome números aleatórios, então o resultado não muda.
8. **`generate_daily_array_route` só concatena as rotas.** O `np.array(route, route[0])` apenas copia o array. A saída é a concatenação das rotas de cada dia, com o hotel no início de cada dia.
