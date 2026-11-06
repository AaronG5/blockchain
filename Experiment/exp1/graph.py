import csv
from pathlib import Path
import matplotlib.pyplot as plt

OUT_DIR = Path(__file__).resolve().parent / "out_konst"
sizes = []
times = []

with open(OUT_DIR / "bench_konst.csv", newline="") as file:
   reader = csv.reader(file, skipinitialspace=True)
   next(reader)  # skip header

   for row in reader:
      print(row)
      sizes.append(float(row[1]))
      times.append(float(row[2]))

plt.figure(figsize=(9, 5))
plt.plot(sizes, times)
plt.plot(sizes, times, 'o')

plt.xscale("log")
plt.yscale("log")

plt.xlabel("File size (bytes, log)")
plt.ylabel("Time (seconds, log)")

plt.savefig(OUT_DIR / "bench_graph.png", dpi=300)
plt.show()
plt.close()