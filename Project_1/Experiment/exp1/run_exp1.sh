#!/usr/bin/env bash
# Runs the whole first experiment: build, correctness tests, benchmark and graph.
#
# Usage (from anywhere):
#    ./Experiment/exp1/run_exp1.sh
#
# Python is picked in this order: $PYTHON, an active virtualenv,
# .venv in the project directory or its parent, then python3.

set -euo pipefail

EXP_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$EXP_DIR/../.." && pwd)"

# The hash reads image.png from the working directory, so run everything from Project_1
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

step "Generating test files (gen_files.py)"
"$PYTHON" "$EXP_DIR/gen_files.py"

step "Testing generated files (test_files.py)"
"$PYTHON" "$EXP_DIR/test_files.py"

step "Generating constitution snippets (gen_konst.py)"
"$PYTHON" "$EXP_DIR/gen_konst.py"

step "Running benchmark (bench_konst)"
"$EXP_DIR/bench_konst"

step "Drawing graph (graph.py)"
if "$PYTHON" -c "import matplotlib" 2>/dev/null; then
   # Agg backend: save the PNG without opening a window that blocks the script
   MPLBACKEND=Agg "$PYTHON" -W "ignore:FigureCanvasAgg is non-interactive" "$EXP_DIR/graph.py" > /dev/null
else
   echo "matplotlib not found for $PYTHON, skipping graph. Set PYTHON=/path/to/python to use another interpreter."
fi

step "Done"
echo "Correctness results: $EXP_DIR/exp1_results.txt"
echo "Benchmark results:   $EXP_DIR/out_konst/bench_konst.csv"
echo "Graph:               $EXP_DIR/out_konst/bench_graph.png"
