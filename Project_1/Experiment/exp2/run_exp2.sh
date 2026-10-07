#!/usr/bin/env bash
# Runs the whole second experiment: build and collision tests.
#
# Usage (from anywhere):
#    ./Experiment/exp2/run_exp2.sh [pairs]
#
# pairs defaults to 100000 per input length.

set -euo pipefail

EXP_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$EXP_DIR/../.." && pwd)"

# The hash reads image.png from the working directory, so run everything from Project_1
cd "$PROJECT_DIR"

step() { printf '\n==> %s\n' "$1"; }

step "Building executables"
make build

step "Running collision tests (collision_test)"
"$EXP_DIR/collision_test" "$EXP_DIR" "${1:-100000}"

step "Done"
echo "Summary:              $EXP_DIR/exp2_results.txt"
echo "Random input details: $EXP_DIR/out/pair_collisions_<length>.txt, $EXP_DIR/out/set_collisions_<length>.txt"
echo "Structured inputs:    $EXP_DIR/out/structured_collisions.txt"
