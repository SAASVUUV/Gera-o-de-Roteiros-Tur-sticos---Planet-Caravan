# GRASP em C++

Tradução **literal** do GRASP dos autores (`src/heuristics/grasp.py` e o que ele usa) para C++. A lógica é a mesma, inclusive os bugs (veja abaixo).

O C++ reproduz o gerador aleatório do numpy. Com a mesma semente, ele gera **exatamente as mesmas rotas e os mesmos fitness** que o Python. Com a semente 1, as 1800 linhas GRASP de `Results Analysis/My Results/results.txt` saem idênticas, e a execução é cerca de 15× mais rápida (no mesmo computador, 1,6 s no C++ contra 23,6 s no Python).

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

O comando gera `cpp/build/grasp`. Precisa de um g++ com C++20 e não tem outras dependências: o leitor de JSON, `third_party/nlohmann/json.hpp`, já vem junto.

### 3. Rodar

```
cpp/build/grasp
```

O programa lê `Parameters/user.json`, `algorithm.json` e `experiments.json`. Ele roda todas as combinações na mesma ordem do `opt_call.py`, executando só o GRASP. A cada execução, acrescenta uma linha a `Results Analysis/My Results/results_cpp.txt`, no mesmo formato de 6 colunas do Python.

Opções:

| Opção | Padrão | |
|---|---|---|
| `--output ARQUIVO` | `Results Analysis/My Results/results_cpp.txt` | arquivo de saída (só acrescenta) |
| `--instance PASTA` | `Input/RJ_instance` | dados exportados |
| `--seed N` | `1` | semente; 1 é a do `main.py` |

## Organização

Cada arquivo C++ corresponde a um arquivo Python, com as mesmas funções:

| C++ (`cpp/src/`) | Python (`src/`) |
|---|---|
| `main.cpp` | `main.py` + `opt_call.py` (só o laço do GRASP) |
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

## Comportamentos do código original mantidos na tradução

Estão marcados com `// ORIGINAL:` no código. São pontos de partida naturais para um GRASP melhorado:

1. **`verify_local_search_changes` não atualiza direito o estado da rota.**
   - A conta que subtrai o trecho antigo é descartada: a nova distância é `daily_distance + d[before][new] + d[new][after]`, sem tirar o trecho antigo.
   - Logo depois, `daily_distance` é sobrescrita com o **custo** (`budget`).
   - `route_parameters["budget"]` nunca é atualizado. As trocas seguintes comparam o orçamento com o custo de antes da busca local.
2. **Os índices da busca local estão deslocados.** O `index` vem de `enumerate(route[1:])`, mas é usado como posição em `route`:
   - `before = route[index-1]` é o local duas posições antes. Para `index = 0`, `route[-1]` é o último local do dia.
   - `after = route[index+1]` é o próprio local que está sendo trocado.
   - O ramo "último local do dia" nunca executa.
3. **A volta ao hotel e os limites são verificados de forma inconsistente.**
   - `build_rcl` mede a volta ao hotel com `d[hotel][poi]`, e não com `d[poi][hotel]`. Hoje isso não faz diferença, porque a matriz gerada por `scripts/build_distance_matrix.py` é simétrica, mas faria com uma matriz assimétrica.
   - O filtro de preço é estrito (`<`), mas `verify_constructive_heuristic_constraints` usa `>`.
   - A distância diária guardada (`daily_distance`) não inclui a volta ao hotel.
4. **Cada dia tem no máximo `amount_daily_places - 1` locais.** O dia é fechado quando `len(route) >= amount_daily_places`, e `route` inclui o hotel. Com `amount_daily_places = 5`, são 4 locais por dia.
5. **`select_alpha` usa o fitness da iteração atual, e não o melhor.** O alpha sorteado só vale para a próxima iteração.
6. **A busca local só troca um local por outro da mesma categoria e com nota maior.** Ela escolhe o de menor `distância/nota` a partir do local atual, sorteando em caso de empate, e faz uma única passada.
7. **`before_ls` é calculado e nunca usado.** Foi omitido no C++; ele não consome números aleatórios, então o resultado não muda.
8. **`generate_daily_array_route` só concatena as rotas.** O `np.array(route, route[0])` apenas copia o array. A saída é a concatenação das rotas de cada dia, com o hotel no início de cada dia.
