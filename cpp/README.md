# GRASP em C++

GRASP dos autores (`src/heuristics/grasp.py` e o que ele usa) traduzido para C++, cerca de 15× mais rápido que o Python. Cada correção gera uma **versão**, e cada versão grava seus resultados com nome próprio, para não misturar nem sobrescrever os anteriores:

| Versão | O que muda | Onde está | Resultados |
|---|---|---|---|
| tradução literal | nada: idêntica ao Python, inclusive os bugs | commit `70eb95a` | `results_cpp.txt` (coluna `GRASP`) |
| `GRASP_v1` (atual) | busca local corrigida: nunca viola orçamento nem distância | código atual | `results_grasp_v1.txt` (coluna `GRASP_v1`) |

O C++ reproduz o gerador aleatório do numpy. A tradução literal, com a semente 1, gera exatamente as mesmas 1800 linhas GRASP de `Results Analysis/My Results/results.txt`. A partir da `GRASP_v1`, as rotas mudam porque a busca local aceita e recusa trocas diferentes. Para voltar à versão literal: `git show 70eb95a:cpp/src/heuristics/grasp.cpp`.

### Resultado da GRASP_v1 (parâmetros padrão, semente 1)

- **100% das soluções viáveis.** O GRASP dos autores tinha 75%: 458 de 1800 violavam distância ou orçamento.
- **Fitness médio maior que o dos autores em todos os 18 grupos**, cerca de +0,6 em média quando comparado só com as soluções viáveis deles. A verificação antiga somava o trecho novo sem descontar o antigo, o que superestimava a distância e recusava trocas válidas. A nova faz a conta exata e aceita essas trocas.
- **Ainda perde para o Greedy** com orçamento ≥ 1000 e 20 km: 44,0–44,2 contra 45,0, com 100 iterações.

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

- `grasp`: o GRASP (versão atual, `GRASP_v1`).
- `check_results`: o verificador de viabilidade.

Precisa de um g++ com C++20 e não tem outras dependências: o leitor de JSON, `third_party/nlohmann/json.hpp`, já vem junto.

### 3. Rodar

```
cpp/build/grasp
```

O programa lê `Parameters/user.json`, `algorithm.json` e `experiments.json`. Ele roda todas as combinações na mesma ordem do `opt_call.py`, executando só o GRASP. A cada execução, acrescenta uma linha a `Results Analysis/My Results/results_grasp_v1.txt`, no mesmo formato de 6 colunas do Python, com `GRASP_v1` na coluna do algoritmo. Antes de cada combinação de parâmetros do algoritmo, escreve um cabeçalho como `# GRASP_v1 max_iterations=10`, que o `check_results` usa para separar os grupos. Essa linha não existe na saída do Python.

Uma versão nova deve trocar `GRASP_NAME` em `heuristics/grasp.hpp` e o arquivo padrão em `apps/grasp.cpp`. Assim os resultados de cada versão ficam separados.

Opções:

| Opção | Padrão | |
|---|---|---|
| `--output ARQUIVO` | `Results Analysis/My Results/results_grasp_v1.txt` | arquivo de saída (só acrescenta) |
| `--instance PASTA` | `Input/RJ_instance` | dados exportados |
| `--seed N` | `1` | semente; 1 é a do `main.py` |

### 4. Verificar a viabilidade dos resultados

```
cpp/build/check_results "Results Analysis/My Results/results.txt"
```

O programa aceita arquivos do Python (GRASP e Greedy) e do C++. Ele faz duas coisas:

- **Resumo por grupo** (seção, budget, distância): % de soluções viáveis, fitness médio de todas e só das viáveis, melhor fitness viável e quantas soluções violam cada restrição. Ele aparece no terminal e também é salvo em `<arquivo>_summary.txt` ao lado do original (ou onde `--summary` indicar), com data e uma legenda das colunas.
- **TSV com uma linha por solução**, em `<arquivo>_checked.tsv` ao lado do original (ou onde `--output` indicar). Ele traz custo real, distância de cada dia, locais por dia, a maior sequência de mesma categoria, as violações e o fitness recalculado. Dá para abrir no pandas: `pd.read_csv(..., sep="\t")`.

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

A distância de cada dia é calculada por `day_route_distance`, a mesma função que a busca local da `GRASP_v1` usa. Por isso as duas sempre concordam.

O mesmo `check_feasibility` pode ser chamado de dentro de uma heurística nova, para garantir que ela só devolve soluções viáveis.

## Organização

Cada arquivo C++ corresponde a um arquivo Python, com as mesmas funções:

| C++ (`cpp/src/`) | Python (`src/`) |
|---|---|
| `apps/grasp.cpp` | `main.py` + `opt_call.py` (só o laço do GRASP) |
| `apps/check_results.cpp` | — (verificador de viabilidade) |
| `model/feasibility.cpp` | — (regras de viabilidade) |
| `heuristics/grasp.cpp` | `heuristics/grasp.py` |
| `heuristics/random_greedy_procedure.cpp` | `heuristics/random_greedy_procedure.py` |
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
6. **A busca local só troca um local por outro da mesma categoria e com nota maior.** Ela escolhe o de menor `distância/nota` a partir do local atual, sorteando em caso de empate, e faz uma única passada.
7. **`before_ls` é calculado e nunca usado.** Foi omitido no C++; ele não consome números aleatórios, então o resultado não muda.
8. **`generate_daily_array_route` só concatena as rotas.** O `np.array(route, route[0])` apenas copia o array. A saída é a concatenação das rotas de cada dia, com o hotel no início de cada dia.
