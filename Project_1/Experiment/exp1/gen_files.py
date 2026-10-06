#!/usr/bin/env python3
"""
Experiment 1: input file generator.

Usage:
   python gen_files.py [--seed 12345]

Files are written to "<script directory>/gen/".
"""

import argparse
import hashlib
import json
import platform
import random
import string
import sys
from datetime import datetime, timezone
from pathlib import Path

# Input alphabet: visible ASCII only (codes 32-126)
ALPHABET = string.ascii_letters + string.digits + string.punctuation + " "

# (size, mutation label, mutation position) -> base file + one changed copy
RANDOM_CASES = [
   (1001, "end", lambda n: n - 1),
   (2003, "start", lambda n: 0),
   (1456, "mid", lambda n: n // 2),
]

OUT_DIR = Path(__file__).resolve().parent / "gen"


def sha256(data: bytes) -> str:
   return hashlib.sha256(data).hexdigest()


def random_ascii(rng: random.Random, size: int) -> bytes:
   return "".join(rng.choice(ALPHABET) for _ in range(size)).encode("ascii")


def mutate_one_byte(rng: random.Random, data: bytes, pos: int):
   """Change exactly one byte; the new value is guaranteed to differ."""
   old = data[pos]
   new = ord(rng.choice([c for c in ALPHABET if ord(c) != old]))
   mutated = bytearray(data)
   mutated[pos] = new
   assert new != old
   assert sum(a != b for a, b in zip(data, mutated)) == 1
   return bytes(mutated), old, new


def main():
   ap = argparse.ArgumentParser()
   ap.add_argument("--seed", type=int, default=12345, help="RNG seed")
   args = ap.parse_args()

   rng = random.Random(args.seed)
   OUT_DIR.mkdir(parents=True, exist_ok=True)
   entries = []

   def add(name, data, category, extra=None):
      (OUT_DIR / name).write_bytes(data)
      entry = {
         "file": name,
         "category": category,
         "bytes": len(data),
         "chars": len(data.decode("utf-8")),
         "sha256": sha256(data),
      }
      if extra:
         entry.update(extra)
      entries.append(entry)

   # Trivial cases (no trailing newline)
   add("empty.txt", b"", "trivial")
   add("one_a.txt", b"a", "trivial")
   add("one_b.txt", b"b", "trivial")

   # Random ASCII files + copies with exactly one changed byte
   for size, label, pos_fn in RANDOM_CASES:
      data = random_ascii(rng, size)
      base = f"random_{size}.txt"
      add(base, data, "random_ascii")

      pos = pos_fn(size)
      mutated, old, new = mutate_one_byte(rng, data, pos)
      add(f"random_{size}_change_{label}.txt", mutated, "mutated_copy", {
         "original": base,
         "position": pos,
         "old_byte": old,
         "new_byte": new,
      })

   # Structured cases
   add("repeat_a_100.txt", b"a" * 100, "structured",
      {"note": "single repeated character"})
   add("repeat_abc_127.txt", (b"abc" * 43)[:127], "structured",
      {"note": "repeated pattern 'abc', cut to 127 bytes"})

   text = b"The quick brown fox jumps over the lazy dog"
   chars = list(text)
   shuffled = chars[:]
   while shuffled == chars:
      rng.shuffle(shuffled)
   add("order_original.txt", text, "structured", {"note": "original order"})
   add("order_chars_shuffle.txt", bytes(shuffled), "structured",
      {"note": "same characters, different order"})

   add("whitespace_none.txt", b"hello world", "structured")
   add("whitespace_leading.txt", b"   hello world", "structured")
   add("whitespace_trailing.txt", b"hello world   ", "structured")
   add("whitespace_both.txt", b"   hello world   ", "structured")

   add("newline_without.txt", b"hello world", "structured")
   add("newline_with_n.txt", b"hello world\n", "structured", {"note": "LF"})
   add("newline_with_rn.txt", b"hello world\r\n", "structured", {"note": "CRLF"})

   # UTF-8 example: character count differs from byte count
   s = "Ąžuolas žaliuoja šalia ūkio, čia gyvena ėdrūs kačiukai."
   data = s.encode("utf-8")
   add("utf8_lt.txt", data, "utf8", {
      "note": "character count differs from byte count",
      "char_count": len(s),
      "byte_count": len(data),
   })

   # Manifest with everything needed to reproduce the experiment
   manifest = {
      "created_utc": datetime.now(timezone.utc).isoformat(),
      "seed": args.seed,
      "alphabet": ALPHABET,
      "alphabet_size": len(ALPHABET),
      "random_file_sizes": [c[0] for c in RANDOM_CASES],
      "environment": {
         "python_version": sys.version,
         "implementation": platform.python_implementation(),
         "os": platform.platform(),
         "machine": platform.machine(),
         "processor": platform.processor(),
         "compiler": platform.python_compiler(),
         "build_options": "-std=c++17 -O3",
      },
      "reproduce_command": f"python {Path(__file__).name} --seed {args.seed}",
      "files": entries,
   }
   (OUT_DIR / "manifest.json").write_text(
      json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
   )

   print(f"Generated {len(entries)} files + manifest.json in '{OUT_DIR}'")


if __name__ == "__main__":
   main()