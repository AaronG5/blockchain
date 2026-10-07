#!/usr/bin/env bash
# Runs the third experiment: build and brute-force preimage search.
#
# Usage (from anywhere):
#    ./Experiment/exp3/run_exp3.sh

set -euo pipefail

EXP_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$EXP_DIR/../.." && pwd)"

# The hash reads image.png from the working directory, so run everything from Project_1
cd "$PROJECT_DIR"

step() { printf '\n==> %s\n' "$1"; }

step "Building executables"
make build

step "Running brute-force search (brute_force)"
"$EXP_DIR/brute_force" "$EXP_DIR"

step "Done"
echo "Results: $EXP_DIR/exp3_results.txt"
