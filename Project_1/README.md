# Image-based hash function

## Description

This is a custom hash function that uses raw pixel data from an image to hash its input.

## How it works

A PNG image named [`image.png`](image.png) must be present in the directory the program is run from.

The hash function is implemented in [`hash.hpp`](hash.hpp).

### Input

Text or a filename (the file must have the `.txt` extension). Input is expected to contain only ASCII characters. Text typed in by hand has its newline character removed.

### Input limitations

Only the first 128 bytes of the input are used directly. Bytes after that only affect the hash through the offset from step 3 (`byte sum + 256 * input length`).

### Step-by-step process

1. The PNG file is read into memory as raw pixel data.
2. The input is converted into an `int` array (`char[] -> int[]`). If a salt is given (optional: 16 bytes written as 32 hex characters), it is decoded and its 16 bytes are appended to the array (`input || salt`).
3. The array is summed into a single `int` value, and the input length multiplied by 256 is added to it. The result is used to offset the X and Y coordinates of the pixel lookup in step 4.
4. The array is processed in groups of four elements, with each group split into two pairs (elements past the end of the array count as 0). Each pair `(a, b)` is combined as `a * 256 + b`. The offset is added to both results, along with the position of the output byte being produced (0–31) multiplied by 7 for X and by 13 for Y, so the same elements point to a different pixel at each output position instead of repeating one pixel. The results are used as the X and Y coordinates of a pixel in the image. If a coordinate exceeds the image resolution, it is wrapped using the modulo operation, and the quotient of the original coordinate divided by the image resolution is added to the result.
5. The pixel's RGB values are combined with bitwise XOR: `R ^ G ^ B`.
6. The result is converted to hexadecimal and appended to the hash string.
7. If the hash is shorter than 64 hex characters, the pixel lookup is run again, with the `int` array shifted by the current iteration count.
8. If the hash is longer than 64 hex characters, it is truncated to 64 characters.
9. The hash is returned.

### Running the hash function (macOS or Linux)

From the `Project_1` directory:

```bash
make build
./cli_hasher
```

The command-line program is [`cli.cpp`](cli.cpp).

## 1 Eksperimentinis tyrimas: teisingumas ir sparta

All experiment files are in [`Experiment/exp1/`](Experiment/exp1).

To run the whole experiment (steps 1–5 below), use [`run_exp1.sh`](Experiment/exp1/run_exp1.sh):

```bash
./Experiment/exp1/run_exp1.sh
```

The script builds the executables, runs each step in order and prints where the results were saved. It stops if any step fails.

1. [`gen_files.py`](Experiment/exp1/gen_files.py) generates the following test files in `gen/`:
   - An empty file and 2 files containing a single character.
   - 3 files with random content, each larger than 1000 bytes.
   - Copies of those 3 random files with a single byte changed.
   - Several files with repeating characters, files with shuffled content, and files with and without spaces and newlines.
   - A file with non-ASCII characters.
2. [`test_files.py`](Experiment/exp1/test_files.py) tests the generated files. For every file, the script checks that the hash has the correct length and that hashing the content as text gives the same hash as hashing the file. The results are written to [`exp1_results.txt`](Experiment/exp1/exp1_results.txt).
3. [`gen_konst.py`](Experiment/exp1/gen_konst.py) generates `.txt` snippets of [`konstitucija.txt`](Experiment/exp1/konstitucija.txt) in `gen_konst/`.
4. [`bench_konst.cpp`](Experiment/exp1/bench_konst.cpp) benchmarks the hash function on each constitution snippet. An untimed warm-up run loads the image into the cache, then each snippet is hashed and timed 10 times. The results are written to `out_konst/bench_konst.csv` and show the filename, file size in bytes, and the average, minimum and maximum hashing time. To run it, use `make build` and then `./Experiment/exp1/bench_konst` from the `Project_1` directory.
5. [`graph.py`](Experiment/exp1/graph.py) generates a graph from `bench_konst.csv` to visualize the hash function's speed and saves it as [`out_konst/bench_graph.png`](Experiment/exp1/out_konst/bench_graph.png).

![Benchmark Graph](Experiment/exp1/out_konst/bench_graph.png)

## 2 Eksperimentinis tyrimas: kolizijos ir lavinos efektas

All experiment files are in [`Experiment/exp2/`](Experiment/exp2).

To run the whole experiment (steps 1–9 below), use [`run_exp2.sh`](Experiment/exp2/run_exp2.sh):

```bash
./Experiment/exp2/run_exp2.sh
```

