import csv
from pathlib import Path
import matplotlib.pyplot as plt

OUT_DIR = Path(__file__).resolve().parent / "out"
STEP = 100 / 256
bit_diffs = []

with open(OUT_DIR / "avalanche.csv", newline="") as file:
   reader = csv.reader(file)
   next(reader)

   for row in reader:
      bit_diffs.append(float(row[1]))

low, high = min(bit_diffs), max(bit_diffs)
bins = [low - STEP / 2 + i * STEP for i in range(round((high - low) / STEP) + 2)]

plt.figure(figsize=(9, 5))
plt.hist(bit_diffs, bins=bins, edgecolor="black", linewidth=0.3)

plt.xlabel("Bit difference (%)")
plt.ylabel("Number of pairs")

plt.savefig(OUT_DIR / "avalanche_hist.png", dpi=300)
plt.show()
plt.close()
