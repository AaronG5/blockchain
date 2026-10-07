#!/usr/bin/env bash
# Runs the whole second experiment: build, collision tests, avalanche test and histogram.
#
# Usage (from anywhere):
#    ./Experiment/exp2/run_exp2.sh [pairs]
#
# pairs defaults to 100000: per input length for the collision tests,
# in total (split evenly between the lengths) for the avalanche test.
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

step "Running collision tests (collision_test)"
"$EXP_DIR/collision_test" "$EXP_DIR" "${1:-100000}"

step "Running avalanche test (avalanche_test)"
"$EXP_DIR/avalanche_test" "$EXP_DIR" "${1:-100000}"

step "Drawing histogram (avalanche_graph.py)"
if "$PYTHON" -c "import matplotlib" 2>/dev/null; then
   # Agg backend: save the PNG without opening a window that blocks the script
   MPLBACKEND=Agg "$PYTHON" -W "ignore:FigureCanvasAgg is non-interactive" "$EXP_DIR/avalanche_graph.py"
else
   echo "matplotlib not found for $PYTHON, skipping histogram. Set PYTHON=/path/to/python to use another interpreter."
fi

step "Done"
echo "Summary:              $EXP_DIR/exp2_results.txt"
echo "Random input details: $EXP_DIR/out/pair_collisions_<length>.txt, $EXP_DIR/out/set_collisions_<length>.txt"
echo "Structured inputs:    $EXP_DIR/out/structured_collisions.txt"
echo "Avalanche summary:    $EXP_DIR/avalanche_results.txt"
echo "Avalanche data:       $EXP_DIR/out/avalanche.csv"
echo "Avalanche histogram:  $EXP_DIR/out/avalanche_hist.png"
