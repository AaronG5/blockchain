import csv
from pathlib import Path
import matplotlib.pyplot as plt

OUT_DIR = Path(__file__).resolve().parent / "out"
STEP = 100 / 256
v2_bits = []
sha_bits = []
sizes = []
v2_times = []
sha_times = []

with open(OUT_DIR / "avalanche.csv", newline="") as file:
   reader = csv.reader(file)
   next(reader)

   for row in reader:
      v2_bits.append(float(row[1]))
      sha_bits.append(float(row[2]))

with open(OUT_DIR / "bench.csv", newline="") as file:
   reader = csv.reader(file)
   next(reader)

   for row in reader:
      sizes.append(float(row[1]))
      v2_times.append(float(row[2]))
      sha_times.append(float(row[3]))

low = min(v2_bits + sha_bits)
high = max(v2_bits + sha_bits)
bins = [low - STEP / 2 + i * STEP for i in range(round((high - low) / STEP) + 2)]

plt.figure(figsize=(9, 5))
plt.hist(v2_bits, bins=bins, alpha=0.6, label="hash_v2")
plt.hist(sha_bits, bins=bins, alpha=0.6, label="SHA-256")

plt.xlabel("Bit difference (%)")
plt.ylabel("Number of pairs")
plt.legend()

plt.savefig(OUT_DIR / "avalanche_compare.png", dpi=300)
plt.show()
plt.close()

plt.figure(figsize=(9, 5))
plt.plot(sizes, v2_times, "o-", label="hash_v2")
plt.plot(sizes, sha_times, "o-", label="SHA-256")

plt.xscale("log")
plt.yscale("log")

plt.xlabel("File size (bytes, log)")
plt.ylabel("Time (seconds, log)")
plt.legend()

plt.savefig(OUT_DIR / "bench_compare.png", dpi=300)
plt.show()
plt.close()
