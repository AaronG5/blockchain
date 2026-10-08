#!/usr/bin/env bash
# Compares the original hash (hash.hpp) with the improved one (hash_v2.hpp): build, comparison and graphs.
#
# Usage (from anywhere):
#    ./Experiment/exp4/run_exp4.sh [pairs]
#
# pairs defaults to 100000, as in experiment 2. The correctness and speed tests use the files
# from Experiment/exp1/gen and Experiment/exp1/gen_konst (run_exp1.sh generates them).
#
# Python is picked in this order: $PYTHON, an active virtualenv,
# .venv in the project directory or its parent, then python3.

set -euo pipefail

EXP_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$EXP_DIR/../.." && pwd)"

# Both hashes read image.png from the working directory, so run everything from Project_1
cd "$PROJECT_DIR"

if [[ -z "${PYTHON:-}" ]]; then
   if [[ -n "${VIRTUAL_ENV:-}" ]]; then
      PYTHON="$VIRTUAL_ENV/bin/python"
   elif [[ -x "$PROJECT_DIR/.venv/bin/python" ]]; then
      PYTHON="$PROJECT_DIR/.venv/bin/python"
   elif [[ -x "$PROJECT_DIR/../.venv/bin/python" ]]; then
      PYTHON="$PROJECT_DIR/../.venv/bin/python"
   else
      PYTHON="python3"
   fi
fi

step() { printf '\n==> %s\n' "$1"; }

step "Building executables"
make build

step "Comparing hashes (compare)"
"$EXP_DIR/compare" "$EXP_DIR" "${1:-100000}"

step "Drawing graphs (compare_graph.py)"
if "$PYTHON" -c "import matplotlib" 2>/dev/null; then
   # Agg backend: save the PNGs without opening windows that block the script
   MPLBACKEND=Agg "$PYTHON" -W "ignore:FigureCanvasAgg is non-interactive" "$EXP_DIR/compare_graph.py"
else
   echo "matplotlib not found for $PYTHON, skipping graphs. Set PYTHON=/path/to/python to use another interpreter."
fi

step "Done"
echo "Summary:             $EXP_DIR/exp4_results.txt"
echo "Data:                $EXP_DIR/out/avalanche.csv, $EXP_DIR/out/bench.csv"
echo "Avalanche histogram: $EXP_DIR/out/avalanche_compare.png"
echo "Speed graph:         $EXP_DIR/out/bench_compare.png"
