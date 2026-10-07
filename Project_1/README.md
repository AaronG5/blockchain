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
2. The input is converted into an `int` array (`char[] -> int[]`).
3. The array is summed into a single `int` value, and the input length multiplied by 256 is added to it. The result is used to offset the X and Y coordinates of the pixel lookup in step 4.
4. The array is processed in groups of four elements, with each group split into two pairs (elements past the end of the array count as 0). Each pair `(a, b)` is combined as `a * 256 + b`. The offset is added to both results, along with the position of the output byte being produced (0–31) multiplied by 7 for X and by 13 for Y, so the same elements point to a different pixel at each output position instead of repeating one pixel. The results are used as the X and Y coordinates of a pixel in the image. If a coordinate exceeds the image resolution, it is wrapped using the modulo operation, and the quotient of the original coordinate divided by the image resolution is added to the result.
5. The pixel's RGB values are combined with bitwise XOR: `R ^ G ^ B`.
6. The result is converted to hexadecimal and appended to the hash string.
7. If the hash is shorter than 64 hex characters, the pixel lookup is run again, with the `int` array shifted by the current iteration count.
8. If the hash is longer than 64 hex characters, it is truncated to 64 characters.
9. The hash is returned.

# Running the hash function (macOS or Linux)

From the `Project_1` directory:

```bash
make build
./cli_hasher
```

The command-line program is [`cli.cpp`](cli.cpp).

# Eksperimentinis tyrimas: teisingumas ir sparta

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
5. [`graph.py`](Experiment/exp1/graph.py) generates a graph from `bench_konst.csv` to visualize the hash function's speed and saves it as `out_konst/bench_graph.png`.
