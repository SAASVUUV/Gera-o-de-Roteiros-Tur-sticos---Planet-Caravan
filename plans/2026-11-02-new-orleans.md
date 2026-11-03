# Plano: New Orleans como segunda instância (antes: Tiradentes)

**Objetivo:** criar a instância de New Orleans a partir do Yelp Open Dataset, gerar a matriz de distâncias a pé pelo OSM e comparar todas as versões (Python dos autores, Greedy, tradução literal, GRASP v1–v4) em New Orleans e contra o Rio.

**Histórico das decisões (02/11/2026):**
- Tiradentes foi descartada: nenhuma fonte com nota, preço e nº de avaliações permite guardar os dados. Google Places e a API do TripAdvisor só deixam guardar o ID do local, e raspar o TripAdvisor viola os termos. Não há dataset aberto de cidades brasileiras.
- Fonte escolhida: Yelp Open Dataset, cidade New Orleans.
- Coletar e guardar o número de avaliações (uso futuro).
- Parâmetros: os mesmos do Rio, mais um perfil local em arquivo separado.
- Comparação com todas as versões.

## Termos do Yelp (o que muda no projeto)

Trechos lidos do Dataset User Agreement:
- Só uso acadêmico ("academic project as part of an ongoing course of study").
- Proibido compartilhar ou exibir os dados a terceiros (4.A, 4.D), e também divulgar resumos ou métricas, exceto o necessário para fins acadêmicos, como artigos (4.E).
- Antes de apresentação ou publicação pública de resultados, enviar ao Yelp para aprovação (responde em 5 dias úteis).

Consequências:
- O usuário baixa o dataset pessoalmente em yelp.com/dataset, aceitando os termos. Não usar espelhos.
- Tudo que deriva do Yelp fica fora do git, porque o repo é público no GitHub: `Input/NOLA*`, `Input/yelp/`, `Results Analysis/My Results/NOLA/`. O código (scripts e testes com dados fictícios) pode ser commitado.

## 1. Conversor: `scripts/yelp_to_csv.py`

- Lê `yelp_academic_dataset_business.json` (JSON por linha, sem carregar tudo de uma vez).
- Filtros:
  - município de New Orleans (Orleans Parish), pelo polígono do OSM salvo em `Input/NOLA_boundary.geojson`, e não pelo campo `city`, que mistura Metairie, Kenner etc.;
  - `is_open == 1` (a confirmar com o usuário);
  - categorias, por mapeamento explícito e testado das categorias do Yelp:
    - hotel: Hotels, Bed & Breakfast, Hostels;
    - restaurant: Restaurants, Food;
    - atração: Museums, Landmarks & Historical Buildings, Parks, Art Galleries, Tours, Jazz & Blues, Music Venues etc.;
  - nº mínimo de avaliações, para chegar a ~2.100 locais, como no Rio (a confirmar).
- Saída `Input/NOLA.csv` no formato do `RJ.csv`, mais a coluna `review count`:
  - `local url` sintética no padrão TripAdvisor (`/Hotel_Review-<business_id>` etc.), para o `main.py` pôr os hotéis primeiro. Com validação de que nenhum id contém "html".
  - `rating` = stars (já de 0,5 em 0,5).
  - `min/max price`: `RestaurantsPriceRange2` ($–$$$$) convertido pelos quartis do `RJ.csv` por categoria, para que os budgets do Rio façam sentido. Locais sem faixa recebem a mediana da categoria.
  - `category`: hotels, restaurant ou a categoria principal da atração (exibição).
  - `time duration` = 1,0 (não usado: código morto).

## 2. Matriz: `scripts/build_distance_matrix.py`

- `load_boundary` passa a aceitar um GeoJSON já existente para cidades fora do IBGE. O GeoJSON do OSM é baixado uma vez com osmnx.
- Rede a pé via Overpass (área ~300 km²; o script já divide em pedaços e tenta servidores alternativos) ou pelo extract `louisiana-latest.osm.pbf` da Geofabrik.
- Instalar `osmnx` e `osmium` no `.venv`.

## 3. Execução

- `main.py`: cidade via `--city` (padrão RJ, sem mudar o comportamento atual).
- `export_instance.py --city NOLA` gera `Input/NOLA_instance/`.
- Perfil local em `Parameters/NOLA/user.json`, com valores propostos ao usuário depois de ver os dados.
- `scripts/run_all_versions.sh`:
  - compila cada versão a partir do seu commit (worktree temporário): literal `70eb95a`, v1 `c09da9d`, v2 `32fdc39`, v3 `aede8fa`, v4 `main` e Greedy;
  - roda com `--instance Input/NOLA_instance` nos dois perfis; os parâmetros vêm do diretório de trabalho, então o perfil local roda numa pasta temporária com seu próprio `Parameters/`;
  - inclui o Python dos autores;
  - roda `check_results` em todas as saídas.

## 4. Comparação: `scripts/compare_versions.py`

- Tabela por grupo: fitness médio, % viável, tempo, `do_grasp`.
- Seções: NOLA/perfil Rio, NOLA/perfil local e Rio × NOLA.
- Saída em `Results Analysis/My Results/NOLA/comparison.md`, local e gitignored.

## Testes (pytest, sem rede, dados fictícios)

- Conversor: mapeamento de categorias, filtro de limite, `is_open`, mínimo de avaliações, conversão de preço, local sem preço, URL e validação de "html", e linhas com `attributes`/`categories` nulos.
- Integração: CSV fictício → `prepare_data` (hotéis primeiro, IDs contíguos) → export → `grasp` → `check_results` 100% viável.
- Matriz: `load_boundary` com GeoJSON local e `--method haversine` em fixture.
- Comparação: tabela gerada a partir de resultados de exemplo.
- Rio intocado: `RJ_instance` re-exportada idêntica (hash) e v4 `--seed 1` reproduzindo as primeiras linhas de `results_grasp_v4.txt`.

## Riscos e fora do escopo

- Hotel nota 5 dentro do budget: hotéis no Yelp raramente têm 5 estrelas. Se não houver, o Greedy falha (`GreedyHotelNotFound`) e o v3/v4 roda sem semente. Isso será medido e reportado.
- Teto de fitness 45 (regra dos autores mantida).
- Yelp ≠ TripAdvisor: Rio × NOLA compara instâncias, não fontes iguais.
- Fora do escopo: mudar regras do problema e usar o nº de avaliações nas heurísticas.

## Critério de pronto

Os testes passam; os dados de NOLA estão gerados e fora do git; todas as versões rodaram nos dois perfis com 100% de viabilidade nas versões corrigidas; `comparison.md` está gerado; o CLAUDE.md foi atualizado.
