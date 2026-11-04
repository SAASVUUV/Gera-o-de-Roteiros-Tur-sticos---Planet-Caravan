#!/usr/bin/env bash
# Roda todas as versões (tradução literal, GRASP v1–v4, Greedy e, opcionalmente,
# o Python dos autores) numa instância, com um perfil de parâmetros, e confere
# cada saída com o check_results.
#
# Uso, a partir da raiz do repositório:
#   scripts/run_all_versions.sh --city NOLA --profile rio   [--with-python] [--only "v1 v4"]
#   scripts/run_all_versions.sh --city NOLA --profile local
#
# Perfis:
#   rio    Parameters/*.json (os mesmos do Rio)
#   local  Parameters/<CITY>/*.json; o que faltar lá vem de Parameters/
#
# Cada versão é compilada a partir do seu próprio commit, num worktree
# temporário, então nada do código atual é alterado. As saídas vão para
# "Results Analysis/My Results/<CITY>/<perfil>/results_<versão>.txt"; a do
# Python dos autores, para "Results Analysis/Authors/<CITY>/<perfil>/". Um
# arquivo que já existe não é sobrescrito: a versão é pulada (apague-o para
# rodar de novo).
set -euo pipefail

declare -A COMMIT=(
  [literal]=70eb95a  # tradução literal (bit-exata com o Python)
  [v1]=c09da9d
  [v2]=32fdc39
  [v3]=aede8fa
  [v4]=a0c7b78
)
# O Greedy (lógica nunca muda) e o check_results vêm do código atual (HEAD).
VERSIONS="literal v1 v2 v3 v4 greedy"

CITY=""; PROFILE=""; WITH_PYTHON=0; ONLY=""
while [ $# -gt 0 ]; do
  case "$1" in
    --city) CITY="$2"; shift 2 ;;
    --profile) PROFILE="$2"; shift 2 ;;
    --with-python) WITH_PYTHON=1; shift ;;
    --only) ONLY="$2"; shift 2 ;;
    -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
    *) echo "Opção desconhecida: $1" >&2; exit 2 ;;
  esac
done
[ -n "$CITY" ] && [ -n "$PROFILE" ] || { echo "Informe --city e --profile" >&2; exit 2; }
[ -n "$ONLY" ] && VERSIONS="$ONLY"

ROOT="$(git rev-parse --show-toplevel)"
INSTANCE="$ROOT/Input/${CITY}_instance"
OUT_DIR="$ROOT/Results Analysis/My Results/$CITY/$PROFILE"
AUTHORS_DIR="$ROOT/Results Analysis/Authors/$CITY/$PROFILE"
[ -f "$INSTANCE/distance_matrix.bin" ] || {
  echo "Instância $INSTANCE não existe: rode scripts/export_instance.py --city $CITY" >&2; exit 1; }

PYTHON="${PYTHON:-$ROOT/.venv/bin/python}"
[ -x "$PYTHON" ] || PYTHON=python

WORK="$(mktemp -d)"
cleanup() {
  for tree in "$WORK"/tree-*; do
    [ -d "$tree" ] && git -C "$ROOT" worktree remove --force "$tree" >/dev/null 2>&1 || true
  done
  rm -rf "$WORK"
}
trap cleanup EXIT

# Pasta de execução: os programas leem Parameters/*.json do diretório atual.
RUN="$WORK/run"
mkdir -p "$RUN/Parameters"
cp "$ROOT"/Parameters/*.json "$RUN/Parameters/"
if [ "$PROFILE" = "local" ]; then
  [ -d "$ROOT/Parameters/$CITY" ] || { echo "Falta Parameters/$CITY/" >&2; exit 1; }
  cp "$ROOT/Parameters/$CITY"/*.json "$RUN/Parameters/"
elif [ "$PROFILE" != "rio" ]; then
  echo "Perfil desconhecido: $PROFILE (use rio ou local)" >&2; exit 2
fi
ln -s "$ROOT/Input" "$RUN/Input"
# record_parameters <pasta>: registra com quais parâmetros os resultados foram
# feitos e não deixa misturar na mesma pasta resultados de parâmetros diferentes.
record_parameters() {
  mkdir -p "$1"
  if [ -d "$1/Parameters" ]; then
    if ! diff -r -q "$RUN/Parameters" "$1/Parameters" >/dev/null; then
      echo "Os parâmetros mudaram desde os resultados que já estão em $1:" >&2
      diff -r "$1/Parameters" "$RUN/Parameters" >&2 || true
      echo "Use outra pasta (mova a atual) ou restaure os parâmetros." >&2
      exit 1
    fi
  else
    cp -r "$RUN/Parameters" "$1/"
  fi
}
record_parameters "$OUT_DIR"
if [ "$WITH_PYTHON" = 1 ]; then record_parameters "$AUTHORS_DIR"; fi
echo "Parâmetros ($PROFILE): $(tr -d ' \n' < "$RUN/Parameters/user.json")"

# build <commit>: compila a versão num worktree e põe a pasta cpp/build em
# $BUILT. Roda no shell atual (não em $(...)), para que o set -e pare o script
# se o checkout ou a compilação falharem; .built marca uma compilação completa.
build() {
  local tree="$WORK/tree-$1"
  if [ ! -e "$tree/.built" ]; then
    rm -rf "$tree"; git -C "$ROOT" worktree prune
    git -C "$ROOT" worktree add --quiet --detach "$tree" "$1"
    make -s -C "$tree/cpp" >/dev/null
    touch "$tree/.built"
  fi
  BUILT="$tree/cpp/build"
}

build HEAD
CHECK="$BUILT/check_results"
GREEDY="$BUILT/greedy"

for version in $VERSIONS; do
  out="$OUT_DIR/results_$version.txt"
  if [ -e "$out" ]; then
    echo "== $version: $out já existe, pulando"; continue
  fi
  echo "== $version"
  # Os programas acrescentam ao arquivo de saída: escreve num .partial e só o
  # renomeia no fim, para que uma execução interrompida não pareça completa.
  partial="$out.partial"
  rm -f "$partial"
  start=$SECONDS
  case "$version" in
    greedy) (cd "$RUN" && "$GREEDY" --instance "$INSTANCE" --output "$partial") ;;
    literal|v1|v2|v3|v4)
      build "${COMMIT[$version]}"
      (cd "$RUN" && "$BUILT/grasp" --instance "$INSTANCE" --output "$partial" --seed 1) ;;
    *) echo "Versão desconhecida: $version" >&2; exit 2 ;;
  esac
  mv "$partial" "$out"
  echo "   $((SECONDS - start)) s"
  (cd "$RUN" && "$CHECK" "$out" --instance "$INSTANCE" > /dev/null)
  echo "   check_results: ${out%.txt}_summary.txt"
done

if [ "$WITH_PYTHON" = 1 ]; then
  out="$AUTHORS_DIR/results_python.txt"
  if [ -e "$out" ]; then
    echo "== python: $out já existe, pulando"
  else
    echo "== python (autores)"
    start=$SECONDS
    partial="$out.partial"
    rm -f "$partial"
    (cd "$RUN" && "$PYTHON" "$ROOT/src/main.py" --city "$CITY" --output "$partial")
    mv "$partial" "$out"
    echo "   $((SECONDS - start)) s"
    (cd "$RUN" && "$CHECK" "$out" --instance "$INSTANCE" > /dev/null)
    echo "   check_results: ${out%.txt}_summary.txt"
  fi
fi
