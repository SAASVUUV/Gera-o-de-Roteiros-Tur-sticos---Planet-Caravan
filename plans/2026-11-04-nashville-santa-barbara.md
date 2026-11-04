# Plano: Nashville e Santa Barbara como novas instâncias Yelp

**Objetivo:** criar as instâncias de Nashville (tamanho "normal") e Santa Barbara (pequena) com o mesmo pipeline de New Orleans, e comparar todas as versões nas quatro instâncias (Rio, NOLA, Nashville, Santa Barbara).

**Decisão do usuário (04/11/2026):** seguir com as duas cidades sugeridas.

## Termos do Yelp

Mesmas regras de NOLA: nada derivado do Yelp entra no git nem em PRs (CSV, matriz, resultados, contagens). Só código e testes com dados fictícios.

## 1. Código (pequeno, reaproveita o de NOLA)

- `scripts/yelp_to_csv.py`: `CITY_STATE` ganha `NASH: TN` e `SBA: CA`.
- `scripts/build_distance_matrix.py`: `CITY_OSM_QUERY` ganha `NASH: Nashville, Tennessee, USA` e `SBA: Santa Barbara, California, USA`.
- Recorte pelo polígono do município no OSM, como em NOLA (o campo `city` do Yelp mistura cidades vizinhas):
  - Nashville: governo consolidado Nashville-Davidson (condado inteiro), mesmo critério de NOLA (Orleans Parish) e Rio (município).
  - Santa Barbara: município.
- `.gitignore`: `Input/NASH*`, `Input/SBA*` e as pastas de resultado.
- `Parameters/NASH/user.json` e `Parameters/SBA/user.json`: cópias do perfil local de NOLA (3/6 km).
- `run_all_versions.sh`: confirmar que acha o perfil local por cidade (hoje é `Parameters/<CITY>/`).

## 2. Dados (locais, fora do git)

- Conversão com alvo de ~2.100 locais pelo mínimo de avaliações, como em NOLA.
- Estimativa de faixa de preço por categoria, como em NOLA.
- Rede a pé: extracts da Geofabrik `tennessee-latest.osm.pbf` e `california/socal-latest.osm.pbf`.

## 3. Execução e comparação

- `export_instance.py` para cada cidade; todas as versões nos perfis rio e local; `check_results` em tudo.
- `compare_versions.py` com as quatro instâncias num relatório só (`Results Analysis/My Results/comparison_all.md`, fora do git).

## Testes

- Conversor: classificação e filtro de estado para TN/CA com dados fictícios.
- Matriz: consulta OSM conhecida para as novas cidades.
- Pipeline: os testes existentes (Rio byte-idêntico, v4 seed 1) continuam verdes.
- Validação dos dados: 100% de viabilidade das versões corrigidas nas novas instâncias; Python = literal (bit a bit).

## Riscos / fora do escopo

- Nashville-Davidson é grande (inclui área rural): muitos pares de locais longe. Se a rede a pé tiver componentes desconexos, aplica-se a mesma regra de NOLA (manter os locais longe da rua).
- O Greedy dos autores depende dos hotéis 5 estrelas de cada cidade; seu comportamento não muda.
- Não mexe na lógica de nenhuma heurística.

## Critério de pronto

Duas instâncias geradas, todas as versões rodadas e verificadas, relatório comparativo com as quatro instâncias, suíte de testes verde, commits na branch, PR só quando o usuário pedir.
