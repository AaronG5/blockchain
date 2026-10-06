#!/usr/bin/env python3
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
EXE = SCRIPT_DIR.parents[1] / ("cli_hasher.exe" if os.name == "nt" else "cli_hasher")  # 2 levels up
INPUT_DIR = SCRIPT_DIR / "gen"
REPORT = SCRIPT_DIR / "exp1_results.txt"
HASH_LEN = 64
TIMEOUT_S = 20


def run(workdir, stdin_bytes):
   """Returns (status, value); status: HASH / REJECTED / CRASH / NONE."""
   try:
      p = subprocess.run([str(EXE)], input=stdin_bytes, cwd=workdir,
                        capture_output=True, timeout=TIMEOUT_S)
   except subprocess.TimeoutExpired:
      return "CRASH", "timeout"
   if p.returncode != 0:
      return "CRASH", f"exit code {p.returncode}"
   out = p.stdout.decode("utf-8", errors="replace")
   m = re.search(r"Hash: (\S+)", out)
   if m:
      return "HASH", m.group(1)
   m = re.search(r"Error: (.*)", out)
   if m:
      return "REJECTED", m.group(1).strip()
   return "NONE", ""


def main():
   if not EXE.exists():
      sys.exit(f"Executable not found: {EXE}")
   image = EXE.parent / "image.png"
   if not image.exists():
      sys.exit(f"image.png not found next to the executable: {image}")
   files = sorted(INPUT_DIR.glob("*.txt"))
   if not files:
      sys.exit(f"No .txt files in {INPUT_DIR}. Run gen_files.py first.")

   rows = []
   with tempfile.TemporaryDirectory() as tmp:
      workdir = Path(tmp)
      shutil.copy(image, workdir / "image.png")

      for path in files:
         data = path.read_bytes()
         typable = b"\n" not in data and b"\r" not in data

         f_status, f_val = run(workdir, b"2\n1\n" + os.fsencode(path) + b"\n3\n")
         if typable:
            t_status, t_val = run(workdir, b"1\n" + data + b"\n3\n")
         else:
            t_status, t_val = None, None

         if f_status == "REJECTED":
            length, match, result = "n/a", "n/a", f"REJECTED ({f_val})"
         elif f_status != "HASH" or t_status not in (None, "HASH"):
            length, match, result = "n/a", "n/a", "FAIL (crash / no output)"
         else:
            hashes = [f_val] + ([t_val] if typable else [])
            length = "OK" if all(len(h) == HASH_LEN for h in hashes) else "FAIL"
            match = ("yes" if f_val == t_val else "NO") if typable else "n/a"
            result = "PASS" if length == "OK" and match != "NO" else "FAIL"

         rows.append((path.name, len(data), length, match, result))

   w = max(len(r[0]) for r in rows) + 2
   header = ("File".ljust(w) + "Bytes".rjust(6) + "  " + "Length".ljust(8)
            + "Text==File".ljust(12) + "Result")
   lines = [f"Executable: {EXE}",
            f"Required hash length: {HASH_LEN} hex digits ({HASH_LEN * 4} bits)", "",
            header, "-" * len(header)]
   for name, size, length, match, result in rows:
      lines.append(name.ljust(w) + str(size).rjust(6) + "  " + length.ljust(8)
                  + match.ljust(12) + result)

   passed = sum(r[4] == "PASS" for r in rows)
   failed = sum(r[4].startswith("FAIL") for r in rows)
   rejected = sum(r[4].startswith("REJECTED") for r in rows)
   lines += ["", f"Files: {len(rows)} | PASS: {passed} | FAIL: {failed} | REJECTED: {rejected}"]

   REPORT.write_text("\n".join(lines) + "\n", encoding="utf-8")
   print("\n".join(lines[-1:]))
   print(f"Report: {REPORT}")
   sys.exit(1 if failed else 0)


if __name__ == "__main__":
   main()