Steps 1–6 are done by [`collision_test.cpp`](Experiment/exp2/collision_test.cpp), steps 7–8 by [`avalanche_test.cpp`](Experiment/exp2/avalanche_test.cpp) and step 9 by [`avalanche_graph.py`](Experiment/exp2/avalanche_graph.py).

1. 100,000 pairs of random ASCII strings are generated for each length: 10, 100, 500 and 1000 (200,000 strings per length). The two strings in a pair always differ. Characters are taken from the 95 printable ASCII characters (`0x20`–`0x7E`, one byte each), using `std::mt19937_64` with seed `20261007 + length`.
2. The two hashes of every pair are compared, and the number of pairs with the same hash is counted for each length.
3. All 200,000 strings of one length are also checked against each other. Identical strings are counted once, and the distinct strings are grouped by hash in a hash map. Any group with more than one string is a collision. This gives the same result as comparing every string with every other one (about 2·10¹⁰ comparisons), but in a single pass.
4. Small structured input sets (permutations, rotations, byte swaps, repeated patterns, short strings, one-byte changes, trailing zero bytes, ...) look for weaknesses that random strings are unlikely to hit.
5. Results are written to [`exp2_results.txt`](Experiment/exp2/exp2_results.txt), with collision examples in `out/`. Random strings gave no collisions. The structured tests found collisions when bytes after position 128 are reordered (`tail_after_128`) and when single bytes are changed to the next character (`single_byte_change`).
6. For an ideal 256-bit hash, each pair collides with probability 2⁻²⁵⁶, so the expected number of collisions is about 8.6·10⁻⁷³ for the pair check and 1.7·10⁻⁶⁷ for the full set (m(m−1)/2 ≈ 2·10¹⁰ pairs).
7. 100,000 pairs are generated, 25,000 per length, with the same generator and seed as step 1. The two strings in a pair differ in exactly one randomly chosen character, and their hashes are compared by the percentage of differing bits (hex decoded to bytes first) and of differing hex digits.
8. The minimum, maximum and average of both measures, for each length and for all pairs together, are written to [`avalanche_results.txt`](Experiment/exp2/avalanche_results.txt). The values for each pair are saved in [`out/avalanche.csv`](Experiment/exp2/out/avalanche.csv). On average 45.63% of the bits and 88.66% of the hex digits change, a little below the reference values of 50% and 93.75% for independent, uniformly distributed outputs.
9. [`avalanche_graph.py`](Experiment/exp2/avalanche_graph.py) draws a histogram of the bit difference percentages from [`avalanche.csv`](Experiment/exp2/out/avalanche.csv) and saves it as [`out/avalanche_hist.png`](Experiment/exp2/out/avalanche_hist.png).

![Bit Difference Histogram](Experiment/exp2/out/avalanche_hist.png)

## 3 Eksperimentinis tyrimas: spėjimas ir išvados

All experiment files are in [`Experiment/exp3/`](Experiment/exp3).

To run the whole experiment (steps 1–4 below), use [`run_exp3.sh`](Experiment/exp3/run_exp3.sh). The results are written to [`exp3_results.txt`](Experiment/exp3/exp3_results.txt).

```bash
./Experiment/exp3/run_exp3.sh
```

1. The candidate set is all four-digit strings `0000`–`9999`. The target input `7164` is chosen randomly (`std::mt19937_64`, seed `20261007`). The attack ([`brute_force.cpp`](Experiment/exp3/brute_force.cpp)) receives only the candidate set and the target hash, and tries every candidate to find all matches.
2. **No salt:** the target was found after 7165 attempts (~22 ms), and it was the only match. A match does not necessarily identify the original input: a different candidate could have the same hash (a collision), so a match only shows that the candidate fits the hash.
3. **Public salt:** the salt is 16 random bytes, written as 32 hex characters and appended to the input as raw bytes. The attacker knows the salt, so one target takes the same effort as without salt: 7165 attempts (~20 ms). The salt only prevents reuse. Without salt, one table of all 10,000 hashes cracks every target by lookup. With salt, every target has its own salt and needs its own 10,000 hashes: 5 targets took 50,000 hashes (~134 ms) instead of 10,000 (~26 ms).
4. **Secret salt:** for `H(input || r)` with an unknown 128-bit `r`, the search space grows to `10000 * 2^128` combinations, which is not feasible to brute force. Once `r` is revealed, anyone can check that `H(input || r)` equals the published hash, and the 10,000 candidates can be brute forced again with that `r`.

## Use of AI

Claude Opus 5.5 was used for file generation, shell and benchmarking script writing and improvement.
