import csv
from pathlib import Path
import matplotlib.pyplot as plt

OUT_DIR = Path(__file__).resolve().parent / "out"
STEP = 100 / 256
original_bits = []
improved_bits = []
sizes = []
original_times = []
improved_times = []

with open(OUT_DIR / "avalanche.csv", newline="") as file:
   reader = csv.reader(file)
   next(reader)

   for row in reader:
      original_bits.append(float(row[1]))
      improved_bits.append(float(row[2]))

with open(OUT_DIR / "bench.csv", newline="") as file:
   reader = csv.reader(file)
   next(reader)

   for row in reader:
      sizes.append(float(row[1]))
      original_times.append(float(row[2]))
      improved_times.append(float(row[3]))

low = min(original_bits + improved_bits)
high = max(original_bits + improved_bits)
bins = [low - STEP / 2 + i * STEP for i in range(round((high - low) / STEP) + 2)]

plt.figure(figsize=(9, 5))
plt.hist(original_bits, bins=bins, alpha=0.6, label="Original (hash.hpp)")
plt.hist(improved_bits, bins=bins, alpha=0.6, label="Improved (hash_v2.hpp)")

plt.xlabel("Bit difference (%)")
plt.ylabel("Number of pairs")
plt.legend()

plt.savefig(OUT_DIR / "avalanche_compare.png", dpi=300)
plt.show()
plt.close()

plt.figure(figsize=(9, 5))
plt.plot(sizes, original_times, "o-", label="Original (hash.hpp)")
plt.plot(sizes, improved_times, "o-", label="Improved (hash_v2.hpp)")

plt.xscale("log")
plt.yscale("log")

plt.xlabel("File size (bytes, log)")
plt.ylabel("Time (seconds, log)")
plt.legend()

plt.savefig(OUT_DIR / "bench_compare.png", dpi=300)
plt.show()
plt.close()
